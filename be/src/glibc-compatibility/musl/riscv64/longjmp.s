.global musl_glibc_longjmp
.type musl_glibc_longjmp,@function
musl_glibc_longjmp:
	// riscv64 callee-saved registers (jmp_buf layout)
	// s0 - s11 (x8,x9,x18-x27), sp, ra
	ld s0,  0*8(a0)
	ld s1,  1*8(a0)
	ld s2,  2*8(a0)
	ld s3,  3*8(a0)
	ld s4,  4*8(a0)
	ld s5,  5*8(a0)
	ld s6,  6*8(a0)
	ld s7,  7*8(a0)
	ld s8,  8*8(a0)
	ld s9,  9*8(a0)
	ld s10, 10*8(a0)
	ld s11, 11*8(a0)
	ld sp,  12*8(a0)
	ld ra,  13*8(a0)

	// return val: if val != 0 use val, else 1
	mv a0, a1
	bnez a1, 1f
	li a0, 1
1:
	ret
