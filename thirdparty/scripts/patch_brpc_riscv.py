import os, sys

def patch_brpc(brpc_src_dir):
    butil = os.path.join(brpc_src_dir, 'src', 'butil')
    # 1. build_config.h
    f = os.path.join(butil, 'build_config.h')
    with open(f) as fh:
        c = fh.read()
    if 'ARCH_CPU_RISCV_FAMILY' in c:
        print('build_config.h already patched')
    else:
        old = '#else\n#error Please add support for your architecture in butil/build_config.h\n#endif'
        new = '#elif defined(__riscv__) || defined(__riscv) || defined(__riscv64__)\n#define ARCH_CPU_RISCV_FAMILY 1\n#define ARCH_CPU_64_BITS 1\n#define ARCH_CPU_LITTLE_ENDIAN 1\n#else\n#error Please add support for your architecture in butil/build_config.h\n#endif'
        assert c.count(old) == 1, f'build_config.h: pattern mismatch ({c.count(old)})'
        c = c.replace(old, new)
        with open(f, 'w') as fh:
            fh.write(c)
        print('build_config.h patched')
    # 2. atomicops.h
    f = os.path.join(butil, 'atomicops.h')
    with open(f) as fh:
        c = fh.read()
    if 'ARCH_CPU_RISCV_FAMILY' in c:
        print('atomicops.h already patched')
    else:
        old = '#elif defined(COMPILER_GCC) && defined(ARCH_CPU_MIPS_FAMILY)\n#include "butil/atomicops_internals_mips_gcc.h"\n#else\n#error "Atomic operations are not supported on your platform"\n#endif'
        new = '#elif defined(COMPILER_GCC) && defined(ARCH_CPU_MIPS_FAMILY)\n#include "butil/atomicops_internals_mips_gcc.h"\n#elif defined(COMPILER_GCC) && defined(ARCH_CPU_RISCV_FAMILY)\n#include "butil/atomicops_internals_gcc.h"\n#else\n#error "Atomic operations are not supported on your platform"\n#endif'
        assert c.count(old) == 1, f'atomicops.h: pattern mismatch ({c.count(old)})'
        c = c.replace(old, new)
        with open(f, 'w') as fh:
            fh.write(c)
        print('atomicops.h patched')
    # 3. time.h
    f = os.path.join(butil, 'time.h')
    with open(f) as fh:
        c = fh.read()
    if 'rdcycle' in c:
        print('time.h already patched')
    else:
        old = '\n#else\n  #error "unsupported arch"\n#endif'
        new = '\n#elif defined(__riscv) && __riscv_xlen == 64\n    uint64_t result;\n    asm volatile("rdcycle %0" : "=r" (result));\n    return result;\n#else\n  #error "unsupported arch"\n#endif'
        assert c.count(old) == 1, f'time.h: pattern mismatch ({c.count(old)})'
        c = c.replace(old, new)
        with open(f, 'w') as fh:
            fh.write(c)
        print('time.h patched')

