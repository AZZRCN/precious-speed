	.file	"o3_verify.cpp"
	.text
	.section	.text$_ZNKSt5ctypeIcE8do_widenEc,"x"
	.linkonce discard
	.align 2
	.p2align 4
	.globl	_ZNKSt5ctypeIcE8do_widenEc
	.def	_ZNKSt5ctypeIcE8do_widenEc;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZNKSt5ctypeIcE8do_widenEc
_ZNKSt5ctypeIcE8do_widenEc:
.LFB2353:
	.seh_endprologue
	movl	%edx, %eax
	ret
	.seh_endproc
	.section	.text.unlikely,"x"
.LCOLDB0:
	.text
.LHOTB0:
	.p2align 4
	.def	_ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_.isra.0;	.scl	3;	.type	32;	.endef
	.seh_proc	_ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_.isra.0
_ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_.isra.0:
.LFB4009:
	pushq	%rbx
	.seh_pushreg	%rbx
	subq	$48, %rsp
	.seh_stackalloc	48
	.seh_endprologue
	movq	(%rcx), %rax
	movq	-24(%rax), %rdx
	movq	%rcx, %rbx
	movq	240(%rcx,%rdx), %rcx
	testq	%rcx, %rcx
	je	.L8
	cmpb	$0, 56(%rcx)
	je	.L5
	movsbl	67(%rcx), %edx
.L6:
	movq	%rbx, %rcx
	call	_ZNSo3putEc
	movq	%rax, %rcx
	addq	$48, %rsp
	popq	%rbx
	jmp	_ZNSo5flushEv
.L5:
	movq	%rcx, 40(%rsp)
	call	_ZNKSt5ctypeIcE13_M_widen_initEv
	movq	40(%rsp), %rcx
	movl	$10, %edx
	leaq	_ZNKSt5ctypeIcE8do_widenEc(%rip), %r9
	movq	(%rcx), %r8
	movq	48(%r8), %rax
	cmpq	%r9, %rax
	je	.L6
	movl	$10, %edx
	call	*%rax
	movsbl	%al, %edx
	jmp	.L6
	.seh_endproc
	.section	.text.unlikely,"x"
	.def	_ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_.isra.0.cold;	.scl	3;	.type	32;	.endef
	.seh_proc	_ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_.isra.0.cold
	.seh_stackalloc	56
	.seh_savereg	%rbx, 48
	.seh_endprologue
_ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_.isra.0.cold:
.L8:
	call	_ZSt16__throw_bad_castv
	nop
	.text
	.section	.text.unlikely,"x"
	.seh_endproc
.LCOLDE0:
	.text
.LHOTE0:
	.section	.text.startup,"x"
	.p2align 4
	.globl	main
	.def	main;	.scl	2;	.type	32;	.endef
	.seh_proc	main
main:
.LFB3252:
	pushq	%rbp
	.seh_pushreg	%rbp
	pushq	%rdi
	.seh_pushreg	%rdi
	pushq	%rsi
	.seh_pushreg	%rsi
	pushq	%rbx
	.seh_pushreg	%rbx
	subq	$56, %rsp
	.seh_stackalloc	56
	.seh_endprologue
	leaq	a_arr(%rip), %rsi
	call	__main
	xorl	%ecx, %ecx
	call	_ZNSt8ios_base15sync_with_stdioEb
	movq	.refptr._ZSt3cin(%rip), %rcx
	leaq	40(%rsp), %rdx
	movq	$0, 232(%rcx)
	call	_ZNSirsERi
	leaq	44(%rsp), %rdx
	movq	%rax, %rcx
	call	_ZNSirsERi
	movl	40(%rsp), %edi
	movq	%rsi, %rcx
	movl	$3518437209, %r8d
	movl	44(%rsp), %ebp
	leaq	6000000(%rsi), %r9
	movss	.LC2(%rip), %xmm0
	imull	$1315423911, %edi, %edx
	xorl	%ebp, %edx
	movl	%edx, %r10d
