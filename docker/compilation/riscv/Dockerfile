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

# RISC-V 64 (riscv64/rv64gcv) Doris 编译镜像。
#
# 注:此镜像与 .github/workflows/build-riscv.yml 中 riscv-ci-* 容器
# 实际使用的运行基底一致(openEuler 24.03 LTS + GCC 15 riscv64 完整工具链),
# 已在 SG2044(k3 构建机)上成功编译并链接掉 doris_be 及 doris_be_test
# (RVV 指令集由 -march=rv64gcv 启用,含 Zba/Zbb/Zbc/Zvbb/Zvbc/Zvkc)。
#
# 用法:
#   docker build -f docker/compilation/riscv/Dockerfile -t doris:riscv64-gcc15 .
#   docker run --rm -v <doris-src>:/opt/doris doris:riscv64-gcc15 \
#       bash -c "cd /opt/doris && ./build.sh"

FROM ccr.ccs.tencentyun.com/xlawy/openeuler:24.03-lts-gcc15-full-base-riscv64

# 语言运行时与构建工具链(Doris BE 依赖的运行时组件)
RUN yum install -y \
        java-11-openjdk-devel \
        which \
        patch \
        unzip \
        zip \
        gcc-c++ \
        gcc \
        make \
        automake \
        autoconf \
        libtool \
        ncurses-devel \
        gettext-devel \
        bison \
        flex \
    && rm -rf /var/cache/yum

# Maven 3.9.9(Doris FE 使用;64 位 riscv 用通用二进制包)
ARG MAVEN_VERSION=3.9.9
RUN mkdir -p /usr/share/maven /usr/share/maven/ref \
    && wget -q -O /tmp/apache-maven.tar.gz \
        https://archive.apache.org/dist/maven/maven-3/${MAVEN_VERSION}/binaries/apache-maven-${MAVEN_VERSION}-bin.tar.gz \
    && tar -xzf /tmp/apache-maven.tar.gz -C /usr/share/maven --strip-components=1 \
    && rm -f /tmp/apache-maven.tar.gz \
    && ln -s /usr/share/maven/bin/mvn /usr/bin/mvn

# Node.js 22(FE frontend 构建所需;riscv64 用官方 riscv64 二进制)
ARG NODEJS_VERSION=22.12.0
RUN wget -q -O /tmp/node.tar.xz \
        https://nodejs.org/dist/v${NODEJS_VERSION}/node-v${NODEJS_VERSION}-linux-riscv64.tar.xz \
    && tar -xJf /tmp/node.tar.xz -C /tmp/ \
    && cp -r /tmp/node-v${NODEJS_VERSION}-linux-riscv64/* /usr/local/ \
    && rm -rf /tmp/node-v${NODEJS_VERSION}-linux-riscv64 /tmp/node.tar.xz

# 环境变量(与 build-riscv.yml / 编译脚本对齐)
ENV JAVA_HOME=/usr/lib/jvm/java-11 \
    PATH="/usr/local/bin:/usr/bin:/bin"
