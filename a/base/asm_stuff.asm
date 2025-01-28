global sse_enable
sse_enable:
    mov rax, cr0
    and ax, 0xFFFB		;clear coprocessor emulation CR0.EM
    or ax, 0x2			;set coprocessor monitoring  CR0.MP
    mov cr0, rax
    mov rax, cr4
    or ax, 3 << 9		;set CR4.OSFXSR and CR4.OSXMMEXCPT at the same time
    mov cr4, rax
    ret

global jump_to_usermode
extern test_user_function
jump_to_usermode:
	;mov rcx, 0xc0000082
	;wrmsr
	;mov rcx, 0xc0000080
	;rdmsr
	;or eax, 1
	;wrmsr
	;mov rcx, 0xc0000081
	;rdmsr
	;mov edx, 0x00180008
	;wrmsr

	lea ecx, [rel test_user_function]
	mov r11, 0x202 ; to be loaded into EFLAGS
	o64 sysret ;use "o64 sysret" if you assemble with NASM