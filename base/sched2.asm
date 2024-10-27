global task_entry
global task_user_mode_entry

task_user_mode_entry:
    mov r11, 0x202 ; to be loaded into EFLAGS
	o64 sysret ;use "o64 sysret" if you assemble with NASM

task_entry:
    ;mov rdi, 0x30
    ;mov ds, rdi
	;mov es, rdi
	;mov fs, rdi
	;mov gs, rdi
	;mov ss, rdi
    sti
    call rax
    mov edi, eax
    mov eax, 60
    int 0x80
halt:
    jmp halt