# Files in base
obj-y += $(addprefix base/, \
	libc.cpp \
	printf.c \
	logger.cpp \
	uacpi_integration.cpp \
	hpet.cpp \
	cxx_runtime.cpp \
	sched.cpp \
	rng.cpp \
)

# IO
obj-y += $(addprefix base/io/, \
	text.cpp \
)

# Flanterm
obj-y += $(addprefix base/flanterm/, \
	flanterm.c \
	fb/fb.c \
)

# Memory stuff
obj-y += $(addprefix base/memory/, \
	pmm.cpp \
	mmap.cpp \
	liballoc.cpp \
)

# Filesystem
obj-y += $(addprefix base/fs/, \
	vfs.cpp \
	tmpfs.cpp \
	resource.cpp \
	initrd_unpacker.cpp \
)

# devtmpfs and related stuff
obj-y += $(addprefix base/fs/dev/, \
	devtmpfs.cpp \
	fbdev.cpp \
)

# Program loading
obj-y += $(addprefix base/prg/, \
	lol_loader.cpp \
	elf_loader.cpp \
)