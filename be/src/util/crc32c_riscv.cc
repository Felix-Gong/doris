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

// RISC-V Zbc-accelerated CRC32C, ported from MariaDB
// (mysys/crc32/crc32c_riscv.cc, MDEV; adapted from apache/brpc#3312).
//
// Carry-less-multiply (clmul/clmulh) 128-bit 4-way parallel folding with
// Barrett reduction. Bitwise fallback for inputs shorter than 64 bytes.
// Runtime-probed via /proc/cpuinfo ("zbc") so binaries remain portable
// to RISC-V implementations without Zbc.
//
// Measured on riscv64 (clang 17, rv64gcv_zba_zbc, 1 MiB): ~1688 MB/s vs
// ~229 MB/s for the software crc32c library -> ~7.4x; bit-exact with the
// canonical crc32c::Crc32c().

#include <stdint.h>

#include <cstdio>
#include <cstring>

#include "util/slice.h"

#if !(defined(__riscv) && (__riscv_xlen == 64) && defined(__riscv_zbc))
// Non-RISC-V or Zbc-less: empty, so callers fall back to the software library.
namespace doris::crc32c_riscv {
bool available() { return false; }
uint32_t compute(uint32_t crc, const void*, size_t) { return crc; }
} // namespace doris::crc32c_riscv
#else

