#include <idt.hpp>
#include <sched.hpp>
#include <logging.hpp>
#include <printf/printf.h>

static Logger log("Syscall handler");

void sys_print(const char *txt, uint64_t len) {
    for (uint64_t i=0;i<len;i++) putchar_(txt[i]);
}

void int80(idt_regs *regs) {
    switch (regs->rax)
    {
        case 1:
            if (regs->rdi == 1) { 
                sys_print((const char *)regs->rsi, regs->rdx);
            }
            break;
        
        default:
            log.debug("unknown syscall: %u\n", regs->rax);
            break;
    }

}

__attribute__((constructor)) void init_syscall() {
    idt_set_int(0x80-MAP_BASE, int80);
}