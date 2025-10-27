#include <arch/interrupts.h>
#include <arch/sched.hpp>
#include <libc.h>

void Arch::Scheduler::SaveState(cpu_ctx *current_ctx, arch_specific_cpu_state_t *state) {
    //printf("savestate: ctx: 0x%lx, state: 0x%lx\n", current_ctx, state);
    state->regs = *current_ctx;
}
void Arch::Scheduler::LoadState(cpu_ctx *current_ctx, arch_specific_cpu_state_t *state) {
    //printf("loadstate: ctx: 0x%lx, state: 0x%lxs\n", current_ctx, state);
    *current_ctx = state->regs;
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