#pragma once
#include <arch/interrupts.h>
typedef struct arch_specific_cpu_state {
    cpu_ctx regs;
    uint64_t tls;
} arch_specific_cpu_state_t;
namespace Arch
{
    namespace Scheduler
    {
        void SaveState(cpu_ctx *current_ctx, arch_specific_cpu_state_t *state);
        void LoadState(cpu_ctx *current_ctx, arch_specific_cpu_state_t *state);
        void SetupSchedState(arch_specific_cpu_state_t *state, bool usermode, uint64_t entry, uint64_t stack);
    } // namespace Scheduler
} // namespace Arch