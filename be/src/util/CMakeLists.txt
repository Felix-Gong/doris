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

# where to put generated libraries
set(LIBRARY_OUTPUT_PATH "${BUILD_DIR}/src/util")

# where to put generated binaries
set(EXECUTABLE_OUTPUT_PATH "${BUILD_DIR}/src/util")

file(GLOB_RECURSE UTIL_FILES CONFIGURE_DEPENDS *.cpp *.cc)

if (OS_MACOSX)
    list(REMOVE_ITEM UTIL_FILES ${CMAKE_CURRENT_SOURCE_DIR}/perf_counters.cpp ${CMAKE_CURRENT_SOURCE_DIR}/disk_info.cpp)
else ()
    list(REMOVE_ITEM UTIL_FILES ${CMAKE_CURRENT_SOURCE_DIR}/perf_counters_mac.cpp ${CMAKE_CURRENT_SOURCE_DIR}/disk_info_mac.cpp)
endif()

list(REMOVE_ITEM UTIL_FILES ${CMAKE_CURRENT_SOURCE_DIR}/libjvm_loader.cpp)

if (ENABLE_TLS)
    list(REMOVE_ITEM UTIL_FILES ${CMAKE_CURRENT_SOURCE_DIR}/oss/client_connection_provider.cpp)
    list(APPEND UTIL_FILES ${CMAKE_CURRENT_SOURCE_DIR}/../${TLS_MODULE_DIR}/client_connection_provider.cpp)
endif()

add_library(Util STATIC
    ${UTIL_FILES}
)

# crc32c_riscv.cc carries the Zbc/Zvbc CRC32C accelerators. The code is gated
# by __riscv_zbc / __riscv_zvbc and probes /proc/cpuinfo at runtime, so cores
# without the extensions fall back to the software crc32c library. Compile it
# with the extensions enabled per-file (matching how x86_64 compiles AVX2
# files separately from the -msse4.2 baseline); Zvbc added only when the
# compiler understands it (GCC 14+; clang 17 rejects -march=..._zvbc).
if (ARCH_RISCV64)
    # The Zvbc march is accepted as a flag by GCC 12+, but crc32c_riscv.cc
    # also needs the C vector intrinsic API (__riscv_vsetvlmax_e64m1 etc.)
    # which only exists in GCC 13+. Compile the real code to decide: a
    # toolchain that takes the flag but lacks the intrinsics (GCC 12, clang
    # 17) falls back to the non-vector rv64gcv_zba_zbc build.
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/zvbc_probe.cpp
"#include <riscv_vector.h>
void probe(void) {
    size_t vl = __riscv_vsetvlmax_e64m1();
    vuint64m1_t v = __riscv_vmv_v_x_u64m1(1, vl);
    (void)v;
}
int main(void) { probe(); return 0; }
")
    try_compile(COMPILER_SUPPORTS_ZVBC
                ${CMAKE_CURRENT_BINARY_DIR}/zvbc_build
                ${CMAKE_CURRENT_BINARY_DIR}/zvbc_probe.cpp
                CMAKE_FLAGS "-DCMAKE_CXX_FLAGS=-march=rv64gcv_zba_zbc_zvbc"
                OUTPUT_VARIABLE zvbc_try_out)
    set(RISCV_CRC32C_MARCH "rv64gcv_zba_zbc")
    if (COMPILER_SUPPORTS_ZVBC)
        set(RISCV_CRC32C_MARCH "rv64gcv_zba_zbc_zvbc")
    endif()
    message(STATUS "crc32c_riscv.cc march: ${RISCV_CRC32C_MARCH}")
    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/crc32c_riscv.cc
        PROPERTIES COMPILE_OPTIONS "-march=${RISCV_CRC32C_MARCH}")
endif()

pch_reuse(Util)

# Unity build for the small util TUs (parse-overhead dominated). Ten files
# leak file-scope macros and stay individual; hash/city.cc keeps its generic
# k0..k4 static hash constants away from sibling scopes. perf_counters.cpp
# is a Linux-only member (macOS builds its _mac variant, which is clean).
# crc32c_riscv.cc stays individual so its per-file -march (Zbc/Zvbc) applies
# to its own TU rather than a merged unity batch.
set(UTIL_UNITY_SKIP
    ${CMAKE_CURRENT_SOURCE_DIR}/bfd_parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/crc32c_riscv.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/hash/city.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/hash/murmur_hash3.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/network_util.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_info.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_util.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/perf_counters.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/simd/parquet_kernels.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thrift_util.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/zlib.cpp)
doris_skip_unity_inclusion(${UTIL_UNITY_SKIP})
set_target_properties(Util PROPERTIES UNITY_BUILD ${DORIS_UNITY_BUILD}
                                      UNITY_BUILD_BATCH_SIZE 24)
