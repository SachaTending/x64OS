.extern syscall_c_entry
.macro THUNK
    swapgs

    push %r15
    push %r14
    push %r13
    push %r12
    push %r11
    push %r10
    push %r9
    push %r8
    push %rbp
    push %rdi
    push %rsi
    push %rdx
    push %rcx
    push %rbx
    push %rax

    mov %es, %eax
    push %rax
    mov %ds, %eax
    push %rax

    mov %cr2, %rax
    push %rax

    cld

    mov $0x30, %eax
    mov %eax, %ds
    mov %eax, %es
    mov %eax, %ss

    mov %rsp, %rdi

    sti
    call syscall_c_entry

    //mov %rax, 16(%rsp)
    //mov %gs:0x8, %rax
    //mov %rax, 24(%rsp)

    cli

    add $8, %rsp
    pop %rax
    mov %eax, %ds
    pop %rax
    mov %eax, %es

    pop %rax
    pop %rbx
    pop %rcx
    pop %rdx
    pop %rsi
    pop %rdi
    pop %rbp
    pop %r8
    pop %r9
    pop %rcx
    pop %rdx
    pop %r12
    pop %r13
    pop %r14
    pop %r15

    swapgs
.endm

.global syscall_entry
syscall_entry:
    pushq $0x53 // SS
	pushq %rsp // RSP
	pushfq // flag
	pushq $0
	pushq %rcx // RIP
	pushq $0
	pushq $1024

    THUNK

    add $16, %rsp
    popq %rcx
	add $8, %rsp
	popfq // flags
	add $16, %rsp

    sti
    sysretq