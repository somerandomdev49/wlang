.intel_syntax noprefix
.global _start
_start:
	call _main
	mov rdi, rax
	mov rax, 60
	syscall

_main:
	push rbp
	mov	rbp, rsp
	mov qword ptr [rbp - 4], 5
	mov qword ptr [rbp - 8], 5
	add qword ptr [rbp - 4], qword ptr [rbp - 8]
	mov rax, qword ptr [rbp - 4]
	pop rbp
	ret
