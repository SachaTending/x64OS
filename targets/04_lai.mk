OBJ += $(addprefix lai/core/, \
	error.o \
	eval.o \
	exec-operand.o \
	exec.o \
	libc.o \
	ns.o \
	object.o \
	opregion.o \
	os_methods.o \
	variable.o \
	vsnprintf.o \
)

INCLUDES += -I lai/include