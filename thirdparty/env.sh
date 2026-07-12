
# riscv64: os-maven-plugin does not detect riscv64; set explicitly
export MAVEN_OPTS="${MAVEN_OPTS} -Dos.detected.arch=riscv64 -Dos.detected.classifier=linux-riscv64"
