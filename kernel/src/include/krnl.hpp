#pragma once
#include <arch/interrupts.h>
#include <logging.hpp>
#include <stddef.h>

typedef void (*int_handler_func_t)(cpu_ctx *, void *priv);
namespace Kernel
{
    void DispatchInterrupt(cpu_ctx *regs, uint64_t vector);
    void RegisterInterruptHandler(uint64_t vector, int_handler_func_t handler, void *priv);
    uint64_t HandleSyscall(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6, cpu_ctx *ctx);
    void Main();
} // namespace Kernel

__attribute__((noreturn)) void panic(const char *file, size_t lnum, const char *msg, ...);
#define PANIC(...) panic(__FILE__, __LINE__, __VA_ARGS__)