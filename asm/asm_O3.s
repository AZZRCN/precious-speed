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
	.text
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
	movq	-24(%rax), %rax
	movq	%rcx, %rbx
	movq	240(%rcx,%rax), %rcx
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
	leaq	_ZNKSt5ctypeIcE8do_widenEc(%rip), %r8
	movq	(%rcx), %rax
	movq	48(%rax), %rax
	cmpq	%r8, %rax
	je	.L6
	movl	$10, %edx
	call	*%rax
	movsbl	%al, %edx
	jmp	.L6
.L8:
	call	_ZSt16__throw_bad_castv
	nop
	.seh_endproc
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
	movq	%rsi, %r8
	movl	$3518437209, %r9d
	movl	44(%rsp), %ebp
	leaq	6000000(%rsi), %r10
	movss	.LC1(%rip), %xmm1
	imull	$1315423911, %edi, %ecx
	xorl	%ebp, %ecx
	movl	%ecx, %eax
.L10:
	imull	$1103515245, %eax, %eax
	pxor	%xmm0, %xmm0
	addq	$4, %r8
	leal	12345(%rax), %edx
	movq	%rdx, %rax
	imulq	%r9, %rdx
	movl	%eax, %r11d
	imull	$1103515245, %eax, %eax
	addl	$12345, %eax
	shrq	$45, %rdx
	imull	$10000, %edx, %edx
	subl	%edx, %r11d
	cvtsi2ssl	%r11d, %xmm0
	divss	%xmm1, %xmm0
	movss	%xmm0, -4(%r8)
	cmpq	%r8, %r10
	jne	.L10
	leaq	b_arr(%rip), %rbx
	movl	$3518437209, %r9d
	leaq	6000000(%rbx), %r10
	movq	%rbx, %rdx
.L11:
	imull	$-1029531031, %ecx, %ecx
	pxor	%xmm0, %xmm0
	addq	$4, %rdx
	leal	-740551042(%rcx), %eax
	movq	%rax, %rcx
	imulq	%r9, %rax
	movl	%ecx, %r8d
	shrq	$45, %rax
	imull	$10000, %eax, %eax
	subl	%eax, %r8d
	cvtsi2ssl	%r8d, %xmm0
	divss	%xmm1, %xmm0
	movss	%xmm0, -4(%rdx)
	cmpq	%rdx, %r10
	jne	.L11
	leaq	c_arr(%rip), %rcx
	xorl	%edx, %edx
	movl	$6000000, %r8d
	call	memset
	movss	.LC3(%rip), %xmm1
	movl	$200, %edx
	movss	5999996+b_arr(%rip), %xmm3
	movss	a_arr(%rip), %xmm2
	shufps	$0, %xmm1, %xmm1
.L12:
	xorl	%eax, %eax
	.p2align 6
	.p2align 4,,10
	.p2align 3
.L13:
	movaps	(%rsi,%rax), %xmm0
	leaq	c_arr(%rip), %rcx
	mulps	%xmm1, %xmm0
	addps	(%rbx,%rax), %xmm0
	movaps	%xmm0, (%rcx,%rax)
	addq	$16, %rax
	cmpq	$6000000, %rax
	jne	.L13
	subl	$1, %edx
	movss	%xmm2, 5999996(%rbx)
	movaps	%xmm2, %xmm0
	movaps	%xmm3, %xmm2
	movss	%xmm3, (%rsi)
	je	.L14
	movaps	%xmm0, %xmm3
	jmp	.L12
.L14:
	leaq	c_arr(%rip), %rax
	pxor	%xmm0, %xmm0
	leaq	6000000(%rax), %rdx
.L15:
	addss	(%rax), %xmm0
	addq	$16, %rax
	addss	-12(%rax), %xmm0
	addss	-8(%rax), %xmm0
	addss	-4(%rax), %xmm0
	cmpq	%rax, %rdx
	jne	.L15
	ucomiss	%xmm0, %xmm0
	jp	.L22
	movq	.refptr._ZSt4cout(%rip), %rcx
	leal	(%rdi,%rbp), %edx
	call	_ZNSolsEi
	movq	%rax, %rcx
	call	_ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_.isra.0
.L17:
	xorl	%eax, %eax
	addq	$56, %rsp
	popq	%rbx
	popq	%rsi
	popq	%rdi
	popq	%rbp
	ret
.L22:
	movq	.refptr._ZSt4cout(%rip), %rcx
	movl	$-1, %edx
	call	_ZNSolsEi
	movq	%rax, %rcx
	call	_ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_.isra.0
	jmp	.L17
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
.LC1:
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
