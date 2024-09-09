#include <libc.h>
#include <tss.h>
#include <krnl.hpp>
#include <descriptor/gdt.hpp>
#include <vmm.h>

extern pagemap krnl_page;

#define STACK_SIZE 64*1024

tss_entry_t *tss;
void idt_set_global_ist(uint8_t ist);
void init_tss() {
    uint64_t stack = (uint64_t)malloc(STACK_SIZE); // Allocate stack
    memset((void *)stack, 0, STACK_SIZE);
    tss = new tss_entry_t; // Create tss
    printf("TSS: stack address: 0x%lx, table: 0x%lx\n", stack, tss);
    tss_set_stack(stack, tss);
    tss->ist[0] = ((uint64_t)malloc(STACK_SIZE)+STACK_SIZE);
    //vmm_map_range(&krnl_page, tss->ist[0]-STACK_SIZE, STACK_SIZE/4096, PTE_WRITABLE | PTE_PRESENT);
    idt_set_global_ist(1);
    gdt_set_tss((uint64_t)tss-VMM_HIGHER_HALF); // (SCARY PART, CAN BREAK ENTIRE OS)Set tss address in gdt and load it
}