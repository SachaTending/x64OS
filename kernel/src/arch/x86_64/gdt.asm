global load_tss
load_tss:
	mov ax, di
	ltr ax
	ret