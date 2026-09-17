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

#include "core/string_ref.h"
#include "core/types.h"
#include "core/uint128.h"
#include "hash.h"
#include <crc32c/crc32c.h>

// CRC32 hash functions that return uint32_t instead of size_t.
// Uses type-appropriate _mm_crc32_u{8,16,32,64} intrinsics to avoid
// unnecessary widening of inputs smaller than 64 bits.

namespace doris {

static constexpr uint32_t CRC32_HASH_SEED = 0xFFFFFFFF;

// Type-dispatched CRC32 computation primitives.
// Each overload uses the narrowest intrinsic that matches the input width.
#if defined(__SSE4_2__) || defined(__aarch64__)
inline uint32_t crc32_compute(uint32_t crc, uint8_t v) {
    return _mm_crc32_u8(crc, v);
}
inline uint32_t crc32_compute(uint32_t crc, uint16_t v) {
    return _mm_crc32_u16(crc, v);
}
inline uint32_t crc32_compute(uint32_t crc, uint32_t v) {
    return _mm_crc32_u32(crc, v);
}
inline uint32_t crc32_compute(uint32_t crc, uint64_t v) {
    return static_cast<uint32_t>(_mm_crc32_u64(crc, v));
}
#else
// Software fallback that is bit-identical to the SSE4.2 / aarch64 hardware
// CRC32C instruction: _mm_crc32_u8/u16/u32/u64 process each input value
// bit-by-bit against the reflected CRC32C polynomial, so a zero input value
// STILL advances the CRC (it is not the identity). Using crc32c::Extend()
// here is wrong: that library's software path treats runs of zero bytes as
// identity on some backends, producing different hash values from the x86
// golden vectors. The loop below mirrors the hardware datapath exactly.
inline uint32_t crc32_compute(uint32_t crc, uint8_t v) {
    return crc32c_hw_equiv(crc, v, 8);
}
inline uint32_t crc32_compute(uint32_t crc, uint16_t v) {
    return crc32c_hw_equiv(crc, v, 16);
}
inline uint32_t crc32_compute(uint32_t crc, uint32_t v) {
    return crc32c_hw_equiv(crc, v, 32);
}
inline uint32_t crc32_compute(uint32_t crc, uint64_t v) {
    return crc32c_hw_equiv(crc, v, 64);
}
#endif


template <typename T>
struct HashCRC32Return32;

// --- Arithmetic types: use the narrowest intrinsic ---

template <>
struct HashCRC32Return32<UInt8> {
    uint32_t operator()(UInt8 key) const { return crc32_compute(CRC32_HASH_SEED, key); }
};

template <>
struct HashCRC32Return32<UInt16> {
    uint32_t operator()(UInt16 key) const { return crc32_compute(CRC32_HASH_SEED, key); }
};

template <>
struct HashCRC32Return32<UInt32> {
    uint32_t operator()(UInt32 key) const { return crc32_compute(CRC32_HASH_SEED, key); }
};

template <>
struct HashCRC32Return32<UInt64> {
    uint32_t operator()(UInt64 key) const { return crc32_compute(CRC32_HASH_SEED, key); }
};

// --- 128-bit types ---

template <>
struct HashCRC32Return32<UInt128> {
    uint32_t operator()(const UInt128& x) const {
        uint32_t crc = CRC32_HASH_SEED;
        crc = crc32_compute(crc, x.low());
        crc = crc32_compute(crc, x.high());
        return crc;
    }
};

// --- 256-bit types ---

template <>
struct HashCRC32Return32<UInt256> {
    uint32_t operator()(const UInt256& x) const {
        uint32_t crc = CRC32_HASH_SEED;
        crc = crc32_compute(crc, static_cast<uint64_t>(x.items[0]));
        crc = crc32_compute(crc, static_cast<uint64_t>(x.items[1]));
        crc = crc32_compute(crc, static_cast<uint64_t>(x.items[2]));
        crc = crc32_compute(crc, static_cast<uint64_t>(x.items[3]));
        return crc;
    }
};

// --- Packed compound types (used by FixedKeyHashTableContext) ---

template <>
struct HashCRC32Return32<UInt72> {
    uint32_t operator()(const UInt72& x) const {
        uint32_t crc = CRC32_HASH_SEED;
        crc = crc32_compute(crc, x.a);
        crc = crc32_compute(crc, x.b);
        return crc;
    }
};

template <>
struct HashCRC32Return32<UInt96> {
    uint32_t operator()(const UInt96& x) const {
        uint32_t crc = CRC32_HASH_SEED;
        crc = crc32_compute(crc, x.a);
        crc = crc32_compute(crc, x.b);
        return crc;
    }
};

template <>
struct HashCRC32Return32<UInt104> {
    uint32_t operator()(const UInt104& x) const {
        uint32_t crc = CRC32_HASH_SEED;
        crc = crc32_compute(crc, x.a);
        crc = crc32_compute(crc, x.b);
        crc = crc32_compute(crc, x.c);
        return crc;
    }
};

template <>
struct HashCRC32Return32<UInt136> {
    uint32_t operator()(const UInt136& x) const {
        uint32_t crc = CRC32_HASH_SEED;
        crc = crc32_compute(crc, x.a);
        crc = crc32_compute(crc, x.b);
        crc = crc32_compute(crc, x.c);
        return crc;
    }
};

// --- StringRef: truncate existing crc32_hash() result ---

template <>
struct HashCRC32Return32<StringRef> {
    uint32_t operator()(const StringRef& x) const {
        return static_cast<uint32_t>(crc32_hash(x.data, x.size));
    }
};

} // namespace doris
