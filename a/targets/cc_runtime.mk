ccr = $(shell cd ccruntime && find -L * -type f -name '*.c')
OBJ += $(addprefix ccruntime/, $(ccr:.c=.o))