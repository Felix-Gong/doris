// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#pragma once

#include <stdint.h>

#include <cstddef>

#if defined(__riscv) && (__riscv_xlen == 64) && defined(__riscv_zbc)
namespace doris::crc32c_riscv {

// Runtime probe: true when the host CPU advertises RISC-V Zbc.
bool available();

// Zbc-accelerated CRC32C, bit-exact with crc32c::Crc32c(). Falls back to a
// bitwise path internally for inputs shorter than 64 bytes.
uint32_t compute(uint32_t crc, const void* buf, size_t size);

} // namespace doris::crc32c_riscv
#endif // __riscv && __riscv_xlen == 64 && __riscv_zbc