def patch_bthread_context(brpc_src_dir):
    bthread = os.path.join(brpc_src_dir, 'src', 'bthread')
    # context.h
    f = os.path.join(bthread, 'context.h')
    with open(f) as fh:
        c = fh.read()
    if 'riscv' in c:
        print('context.h already patched')
    else:
        old = '\t#elif __aarch64__\n\t    #define BTHREAD_CONTEXT_PLATFORM_linux_arm64\n\t    #define BTHREAD_CONTEXT_CALL_CONVENTION\n\t#endif'
        new = '\t#elif __aarch64__\n\t    #define BTHREAD_CONTEXT_PLATFORM_linux_arm64\n\t    #define BTHREAD_CONTEXT_CALL_CONVENTION\n\t#elif defined(__riscv) && __riscv_xlen == 64\n\t    #define BTHREAD_CONTEXT_PLATFORM_linux_riscv64\n\t    #define BTHREAD_CONTEXT_CALL_CONVENTION\n\t#endif'
        assert c.count(old) == 1, f'context.h: pattern mismatch ({c.count(old)})'
        c = c.replace(old, new)
        with open(f, 'w') as fh:
            fh.write(c)
        print('context.h patched')
    # context.cpp
    f = os.path.join(bthread, 'context.cpp')
    with open(f) as fh:
        c = fh.read()
    if 'riscv' in c:
        print('context.cpp already patched')
    else:
        riscv_code = '''
#if defined(BTHREAD_CONTEXT_PLATFORM_linux_riscv64) && defined(BTHREAD_CONTEXT_COMPILER_gcc)
__asm (
".text\\n"
".globl bthread_jump_fcontext\\n"
".type bthread_jump_fcontext,@function\\n"
".align 4\\n"
"bthread_jump_fcontext:\\n"
"    addi sp, sp, -0xd0\\n"
"    sd s0, 0x60(sp)\\n"
"    sd s1, 0x68(sp)\\n"
"    sd s2, 0x70(sp)\\n"
"    sd s3, 0x78(sp)\\n"
"    sd s4, 0x80(sp)\\n"
"    sd s5, 0x88(sp)\\n"
"    sd s6, 0x90(sp)\\n"
"    sd s7, 0x98(sp)\\n"
"    sd s8, 0xa0(sp)\\n"
"    sd s9, 0xa8(sp)\\n"
"    sd s10, 0xb0(sp)\\n"
"    sd s11, 0xb8(sp)\\n"
"    sd ra, 0xc0(sp)\\n"
"    sd ra, 0xc8(sp)\\n"
"    sd sp, 0(a0)\\n"
"    mv sp, a1\\n"
"    ld s0, 0x60(sp)\\n"
"    ld s1, 0x68(sp)\\n"
"    ld s2, 0x70(sp)\\n"
"    ld s3, 0x78(sp)\\n"
"    ld s4, 0x80(sp)\\n"
"    ld s5, 0x88(sp)\\n"
"    ld s6, 0x90(sp)\\n"
"    ld s7, 0x98(sp)\\n"
"    ld s8, 0xa0(sp)\\n"
"    ld s9, 0xa8(sp)\\n"
"    ld s10, 0xb0(sp)\\n"
"    ld s11, 0xb8(sp)\\n"
"    ld ra, 0xc0(sp)\\n"
"    ld t0, 0xc8(sp)\\n"
"    addi sp, sp, 0xd0\\n"
"    mv a0, a2\\n"
"    jr t0\\n"
".size bthread_jump_fcontext,.-bthread_jump_fcontext\\n"
".section .note.GNU-stack,\\"\\",%progbits\\n"
);
#endif

#if defined(BTHREAD_CONTEXT_PLATFORM_linux_riscv64) && defined(BTHREAD_CONTEXT_COMPILER_gcc)
__asm (
".text\\n"
".globl bthread_make_fcontext\\n"
".type bthread_make_fcontext,@function\\n"
".align 4\\n"
"bthread_make_fcontext:\\n"
"    andi a0, a0, ~0xF\\n"
"    addi a0, a0, -0xd0\\n"
"    sd a2, 0xc8(a0)\\n"
"    lla a4, riscv_finish\\n"
"    sd a4, 0xc0(a0)\\n"
"    ret\\n"
"riscv_finish:\\n"
"    li a0, 0\\n"
"    tail _exit@plt\\n"
".size bthread_make_fcontext,.-bthread_make_fcontext\\n"
".section .note.GNU-stack,\\"\\",%progbits\\n"
);
#endif
'''
        last_endif = c.rstrip().rfind('#endif')
        if last_endif > 0:
            c = c[:last_endif] + '#endif' + riscv_code
            with open(f, 'w') as fh:
                fh.write(c)
            print('context.cpp patched (appended after final #endif)')
        else:
            print('ERROR: no #endif found in context.cpp')

def patch_processor(brpc_src_dir):
    bthread = os.path.join(brpc_src_dir, 'src', 'bthread')
    f = os.path.join(bthread, 'processor.h')
    with open(f) as fh:
        c = fh.read()
    if 'ARCH_CPU_RISCV_FAMILY' in c:
        print('processor.h already patched')
    else:
        old = ('#if defined(ARCH_CPU_ARM_FAMILY)\n'
               '# define cpu_relax() asm volatile("yield\\n": : :"memory")')
        new = ('#if defined(ARCH_CPU_RISCV_FAMILY)\n'
               '# define cpu_relax() asm volatile("nop\\n": : :"memory")\n'
               '#elif defined(ARCH_CPU_ARM_FAMILY)\n'
               '# define cpu_relax() asm volatile("yield\\n": : :"memory")')
        if old in c:
            c = c.replace(old, new, 1)
            with open(f, 'w') as fh:
                fh.write(c)
            print('processor.h patched')
        else:
            print('ERROR: processor.h pattern not found')

