# How to add my code to kernel?
x64OS kernel uses makefile-based building system, and to make your code compile into kernel, simply create(or add to existing) target file, which should be located in kernel/targets/(NAME OF FILE).mk

For example, to compile file base/very_important_code.cpp, you can
1. Modify 02_base.mk like this

Before adding:
```makefile
# Files in base
obj-y += $(addprefix base/, \
	libc.cpp \
	printf.c \
	logger.cpp \
	uacpi_integration.cpp \
	hpet.cpp \
	cxx_runtime.cpp \
)
```

After adding
```makefile
# Files in base
obj-y += $(addprefix base/, \
	libc.cpp \
	printf.c \
	logger.cpp \
	uacpi_integration.cpp \
	hpet.cpp \
	cxx_runtime.cpp \
    very_important_file.cpp \
)
```

2. Create your own target file
```Makefile
obj-y += $(addprefix base/, \
    very_important_file.cpp \
)
```

You may be asking now, why obj-y has suffix "-y"? It's for future building configuration support, so you can define what files to compile, and what NOT to compile