.extern syscall_c_entry

.macro THUNK
    swapgs
    //mov %rsp, %gs:16
    //mov %gs:0, %rsp

    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %r11
    pushq %r10
    pushq %r9
    pushq %r8
    pushq %rbp
    pushq %rdi
    pushq %rsi
    pushq %rdx
    pushq %rcx
    pushq %rbx
    pushq %rax

    mov %es, %eax
    pushq %rax
    mov %ds, %eax
    pushq %rax

    mov %cr2, %rax
    pushq %rax

    cld

    mov $0x30, %eax
    mov %eax, %ds
    mov %eax, %es
    mov %eax, %ss

    mov %rsp, %rdi

    //sti
    call syscall_c_entry
    cli

    //mov %rax, 16(%rsp)
    //mov %gs:0x8, %rax
    //mov %rax, 24(%rsp)

    cli

    add $8, %rsp
    popq %rax
    mov %eax, %ds
    popq %rax
    mov %eax, %es

    popq %rax
    popq %rbx
    popq %rcx
    popq %rdx
    popq %rsi
    popq %rdi
    popq %rbp
    popq %r8
    popq %r9
    popq %r10
    popq %r11
    popq %r12
    popq %r13
    popq %r14
    popq %r15

    //mov %gs:16, %rsp
    swapgs
.endm

.global syscall_entry
syscall_entry:
    pushq $0x53 // SS
	pushq %rsp // RSP
	//pushfq // flag
    push %r11
	pushq $0
	pushq %rcx // RIP
	pushq $0
	pushq $1024 // interrupt number

    THUNK

    add $16, %rsp
    popq %rcx
	add $8, %rsp
	//popfq // flags
    pop %r11
	add $16, %rsp

    sti
    sysretq