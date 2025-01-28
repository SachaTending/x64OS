OBJ += $(addprefix base/flanterm/, \
	flanterm.o \
	backends/fb.o \
)

INCLUDES += -I base/flanterm