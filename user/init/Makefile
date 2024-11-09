OBJ = entry.o entry2.o

%.o: %.c
	@echo "  [  CC] $@"
	@gcc -o $@ $< -c

%.o: %.asm
	@echo "  [NASM] $@"
	@nasm -felf64 -o $@ $<

init: $(OBJ)
	@echo "  [  LD] init"
	@gcc -o init $(OBJ) -nostdlib -static -lc

pack: init
	tar cv . > ../../iso_root/test.tar
