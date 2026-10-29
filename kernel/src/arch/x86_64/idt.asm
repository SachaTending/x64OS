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
    push qword r15
    push qword r14
    push qword r13
    push qword r12
    push qword r11
    push qword r10
    push qword r9
    push qword r8
    push qword rbp
    push qword rdi
    push qword rsi
    push qword rdx
    push qword rcx
    push qword rbx
    push qword rax
    mov eax, es
    push qword rax
    mov eax, ds
    push qword rax
    mov rax, cr2
    push qword rax

    cld

    mov eax, 0x30
    mov ds, eax
    mov es, eax
    mov ss, eax

    mov rdi, rsp
    xor rbp, rbp
    call idt_main_handler
    ;mov rsp, rax
    add rsp, 8 ; pop cr2
    pop qword rax
    mov ds, eax
    pop qword rax
    mov es, eax
    pop qword rax
    pop qword rbx
    pop qword rcx
    pop qword rdx
    pop qword rsi
    pop qword rdi
    pop qword rbp
    pop qword r8
    pop qword r9
    pop qword r10
    pop qword r11
    pop qword r12
    pop qword r13
    pop qword r14
    pop qword r15
    add rsp, 16
    iretq