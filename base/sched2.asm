global task_entry

task_entry:
    sti
    call rax
    mov edi, eax
    mov eax, 1
    int 0x80
halt:
    hlt
    jmp halt