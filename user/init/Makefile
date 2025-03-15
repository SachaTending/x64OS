OBJ = entry2.o entry.o rain_eater.o

rain_eater.o:
	@echo "  [OBJC] $@"
	@objcopy -O elf64-x86-64 -B i386 -I binary rain_eater.xm rain_eater.o

%.o: %.c
	@echo "  [  CC] $@"
	@gcc -o $@ $< -c -g -fpic -nostdinc -nostdlib -I ~/mlibc_base/usr/local/include

%.o: %.asm
	@echo "  [NASM] $@"
	@nasm -felf64 -o $@ $< -g 
.PHONY: init dash
init: $(OBJ)
	@echo "  [  LD] init"
	@gcc -o init $(OBJ) -g -nostdinc -nostdlib libc.a -L . -Wl,-dynamic-linker=/ld.so -static crt1.o ld.a libxmp.a libm.a *.a
dash:
	@echo "  [  LD] dash"
	@gcc -o dash dash.c -g -nostdlib libc.a -L . -Wl,-dynamic-linker=/ld.so -static crt1.o ld.a libxmp.a libm.a *.a -I ~/mlibc_base/usr/local/include
pack: init dash
	tar cv . > ../../iso_root/test.tar
