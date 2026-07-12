import os, sys

def patch_archdetect(hyperscan_src_dir):
    f = os.path.join(hyperscan_src_dir, 'cmake', 'archdetect.cmake')
    with open(f) as fh:
        c = fh.read()
    if 'riscv64' in c:
        print('archdetect.cmake already patched')
        return
    old = ('        elseif(ARCH_PPC64EL)\n'
           '            set(GNUCC_ARCH power8)\n'
           '            set(TUNE_FLAG power8)\n'
           '        else()\n'
           '            set(GNUCC_ARCH x86-64-v2)\n'
           '            set(TUNE_FLAG generic)\n'
           '        endif()')
    new = ('        elseif(ARCH_PPC64EL)\n'
           '            set(GNUCC_ARCH power8)\n'
           '            set(TUNE_FLAG power8)\n'
           '        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "riscv64")\n'
           '            set(GNUCC_ARCH rv64imafdc)\n'
           '            set(TUNE_FLAG generic)\n'
           '        else()\n'
           '            set(GNUCC_ARCH x86-64-v2)\n'
           '            set(TUNE_FLAG generic)\n'
           '        endif()')
    assert c.count(old) == 1, f'archdetect.cmake: pattern mismatch ({c.count(old)})'
    c = c.replace(old, new, 1)
    with open(f, 'w') as fh:
        fh.write(c)
    print('archdetect.cmake patched for RISC-V')

if __name__ == '__main__':
    if len(sys.argv) > 1:
        patch_archdetect(sys.argv[1])
    else:
        print('Usage: patch_vectorscan_riscv.py <hyperscan_source_dir>')
