#ifndef LEVMARQ_HPP
#define LEVMARQ_HPP
//! \file

#include <cstdio>
#include "../Config/Types.hpp"
#include "MatMul.hpp"
#include "GaussJordan.hpp"
#include "../Config/FitFunctions/FitFunctor.hpp"
#include "../Utility/CudaStopWatch.hpp"
#include "../Utility/PrintMat.hpp"

DEVICE float getSample(const cudaTextureObject_t texObj, const float I, const int INDEXDATASET) {
	return tex2D<float>(texObj, I+0.5, static_cast<float>(INDEXDATASET)+0.5);
}

template <class Fit, unsigned int bs, class MatrixAccess>
DEVICE void calcF(const cudaTextureObject_t texObj, const float* const param, MatrixAccess& F, const Window& window, const unsigned int sample_count, const unsigned int interpolation_count) {
	for(int i = threadIdx.x; i < window.width+Fit::numberOfParams; i+=bs) {
		const int x = i*interpolation_count+window.offset;
		F[make_uint2(0,i)] = 0;
		if(i < window.width) {
			const float xval = static_cast<float>(x);
			const float yval = getSample(texObj,x,blockIdx.x);
			F[make_uint2(0,i)] = -1*Fit::modelFunction(xval,yval,param);
			//printf("F[%i]=%f\n",i,F[i]);
		}	
	}
	__syncthreads();
}



template <class Fit, unsigned int bs, class MatrixAccess>
DEVICE void calcDerivF(const cudaTextureObject_t texObj, const float * const param, const float mu, MatrixAccess& deriv_F, const Window& window, const unsigned sample_count, const unsigned int interpolation_count) {
	const unsigned int imax = Fit::numberOfParams*(window.width+Fit::numberOfParams);
	for(unsigned int i = threadIdx.x; i < imax; i+=bs) {
		const unsigned int x = i%Fit::numberOfParams;
		const unsigned int y = i/Fit::numberOfParams;
		deriv_F[make_uint2(x,y)] = 0;
		if(y < window.width) {
			deriv_F[make_uint2(x,y)] = Fit::derivation(x,static_cast<float>(window.offset+y*interpolation_count),getSample(texObj, window.offset+y*interpolation_count, blockIdx.x),param);
		} else if(y < window.width + Fit::numberOfParams) {
			if((y - window.width) == x) {
				deriv_F[make_uint2(x,y)] = mu;
			}
		}
		if(deriv_F[make_uint2(x,y)] != deriv_F[make_uint2(x,y)]) printf("A[%i, %i] is %f", x, y, deriv_F[make_uint2(x,y)]);
	}
	__syncthreads();
}

