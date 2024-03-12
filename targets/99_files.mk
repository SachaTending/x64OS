OBJ += freesans.o

freesans.o:
	@echo "  [ GEN] freesans.o"
	@objcopy -O elf64-x86-64 -B i386 -I binary freesans.sfn freesans.o

.PHONY: freesans.o