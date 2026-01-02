.section .text
.global _start
_start:
	mov %rsp, %rdi
	lea main(%rip), %rsi
	//call __mlibc_entry
	call pre_main
.section .note.GNU-stack,"",%progbits