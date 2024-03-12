global load_gdt
load_gdt: ; did you know that this asm code taken from boron os? No? Compare this asm to this https://github.com/iProgramMC/Boron/blob/6be98469b480e91febcd07130c071bf1cbcf87d1/boron/source/arch/amd64/misc.asm#L56
	mov rax, rdi
	lgdt [rax]
	
	; update the code segment
	push 0x08           ; code segment
	lea rax, [rel .a]   ; jump address
	push rax
	retfq               ; return far - will go to .a now
.a:
	; update the segments
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
	ret