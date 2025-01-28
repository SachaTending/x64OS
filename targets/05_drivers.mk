OBJ += $(addprefix drivers/, \
	ps2.o \
	pci.o \
	serial.o \
	ahci.o \
)

CONFIG_SB16=y

ifdef CONFIG_SB16
OBJ += $(addprefix drivers/, \
	sb16.o \
)
endif

CONFIG_AC97=y
ifdef CONFIG_AC97
OBJ += drivers/ac97.o

endif