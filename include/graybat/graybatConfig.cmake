# - Config file for the graybat package
# It defines the following variables
#  graybat_INCLUDE_DIRS - include directories for FooBar
#  graybat_LIBRARIES    - libraries to link against

###############################################################################
# graybat
###############################################################################
cmake_minimum_required(VERSION 3.3.0)
project("graybat")

set(graybat_INCLUDE_DIRS ${graybat_INCLUDE_DIRS} "${graybat_DIR}/include")
set(graybat_INCLUDE_DIRS ${graybat_INCLUDE_DIRS} "${graybat_DIR}/include/graybat/utils/hana/include/")


###############################################################################
# COMPILER FLAGS
###############################################################################
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")


###############################################################################
# MODULES
###############################################################################
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${graybat_DIR}/include/graybat/utils/cmake/modules/" )

###############################################################################
# DEPENDENCIES
###############################################################################

###############################################################################
# METIS LIB
###############################################################################
find_package(METIS MODULE 5.1.0)
include_directories(SYSTEM ${METIS_INCLUDE_DIRS})
set(graybat_LIBRARIES ${graybat_LIBRARIES} ${METIS_LIBRARIES})

###############################################################################
# ZMQ LIB
###############################################################################
find_package(ZMQ MODULE 4.0.0)
include_directories(SYSTEM ${ZMQ_INCLUDE_DIRS})
set(graybat_LIBRARIES ${graybat_LIBRARIES} ${ZMQ_LIBRARIES})

###############################################################################
# Boost LIB
###############################################################################
find_package(Boost 1.56.0 MODULE COMPONENTS mpi serialization REQUIRED)
include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
set(graybat_LIBRARIES ${graybat_LIBRARIES} ${Boost_LIBRARIES})

################################################################################
# MPI LIB
################################################################################
find_package(MPI MODULE)
include_directories(SYSTEM ${MPI_C_INCLUDE_PATH})
set(graybat_LIBRARIES ${graybat_LIBRARIES} ${MPI_C_LIBRARIES})
set(graybat_LIBRARIES ${graybat_LIBRARIES} ${MPI_CXX_LIBRARIES})

################################################################################
# Find PThreads
################################################################################
find_package(Threads MODULE)
set(graybat_LIBRARIES ${graybat_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
