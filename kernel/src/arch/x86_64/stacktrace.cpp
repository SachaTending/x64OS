#include <libc.h>
#include <arch/arch.hpp>

struct stackframe {
    struct stackframe* rbp;
    uint64_t rip;
};

#define MAX_FRAMES 50

void arch_print_stack(uint64_t rbp) {
    uintptr_t *base_ptr = (uintptr_t *)rbp;
    if (base_ptr == NULL) {
        asm volatile ("mov %%rbp, %0" : "=g"(base_ptr) :: "memory");
    }

    if (base_ptr == NULL) {
        return;
    }
    //printf("Stack trace:\n\n");
    for (;;) {
        uintptr_t *old_bp = (uintptr_t *)base_ptr[0];
        uintptr_t *ret_addr = (uintptr_t *)base_ptr[1];
        if (ret_addr == NULL || old_bp == NULL || (uintptr_t)ret_addr < 0xffffffff80000000) {
            break;
        }
        printf("\t- 0x%016lx\n", ret_addr);
        base_ptr = old_bp;
    }
}

void Arch::StackTrace() {
    printf("Stack trace:\n");
    //printf("rbp=0x%lx\n", rbp);
    arch_print_stack(0);
}