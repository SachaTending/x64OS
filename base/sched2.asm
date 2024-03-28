global task_entry

task_entry:
    sti
    call rax
halt:
    hlt
    jmp halt