.L10:
	pxor	%xmm1, %xmm1
	pxor	%xmm2, %xmm2
	pxor	%xmm3, %xmm3
	pxor	%xmm4, %xmm4
	imull	$1103515245, %r10d, %eax
	pxor	%xmm5, %xmm5
	addq	$20, %rcx
	leal	12345(%rax), %r10d
	movq	%r10, %rbx
	imulq	%r8, %r10
	movl	%ebx, %r11d
	shrq	$45, %r10
	imull	$10000, %r10d, %eax
	imull	$-1029531031, %ebx, %r10d
	subl	%eax, %r11d
	leal	-740551042(%r10), %ebx
	cvtsi2ssl	%r11d, %xmm1
	movq	%rbx, %rax
	imulq	%r8, %rbx
	movl	%eax, %r11d
	divss	%xmm0, %xmm1
	shrq	$45, %rbx
	imull	$10000, %ebx, %r10d
	imull	$-1029531031, %eax, %ebx
	subl	%r10d, %r11d
	leal	-740551042(%rbx), %r10d
	cvtsi2ssl	%r11d, %xmm2
	movq	%r10, %rax
	imulq	%r8, %r10
	movl	%eax, %r11d
	shrq	$45, %r10
	imull	$10000, %r10d, %ebx
	imull	$-1029531031, %eax, %r10d
	divss	%xmm0, %xmm2
	movss	%xmm1, -20(%rcx)
	subl	%ebx, %r11d
	leal	-740551042(%r10), %ebx
	cvtsi2ssl	%r11d, %xmm3
	movq	%rbx, %rax
	imulq	%r8, %rbx
	movl	%eax, %r11d
	shrq	$45, %rbx
	imull	$10000, %ebx, %r10d
	imull	$-1029531031, %eax, %ebx
	subl	%r10d, %r11d
	leal	-740551042(%rbx), %r10d
	cvtsi2ssl	%r11d, %xmm4
	movq	%r10, %rax
	imulq	%r8, %r10
	movl	%eax, %r11d
	divss	%xmm0, %xmm3
	movss	%xmm2, -16(%rcx)
	shrq	$45, %r10
	imull	$10000, %r10d, %ebx
	imull	$1103515245, %eax, %r10d
	subl	%ebx, %r11d
	cvtsi2ssl	%r11d, %xmm5
	addl	$12345, %r10d
	divss	%xmm0, %xmm4
	movss	%xmm3, -12(%rcx)
	divss	%xmm0, %xmm5
	movss	%xmm4, -8(%rcx)
	movss	%xmm5, -4(%rcx)
	cmpq	%r9, %rcx
	jne	.L10
	leaq	b_arr(%rip), %rbx
	movl	$3518437209, %r8d
	leaq	6000000(%rbx), %r9
	movq	%rbx, %rcx
