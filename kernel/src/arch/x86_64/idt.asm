extern int_common

%macro INT 2

global int_%1
extern int_%1
int_%1:
	%ifidn %2, N
	push qword 0          ; Push a fake error code
	%endif
	cmp qword [rsp + 16], 0x4b  ; if user
	swapgs
	jne ._after_swap_gs
._after_swap_gs:
	push qword 0x%1       ; Push the interrupt number
	jmp int_common        ; Jump to the common trap handler
%endmacro


INT 00, N
INT 01, N
INT 02, N
INT 03, N
INT 04, N
INT 05, N
INT 06, N
INT 07, N
INT 08, Y
INT 09, N
INT 0A, Y
INT 0B, Y
INT 0C, Y
INT 0D, Y
INT 0E, Y
INT 0F, N
INT 10, N
INT 11, Y
INT 12, N
INT 13, N
INT 14, N
INT 15, N
INT 16, N
INT 17, Y
INT 18, N
INT 19, N
INT 1A, N
INT 1B, Y
INT 1C, Y
INT 1D, N
INT 1E, N
INT 1F, N

%macro INTGRP 1
INT %{1}0, N
INT %{1}1, N
INT %{1}2, N
INT %{1}3, N
INT %{1}4, N
INT %{1}5, N
INT %{1}6, N
INT %{1}7, N
INT %{1}8, N
INT %{1}9, N
INT %{1}A, N
INT %{1}B, N
INT %{1}C, N
INT %{1}D, N
INT %{1}E, N
INT %{1}F, N
%endmacro

INTGRP 2
INTGRP 3
INTGRP 4
INTGRP 5
INTGRP 6
INTGRP 7
INTGRP 8
INTGRP 9
INTGRP A
INTGRP B
INTGRP C
INTGRP D
INTGRP E
INTGRP F

%unmacro INTGRP 1

%unmacro INT 2
section .data
global int_lst
int_lst:
%macro INT 2
    dq int_%1
%endmacro


INT 00, N
INT 01, N
INT 02, N
INT 03, N
INT 04, N
INT 05, N
INT 06, N
INT 07, N
INT 08, Y
INT 09, N
INT 0A, Y
INT 0B, Y
INT 0C, Y
INT 0D, Y
INT 0E, Y
INT 0F, N
INT 10, N
INT 11, Y
INT 12, N
INT 13, N
INT 14, N
INT 15, N
INT 16, N
INT 17, Y
INT 18, N
INT 19, N
INT 1A, N
INT 1B, Y
INT 1C, Y
INT 1D, N
INT 1E, N
INT 1F, N

%macro INTGRP 1
INT %{1}0, N
INT %{1}1, N
INT %{1}2, N
INT %{1}3, N
INT %{1}4, N
INT %{1}5, N
INT %{1}6, N
INT %{1}7, N
INT %{1}8, N
INT %{1}9, N
INT %{1}A, N
INT %{1}B, N
INT %{1}C, N
INT %{1}D, N
INT %{1}E, N
INT %{1}F, N
%endmacro

INTGRP 2
INTGRP 3
INTGRP 4
INTGRP 5
INTGRP 6
INTGRP 7
INTGRP 8
INTGRP 9
INTGRP A
INTGRP B
INTGRP C
INTGRP D
INTGRP E
INTGRP F

%unmacro INTGRP 1
section .text
extern idt_main_handler
int_common:
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rbp
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    push rax
    mov eax, es
    push rax
    mov eax, ds
    push rax
    mov rax, cr2
    push rax

    cld

    mov eax, 0x30
    mov ds, eax
    mov es, eax
    mov ss, eax

    mov rdi, rsp

    call idt_main_handler
    pop rax
    mov ds, eax
    pop rax
    mov es, eax
    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    add rsp, 16
    iretq