# riscv-clang-wrapper: RISC-V Clang Workaround Guide

## Purpose

`/usr/local/bin/clang++` is a wrapper script on the openEuler RISC-V build
environment that works around several clang/LLVM limitations on RISC-V.
It MUST be documented because it contains changes that cannot be upstreamed
directly (system-level workarounds) and some that SHOULD be upstreamed.

## What the Wrapper Does

### 1. Clang++ Replacement
- Detects when the first argument is clang++ itself
- Calls the real compiler (`/usr/bin/clang++.real`) instead
- All transforms happen in-memory before exec

### 2. Compile-time Fixes
- **`-fno-integrated-as`**: For `aggregate_function_map_combinator.cpp` only
  (RISC-V gas compatibility issue with clang integrated assembler)

### 3. Link-time Fixes

| Fix | Flag | Reason |
|-----|------|--------|
| Static linking | `-no-pie` | Required for static PIE on RISC-V |
| Linker relaxation | `-Wl,--no-relax` | ld.bfd on RISC-V needs relaxation disabled |
| libsframe | `-l:libsframe.a` | Required by libbfd for frame handling |
| Absl whole-archive | `-Wl,--whole-archive` libabsl_*.a | Prevents abseil symbol stripping (gRPC/arrow deps) |
| pugixml | `-lstdc++` | clang++ doesn't auto-link libstdc++ on RISC-V |
| OpenBLAS | `-lgfortran` | Fortran runtime needed by system OpenBLAS |

### 4. Absl Dedup Logic
- Scans all linker args for abseil libraries (full path / -l: / -l patterns)
- Replaces system abseil paths with thirdparty-installed versions
- Wraps each with `-Wl,--whole-archive` / `-Wl,--no-whole-archive`
- Deduplicates identical libraries seen multiple times
- Adds ALL thirdparty absl libs not yet in the link line

### 5. ICU Fix (Commented Out)
- Original intent: replace thirdparty ICU 69 static libs with system ICU 74
- **NOW COMMENTED OUT** because commit 1 moved ICU whole-archive into CMakeLists.txt
- Can be removed in future wrapper cleanup

## Items to Upstream Properly

| Wrapper Feature | Target Location | Status |
|-----------------|-----------------|--------|
| `-lstdc++` | env.sh LDFLAGS (already done) | Done |
| Absl whole-archive | `thirdparty.cmake` or `CMakeLists.txt` | TODO |
| `-no-pie` | `CMAKE_EXE_LINKER_FLAGS` in CMakeLists.txt | TODO |
| `--no-relax` | `CMAKE_EXE_LINKER_FLAGS` in CMakeLists.txt | TODO |
| libsframe | bfd detection logic | TODO (upstream binutils) |
| `-lgfortran` | OpenBLAS cmake config | Done via system OpenBLAS |

## Why These Can't Be in the PR

1. **System-specific**: Wrapper is installed at `/usr/local/bin/` outside the repo
2. **Not portable**: Other RISC-V distros may not need all these workarounds
3. **lld.bfd limitation**: `--no-relax` and `-no-pie` may be fixed in newer LLVM
