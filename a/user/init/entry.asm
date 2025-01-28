global  _startaaaaaaaaaaaaaaaaa
extern __mlibc_entry
_start2:
    jmp __mlibc_entry

section .text

syscall2:
    endbr64
    mov rax, rdi
    mov rdi, rsi
    mov rsi, rdx
    mov rdx, rcx
    mov r8, r10
    mov r9, r8
    syscall
    ret

syscall:
    jmp syscall2

global syscall

global syscall2