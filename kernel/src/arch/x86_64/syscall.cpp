#include <arch/interrupts.h>
#include <krnl.hpp>
#include <libc.h>
extern "C" int syscall_c_entry(cpu_ctx *ctx) {
    //printf("GOT SYSCALL\n");
    //printf("RIP: 0x%lx\n", ctx->rip);
    //if (ctx->rax == 512) {
    //    printf("debug syscall: %s", ctx->rdi);
    //} else {
    //    printf("wtf is syscall %d(0x%lx)\n", ctx->rax, ctx->rax);
    //}
    Kernel::HandleSyscall(ctx->rax, ctx->rdi, ctx->rsi, ctx->rdx);
    return 0;
}
extern "C" void syscall_entry();

static inline uint64_t wrmsr(uint32_t msr, uint64_t val) {
    if (msr == 0xc0000100) {
        //printf("wrfsbase 0x%lx %s\n", val, __BASE_FILE__);
        asm volatile ("wrfsbase %%rax" : : "a" (val) : "memory");
        return val;
    }
    uint32_t eax = (uint32_t)val;
    uint32_t edx = (uint32_t)(val >> 32);
    asm volatile (
        "wrmsr\n\t"
        :
        : "a" (eax), "d" (edx), "c" (msr)
        : "memory"
    );
    return ((uint64_t)edx << 32) | eax;
}

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t edx = 0, eax = 0;
    asm volatile (
        "rdmsr\n\t"
        : "=a" (eax), "=d" (edx)
        : "c" (msr)
        : "memory"
    );
    return ((uint64_t)edx << 32) | eax;
}

void arch_setup_syscall() {
    wrmsr(0xc0000080, rdmsr(0xc0000080) | 1);
    wrmsr(0xc0000081, (((uint64_t)6*8) << 48) | ((uint64_t)0x28 << 32));
    wrmsr(0xC0000082, (uint64_t)syscall_entry);
}