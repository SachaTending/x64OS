section .data

magic: db "This program is only for x64OS. Please use x64OS to run this program.", 10, 0
magiclen: equ $ - magic

a: db "Тест, загрузка программ и (надесь) успешное выполнение проги", 10, 0
b: db "ну вобщем теперь надо переместить загрузку прог из krnl/main.cpp в другое место, потом портануть... что портануть? mlibc или newlib(или как там его)?", 10, 0
section .text
global _start
_start:
    mov rax, 1
    mov rdi, 1
    mov rsi, magic
    mov rdx, magiclen
    syscall ; Print "magic" string

    mov rax, 60
    mov rdi, -1
    syscall ; On linux this should end process, but on x64OS it actually switches from linux compatibility mode to native mode

    mov rax, 512
    mov rdi, a
    syscall

    mov rax, 512
    mov rdi, b
    syscall

.1:
    nop
    jmp .1