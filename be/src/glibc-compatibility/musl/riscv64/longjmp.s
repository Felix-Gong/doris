.global musl_glibc_longjmp
.type musl_glibc_longjmp,@function
musl_glibc_longjmp:
	# dummy
	# dummy
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

	# dummy
	mv a0, a1
	bnez a1, 1f
	li a0, 1
1:
	ret
