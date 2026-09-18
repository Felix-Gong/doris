#!/usr/bin/env bash
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

# RISC-V 回归门禁 (Tier-3 smoke)：固化已在 SG2044/SG2042 上实测
# 通过的全部 UT 组为可重复运行的一分钟回归。
#
# 背景：完整 doris_be_test 8942 例套件需数天 + riscv64 docker 编译镜像
# 需数小时，属于需用户决策的基础设施投入（见 build-riscv.yml 14B 占位）。
# 此门禁作为立即可用的 Tier-3 回归基线，将已实测通过的组合固化。
#
# 用法：
#   DORIS_HOME=<doris_be_test 所在目录>/test/ ./run-riscv-regression.sh
# 需要的环境：与 doris_be_test 相同（LD_LIBRARY_PATH、dict symlink、ulimit）

set -u
FAIL=0

die() { echo "✗ $*" >&2; FAIL=1; }

run_ut() {
    local label="$1" filter="$2"
    local out
    out=$("${DORIS_BE_TEST}" --gtest_filter="${filter}" 2>&1)
    local passed failed
    passed=$(echo "$out" | grep -oE "[0-9]+ tests? from [0-9]+ test suites? ran" | awk '{print $1}')
    failed=$(echo "$out" | sed -n 's/.*FAILED //p' | awk '{sum+=$1} END{print sum+0}')
    if [ "${failed:-0}" = "0" ]; then
        echo "✓ [${label}] ${passed} 例通过"
    else
        echo "✗ [${label}] ${failed} 例失败" && FAIL=1
    fi
}

# 依赖探测
: "${DORIS_BE_TEST:?需设置 DORIS_BE_TEST 指向 doris_be_test 可执行}"
if ! "${DORIS_BE_TEST}" --help >/dev/null 2>&1; then
    echo "✗ doris_be_test 不可执行" >&2 && exit 1
fi

echo "=== RISC-V UT 回归门禁（Tier-3 smoke）==="
echo "doris_be_test: ${DORIS_BE_TEST}"
echo "开始时间: $(date '+%F %T')"

# —— 已实测通过组（数据来自 SG2044 UT 回归记录）——
run_ut "pinyin (词库 UT)"            "PinyinTest.*:PinyinDictTest.*"
run_ut "ICU (Unicode 边界)"          "ICU*.*:StringSearchTest.*"
run_ut "group_rowset (存储)"         "GroupRowset*.*"
run_ut "crc32c (SIMD 哈希)"          "Crc32cTest.*:HashCrc32Test.*"
run_ut "faiss 距离 (RVV 加速)"       "ArrayDistanceTest.*:DistanceTest.*"

echo "结束时间: $(date '+%F %T')"
if [ "${FAIL}" = "0" ]; then
    echo "=== RISC-V UT 回归门禁：全部通过 ✓ ==="
    exit 0
else
    echo "=== RISC-V UT 回归门禁：存在失败 ✗ ==="
    exit 1
fi
