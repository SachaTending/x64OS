#include <stdint.h>
#include <stddef.h>
#include <libc.h>

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct gdt_struct {
    uint16_t limit = 0;
    uint16_t base_low16 = 0;
    uint8_t  base_mid8 = 0;
    uint8_t  access = 0;
    uint8_t  granularity = 0;
    uint8_t  base_high8 = 0;
};

struct tss_descriptor {
    uint16_t length = 0;
    uint16_t base_low16 = 0;
    uint8_t  base_mid8 = 0;
    uint8_t  flags1 = 0;
    uint8_t  flags2 = 0;
    uint8_t  base_high8 = 0;
    uint32_t base_upper32 = 0;
    uint32_t reserved = 0;
};

struct gdt_desc {
    gdt_struct descs[11];
    tss_descriptor tss;
};

gdt_desc gdt_d = {0};
gdt_ptr gdtr;
void gdt_reload();
void arch_gdt_init() {
    // TODO: Populate GDT
    // Null descriptor.
    #define gdt_descs gdt_d.descs
    gdt_descs[0].limit       = 0;
    gdt_descs[0].base_low16  = 0;
    gdt_descs[0].base_mid8   = 0;
    gdt_descs[0].access      = 0;
    gdt_descs[0].granularity = 0;
    gdt_descs[0].base_high8  = 0;

    // Kernel code 16.
    gdt_descs[1].limit       = 0xffff;
    gdt_descs[1].base_low16  = 0;
    gdt_descs[1].base_mid8   = 0;
    gdt_descs[1].access      = 0b10011010;
    gdt_descs[1].granularity = 0b00000000;
    gdt_descs[1].base_high8  = 0;

    // Kernel data 16.
    gdt_descs[2].limit       = 0xffff;
    gdt_descs[2].base_low16  = 0;
    gdt_descs[2].base_mid8   = 0;
    gdt_descs[2].access      = 0b10010010;
    gdt_descs[2].granularity = 0b00000000;
    gdt_descs[2].base_high8  = 0;

    // Kernel code 32.
    gdt_descs[3].limit       = 0xffff;
    gdt_descs[3].base_low16  = 0;
    gdt_descs[3].base_mid8   = 0;
    gdt_descs[3].access      = 0b10011010;
    gdt_descs[3].granularity = 0b11001111;
    gdt_descs[3].base_high8  = 0;

    // Kernel data 32.
    gdt_descs[4].limit       = 0xffff;
    gdt_descs[4].base_low16  = 0;
    gdt_descs[4].base_mid8   = 0;
    gdt_descs[4].access      = 0b10010010;
    gdt_descs[4].granularity = 0b11001111;
    gdt_descs[4].base_high8  = 0;

    // Kernel code 64.
    gdt_descs[5].limit       = 0;
    gdt_descs[5].base_low16  = 0;
    gdt_descs[5].base_mid8   = 0;
    gdt_descs[5].access      = 0b10011011;
    gdt_descs[5].granularity = 0b00100000;
    gdt_descs[5].base_high8  = 0;

    // Kernel data 64.
    gdt_descs[6].limit       = 0;
    gdt_descs[6].base_low16  = 0;
    gdt_descs[6].base_mid8   = 0;
    gdt_descs[6].access      = 0b10010011;
    gdt_descs[6].granularity = 0;
    gdt_descs[6].base_high8  = 0;

    // SYSENTER related entries
    gdt_descs[7].limit       = 0;
    gdt_descs[7].base_low16  = 0;
    gdt_descs[7].base_mid8   = 0;
    gdt_descs[7].access      = 0b11110010;
    gdt_descs[7].granularity = 0;
    gdt_descs[7].base_high8  = 0;

    gdt_descs[8].limit       = 0;
    gdt_descs[8].base_low16  = 0;
    gdt_descs[8].base_mid8   = 0;
    gdt_descs[8].access      = 0b11111010;
    gdt_descs[8].granularity = 0b00100000;
    gdt_descs[8].base_high8  = 0;

    // User code 64.
    gdt_descs[9].limit       = 0;
    gdt_descs[9].base_low16  = 0;
    gdt_descs[9].base_mid8   = 0;
    gdt_descs[9].access      = 0b11111010;
    gdt_descs[9].granularity = 0b00100000;
    gdt_descs[9].base_high8  = 0;

    // User data 64.
    gdt_descs[10].limit       = 0;
    gdt_descs[10].base_low16  = 0;
    gdt_descs[10].base_mid8   = 0;
    gdt_descs[10].access      = 0b11110010;
    gdt_descs[10].granularity = 0;
    gdt_descs[10].base_high8  = 0;


    gdt_d.tss.length       = 104;
    gdt_d.tss.base_low16   = 0;
    gdt_d.tss.base_mid8    = 0;
    gdt_d.tss.flags1       = 0b10001001;
    gdt_d.tss.flags2       = 0;
    gdt_d.tss.base_high8   = 0;
    gdt_d.tss.base_upper32 = 0;
    gdt_d.tss.reserved     = 0;

    gdtr.base = (uint64_t)&gdt_d;
    gdtr.limit = (sizeof(gdt_d)) - 1;
    printf("kdata offset: 0x%lx\nkcode offset: 0x%lx\n",  ((uint64_t)&gdt_descs[6]) - ((uint64_t)&gdt_descs), ((uint64_t)&gdt_descs[5]) - ((uint64_t)&gdt_descs));
    gdt_reload();
}
extern "C" void load_tss(int desc);
void gdt_set_tss(uint64_t tss) {
    uint64_t addr = tss;
    gdt_d.tss.base_low16   = (uint16_t)addr;
    gdt_d.tss.base_mid8    = (uint8_t)(addr >> 16);
    gdt_d.tss.flags1       = 0b10001001;
    gdt_d.tss.flags2       = 0;
    gdt_d.tss.base_high8   = (uint8_t)(addr >> 24);
    gdt_d.tss.base_upper32 = (uint32_t)(addr >> 32);
    gdt_d.tss.reserved     = 0;
    //gdt_reload();
    printf("11*8=%d\n", 11*8);
    printf("tss offset: 0x%lx\n", offsetof(struct gdt_desc, tss));
    printf("tss addr: 0x%016lx\n", tss);
    asm volatile ("ltr %0" : : "rm" ((uint16_t)offsetof(struct gdt_desc, tss)) : "memory");
}

void gdt_reload(void) {
    asm volatile (
        "lgdt %0\n\t"
        "push $0x28\n\t"
        "lea 1f(%%rip), %%rax\n\t"
        "push %%rax\n\t"
        "lretq\n\t"
        "1:\n\t"
        "mov $0x30, %%eax\n\t"
        "mov %%eax, %%ds\n\t"
        "mov %%eax, %%es\n\t"
        "mov %%eax, %%fs\n\t"
        "mov %%eax, %%gs\n\t"
        "mov %%eax, %%ss\n\t"
        :
        : "m"(gdtr)
        : "rax", "memory"
    );
    printf("gdtr.base=0x%016lx\n", gdtr.base);
}