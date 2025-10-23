#pragma once
#include <arch/interrupts.h>
namespace Kernel
{
    void DispatchInterrupt(cpu_ctx *regs, uint64_t vector);
} // namespace Kernel
