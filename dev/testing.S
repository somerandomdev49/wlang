.intel_syntax noprefix
.global _start
_start:
    call _main
    mov rdi, 5
    mov eax, 0x2000001
    syscall

.global _main
_main:
    push rbp
    mov rbp, rsp
    mov rax, 5
    pop rbp
    ret
