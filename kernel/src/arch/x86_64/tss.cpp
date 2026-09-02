#include <stdint.h>
#include <libc.h>

typedef struct tss_entry {
    uint32_t reserved1;
    uint64_t rsp[3];
    uint32_t reserved2[2];
    uint64_t ist[7];
    uint32_t reserved3[2];
    uint16_t reserver4;
    uint16_t iopb;
} __attribute__((packed)) tss_entry_t;

tss_entry_t *entry;
void gdt_set_tss(uint64_t tss);
void idt_set_global_ist(uint8_t ist);
void arch_tss_setup() {
    uint64_t orig_addr = (uint64_t)pmm_alloc(1);
    entry = (tss_entry_t *)(orig_addr + VMM_HIGHER_HALF);
    memset(entry, 0, sizeof(tss_entry_t));
    //entry->ist[1] = (uint64_t)malloc(128*1024)+(128*1024);
    //entry->ist[2] = (uint64_t)malloc(128*1024)+(128*1024);
    entry->rsp[0] = (uint64_t)malloc(128*1024)+(128*1024);
    for (int i=0;i<7;i++) {
        entry->ist[i] = (uint64_t)malloc(128*1024)+(128*1024);
    }
    gdt_set_tss((uint64_t)entry);
    idt_set_global_ist(1);
}