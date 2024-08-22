OBJ = entry.o entry2.o

%.o: %.c
	@echo "  [  CC] $@"
	@gcc -o $@ $< -c

%.o: %.asm
	@echo "  [NASM] $@"
	@nasm -felf64 -o $@ $<

init: $(OBJ)
	@echo "  [  LD] init"
	@ld -o init $(OBJ)