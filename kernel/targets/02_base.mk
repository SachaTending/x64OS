# Files in base
obj-y += $(addprefix base/, \
	libc.cpp \
	printf.c \
	logger.cpp \
	uacpi_integration.cpp \
	hpet.cpp \
	cxx_runtime.cpp \
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
)