#include <stdint.h>
#include <descriptor/gdt.hpp>
#include <tss.h>

struct gdt_descriptor {
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

typedef struct tss_desc2
{
	uint64_t Limit1 : 16;
	uint64_t Base1  : 24;
	uint64_t Access : 8;
	uint64_t Limit2 : 4;
	uint64_t Flags  : 4;
	uint64_t Base2  : 40;
	uint64_t Resvd  : 32;
} tss_desc2_t;

struct gdt {
    struct gdt_descriptor descriptors[11];
    tss_desc2_t tss;
};

struct gdtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct gdt gdt = {0};
static struct gdtr gdtr = {0};
void gdt_reload(void);
#define offsetof __builtin_offsetof
#include <libc.h>
void GDT::Init() {
    // Null descriptor.
    gdt.descriptors[0].limit       = 0;
    gdt.descriptors[0].base_low16  = 0;
    gdt.descriptors[0].base_mid8   = 0;
    gdt.descriptors[0].access      = 0;
    gdt.descriptors[0].granularity = 0;
    gdt.descriptors[0].base_high8  = 0;

    // Kernel code 16.
    gdt.descriptors[1].limit       = 0xffff;
    gdt.descriptors[1].base_low16  = 0;
    gdt.descriptors[1].base_mid8   = 0;
    gdt.descriptors[1].access      = 0b10011010;
    gdt.descriptors[1].granularity = 0b00000000;
    gdt.descriptors[1].base_high8  = 0;

    // Kernel data 16.
    gdt.descriptors[2].limit       = 0xffff;
    gdt.descriptors[2].base_low16  = 0;
    gdt.descriptors[2].base_mid8   = 0;
    gdt.descriptors[2].access      = 0b10010010;
    gdt.descriptors[2].granularity = 0b00000000;
    gdt.descriptors[2].base_high8  = 0;

    // Kernel code 32.
    gdt.descriptors[3].limit       = 0xffff;
    gdt.descriptors[3].base_low16  = 0;
    gdt.descriptors[3].base_mid8   = 0;
    gdt.descriptors[3].access      = 0b10011010;
    gdt.descriptors[3].granularity = 0b11001111;
    gdt.descriptors[3].base_high8  = 0;

    // Kernel data 32.
    gdt.descriptors[4].limit       = 0xffff;
    gdt.descriptors[4].base_low16  = 0;
    gdt.descriptors[4].base_mid8   = 0;
    gdt.descriptors[4].access      = 0b10010010;
    gdt.descriptors[4].granularity = 0b11001111;
    gdt.descriptors[4].base_high8  = 0;

    // Kernel code 64.
    gdt.descriptors[5].limit       = 0;
    gdt.descriptors[5].base_low16  = 0;
    gdt.descriptors[5].base_mid8   = 0;
    gdt.descriptors[5].access      = 0b10011010;
    gdt.descriptors[5].granularity = 0b00100000;
    gdt.descriptors[5].base_high8  = 0;

    // Kernel data 64.
    gdt.descriptors[6].limit       = 0;
    gdt.descriptors[6].base_low16  = 0;
    gdt.descriptors[6].base_mid8   = 0;
    gdt.descriptors[6].access      = 0b10010010;
    gdt.descriptors[6].granularity = 0;
    gdt.descriptors[6].base_high8  = 0;

    // SYSENTER related entries
    gdt.descriptors[7].limit       = 0;
    gdt.descriptors[7].base_low16  = 0;
    gdt.descriptors[7].base_mid8   = 0;
    gdt.descriptors[7].access      = 0b11110010;
    gdt.descriptors[7].granularity = 0;
    gdt.descriptors[7].base_high8  = 0;
    gdt.descriptors[8].limit       = 0;
    gdt.descriptors[8].base_low16  = 0;
    gdt.descriptors[8].base_mid8   = 0;
    gdt.descriptors[8].access      = 0b11111010;
    gdt.descriptors[8].granularity = 0b00100000;
    gdt.descriptors[8].base_high8  = 0;

    // User code 64.
    gdt.descriptors[9].limit       = 0;
    gdt.descriptors[9].base_low16  = 0;
    gdt.descriptors[9].base_mid8   = 0;
    gdt.descriptors[9].access      = 0b11111010;
    gdt.descriptors[9].granularity = 0b00100000;
    gdt.descriptors[9].base_high8  = 0;

    // User data 64.
    gdt.descriptors[10].limit       = 0;
    gdt.descriptors[10].base_low16  = 0;
    gdt.descriptors[10].base_mid8   = 0;
    gdt.descriptors[10].access      = 0b11110010;
    gdt.descriptors[10].granularity = 0;
    gdt.descriptors[10].base_high8  = 0;

    // TSS.
    gdt.tss.Limit1       = 104;
    gdt.tss.Base1        = 0;
    gdt.tss.Access       = 0b10001001;
    gdt.tss.Limit2       = 0;
    gdt.tss.Base2        = 0;
    gdt.tss.Resvd        = 0;
    //printf("gdt.tss offset: %u\n", offsetof(struct gdt, tss));

    // Set the pointer.
    gdtr.limit = (sizeof(gdt) - 1);
    gdtr.base  = (uint64_t)&gdt;
    gdt.descriptors[7] = gdt.descriptors[9];
    gdt.descriptors[8] = gdt.descriptors[10];
    gdt_reload();
}
extern "C" void load_tss(int desc);
void gdt_set_tss(uint64_t tss) {
    gdt.tss.Limit1 = sizeof(tss_entry_t);
    gdt.tss.Base1 = tss;
    gdt.tss.Base2 = (tss >> 24);
    gdt.tss.Access = 0x89;
    gdt.tss.Limit2 = 0x40;
    load_tss(sizeof(gdt_entry_t)*11);
}

void tss_set_stack(uint64_t stack, tss_entry_t *tss) {
    tss->rsp[0] = stack;
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
}