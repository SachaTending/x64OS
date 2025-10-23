obj-y = $(addprefix krnl/,\
	c_init.c \
	starter.cpp \
	int_dispatcher.cpp \
)
obj-n = $(addprefix ,\
	main.c \
)