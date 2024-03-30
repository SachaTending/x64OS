OBJ += $(addprefix base/, \
	ssfn.o \
	logging.o \
	funny.o \
	pit.o \
	libcpp.o \
	asm_stuff.o \
	compinfo.o \
	smp.o \
	sched.o \
	sched2.o \
	vfs.o \
)

base/compinfo.o:
	@echo "  [  CC] base/compinfo.o"
	@gcc -c -o base/compinfo.o base/compinfo.c $(CFLAGS)

.PHONY: base/compinfo.o

OBJ += $(addprefix base/libc/, \
	mem.o \
	str.o \
	print.o \
	spinlock.o \
	printf.o \
	panic.o \
)

OBJ += $(addprefix base/extra/, \
	stackwalk.o \
)