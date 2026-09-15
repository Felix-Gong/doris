import os
import sys


def patch_turbopfor(clucene_src_dir):
    """Disable src/ext/for (TurboPFOR -> 'ic' lib) on RISC-V.

    vint.h/vp4c.c assume x86/ARM bswap16 and register macros, so the C
    translation units fail to compile on riscv64 ('expression is not
    assignable'). No Doris BE code consumes ic symbols (clucene core only
    adds the include dir), so the subdirectory is skipped on RISC-V.
    """
    f = os.path.join(clucene_src_dir, "CMakeLists.txt")
    with open(f) as fh:
        c = fh.read()
    if "TurboPFOR" in c and "riscv64" in c:
        print("clucene CMakeLists.txt already patched")
        return
    old = "ADD_SUBDIRECTORY (src/ext/for)"
    new = (
        "if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES \"riscv64\")\n"
        "    # TurboPFOR (ext/for -> ic lib) fails on RISC-V: assumes x86/ARM\n"
        "    # bswap16 + register macros\n"
        "    ADD_SUBDIRECTORY (src/ext/for)\n"
        "endif()"
    )
    assert c.count(old) == 1, f"ext/for pattern mismatch ({c.count(old)})"
    c = c.replace(old, new, 1)
    with open(f, "w") as fh:
        fh.write(c)
    print("clucene CMakeLists.txt patched for RISC-V")


if __name__ == "__main__":
    if len(sys.argv) > 1:
        patch_turbopfor(sys.argv[1])
    else:
        print("Usage: patch_clucene_riscv.py <clucene_source_dir>")