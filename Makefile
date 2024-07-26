# x64OS Makefile, some parts copied from https://wiki.osdev.org/Limine_Bare_Bones
# Its edited to be more readable.

# Internal C flags that should not be changed by the user.
USER=$(shell whoami)
HOST=$(shell hostnamectl hostname)

override CFLAGS += \
    -Wall \
    -Wextra \
    -ffreestanding \
    -fno-stack-protector \
    -fno-stack-check \
    -fno-lto \
    -fPIE \
    -m64 \
    -march=x86-64 \
    -mno-mmx \
    -mno-sse2 \
    -mno-red-zone -c \
	-I include -g \
    -D__BUILD_USER=\"$(USER)\" -D__BUILD_HOST=\"$(HOST)\"
 
# Internal C preprocessor flags that should not be changed by the user.
override CPPFLAGS := \
    $(CFLAGS) \
    -MMD \
    -MP \
	-fpermissive -fno-threadsafe-statics -fno-use-cxa-atexit -fno-rtti  -fno-exceptions -fno-use-cxa-atexit
 
# Internal linker flags that should not be changed by the user.
override LDFLAGS += \
    -m elf_x86_64 \
    -nostdlib \
    -static \
    --no-dynamic-linker \
    -pie \
    -z text \
    -z max-page-size=0x1000 \
    -T linker.ld --export-dynamic

# Internal nasm flags that should not be changed by the user.
override NASMFLAGS += -f elf64 -g

all: build
OBJ = 
INCLUDES = 
-include targets/*.mk
override COMP_OBJ := $(OBJ)
override HEADER_DEPS := $(OBJ:.o=.d)

build: image.iso
%.o: %.c
	@echo "  [  CC] $@"
	@$(CC) $(CFLAGS) -o $@ $< $(INCLUDES)

%.o: %.cpp
	@echo "  [ CPP] $@"
	@$(CC) $(CPPFLAGS) -o $@ $< $(INCLUDES)

%.o: %.S
	@echo "  [  AS] $@"
	@$(CC) $(CFLAGS) -o $@ $< $(INCLUDES)

%.o: %.asm
	@echo "  [NASM] $@"
	@nasm $(NASMFLAGS) -o $@ $<

bin/kernel.elf: $(OBJ)
	@mkdir -p bin
	@echo "  [  LD] bin/kernel.elf"
	@ld $(LDFLAGS) -o bin/kernel.elf -g $(COMP_OBJ)

image.iso: bin/kernel.elf
	@cp bin/kernel.elf iso_root
	@xorriso -as mkisofs -b limine-bios-cd.bin \
        -no-emul-boot -boot-load-size 4 -boot-info-table \
        --efi-boot limine-uefi-cd.bin \
        -efi-boot-part --efi-boot-image --protective-msdos-label \
        iso_root -o image.iso
.PHONY: image.iso bin/kernel.elf
run: image.iso
	@qemu-system-x86_64 -cdrom image.iso -serial stdio -smp cores=2 $(KVM)

rung: image.iso
	@qemu-system-x86_64 -cdrom image.iso -serial stdio -s -S $(KVM) -m 256m

run_uefi: image.iso
	@qemu-system-x86_64 -cdrom image.iso -serial stdio -smp cores=2 $(KVM) -bios /usr/share/ovmf/x64/OVMF.fd

clean:
	@rm $(COMP_OBJ) bin/kernel.elf $(HEADER_DEPS) -f