.L11:
	pxor	%xmm1, %xmm1
	pxor	%xmm2, %xmm2
	pxor	%xmm3, %xmm3
	pxor	%xmm4, %xmm4
	imull	$-1029531031, %edx, %edx
	pxor	%xmm5, %xmm5
	addq	$20, %rcx
	leal	-740551042(%rdx), %r11d
	movq	%r11, %rax
	imulq	%r8, %r11
	movl	%eax, %r10d
	shrq	$45, %r11
	imull	$10000, %r11d, %edx
	imull	$-1029531031, %eax, %r11d
	subl	%edx, %r10d
	leal	-740551042(%r11), %edx
	cvtsi2ssl	%r10d, %xmm1
	movq	%rdx, %rax
	imulq	%r8, %rdx
	movl	%eax, %r10d
	divss	%xmm0, %xmm1
	shrq	$45, %rdx
	imull	$10000, %edx, %r11d
	imull	$-1029531031, %eax, %edx
	subl	%r11d, %r10d
	leal	-740551042(%rdx), %r11d
	cvtsi2ssl	%r10d, %xmm2
	movq	%r11, %rax
	imulq	%r8, %r11
	movl	%eax, %r10d
	shrq	$45, %r11
	imull	$10000, %r11d, %edx
	imull	$-1029531031, %eax, %r11d
	divss	%xmm0, %xmm2
	movss	%xmm1, -20(%rcx)
	subl	%edx, %r10d
	leal	-740551042(%r11), %edx
	cvtsi2ssl	%r10d, %xmm3
	movq	%rdx, %rax
	imulq	%r8, %rdx
	movl	%eax, %r10d
	imull	$-1029531031, %eax, %eax
	subl	$740551042, %eax
	shrq	$45, %rdx
	imull	$10000, %edx, %r11d
	movq	%rax, %rdx
	imulq	%r8, %rax
	subl	%r11d, %r10d
	cvtsi2ssl	%r10d, %xmm4
	movl	%edx, %r10d
	divss	%xmm0, %xmm3
	shrq	$45, %rax
	imull	$10000, %eax, %r11d
	movss	%xmm2, -16(%rcx)
	subl	%r11d, %r10d
	cvtsi2ssl	%r10d, %xmm5
	divss	%xmm0, %xmm4
	movss	%xmm3, -12(%rcx)
	divss	%xmm0, %xmm5
	movss	%xmm4, -8(%rcx)
	movss	%xmm5, -4(%rcx)
	cmpq	%rcx, %r9
	jne	.L11
	leaq	c_arr(%rip), %rcx
	movl	$6000000, %r8d
	xorl	%edx, %edx
	call	memset
	movss	5999996+b_arr(%rip), %xmm4
	movl	$200, %r8d
	movss	a_arr(%rip), %xmm3
	movss	.LC3(%rip), %xmm0
	movaps	%xmm4, %xmm5
	movaps	%xmm3, %xmm2
.L13:
	movaps	%xmm2, %xmm1
	movl	$20, %r9d
	mulss	%xmm0, %xmm1
	addss	(%rbx), %xmm1
	movss	%xmm1, c_arr(%rip)
	movss	4(%rsi), %xmm1
	mulss	%xmm0, %xmm1
	addss	4(%rbx), %xmm1
	movss	%xmm1, 4+c_arr(%rip)
	movss	8(%rsi), %xmm1
	mulss	%xmm0, %xmm1
	addss	8(%rbx), %xmm1
	movss	%xmm1, 8+c_arr(%rip)
	movss	12(%rsi), %xmm1
	mulss	%xmm0, %xmm1
	addss	12(%rbx), %xmm1
	movss	%xmm1, 12+c_arr(%rip)
	movss	16(%rsi), %xmm1
	mulss	%xmm0, %xmm1
	addss	16(%rbx), %xmm1
	movss	%xmm1, 16+c_arr(%rip)
	movss	20(%rsi), %xmm1
	jmp	.L12
	.p2align 4,,10
	.p2align 3
.L63:
	movss	4(%rsi,%r9), %xmm1
	mulss	%xmm0, %xmm1
	addss	4(%rbx,%r9), %xmm1
	movss	%xmm1, 4(%rcx,%r9)
	movss	8(%rsi,%r9), %xmm1
	mulss	%xmm0, %xmm1
	addss	8(%rbx,%r9), %xmm1
	movss	%xmm1, 8(%rcx,%r9)
	movss	12(%rsi,%r9), %xmm1
	mulss	%xmm0, %xmm1
	addss	12(%rbx,%r9), %xmm1
	movss	%xmm1, 12(%rcx,%r9)
	movss	16(%rsi,%r9), %xmm1
	mulss	%xmm0, %xmm1
	addss	16(%rbx,%r9), %xmm1
	movss	%xmm1, 16(%rcx,%r9)
	movss	20(%rsi,%r9), %xmm1
	mulss	%xmm0, %xmm1
	addss	20(%rbx,%r9), %xmm1
	movss	%xmm1, 20(%rcx,%r9)
	movss	24(%rsi,%r9), %xmm1
	mulss	%xmm0, %xmm1
	addss	24(%rbx,%r9), %xmm1
	movss	%xmm1, 24(%rcx,%r9)
	movss	28(%rsi,%r9), %xmm1
	mulss	%xmm0, %xmm1
	addss	28(%rbx,%r9), %xmm1
	movss	%xmm1, 28(%rcx,%r9)
	movss	32(%rsi,%r9), %xmm1
	mulss	%xmm0, %xmm1
	addss	32(%rbx,%r9), %xmm1
	movss	%xmm1, 32(%rcx,%r9)
	addq	$36, %r9
	movss	(%rsi,%r9), %xmm1
