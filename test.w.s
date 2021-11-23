.intel_syntax noprefix
.global _start
_start:
    call _main
    mov rdi, rax
    mov eax, 0x3c
    syscall

.global _main
_main:
    push rbp
    mov rbp, rsp
    mov qword ptr [rbp - 4], 5
    mov rax, qword ptr [rbp - 4]
    mov rbx, 1
    add rax, rbx
    mov rax, qword ptr [rbp - 12]
    mov rbx, qword ptr [rbp - 4]
    add rax, rbx
    pop rbp
    ret

