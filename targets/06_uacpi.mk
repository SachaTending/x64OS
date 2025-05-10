uacpi = $(shell cd base/acpi/uacpi/source && find -L * -type f -name '*.c')
OBJ += $(addprefix base/acpi/uacpi/source/, $(uacpi:.c=.o)) base/acpi/uacpi_glue.o

INCLUDES += -I base/acpi/uacpi/include