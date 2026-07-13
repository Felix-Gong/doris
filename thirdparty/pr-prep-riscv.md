# Apache Doris RISC-V Porting -- PR Preparation Guide

- **Branch**: riscv-dev-v2 (base: upstream/master)
- **Date**: 2026-07-12
- **Platform**: RISC-V (riscv64), openEuler 24.03, GCC 12.3.1 / clang 17

---

## Commit Summary (7 RISC-V commits)

| # | Commit | Scope | Files | +Lines |
|---|--------|-------|-------|--------|
| 1 | 6da382f01 | CMake build system + arch detection | 16 | +90 |
| 2 | c00d7d3de | thirdparty build scripts + env | 6 | +103 |
| 3 | 875ec4a4a | glibc-compatibility / musl RISC-V | 6 | +139 |
| 4 | c71e61187 | SSE/CRC32 software fallbacks | 9 | +125 |
| 5 | ba69ac1a2 | GCC/clang strict mode fixes | 22 | +67 |
| 6 | 7ee9b88d7 | FAISS/paimon/FE build fixes | 13 | +126 |
| 7 | 6a6ae9de4 | thirdparty patch scripts + build notes | 4 | +384 |

**Total**: 7 commits, ~1034 lines changed

Plus 2 prior commits:
- 73f09b023: RE2 fallback for multi-wildcard LIKE patterns
- 676a5b2c0: vectorscan 5.4.12 build system support

---

## Change Categories & PR Structure Suggestions

### 1. CMake Build System (15 files)

be/CMakeLists.txt + all sub-CMakeLists.txt changes:

- ARCH_RISCV64 detection from CMAKE_SYSTEM_PROCESSOR
- Skip LLD linker on RISC-V (use ld.bfd) -- LLD lacks R_RISCV_ADD64/SUB64
- --allow-multiple-definition for abseil symbol duplicates
- ICU libs (icuuc/icui18n/icudata) with -Wl,--whole-archive
- _UCS2/_UNICODE defines for ICU Unicode
- -march=rv64gcv_zba on 12 sub-targets (Vector + Zba)
- -O1 for Exec sub-target (template-heavy code)
- -O0 for DorisGen (code generation)
- pch.h: guard CLucene/SSE headers

### 2. Thirdparty Build Scripts (6 files)

build-thirdparty.sh, download-thirdparty.sh, vars.sh, paimon-cpp-cache.cmake, env.sh, build.sh

- OpenSSL: no-asm no-async; make build_libs
- vectorscan: SIMDE headers; -DSIMDE_BACKEND
- MySQL: skip -static; use ln -sf for boost
- Arrow: use SYSTEM Brotli
- bitshuffle: default arch only
- cctz: SIMDE_BACKEND
- ICU: -lstdc++ LDFLAGS
- AWS SDK: -latomic
- build.sh: skip rm -rf installed; skip update_submodule

### 3. glibc-compatibility / musl RISC-V (6 files)

- Enable ASM for riscv64 (longjmp.s, syscall.s)
- a_cas_p 64-bit for __riscv/__LP64__
- Guard strfromf128 with !__riscv
- New: atomic_arch.h, syscall_arch.h

### 4. SSE/CRC32 Software Fallbacks (9 files)

Guard SSE4.2 intrinsics with __SSE4_2__ || __aarch64__:
- hash.h: Dorris Hash128to64 software fallback
- hash_crc32_return32.h: int_hash64 fallback
- string_ref.h: CRC32Hash via crc32c library
- uint128.h: UInt128HashCRC32 fallback
- block_bloom_filter_impl.cc: scalar bucket_insert
- hash_util.hpp: crc32c_fixed/crc_hash SSE guards
- sse_util.hpp: SSE constants for compilation

### 5. GCC/Clang Strict Mode Fixes (22 files)

- Structured bindings to explicit vars (Clang++ restriction)
- Enum NONE/Limit conflicts renamed
- -Wshadow pragmas
- ::doris::util_hash:: namespace qualifiers
- std::powf -> powf, std::format -> fmt::format

### 6. FAISS/paimon-cpp/FE (13 files)

- System OpenBLAS on RISC-V; FAISS generic level
- paimon-cpp __has_include guards
- Inline faiss distance fns
- FE pom.xml: remove os-maven-plugin, add arch override
- protoc-gen-grpc-java via exec-maven-plugin

---

## External Dependencies NOT in This PR

### riscv-clang-wrapper (/usr/local/bin/clang++ wrapper)
1. Replace clang++ with clang++.real
2. -no-pie: required for static linking
3. --no-relax: disable linker relaxation
4. libsframe for libbfd
5. Absl whole-archive dedup
6. OpenBLAS -lgfortran

Items to upstream properly:
- -lstdc++ in CMakeLists.txt (already in env.sh LDFLAGS)
- --whole-archive for absl in thirdparty.cmake
- -no-pie via CMAKE_EXE_LINKER_FLAGS

### Build.ninja ICU --whole-archive
**NOW FIXED** in be/CMakeLists.txt (Commit 1). Manual build.ninja edits no longer needed.

---

## PR Submission Checklist

- [ ] Rebase onto latest upstream/master
- [ ] Squash related commits (recommend 4-5 commits for upstream)
- [ ] Make -march=rv64gcv_zba configurable (-DRISCV_MARCH=...)
- [ ] Replace if(ARCH_RISCV64) with if(DEFINED ARCH_RISCV64)
- [ ] Remove clang-specific pragmas if not acceptable upstream
- [ ] Review inline faiss distance fns for header conflicts
- [ ] Verify system OpenBLAS version requirement
- [ ] Add RISC-V cross-compilation CI (QEMU user-mode)

---

## Build Verification

```
export DORIS_TOOLCHAIN=gcc
export DORIS_GCC_HOME=/usr
cd /path/to/doris
bash build.sh --be --fe --no-fe
```

Expected: output/be/lib/doris_be (binary), output/fe/ (JARs)

---

## Known Limitations

| Issue | Impact | Workaround |
|-------|--------|------------|
| LLD unsupported | ld.bfd slower link | Wait for LLD fix |
| FAISS AVX2 only | ANN perf on generic | Runtime dispatch |
| paimon-cpp partial | Some features unverified | __has_include guards |
| ICU whole-archive | Link without ICU GC | -Wl,--whole-archive |