template <class Fit, unsigned int bs>
__global__ void levMarqIt(const cudaTextureObject_t texObj, FitData* const results, const unsigned sample_count, const unsigned int max_window_size, const unsigned int interpolation_count, float* mem, TickCounter* swMem = NULL) {
	//DEBUG ONLY!
	//if(blockIdx.x != 0) return;
	
	const unsigned int numberOfParams = Fit::numberOfParams;
	//const unsigned int SPACE = ((max_window_size+numberOfParams)*2+(max_window_size+numberOfParams)*numberOfParams);
	const unsigned int memOffset = blockIdx.x*SPACE;
	__shared__ MatrixAccess<> G,G_inverse,u1,u2,u3,AT,FT,F1T;
	__shared__ MatrixAccess<float, trans> F,F1,b,s,A,param,param2,param_last_it;
	__shared__ float sleft[bs*Fit::numberOfParams];
	__shared__ float b_shared[numberOfParams], s_shared[numberOfParams], G_shared[numberOfParams*numberOfParams], 
					 G_inverse_shared[numberOfParams*numberOfParams],
					 param_shared[numberOfParams], param2_shared[numberOfParams],
					 u1_shared[1], u2_shared[1], u3_shared[1];
					 
	CUDA_SW_INIT(10, swMem);
					 
	__shared__ bool finished;
	if(threadIdx.x == 0) {
		F = MatrixAccess<float, trans>(&mem[memOffset], max_window_size+numberOfParams, 1);
		F1 = MatrixAccess<float, trans>(&mem[memOffset+max_window_size+numberOfParams], max_window_size+numberOfParams, 1);
		A = MatrixAccess<float, trans>(&mem[memOffset+(max_window_size+numberOfParams)*2], max_window_size+numberOfParams, numberOfParams);
		b = MatrixAccess<float, trans>(b_shared, numberOfParams, 1);
		s = MatrixAccess<float, trans>(s_shared, numberOfParams, 1);
		AT = A.transpose();
		G = MatrixAccess<>(G_shared, numberOfParams, numberOfParams);
		G_inverse = MatrixAccess<>(G_inverse_shared, numberOfParams, numberOfParams);
		param = MatrixAccess<float, trans>(param_shared, numberOfParams, 1);
		param2 = MatrixAccess<float, trans>(param2_shared, numberOfParams, 1);
		u1 = MatrixAccess<>(u1_shared, 1,1);
		u2 = MatrixAccess<>(u2_shared, 1,1);
		u3 = MatrixAccess<>(u3_shared, 1,1);
		FT = F.transpose();
		F1T = F1.transpose();
	}	
	__syncthreads();
	float mu = 1, roh;
	unsigned int counter = 0;
	if(threadIdx.x < numberOfParams) {
		param[make_uint2(0,threadIdx.x)] = 0;
	};		
	
	__shared__ Window window;
	Fit::getWindow(texObj, window, blockIdx.x, sample_count);
	const int sampling_points = window.width/interpolation_count;
	Fit::guessParams(texObj, param, window);
	__syncthreads();
	do {
		counter++;
		CUDA_SW_START();
		/* Abschnitt 1 */
		//Calc F(param)
		calcF<Fit, bs>(texObj, param.getRawPointer(), F, window, sample_count, interpolation_count); //30%
		//if(threadIdx.x == 0 && blockIdx.x == 0) printMat(param.transpose());
		CUDA_SW_STOP(); //1
		//Calc F'(param)
		calcDerivF<Fit, bs>(texObj, param.getRawPointer(), mu, A, window, sample_count, interpolation_count);
		//if(threadIdx.x == 0 && blockIdx.x == 0) printMat(A);
		CUDA_SW_STOP(); //2
		/* Abschnitt 2 */
		//Solve minimization problem
		//calc A^T*A => G
		
		matProdKernel<bs, Fit::numberOfParams>(G, AT, A, sleft);
		//if(threadIdx.x == 0 && blockIdx.x == 0) printf("G:\n");
		//if(threadIdx.x == 0 && blockIdx.x == 0) printMat(G);
		
		CUDA_SW_STOP(); //3
		//calc G^-1
		gaussJordan<Fit,bs>(G_inverse, G);
		CUDA_SW_STOP(); //4
		//if(threadIdx.x == 0 && blockIdx.x == 0) printMat(G_inverse);
		//calc A^T*F => b		
		matProdKernel<bs, Fit::numberOfParams>(b, AT, F, sleft);
		//if(threadIdx.x == 0 && blockIdx.x == 0) printMat(s);
		CUDA_SW_STOP(); //5
		//calc G^-1*b => s
		matProdKernel<bs, Fit::numberOfParams>(s, G_inverse, b, sleft);
		CUDA_SW_STOP(); //6
		/* Abschnitt 3 */
		//Reduce F(param)
		matProdKernel<bs, 1>(u1, FT, F, sleft);
		//Calc F(param+s)
		const uint2 c = make_uint2(0,threadIdx.x);
		if(threadIdx.x < numberOfParams) param2[c] = param[c] + s[c];
		__syncthreads();
		//Fold F(param+s)
		calcF<Fit, bs>(texObj, param2.getRawPointer(), F1, window, sample_count, interpolation_count); //30%
		matProdKernel<bs, 1>(u2, F1T, F1, sleft);
		CUDA_SW_STOP(); //7
		//Calc F'(param)*s
		matProdKernel2<bs, Fit::numberOfParams>(F1, A, s, sleft); //30%
		CUDA_SW_STOP(); //8
		//Calc F(param) + F'(param)*s'
		//Fold F'(param)*s
		for(int j = threadIdx.x; j < sampling_points; j+=bs) {
			uint2 c = make_uint2(0,j);
			F1[c] = -1*F[c]+F1[c];
		}
		if(threadIdx.x == 0) finished = true;
		CUDA_SW_STOP(); //9
		__syncthreads();
		matProdKernel<bs, 1>(u3, F1T, F1, sleft);
		//calc roh
		const float z = u1[make_uint2(0,0)]-u2[make_uint2(0,0)];
		const float n = u1[make_uint2(0,0)]-u3[make_uint2(0,0)];
		roh = (z/n); //Div by 0 if not valid
		//if(threadIdx.x == 0 && blockIdx.x == 0) printf("z=%f, n=%f, roh=z/n=%f\n", z, n, roh);
		CUDA_SW_STOP(); //10
		//decide if s is accepted or discarded
		if(roh <= 0.2 || roh != roh) {
			//s verwerfen, mu erhöhen
			mu *= 2;
			if(threadIdx.x == 0 && roh == roh) finished = false;
		} else  {
				uint2 j = make_uint2(0,threadIdx.x);
				if(threadIdx.x < numberOfParams) {
					param[j] = param[j] + s[j];
					if(s[j] > 1e-6) finished = false;
				}
			if( roh >= 0.8){
				mu /= 2;
			}
		}
		//if(threadIdx.x == 0 && blockIdx.x == 0) printf("roh=%f, mu=%f, u1=%f, u2=%f, u3=%f\n", roh, mu, u1_shared[0], u2_shared[0], u3_shared[0]);
		//if(threadIdx.x == 0 && blockIdx.x == 0) printMat(param.transpose());
		//if(threadIdx.x == 0 && blockIdx.x == 0) printMat(s.transpose());
		__syncthreads();
	} while(!finished && counter < MAX_ITERATIONS);
	if(threadIdx.x < numberOfParams) {
		const float p = param[make_uint2(0,threadIdx.x)];
		results[blockIdx.x].param[threadIdx.x] = p;
	}
	if(threadIdx.x == numberOfParams) {
		results[blockIdx.x].status = !finished;
		results[blockIdx.x].woffset = window.offset;
	}

	return;
}

template <class Fit>
int levenbergMarquardt(cudaStream_t& stream, cudaTextureObject_t texObj, FitData* results, const unsigned sample_count, const unsigned int max_window_size, const unsigned int chunk_count, const unsigned int interpolation_count, float* mem) {
	const unsigned int bsx = 128;
	const dim3 gs(chunk_count,1);
	const dim3 bs(bsx,1);
	//TickCounter* swMem = NULL;
	//cudaMalloc((void**) &swMem, 10*sizeof(TickCounter));
	//initTicks<<<1,10,0, stream>>>(swMem);
	levMarqIt<Fit,bsx><<<gs,bs, 0, stream>>>(texObj, results, sample_count, max_window_size,interpolation_count, mem);
	CUDA_SW_PRINT(10,swMem, stream);
	handleLastError();
	//cudaFree(swMem);
	return 0;

}

#endif
