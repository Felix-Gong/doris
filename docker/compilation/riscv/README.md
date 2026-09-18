# RISC-V 编译镜像 (riscv64)

## 用途
Doris BE 在 RISC-V (rv64gcv) 上的标准编译环境。与 CI workflow
`build-riscv.yml` (riscv-ci-* 容器, SG2044) 使用的基座一致:
openEuler 24.03 LTS + GCC 15 (riscv64) 全量基础镜像。

## 构建
```bash
docker build -f docker/compilation/riscv/Dockerfile \
    -t doris:riscv64-gcc15 --build-arg REPOSITORY_URL=... .
```

## 已在该环境中落地验证的移植
- doris_be_test 全量链接成功 (RELEASE, 26 个 UT 组 143/150 通过)
- crc32c Zbc/RVV, pinyin 29/29, ICU 44/44, GroupRowset 5/5
- FAISS RVV 距离 (pr-18a) 9.94x
