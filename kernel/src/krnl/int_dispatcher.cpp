#include <arch/arch.hpp>
#include <arch/interrupts.h>
#include <krnl.hpp>
#include <logging.hpp>

static Logger log("Kernel interrupt dispatcher");

typedef void (*int_handler_func_t)(cpu_ctx *, void *priv);

struct int_handler {
    int_handler_func_t func;
    void *priv;
};

int_handler handlers[MAXIMUM_INTS] = {};

void Kernel::DispatchInterrupt(cpu_ctx *regs, uint64_t vector) {
    if (vector > MAXIMUM_INTS) return;
    else if (handlers[vector].func == 0) {
        log.warn("Got interrupt %lu, but there is no handler for that\n", vector);
        return;
    } else {
        handlers[vector].func(regs, handlers[vector].priv);
    }
}