namespace doris::crc32c_riscv {

// RISC-V Zbc carry-less multiplication inline helpers.
static inline uint64_t rv_clmul(uint64_t a, uint64_t b) {
    uint64_t result;
    __asm__ volatile("clmul %0, %1, %2" : "=r"(result) : "r"(a), "r"(b));
    return result;
}

static inline uint64_t rv_clmulh(uint64_t a, uint64_t b) {
    uint64_t result;
    __asm__ volatile("clmulh %0, %1, %2" : "=r"(result) : "r"(a), "r"(b));
    return result;
}

// Bitwise CRC32C fallback for small chunks.
static inline uint32_t rv_crc32c_bitwise(uint32_t crc, const uint8_t* buf, size_t len) {
    uint32_t c = crc;
    for (size_t i = 0; i < len; ++i) {
        c ^= buf[i];
        for (int k = 0; k < 8; ++k) {
            c = (c >> 1) ^ ((c & 1) ? 0x82F63B78U : 0);
        }
    }
    return c;
}

static inline void rv_fold_pair_xor_data(uint64_t* lo, uint64_t* hi, uint64_t k0, uint64_t k1,
                                         uint64_t d0, uint64_t d1) {
    uint64_t l = rv_clmul(*lo, k0) ^ rv_clmul(*hi, k1);
    uint64_t h = rv_clmulh(*lo, k0) ^ rv_clmulh(*hi, k1);
    *lo = l ^ d0;
    *hi = h ^ d1;
}

static inline void rv_fold_pair_xor_state(uint64_t* lo, uint64_t* hi, uint64_t k0, uint64_t k1,
                                          uint64_t s0, uint64_t s1) {
    uint64_t l = rv_clmul(*lo, k0) ^ rv_clmul(*hi, k1);
    uint64_t h = rv_clmulh(*lo, k0) ^ rv_clmulh(*hi, k1);
    *lo = l ^ s0;
    *hi = h ^ s1;
}

// Folding constants for CRC32C (Castagnoli polynomial 0x1EDC6F41).
static const uint64_t crc32c_fold_const[4] __attribute__((aligned(16))) = {
        0x00000000740eef02ULL, // k1: fold 512->256
        0x000000009e4addf8ULL, // k2: fold 512->256
        0x00000000f20c0dfeULL, // k3: fold 256->128
        0x00000000493c7d27ULL  // k4: fold 256->128
};

#define RV_CRC32C_CONST_0    0x00000000dd45aab8ULL // x^64 mod P
#define RV_CRC32C_CONST_1    0x00000000493c7d27ULL // x^96 mod P
#define RV_CRC32C_CONST_QUO  0x0000000dea713f1ULL  // floor(x^64 / P)
#define RV_CRC32C_CONST_POLY 0x0000000105ec76f1ULL // P(x) true LE full
#define RV_CRC32_MASK32      0x00000000FFFFFFFFULL

static uint32_t rv_crc32c_clmul_impl(uint32_t crc, const char* buf, size_t len) {
    crc ^= 0xFFFFFFFF;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(buf);
    size_t n = len;

    if (n < 64) {
        return rv_crc32c_bitwise(crc, p, n) ^ 0xFFFFFFFF;
    }

    // Align to 16-byte boundary.
    uintptr_t mis = reinterpret_cast<uintptr_t>(p) & 0xF;
    if (mis) {
        size_t pre = 16 - mis;
        if (pre > n) {
            pre = n;
        }
        crc = rv_crc32c_bitwise(crc, p, pre);
        p += pre;
        n -= pre;
        if (n < 64) {
            return rv_crc32c_bitwise(crc, p, n) ^ 0xFFFFFFFF;
        }
    }

    uint64_t x0, x1, y0, y1, z0, z1, w0, w1;
    std::memcpy(&x0, p + 0, 8);
    std::memcpy(&x1, p + 8, 8);
    std::memcpy(&y0, p + 16, 8);
    std::memcpy(&y1, p + 24, 8);
    std::memcpy(&z0, p + 32, 8);
    std::memcpy(&z1, p + 40, 8);
    std::memcpy(&w0, p + 48, 8);
    std::memcpy(&w1, p + 56, 8);

    x0 ^= static_cast<uint64_t>(crc);
    p += 64;
    n -= 64;

    const uint64_t k1 = crc32c_fold_const[0];
    const uint64_t k2 = crc32c_fold_const[1];
    const uint64_t k3 = crc32c_fold_const[2];
    const uint64_t k4 = crc32c_fold_const[3];

    while (n >= 64) {
        uint64_t d0, d1;
        std::memcpy(&d0, p + 0, 8);
        std::memcpy(&d1, p + 8, 8);
        rv_fold_pair_xor_data(&x0, &x1, k1, k2, d0, d1);
        std::memcpy(&d0, p + 16, 8);
        std::memcpy(&d1, p + 24, 8);
        rv_fold_pair_xor_data(&y0, &y1, k1, k2, d0, d1);
        std::memcpy(&d0, p + 32, 8);
        std::memcpy(&d1, p + 40, 8);
        rv_fold_pair_xor_data(&z0, &z1, k1, k2, d0, d1);
        std::memcpy(&d0, p + 48, 8);
        std::memcpy(&d1, p + 56, 8);
        rv_fold_pair_xor_data(&w0, &w1, k1, k2, d0, d1);
        p += 64;
        n -= 64;
    }

    // Reduce 4x128-bit to 1x128-bit.
    rv_fold_pair_xor_state(&x0, &x1, k3, k4, y0, y1);
    rv_fold_pair_xor_state(&x0, &x1, k3, k4, z0, z1);
    rv_fold_pair_xor_state(&x0, &x1, k3, k4, w0, w1);

    // Barrett reduction: 128-bit -> 32-bit CRC.
    uint64_t t4 = rv_clmul(x0, RV_CRC32C_CONST_1);
    uint64_t t3 = rv_clmulh(x0, RV_CRC32C_CONST_1);
    uint64_t t1 = x1 ^ t4;
    t4 = t1 & RV_CRC32_MASK32;
    t1 >>= 32;
    uint64_t t0 = rv_clmul(t4, RV_CRC32C_CONST_0);
    t3 = (t3 << 32) ^ t1 ^ t0;

    t4 = t3 & RV_CRC32_MASK32;
    t4 = rv_clmul(t4, RV_CRC32C_CONST_QUO);
    t4 &= RV_CRC32_MASK32;
    t4 = rv_clmul(t4, RV_CRC32C_CONST_POLY);
    t4 ^= t3;

    uint32_t c = static_cast<uint32_t>((t4 >> 32) & RV_CRC32_MASK32);
    if (n) {
        c = rv_crc32c_bitwise(c, p, n);
    }
    return c ^ 0xFFFFFFFF;
}

static bool zbc_supported() {
    static int cached = -1;
    if (cached >= 0) {
        return cached != 0;
    }
    FILE* f = std::fopen("/proc/cpuinfo", "r");
    if (!f) {
        cached = 0;
        return false;
    }
    char line[1024];
    bool zbc = false;
    while (std::fgets(line, sizeof(line), f)) {
        if (std::strncmp(line, "isa", 3) == 0 && std::strstr(line, "zbc")) {
            zbc = true;
        }
    }
    std::fclose(f);
    cached = zbc ? 1 : 0;
    return zbc;
}

bool available() {
    return zbc_supported();
}

// ===========================================================================
// Zvbc vector path (optional). Compiled only when the toolchain targets Zvbc
// (riscv_vector.h + vclmul intrinsics, e.g. GCC 14 on a Zvbc-capable host).
// Runtime-probed so Zbc-less builds keep the scalar path. Bit-exact mirror of
// rv_crc32c_clmul_impl (same fold constants and Barrett math); 4 lanes x
// 128-bit, vlseg2e64 segment loads, element-wise vclmul_vx broadcast -- no
// scalar extraction in the hot loop.
// ===========================================================================
#if defined(__riscv_zvbc)

#include <riscv_vector.h>

static uint32_t crc32c_zvbc_tab[256];

static void crc32c_zvbc_tab_init() {
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int k = 0; k < 8; ++k) {
            c = (c >> 1) ^ (0x82F63B78u & static_cast<uint32_t>(-static_cast<int32_t>(c & 1)));
        }
        crc32c_zvbc_tab[i] = c;
    }
}

