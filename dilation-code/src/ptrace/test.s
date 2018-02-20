	.file	"test.c"
	.section	.rodata
.LC1:
	.string	"%Y:%m:%d %H:%M:%S"
.LC2:
	.string	"%s.%03d\n"
	.text
	.globl	print_curr_time
	.type	print_curr_time, @function
print_curr_time:
.LFB0:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	subq	$80, %rsp
	movq	%fs:40, %rax
	movq	%rax, -8(%rbp)
	xorl	%eax, %eax
	leaq	-64(%rbp), %rax
	movl	$0, %esi
	movq	%rax, %rdi
	call	gettimeofday
	movq	-56(%rbp), %rax
	pxor	%xmm0, %xmm0
	cvtsi2sd	%eax, %xmm0
	movsd	.LC0(%rip), %xmm1
	divsd	%xmm1, %xmm0
	cvttsd2si	%xmm0, %eax
	movl	%eax, -76(%rbp)
	cmpl	$999, -76(%rbp)
	jle	.L2
	subl	$1000, -76(%rbp)
	movq	-64(%rbp), %rax
	addq	$1, %rax
	movq	%rax, -64(%rbp)
.L2:
	leaq	-64(%rbp), %rax
	movq	%rax, %rdi
	call	localtime
	movq	%rax, -72(%rbp)
	movq	-72(%rbp), %rdx
	leaq	-48(%rbp), %rax
	movq	%rdx, %rcx
	movl	$.LC1, %edx
	movl	$26, %esi
	movq	%rax, %rdi
	call	strftime
	movq	stderr(%rip), %rax
	movl	-76(%rbp), %ecx
	leaq	-48(%rbp), %rdx
	movl	$.LC2, %esi
	movq	%rax, %rdi
	movl	$0, %eax
	call	fprintf
	movq	stdout(%rip), %rax
	movq	%rax, %rdi
	call	fflush
	nop
	movq	-8(%rbp), %rax
	xorq	%fs:40, %rax
	je	.L3
	call	__stack_chk_fail
.L3:
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE0:
	.size	print_curr_time, .-print_curr_time
	.section	.rodata
.LC3:
	.string	"Hello World\n"
	.text
	.globl	main
	.type	main, @function
main:
.LFB1:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	subq	$16, %rsp
	movq	stderr(%rip), %rax
	movq	%rax, %rcx
	movl	$12, %edx
	movl	$1, %esi
	movl	$.LC3, %edi
	call	fwrite
	movq	stdout(%rip), %rax
	movq	%rax, %rdi
	call	fflush
	movl	$0, -4(%rbp)
	jmp	.L5
.L6:
	addl	$1, -4(%rbp)
.L5:
	cmpl	$999999, -4(%rbp)
	jle	.L6
	nop
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE1:
	.size	main, .-main
	.section	.rodata
	.align 8
.LC0:
	.long	0
	.long	1083129856
	.ident	"GCC: (Ubuntu 5.4.0-6ubuntu1~16.04.5) 5.4.0 20160609"
	.section	.note.GNU-stack,"",@progbits
