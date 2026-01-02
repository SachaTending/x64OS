#include <arch/interrupts.h>
#include <arch/sched.hpp>
#include <libc.h>

void Arch::Scheduler::SaveState(cpu_ctx *current_ctx, arch_specific_cpu_state_t *state) {
    //printf("savestate: ctx: 0x%lx, state: 0x%lx\n", current_ctx, state);
    state->regs = *current_ctx;
}

static inline uint64_t wrmsr(uint32_t msr, uint64_t val) {
    //if (msr == 0xc0000100) {
    //    //printf("wrfsbase 0x%lx %s\n", val, __BASE_FILE__);
    //    asm volatile ("wrfsbase %%rax" : : "a" (val) : "memory");
    //    return val;
    //}
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

void Arch::Scheduler::LoadState(cpu_ctx *current_ctx, arch_specific_cpu_state_t *state) {
    //printf("loadstate: ctx: 0x%lx, state: 0x%lxs\n", current_ctx, state);
    if (current_ctx != NULL) *current_ctx = state->regs;
    //asm volatile ("wrfsbase %%rax" : : "a" (state->tls) : "memory");
    wrmsr(0xc0000100, state->tls);
}

void Arch::Scheduler::SetupSchedState(arch_specific_cpu_state_t *state, bool usermode, uint64_t entry, uint64_t stack) {
    state->regs.rsp = stack;
    state->regs.rip = entry;
    state->regs.rflags = 0x202;
    if (usermode == true) {
        state->regs.cs = (8 * 8) | 3;
        state->regs.ss = (7 * 8) | 3;
        state->regs.es = state->regs.ds = 7 * 8;
    } else {
        state->regs.cs = 0x28;
        state->regs.ss = state->regs.ds = state->regs.es = 0x30;
    }
}