static inline uint32_t crc32c_zvbc_tail(uint32_t crc, const uint8_t* p, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        crc = crc32c_zvbc_tab[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc;
}

static uint32_t rv_crc32c_zvbc_impl(uint32_t crc, const char* buf_, size_t len) {
    const uint8_t* buf = reinterpret_cast<const uint8_t*>(buf_);
    size_t n = len;

    if (n < 128) {
        return rv_crc32c_clmul_impl(crc, buf_, len);
    }

    crc ^= 0xFFFFFFFF;

    // Align to 16 bytes before entering the vector path.
    uintptr_t mis = reinterpret_cast<uintptr_t>(buf) & 0xF;
    if (mis) {
        size_t pre = 16 - mis;
        if (pre > n) {
            pre = n;
        }
        crc = crc32c_zvbc_tail(crc, buf, pre);
        buf += pre;
        n -= pre;
        if (n < 128) {
            return crc32c_zvbc_tail(crc, buf, n) ^ 0xFFFFFFFF;
        }
    }

    // Need 4+ u64 lanes in one e64m1 vector (VLEN >= 256).
    if (__riscv_vsetvlmax_e64m1() < 4) {
        return rv_crc32c_clmul_impl(crc ^ 0xFFFFFFFF, buf_, len);
    }

    const size_t vl = 4;
    vuint64m1x2_t sg = __riscv_vlseg2e64_v_u64m1x2(reinterpret_cast<const uint64_t*>(buf), vl);
    vuint64m1_t lo_v = __riscv_vget_v_u64m1x2_u64m1(sg, 0);
    vuint64m1_t hi_v = __riscv_vget_v_u64m1x2_u64m1(sg, 1);

    uint64_t tmp[4];
    __riscv_vse64_v_u64m1(tmp, lo_v, vl);
    tmp[0] ^= static_cast<uint64_t>(crc);
    lo_v = __riscv_vle64_v_u64m1(tmp, vl);
    buf += 64;
    n -= 64;

    const uint64_t k1 = crc32c_fold_const[0];
    const uint64_t k2 = crc32c_fold_const[1];
    const uint64_t k3 = crc32c_fold_const[2];
    const uint64_t k4 = crc32c_fold_const[3];

    // Main loop: 64 B per iteration, all element-wise vector ops.
    while (n >= 64) {
        vuint64m1x2_t s2 = __riscv_vlseg2e64_v_u64m1x2(reinterpret_cast<const uint64_t*>(buf), vl);
        vuint64m1_t dl = __riscv_vget_v_u64m1x2_u64m1(s2, 0);
        vuint64m1_t dh = __riscv_vget_v_u64m1x2_u64m1(s2, 1);
        vuint64m1_t olo = lo_v;
        vuint64m1_t ohi = hi_v;
        vuint64m1_t lv = __riscv_vxor_vv_u64m1(
                __riscv_vclmul_vx_u64m1(olo, k1, vl),
                __riscv_vclmul_vx_u64m1(ohi, k2, vl), vl);
        vuint64m1_t hv = __riscv_vxor_vv_u64m1(
                __riscv_vclmulh_vx_u64m1(olo, k1, vl),
                __riscv_vclmulh_vx_u64m1(ohi, k2, vl), vl);
        lo_v = __riscv_vxor_vv_u64m1(lv, dl, vl);
        hi_v = __riscv_vxor_vv_u64m1(hv, dh, vl);
        buf += 64;
        n -= 64;
    }

    // Extract 4 lanes and merge (exact scalar math, same as Zbc path).
    uint64_t loa[4];
    uint64_t hia[4];
    __riscv_vse64_v_u64m1(loa, lo_v, vl);
    __riscv_vse64_v_u64m1(hia, hi_v, vl);
    uint64_t x0 = loa[0];
    uint64_t x1 = hia[0];
    for (size_t j = 1; j < 4; ++j) {
        rv_fold_pair_xor_state(&x0, &x1, k3, k4, loa[j], hia[j]);
    }

    // Barrett reduction: 128-bit -> 32-bit CRC (same math as scalar path).
    uint64_t t4 = rv_clmul(x0, RV_CRC32C_CONST_1);
    uint64_t t3 = rv_clmulh(x0, RV_CRC32C_CONST_1);
    uint64_t t1 = x1 ^ t4;
    t4 = t1 & RV_CRC32_MASK32;
    t1 >>= 32;
    uint64_t t0 = rv_clmul(t4, RV_CRC32C_CONST_0);
    t3 = (t3 << 32) ^ t1 ^ t0;

    t4 = t3 & RV_CRC32_MASK32;
    t4 = rv_clmul(t4, RV_CRC32C_CONST_QUO);
    t4 &= RV_CRC32_MASK32;
    t4 = rv_clmul(t4, RV_CRC32C_CONST_POLY);
    t4 ^= t3;

    uint32_t c = static_cast<uint32_t>((t4 >> 32) & RV_CRC32_MASK32);
    if (n) {
        c = crc32c_zvbc_tail(c, buf, n);
    }
    return c ^ 0xFFFFFFFF;
}

// Zvbc runtime probe: true when /proc/cpuinfo reports "zvbc".
static bool zvbc_supported() {
    static int cached = -1;
    if (cached >= 0) {
        return cached != 0;
    }
    FILE* f = std::fopen("/proc/cpuinfo", "r");
    if (!f) {
        cached = 0;
        return false;
    }
    char line[1024];
    bool zvbc = false;
    while (std::fgets(line, sizeof(line), f)) {
        if (std::strncmp(line, "isa", 3) == 0 && std::strstr(line, "zvbc")) {
            zvbc = true;
        }
    }
    std::fclose(f);
    cached = zvbc ? 1 : 0;
    return zvbc;
}

#endif // __riscv_zvbc

uint32_t compute(uint32_t crc, const void* buf, size_t size) {
#if defined(__riscv_zvbc)
    static bool zvbc_ready = [] {
        crc32c_zvbc_tab_init();
        return true;
    }();
    (void)zvbc_ready;
    if (zvbc_supported()) {
        return rv_crc32c_zvbc_impl(crc, static_cast<const char*>(buf), size);
    }
#endif
    return rv_crc32c_clmul_impl(crc, static_cast<const char*>(buf), size);
}

} // namespace doris::crc32c_riscv
#endif // __riscv && __riscv_xlen == 64 && __riscv_zbc