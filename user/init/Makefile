OBJ = entry2.o entry.o

%.o: %.c
	@echo "  [  CC] $@"
	@gcc -o $@ $< -c -g -fpic -nostdinc -nostdlib -I ~/mlibc_base/usr/local/include

%.o: %.asm
	@echo "  [NASM] $@"
	@nasm -felf64 -o $@ $< -g 
.PHONY: init
init: $(OBJ)
	@echo "  [  LD] init"
	@gcc -o init $(OBJ) -g -nostdinc -nostdlib libc.a -L . -Wl,-dynamic-linker=/ld.so -static crt1.o ld.a
pack: init
	tar cv . > ../../iso_root/test.tar
