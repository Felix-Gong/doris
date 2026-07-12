# Apache Doris Thirdparty Build Notes (RISC-V)

**Date**: 2026-07-06  
**Platform**: RISC-V (riscv64), openEuler 24.03  
**GCC**: 12.3.1  
**Doris Revision**: 73f09b023

## Backup

Before rebuilding, backup the old compiled libraries:

```bash
cd /bigdata/gxf/agent-team/doris/thirdparty
cp -r installed installed.bak   # 3.8G
```

## Build Command

```bash
DORIS_TOOLCHAIN=gcc \
DORIS_GCC_HOME=/usr \
DORIS_HOME=/bigdata/gxf/agent-team/doris \
nohup /bigdata/gxf/agent-team/doris/thirdparty/build-thirdparty.sh \
  > /tmp/thirdparty_build.log 2>&1 &
```

## Issues & Fixes

### Issue 1: MySQL MD5SUM mismatch

**Symptom**: build-thirdparty.sh fails during download phase (MD5 mismatch for mysql-5.7.18.tar.gz).

**Root cause**: vars.sh had MYSQL_MD5SUM changed to 25b3c537b75542c31d9b194fc8e50cdc, but the GitHub tarball for mysql-5.7.18 has not changed and still has MD5 58598b10dce180e4d1fbdd7cf5fa68d6.

**Fix**: Revert to the correct MD5:
sed -i "s/MYSQL_MD5SUM=\"25b3c537b75542c31d9b194fc8e50cdc\"/MYSQL_MD5SUM=\"58598b10dce180e4d1fbdd7cf5fa68d6\"/" vars.sh

### Issue 2: ccache causes wrong compiler home detection

**Symptom**: Build fails during unixODBC configure: C compiler cannot create executables.

**Root cause**: env.sh defaults DORIS_TOOLCHAIN=clang on Linux. With /usr/lib64/ccache/ in PATH before /usr/bin/, command -v clang returns the ccache path, causing DORIS_CLANG_HOME to be computed as /usr/lib64/ which lacks bin/clang.

**Fix**: Use GCC toolchain explicitly:
export DORIS_TOOLCHAIN=gcc
export DORIS_GCC_HOME=/usr
GCC 12.3.1 works correctly and was previously verified with the BE build.

### Issue 3: (not encountered in this build, but from earlier BE link)
absl/cctz duplicate symbols. Fix: Keep both libcctz.a and libabsl_time_zone.a; use --allow-multiple-definition with ld.bfd.

## Build Progress

| Package | Status | Notes |
|---------|--------|-------|
| jindofs | ✅ | jar copy |
| juicefs | ✅ | jar copy |
| unixODBC | ✅ | |
| OpenSSL 1.1.1s | ✅ | no-asm no-async on RISC-V |
| libevent 2.1.12 | ✅ | |
| zlib 1.3.1 | ✅ | |
| crc32c 1.1.2 | ✅ | |
| lz4 1.9.4 | ✅ | |
| bzip2 1.0.8 | ✅ | |
| lzo2 2.10 | ✅ | |
| zstd 1.5.7 | ✅ | |
| boost 1.81.0 | ✅ |
