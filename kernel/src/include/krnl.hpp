#pragma once
#include <arch/interrupts.h>
#include <logging.hpp>

typedef void (*int_handler_func_t)(cpu_ctx *, void *priv);
namespace Kernel
{
    void DispatchInterrupt(cpu_ctx *regs, uint64_t vector);
    void RegisterInterruptHandler(uint64_t vector, int_handler_func_t handler, void *priv);
    void HandleSyscall(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3);
    void Main();
} // namespace Kernel
