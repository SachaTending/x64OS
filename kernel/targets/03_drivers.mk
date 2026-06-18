obj-y += $(addprefix drivers/, \
	pci.cpp \
)

obj-y += $(addprefix drivers/storage/, \
	nvme.cpp \
	part_parser.cpp \
)