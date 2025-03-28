OBJ += base/full_hd.o LICENSE.o

freesans.o:
	@echo "  [ GEN] freesans.o"
	@objcopy -O elf64-x86-64 -B i386 -I binary freesans.sfn freesans.o

base/full_hd.o:
	@echo "  [ GEN] full_hd.o"
	@objcopy -O elf64-x86-64 -B i386 -I binary base/full_hd.png base/full_hd.o

LICENSE.o:
	@echo "  [ GEN] LICENSE.o"
	@objcopy -O elf64-x86-64 -B i386 -I binary LICENSE LICENSE.o


.PHONY: freesans.o