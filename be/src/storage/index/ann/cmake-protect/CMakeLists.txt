# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

# Make sure compile check in doris will not break compilation of faiss and openblas
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")

# RISC-V: Use system OpenBLAS instead of building from contrib.
# Building OpenBLAS from contrib fails on RISC-V due to assembly issues.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "riscv64")
    message(STATUS "RISC-V: Using system OpenBLAS (/usr/lib64/libopenblas.a)")
    # Set BLAS variables so FAISS's find_package(BLAS) finds the system one
    set(BLAS_LIBRARIES "/usr/lib64/libopenblas.a" CACHE FILEPATH "System OpenBLAS library path")
    set(BLAS_INCLUDE_DIRS "/usr/include" CACHE PATH "OpenBLAS include directory")
    set(OPENBLAS_LIBRARY "/usr/lib64/libopenblas.a" CACHE FILEPATH "System OpenBLAS library path")
    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};/usr/lib64/pkgconfig" CACHE STRING "CMake prefix path")
else()
    # Non-RISC-V: Build OpenBLAS from contrib as normal
    set(BUILD_WITHOUT_LAPACK OFF CACHE BOOL "Disable LAPACK support in OpenBLAS")
    set(NO_SHARED TRUE CACHE BOOL "Disable shared library in OpenBLAS")
    set(C_LAPACK TRUE CACHE BOOL "Enable C interface for LAPACK in OpenBLAS")
    set(USE_OPENMP TRUE CACHE BOOL "Enable OpenMP in OpenBLAS")
    set(NOFORTRAN ON CACHE BOOL "Disable Fortran in OpenBLAS")
    set(BUILD_STATIC_LIBS ON CACHE BOOL "Build static libraries in OpenBLAS")
    set(BUILD_TESTING OFF CACHE BOOL "Build shared libraries in OpenBLAS")
    set(BUILD_RELAPACK ON CACHE BOOL "Build relapack in OpenBLAS")
    set(BUILD_BENCHMARKS OFF CACHE BOOL "Build benchmarks in OpenBLAS")
    set(NO_LAPACK OFF CACHE BOOL "Disable LAPACK in OpenBLAS")
    set(NO_CBLAS ON CACHE BOOL "Disable CBLAS in OpenBLAS")
    set(NO_AVX512 ON CACHE BOOL "Disable AVX512 in OpenBLAS")

    if (DEFINED ENABLE_DYNAMIC_ARCH)
        if (NOT CMAKE_SYSTEM_NAME MATCHES "Darwin" AND NOT CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
            set(DYNAMIC_ARCH "${ENABLE_DYNAMIC_ARCH}" CACHE BOOL "Enable runtime CPU detection in OpenBLAS")
        endif ()
    else()
        if (NOT CMAKE_SYSTEM_NAME MATCHES "Darwin" AND NOT CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
            set(DYNAMIC_ARCH ON CACHE BOOL "Enable runtime CPU detection in OpenBLAS")
        endif ()
    endif ()
    if (DEFINED USE_AVX2 AND NOT USE_AVX2)
        set(NO_AVX2 ON CACHE BOOL "Disable AVX2 support in OpenBLAS" FORCE)
        set(NO_AVX ON CACHE BOOL "Disable AVX support in OpenBLAS" FORCE)
    else()
        set(NO_AVX OFF CACHE BOOL "Disable AVX support in OpenBLAS" FORCE)
        set(NO_AVX2 OFF CACHE BOOL "Disable AVX2 support in OpenBLAS" FORCE)
    endif()

    # EXCLUDE_FROM_ALL so that binary in openblas is not installed.
    add_subdirectory(${PROJECT_SOURCE_DIR}/../contrib/openblas ${PROJECT_BINARY_DIR}/openblas EXCLUDE_FROM_ALL)
    set(OPENBLAS_LIBRARY "${PROJECT_BINARY_DIR}/openblas/lib/libopenblas.a" CACHE FILEPATH "Path to OpenBLAS build directory")
endif()

# Set FAISS_ENABLE_GPU OFF before including FAISS to prevent CUDA language detection
set(FAISS_ENABLE_GPU OFF CACHE BOOL "Enable GPU support in FAISS")
set(FAISS_ENABLE_MKL OFF CACHE BOOL "Disable MKL support in FAISS")
set(FAISS_ENABLE_PYTHON OFF CACHE BOOL "Disable Python support in FAISS")
set(FAISS_ENABLE_EXTRAS OFF CACHE BOOL "Disable FAISS extras")
set(BUILD_TESTING OFF CACHE BOOL "Disable FAISS testing")
if (DEFINED USE_AVX2 AND NOT USE_AVX2)
    set(FAISS_OPT_LEVEL "generic" CACHE STRING "FAISS CPU dispatch level" FORCE)
else()
    set(FAISS_OPT_LEVEL "generic" CACHE STRING "FAISS CPU dispatch level" FORCE)
endif()

# EXCLUDE_FROM_ALL so that binary in faiss is not installed.
add_subdirectory(${PROJECT_SOURCE_DIR}/../contrib/faiss ${PROJECT_BINARY_DIR}/faiss EXCLUDE_FROM_ALL)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "riscv64")
    # FAISS -O3 on RISC-V clang is extremely slow (hours for template-heavy files).
    # Override to -O1 for practical build times.
    target_compile_options(faiss PRIVATE "-O1")
endif()
