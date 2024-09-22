; Part of source copied from... boron os, more details in gdt2.asm
default rel
section .text
extern idt_handler2
global fetch_cr0
global fetch_cr3
global int_common

fetch_cr0:
	push rbp
	mov rbp, rsp
	mov rax, cr0
	mov rsp, rbp
	pop rbp
	ret

fetch_cr3:
	push rbp
	mov rbp, rsp
	mov rax, cr3
	mov rsp, rbp
	pop rbp
	ret


%macro PUSH_STATE 0
	push r8
	push r9
	push r10
	push r11
	push r12
	push r13
	push r14
	push r15
	push rsi
	push rdi
	;mov  rax, cr3
	;push rax
	mov  rax, cr2
	push rax
	mov  ax,  gs
	push ax
	mov  ax,  fs
	push ax
	mov  ax,  es
	push ax
	mov  ax,  ds
	push ax
	cmp  ax,  0x4b     			  ; If the data segment is in user mode...
	jne  .a1
	swapgs                        ; Swap to the kernel GS.
.a1:
	nop
%endmacro

; Pop the entire state except RAX and RBX
%macro POP_STATE 0
	xor rax, rax
	pop  ax
	mov  ds, ax
	cmp  ax,  0x4b     			  ; If the data segment is in user mode...
	jne  .a2
	swapgs                        ; Swap to the user's GS.
.a2:
	;mov ax, 0x10
	pop  ax
	mov  es, ax
	pop  ax
	;mov ax, 0x10
	mov  fs, ax
	pop  ax
	;mov ax, 0x10
	mov  gs, ax
	add  rsp, 8    ; the space occupied by the cr2 register
	;pop  rax
	;mov  cr3, rax
	pop  rdi
	pop  rsi
	pop  r15
	pop  r14
	pop  r13
	pop  r12
	pop  r11
	pop  r10
	pop  r9
	pop  r8
%endmacro

int_common:
    push  rax
	push  rbx
	push  rcx
	push  rdx
	lea   rbx, [rsp + 32]                  ; Get the pointer to the value after rbx and rax on the stack
	lea   rcx, [rsp + 48]                  ; Get the pointer to the RIP from the interrupt frame.
	lea   rdx, [rsp + 56]                  ; Get the pointer to the CS from the interrupt frame.
	; Note that LEA doesn't actually perform any memory accesses, all it
	; does it load the address of certain things into a register. We then
	; defer actually loading those until after DS was changed.
	PUSH_STATE                             ; Push the state, except for the old ipl
	mov   ax, 0x30                         ; Use ring 0 segment for kernel mode use. We have backed the old values up
	mov   ds,  ax
	mov   es,  ax
	mov   fs,  ax
	mov   ss,  ax
	mov   rbx, [rbx]                       ; Retrieve the interrupt number and RIP from interrupt frame. These were deferred
	mov   rcx, [rcx]                       ; so that we wouldn't attempt to access the kernel stack using the user's data segment.
	mov   rdx, [rdx]                       ; Load CS, to determine the previous mode when entering a hardware interrupt
	push  rcx                              ; Enter a stack frame so that stack printing doesn't skip over anything
	push  rbp
	mov   rbp, rsp
	cld                                    ; Clear direction flag, will be restored by iretq
	mov   rdi, rsp                         ; Retrieve the idt_regs to call the trap handler
	push  rdi							   ; Push pointer to registers
	call  idt_handler2                     ; And call idt_handler!!
	pop   rax							   ; a small workaround for interrupts to work, idt_handler2 returns nothing which results unexpected behaviour without pop rax
	mov   rsp, rax                         ; Use the new idt_regs instance as what to pull:
	pop   rbp                              ; Leave the stack frame
	pop   rcx                              ; Skip over the RIP duplicate that we pushed
	POP_STATE                              ; Pop the state
	pop   rdx                              ; Pop the RDX register
	pop   rcx                              ; Pop the RCX register
	pop   rbx                              ; Pop the RBX register
	pop   rax                              ; Pop the RAX register
	add   rsp, 16                          ; Pop the interrupt number and the error code
	iretq
%macro INT 2

global int_%1
extern int_%1
int_%1:
	%ifidn %2, N
	push qword 0          ; Push a fake error code
	%endif
	push qword 0x%1       ; Push the interrupt number
	jmp int_common        ; Jump to the common trap handler
%endmacro
global syscall_entry
syscall_entry:
	push qword 0
	push qword 1024
	push  rax
	push  rbx
	push  rcx
	push  rdx
	lea   rbx, [rsp + 32]                  ; Get the pointer to the value after rbx and rax on the stack
	lea   rcx, [rsp + 48]                  ; Get the pointer to the RIP from the interrupt frame.
	lea   rdx, [rsp + 56]                  ; Get the pointer to the CS from the interrupt frame.
	; Note that LEA doesn't actually perform any memory accesses, all it
	; does it load the address of certain things into a register. We then
	; defer actually loading those until after DS was changed.
	PUSH_STATE                             ; Push the state, except for the old ipl
	mov   rax, 0x30                        ; Use ring 0 segment for kernel mode use. We have backed the old values up
	mov   ds,  ax
	mov   es,  ax
	mov   fs,  ax
	mov   gs,  ax
	mov   rbx, [rbx]                       ; Retrieve the interrupt number and RIP from interrupt frame. These were deferred
	mov   rcx, [rcx]                       ; so that we wouldn't attempt to access the kernel stack using the user's data segment.
	mov   rdx, [rdx]                       ; Load CS, to determine the previous mode when entering a hardware interrupt
	push  rcx                              ; Enter a stack frame so that stack printing doesn't skip over anything
	push  rbp
	mov   rbp, rsp
	cld                                    ; Clear direction flag, will be restored by iretq
	mov   rdi, rsp                         ; Retrieve the idt_regs to call the trap handler
	push  rdi							   ; Push pointer to registers
	call  idt_handler2                     ; And call idt_handler!!
	pop   rax							   ; a small workaround for interrupts to work, idt_handler2 returns nothing which results unexpected behaviour without pop rax
	mov   rsp, rax                         ; Use the new idt_regs instance as what to pull:
	pop   rbp                              ; Leave the stack frame
	pop   rcx                              ; Skip over the RIP duplicate that we pushed
	POP_STATE                              ; Pop the state
	pop   rdx                              ; Pop the RDX register
	pop   rcx                              ; Pop the RCX register
	pop   rbx                              ; Pop the RBX register
	pop   rax                              ; Pop the RAX register
	add   rsp, 16                          ; Pop the interrupt number and the error code
	o64 sysret

%include "base/descriptor/intlist.asm"

%unmacro INT 2
section .data
global int_lst
int_lst:
%macro INT 2
    dq int_%1
%endmacro

%include "base/descriptor/intlist.asm"