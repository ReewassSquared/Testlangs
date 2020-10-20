	.file	"undeflang.c"
	.text
	.globl	main
	.type	main, @function
main:
.LFB5:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	subq	$32, %rsp
	movl	%edi, -20(%rbp)
	movq	%rsi, -32(%rbp)
	movl	$1000000, %edi
	call	malloc@PLT
	movq	%rax, -16(%rbp)
	movq	-16(%rbp), %rax
	movq	%rax, %rdi
	call	entry@PLT
	movq	%rax, -8(%rbp)
	movq	-16(%rbp), %rax
	movq	%rax, %rdi
	call	free@PLT
	movl	$0, %eax
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE5:
	.size	main, .-main
	.section	.rodata
.LC0:
	.string	"err"
	.text
	.globl	error
	.type	error, @function
error:
.LFB6:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	leaq	.LC0(%rip), %rdi
	call	puts@PLT
	movl	$1, %edi
	call	exit@PLT
	.cfi_endproc
.LFE6:
	.size	error, .-error
	.section	.rodata
.LC1:
	.string	"rts-error"
	.text
	.globl	internal_error
	.type	internal_error, @function
internal_error:
.LFB7:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	leaq	.LC1(%rip), %rdi
	movl	$0, %eax
	call	printf@PLT
	movl	$1, %edi
	call	exit@PLT
	.cfi_endproc
.LFE7:
	.size	internal_error, .-internal_error
	.globl	print_result_
	.type	print_result_, @function
print_result_:
.LFB8:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	subq	$16, %rsp
	movq	%rdi, -8(%rbp)
	movq	-8(%rbp), %rax
	movq	%rax, %rdi
	call	print_result
	movl	$10, %edi
	call	putchar@PLT
	nop
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE8:
	.size	print_result_, .-print_result_
	.section	.rodata
.LC2:
	.string	"#&"
	.text
	.globl	print_result
	.type	print_result, @function
print_result:
.LFB9:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	subq	$16, %rsp
	movq	%rdi, -8(%rbp)
	movq	-8(%rbp), %rax
	andl	$7, %eax
	cmpq	$2, %rax
	je	.L8
	cmpq	$3, %rax
	je	.L9
	testq	%rax, %rax
	jne	.L12
	movq	-8(%rbp), %rax
	movq	%rax, %rdi
	call	print_immediate
	jmp	.L11
.L8:
	leaq	.LC2(%rip), %rdi
	movl	$0, %eax
	call	printf@PLT
	movq	-8(%rbp), %rax
	xorq	$2, %rax
	movq	(%rax), %rax
	movq	%rax, %rdi
	call	print_result
	jmp	.L11
.L9:
	movl	$40, %edi
	call	putchar@PLT
	movq	-8(%rbp), %rax
	movq	%rax, %rdi
	call	print_pair
	movl	$41, %edi
	call	putchar@PLT
	jmp	.L11
.L12:
	movl	$0, %eax
	call	internal_error
.L11:
	nop
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE9:
	.size	print_result, .-print_result
	.section	.rodata
.LC3:
	.string	"%ld"
.LC4:
	.string	"#t"
.LC5:
	.string	"#f"
.LC6:
	.string	"()"
	.text
	.globl	print_immediate
	.type	print_immediate, @function
print_immediate:
.LFB10:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	subq	$16, %rsp
	movq	%rdi, -8(%rbp)
	movq	-8(%rbp), %rax
	andl	$31, %eax
	cmpq	$8, %rax
	je	.L15
	cmpq	$24, %rax
	je	.L16
	testq	%rax, %rax
	jne	.L21
	movq	-8(%rbp), %rax
	sarq	$5, %rax
	movq	%rax, %rsi
	leaq	.LC3(%rip), %rdi
	movl	$0, %eax
	call	printf@PLT
	jmp	.L18
.L15:
	movq	-8(%rbp), %rax
	andq	$-32, %rax
	testq	%rax, %rax
	je	.L19
	leaq	.LC4(%rip), %rdi
	movl	$0, %eax
	call	printf@PLT
	jmp	.L18
.L19:
	leaq	.LC5(%rip), %rdi
	movl	$0, %eax
	call	printf@PLT
	jmp	.L18
.L16:
	leaq	.LC6(%rip), %rdi
	movl	$0, %eax
	call	printf@PLT
	jmp	.L18
.L21:
	movl	$0, %eax
	call	internal_error
.L18:
	nop
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE10:
	.size	print_immediate, .-print_immediate
	.section	.rodata
.LC7:
	.string	" . "
	.text
	.globl	print_pair
	.type	print_pair, @function
print_pair:
.LFB11:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	subq	$32, %rsp
	movq	%rdi, -24(%rbp)
	movq	-24(%rbp), %rax
	xorq	$3, %rax
	movq	(%rax), %rax
	movq	%rax, -16(%rbp)
	movq	-24(%rbp), %rax
	addq	$8, %rax
	xorq	$3, %rax
	movq	(%rax), %rax
	movq	%rax, -8(%rbp)
	movq	-16(%rbp), %rax
	movq	%rax, %rdi
	call	print_result
	movq	-8(%rbp), %rax
	andl	$31, %eax
	cmpq	$24, %rax
	je	.L25
	movq	-8(%rbp), %rax
	andl	$7, %eax
	cmpq	$3, %rax
	jne	.L24
	movl	$32, %edi
	call	putchar@PLT
	movq	-8(%rbp), %rax
	movq	%rax, %rdi
	call	print_pair
	jmp	.L25
.L24:
	leaq	.LC7(%rip), %rdi
	movl	$0, %eax
	call	printf@PLT
	movq	-8(%rbp), %rax
	movq	%rax, %rdi
	call	print_result
.L25:
	nop
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE11:
	.size	print_pair, .-print_pair
	.ident	"GCC: (Ubuntu 7.5.0-3ubuntu1~18.04) 7.5.0"
	.section	.note.GNU-stack,"",@progbits
