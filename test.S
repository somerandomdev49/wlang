	.text
	.intel_syntax noprefix
	.file	"test.c"
	.globl	main                    # -- Begin function main
	.p2align	4, 0x90
	.type	main,@function
main:                                   # @main
	.cfi_startproc
# %bb.0:                                # %entry
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset rbp, -16
	mov	rbp, rsp
	.cfi_def_cfa_register rbp
	mov	dword ptr [rbp - 4], 0
	mov	dword ptr [rbp - 8], 5
	mov	eax, dword ptr [rbp - 8]
	mov	dword ptr [rbp - 12], eax
	mov	eax, dword ptr [rbp - 12]
	pop	rbp
	.cfi_def_cfa rsp, 8
	ret
.Lfunc_end0:
	.size	main, .Lfunc_end0-main
	.cfi_endproc
                                        # -- End function

	.ident	"clang version 7.0.0 (git@github.com:apple/swift-clang.git 5c9d04dc0697297a47b5edb0c1a581b306a42bdb) (git@github.com:apple/swift-llvm.git 34250a6eef79ee8a83c5cfb4deb1583176dcbb63)"
	.section	".note.GNU-stack","",@progbits
	.addrsig
	.addrsig_sym main
