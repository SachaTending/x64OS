#include <libc.h>
#include <elf.h>
#include <limine.h>

limine_kernel_file_request krnl_file = {
    .id = LIMINE_KERNEL_FILE_REQUEST,
    .revision = 0,
    .response = 0
};

char *strtab_real;
size_t strtab_size;
Elf64_Sym *symtab;
size_t symtab_entries = 0;
uint64_t orig_start;
uint64_t kaslr_off;

extern char kernel_start;

Elf64_Sym n = {0,0,0,0,0};

Elf64_Sym *find_sym(uint64_t addr);
Elf64_Sym *stack_lookup(uintptr_t addr) {
    addr -= kaslr_off;
    //printf("stack_lookup: 0x%lx\n", addr);
    if (!symtab) {
        return &n;
    }
    while (addr) {
        //printf("addr=0x%lx\n", addr);
        Elf64_Sym *a = find_sym(addr);
        if (a) {
            return a;
        }
        addr--;
    }
    return 0;
}

Elf64_Sym *find_sym(uint64_t addr) {
    for (size_t i=0;i<symtab_entries;i++) {
        //printf("i=%u ", i);
        if (symtab[i].st_value == addr) {
            //printf("\n");
            return &symtab[i];
        }
    }
    return 0;
}

typedef struct stack {
    struct stack *old_rbp;
    uint64_t ret_addr;
} stack_entry;

char *strtab_get(size_t off) {
    if (off > strtab_size or !off) {
        return (char *)"NO NAME";
    }
    return strtab_real+off;
}

void stacktrace(uintptr_t *s) {
    volatile stack_entry *stack = (stack_entry *)s;
    if (!s) {
        asm volatile (
            "movq %%rbp, %0"
            : "=r"(stack)
            :: "memory"
        );
    }
    Elf64_Sym *sym;
    //asm volatile ("movq %%rbp, %0" : "=r"(stack) :: "memory");
    printf("Stack trace:\n");
    while (stack->old_rbp) {
        sym = stack_lookup(stack->ret_addr);
        printf("0x%lx: <%s+0x%lx>\n", stack->ret_addr, strtab_get(sym->st_name), (stack->ret_addr)-(sym->st_value+kaslr_off));
        stack = stack->old_rbp;
    }
    printf("END OF STACK\n");
}

void stackwalk_init() {
    Elf64_Ehdr *khdr = (Elf64_Ehdr *)krnl_file.response->kernel_file->address;
    if (memcmp(khdr, (const void *)ELFMAG, SELFMAG)) {
        PANIC("Failed to init stacktrace: %s", "Invalid ELF header");
        return;
    } if (khdr->e_machine != EM_X86_64) {
        PANIC("Failed to init stacktrace: %s", "Invalid machine");
    }
    Elf64_Shdr *shdr;
    for (Elf64_Half i=0;i<khdr->e_shnum;i++) {
        size_t offset = khdr->e_shoff + i * khdr->e_shentsize;
        shdr = (Elf64_Shdr *)((uint64_t)krnl_file.response->kernel_file->address+offset);
        if (shdr->sh_type == SHT_PROGBITS and shdr->sh_addr and !orig_start) {
            orig_start = shdr->sh_addr;
            kaslr_off = (uint64_t)&kernel_start - orig_start;
        }
        if (shdr->sh_type == SHT_STRTAB and shdr->sh_addr) {
            strtab_real = (char *)shdr->sh_addr;
            strtab_size = shdr->sh_size;
        } else if (shdr->sh_type == SHT_SYMTAB and shdr->sh_addr) {
            symtab = (Elf64_Sym *)shdr->sh_addr;
            symtab_entries = shdr->sh_size / sizeof(Elf64_Sym);
        }
    }
    if (!strtab_real) {
        printf("WARNING: .strtab(or .dynstr) not found.\n");
    }
    if (!symtab) {
        printf("WARNING: .symtab(or .dynsym) not found\n");
    }
    strtab_real += kaslr_off;
    symtab += kaslr_off;
    printf("kaslr_off=0x%lx orig_start=0x%lx\n", kaslr_off, orig_start);
}