def patch_atomic64(brpc_src_dir):
    butil = os.path.join(brpc_src_dir, 'src', 'butil')
    f = os.path.join(butil, 'atomicops_internals_gcc.h')
    with open(f) as fh:
        c = fh.read()
    if 'Atomic64 Acquire_Load' in c:
        print('atomic64 already patched')
    else:
        a64 = '''
// Atomic64 implementations for 64-bit platforms
#if defined(ARCH_CPU_64_BITS)

inline Atomic64 NoBarrier_CompareAndSwap(volatile Atomic64* ptr,
                                         Atomic64 old_value,
                                         Atomic64 new_value) {
  Atomic64 prev_value;
  do {
    if (__sync_bool_compare_and_swap(ptr, old_value, new_value))
      return old_value;
    prev_value = *ptr;
  } while (prev_value == old_value);
  return prev_value;
}

inline Atomic64 NoBarrier_AtomicExchange(volatile Atomic64* ptr,
                                         Atomic64 new_value) {
  Atomic64 old_value;
  do {
    old_value = *ptr;
  } while (!__sync_bool_compare_and_swap(ptr, old_value, new_value));
  return old_value;
}

inline Atomic64 NoBarrier_AtomicIncrement(volatile Atomic64* ptr,
                                          Atomic64 increment) {
  return Barrier_AtomicIncrement(ptr, increment);
}

inline Atomic64 Barrier_AtomicIncrement(volatile Atomic64* ptr,
                                        Atomic64 increment) {
  for (;;) {
    Atomic64 old_value = *ptr;
    Atomic64 new_value = old_value + increment;
    if (__sync_bool_compare_and_swap(ptr, old_value, new_value)) {
      return new_value;
    }
  }
}

inline Atomic64 Acquire_CompareAndSwap(volatile Atomic64* ptr,
                                       Atomic64 old_value,
                                       Atomic64 new_value) {
  return NoBarrier_CompareAndSwap(ptr, old_value, new_value);
}

inline Atomic64 Release_CompareAndSwap(volatile Atomic64* ptr,
                                       Atomic64 old_value,
                                       Atomic64 new_value) {
  return NoBarrier_CompareAndSwap(ptr, old_value, new_value);
}

inline void NoBarrier_Store(volatile Atomic64* ptr, Atomic64 value) {
  *ptr = value;
}

inline void Acquire_Store(volatile Atomic64* ptr, Atomic64 value) {
  *ptr = value;
  MemoryBarrier();
}

inline void Release_Store(volatile Atomic64* ptr, Atomic64 value) {
  MemoryBarrier();
  *ptr = value;
}

inline Atomic64 NoBarrier_Load(volatile const Atomic64* ptr) {
  return *ptr;
}

inline Atomic64 Acquire_Load(volatile const Atomic64* ptr) {
  Atomic64 value = *ptr;
  MemoryBarrier();
  return value;
}

inline Atomic64 Release_Load(volatile const Atomic64* ptr) {
  MemoryBarrier();
  return *ptr;
}

#endif  // ARCH_CPU_64_BITS
'''
        marker = '}  // namespace butil::subtle\n}  // namespace butil\n\n#endif  // BUTIL_ATOMICOPS_INTERNALS_GCC_H_'
        assert marker in c, f'atomicops_internals_gcc.h: end marker not found ({repr(c[-200:])})'
        c = c.replace(marker, a64 + marker, 1)
        with open(f, 'w') as fh:
            fh.write(c)
        print('atomic64 patched')


if __name__ == '__main__':
    if len(sys.argv) > 1:
        patch_brpc(sys.argv[1])
        patch_bthread_context(sys.argv[1])
        patch_processor(sys.argv[1])
        patch_atomic64(sys.argv[1])
    else:
        print('Usage: patch_brpc.py <brpc_source_dir>')
