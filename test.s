.intel_syntax noprefix
.global _start
_start:
    call _main
    mov rdi, rax
    mov eax, 0x2000001
    syscall
.global _main
_main:
    push rbp
    mov rbp, rsp
    mov rax, 5
    sub rsp, 8
    mov qword ptr [rbp - 8], rax
    mov rax, 1
    push rax
    mov rax, qword ptr [rbp - 8]
    pop rbx
    add rax, rbx
    sub rsp, 8
    mov qword ptr [rbp - 16], rax
    mov rax, qword ptr [rbp - 16]
    push rax
    mov rax, qword ptr [rbp - 8]
    pop rbx
    add rax, rbx
    mov rsp, rbp
    pop rbp
    ret
