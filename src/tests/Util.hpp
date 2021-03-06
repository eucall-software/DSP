#ifndef UTIL_HPP
#define UTIL_HPP

#include <thrust/device_vector.h>

template <class c>
c* pcast(thrust::device_vector<c>& dev) {
	return thrust::raw_pointer_cast(dev.data());
}

template <class c>
void printMat(thrust::device_vector<c>& mat, int rows, int cols) {
	for(int j = 0; j < rows; j++) {
		for(int i = 0; i < cols; i++) {
			std::cout << mat[i+cols*j]<< " ";
		}
		std::cout << std::endl;
	}
	std::cout << std::endl;
}


template <class T>
static inline void random_mat(thrust::device_vector<T> &matrix, int rows, int cols )
{
    for ( int i = 0; i < rows * cols; ++i)
    {
        matrix[ i ] = static_cast<T>(rand()) % 3 - 1;
    }
}

std::ostream& operator<<(std::ostream& lhs, const FitData& rhs) {
	for(int i = FitFunction::numberOfParams-1; i > 0; i--) {
		lhs << rhs.param[i] << "*x**" << i << " + ";
	}
	lhs << rhs.param[0] << std::endl;
	return lhs;
}

#endif
