#include <libc.h>
#include <tss.h>
#include <krnl.hpp>
#include <descriptor/gdt.hpp>

#define STACK_SIZE 64*1024

void init_tss() {
    uint64_t stack = (uint64_t)malloc(STACK_SIZE); // Allocate stack
    memset((void *)stack, 0, STACK_SIZE);
    tss_entry_t *tss = new tss_entry_t; // Create tss
    printf("TSS: stack address: 0x%lx, table: 0x%lx\n", stack, tss);
    tss_set_stack(stack, tss);
    gdt_set_tss((uint64_t)tss-VMM_HIGHER_HALF); // (SCARY PART, CAN BREAK ENTIRE OS)Set tss address in gdt and load it
}