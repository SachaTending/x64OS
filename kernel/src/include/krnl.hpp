#pragma once
#include <arch/interrupts.h>

typedef void (*int_handler_func_t)(cpu_ctx *, void *priv);
namespace Kernel
{
    void DispatchInterrupt(cpu_ctx *regs, uint64_t vector);
    void RegisterInterruptHandler(uint64_t vector, int_handler_func_t handler, void *priv);
    
} // namespace Kernel
