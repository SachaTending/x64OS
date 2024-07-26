global task_entry

task_user_mode_entry:
    sti

task_entry:
    sti
    call rax
    mov edi, eax
    mov eax, 1
    int 0x80
halt:
    jmp halt