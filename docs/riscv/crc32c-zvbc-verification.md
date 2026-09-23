<!--
Licensed to the Apache Software Foundation (ASF) under one
or more contributor license agreements.  See the NOTICE file
distributed with this work for additional information
regarding copyright ownership.  The ASF licenses this file
to you under the Apache License, Version 2.0 (the
"License"); you may not use this file except in compliance
with the License.  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing,
software distributed under the License is distributed on an
"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
KIND, either express or implied.  See the License for the
specific language governing permissions and limitations
under the License.
-->

# Doris crc32c Zvbc 向量路径验证记录（143 / SG2044）

## 背景

`be/src/util/crc32c_riscv.cc`（pr-19，commit f22d39a02a）同时携带两条 RISC-V
加速路径，均由 `/proc/cpuinfo` 运行时探测并以 bit-exact 对软件库保持一致性：

| 路径 | 门控 | 指令 | 硬件要求 |
|---|---|---|---|
| Zbc 标量 | `#if defined(__riscv_zbc)` | clmul/clmulh 标量 4 路并行折叠 + Barrett | Zbc 扩展 |
| Zvbc 向量 | `#if defined(__riscv_zvbc)` + `<riscv_vector.h>` | vclmul/vclmulh 向量 K-lane（vlseg2e64） | Zvbc 扩展 + VLEN>=256 |

Zvbc 路径移植自 redis 官方 PR15786 / MariaDB PR-B（K-lane 结构，热点循环无标量
提取），与 Zbc 标量共享同一组折叠常数与 Barrett 归约数学，保证 bit-exact。

## 验证环境

- 机器：10.213.6.143 (SG2044, riscv64, openEuler)
- 硬件 ISA：`rv64imafdcv_..._zba_zbb_zbc_...` —— **无 zvbc**（VLEN=128）
- 编译器：GCC 15.1.0 (`/opt/gcc-15.1/bin/g++`, riscv64-unknown-linux-gnu)
- 参考库：crc32c-1.1.2（Doris thirdparty 软件实现，`libcrc32c.a`）

## 验证方法

同一份源文件（pr-19 HEAD 的 `crc32c_riscv.cc`，剥离 `util/slice.h` 依赖后原样
编译）构建两个变体：

| 变体 | march | Zvbc 代码是否编译 |
|---|---|---|
| A | `rv64gcv_zba_zbc` | 否（`__riscv_zvbc` 未定义） |
| B | `rv64gcv_zba_zbc_zvbc` | **是**（`__riscv_zvbc` 已定义，vclmul 向量 intrinsics 全部编译通过） |

验证内容：
1. **bit-exact**：23 种长度 × {0,8,16}-字节偏移 × 链式追加 → 与软件库 `crc32c::Crc32c/Extend` 逐位一致
2. **运行时回退**：143 无 zvbc 硬件，Variant B 的 `zvbc_supported()==false` → 必须正确落回 Zbc 标量路径
3. **性能**：1 MiB 块 × 2000 次，软件库 vs RISC-V 路径

## 结果

```
=== Variant A: rv64gcv_zba_zbc (baseline) ===
COMPILE-ZVBC=0 AVAILABLE=1
bit-exact: 90/90 passed, 0 failed
bench 1MiB x2000: soft=200 MB/s  riscv=2800 MB/s  speedup=13.98x

=== Variant B: rv64gcv_zba_zbc_zvbc (Zvbc compiled) ===
COMPILE-ZVBC=1 AVAILABLE=1
bit-exact: 90/90 passed, 0 failed
bench 1MiB x2000: soft=246 MB/s  riscv=2781 MB/s  speedup=11.29x
```

## 结论

1. **Zvbc 向量实现完整且可编译**：GCC 15.1 下 `-march=rv64gcv_zba_zbc_zvbc` 全量
   编译通过（包括 `__riscv_vlseg2e64_v_u64m1x2`、`__riscv_vclmul_vx_u64m1` 等
   全部 Zvbc intrinsics），无任何错误。
2. **bit-exact 正确**：Zbc 与 Zvbc 两种 march 变体均对软件库 90/90 通过。
3. **运行时回退安全**：无 Zvbc 硬件（143）上 Variant B 正确回退到 Zbc 标量路径，
   结果与 Variant A 完全一致 —— 编译带 Zvbc 的二进制在旧硬件上仍可安全运行。
4. **性能（143 可测部分）**：Zbc 标量路径 1 MiB 实测 **13.98x** vs 软件库。
5. **性能（硬件边界）**：Zvbc 向量指令的真实执行需要 `zvbc` 扩展 + VLEN>=256 的
   硬件（如 Spacemit X100）。SG2044 无 zvbc 扩展，该项如实标注为待实测，不宣称
   已达到；MariaDB PR-B 在同型 X100 上报告 64 KiB 块约 10.8x vs Zbc 标量。

## 复现

```bash
GCC=/opt/gcc-15.1/bin/g++
g++ -O3 -march=rv64gcv_zba_zbc_zvbc -I<thirdparty>/include -I. -std=c++17 \
    crc32c_riscv.cc verify_main.cc <thirdparty>/lib/libcrc32c.a -o verify
./verify --bench
```

（验证源 `verify_main.cc` 与 `crc32c_riscv.cc` 全量见会话记录；`docs/riscv/`
下保留本记录供 PR 引用。）