obj-y = $(addprefix krnl/,\
	c_init.c \
	starter.cpp \
	int_dispatcher.cpp \
	main.cpp \
	syscalls.cpp \
	linux_syscall_translator.cpp \
)
obj-n = $(addprefix ,\
	main.c \
)