.L12:
	mulss	%xmm0, %xmm1
	addss	(%rbx,%r9), %xmm1
	cmpq	$5999996, %r9
	leaq	c_arr(%rip), %rcx
	movss	%xmm1, (%rcx,%r9)
	jne	.L63
	subl	$1, %r8d
	movss	%xmm2, 5999996(%rbx)
	je	.L64
	movaps	%xmm5, %xmm1
	movaps	%xmm2, %xmm5
	movaps	%xmm1, %xmm2
	jmp	.L13
.L64:
	movl	$199, %r11d
	jmp	.L14
.L20:
	movaps	%xmm3, %xmm4
	subl	$9, %r11d
	movaps	%xmm2, %xmm3
.L14:
	cmpl	$1, %r11d
	movaps	%xmm4, %xmm2
	jne	.L20
	leaq	c_arr(%rip), %rax
	movss	%xmm3, a_arr(%rip)
	pxor	%xmm3, %xmm3
	leaq	6000000(%rax), %rdx
.L16:
	addss	(%rax), %xmm3
	addq	$32, %rax
	addss	-28(%rax), %xmm3
	addss	-24(%rax), %xmm3
	addss	-20(%rax), %xmm3
	addss	-16(%rax), %xmm3
	addss	-12(%rax), %xmm3
	addss	-8(%rax), %xmm3
	addss	-4(%rax), %xmm3
	cmpq	%rax, %rdx
	jne	.L16
	ucomiss	%xmm3, %xmm3
	jp	.L65
	movq	.refptr._ZSt4cout(%rip), %rcx
	leal	(%rdi,%rbp), %edx
	call	_ZNSolsEi
	movq	%rax, %rcx
	call	_ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_.isra.0
.L18:
	xorl	%eax, %eax
	addq	$56, %rsp
	popq	%rbx
	popq	%rsi
	popq	%rdi
	popq	%rbp
	ret
.L65:
	movq	.refptr._ZSt4cout(%rip), %rcx
	movl	$-1, %edx
	call	_ZNSolsEi
	movq	%rax, %rcx
	call	_ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_.isra.0
	jmp	.L18
	.seh_endproc
	.globl	c_arr
	.bss
	.align 32
c_arr:
	.space 6000000
	.globl	b_arr
	.align 32
b_arr:
	.space 6000000
	.globl	a_arr
	.align 32
a_arr:
	.space 6000000
	.section .rdata,"dr"
	.align 4
.LC2:
	.long	1176256512
	.align 4
.LC3:
	.long	1065354055
	.def	__main;	.scl	2;	.type	32;	.endef
	.ident	"GCC: (x86_64-win32-seh-rev0, Built by MinGW-Builds project) 15.2.0"
	.def	_ZNSo3putEc;	.scl	2;	.type	32;	.endef
	.def	_ZNSo5flushEv;	.scl	2;	.type	32;	.endef
	.def	_ZNKSt5ctypeIcE13_M_widen_initEv;	.scl	2;	.type	32;	.endef
	.def	_ZSt16__throw_bad_castv;	.scl	2;	.type	32;	.endef
	.def	_ZNSt8ios_base15sync_with_stdioEb;	.scl	2;	.type	32;	.endef
	.def	_ZNSirsERi;	.scl	2;	.type	32;	.endef
	.def	memset;	.scl	2;	.type	32;	.endef
	.def	_ZNSolsEi;	.scl	2;	.type	32;	.endef
	.section	.rdata$.refptr._ZSt4cout, "dr"
	.p2align	3, 0
	.globl	.refptr._ZSt4cout
	.linkonce	discard
.refptr._ZSt4cout:
	.quad	_ZSt4cout
	.section	.rdata$.refptr._ZSt3cin, "dr"
	.p2align	3, 0
	.globl	.refptr._ZSt3cin
	.linkonce	discard
.refptr._ZSt3cin:
	.quad	_ZSt3cin
