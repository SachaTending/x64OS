#include <idt.hpp>
#include <sched.hpp>
#include <logging.hpp>
#include <printf/printf.h>
#include <msr.h>
#include <libc.h>

static Logger log("Syscall handler");

void sys_print(const char *txt, uint64_t len) {
    for (uint64_t i=0;i<len;i++) putchar_(txt[i]);
}

void int80(idt_regs *regs) {
    switch (regs->rax)
    {
        case 2:
            if (regs->rdi == 1) { 
                sys_print((const char *)regs->rsi, regs->rdx);
            }
            break;
        
        default:
            log.debug("unknown syscall: %u\n", regs->rax);
            break;
    }

}

void syscall_c_entry(idt_regs *regs) {
    printf("SYSCALL: %u\n", regs->rax);
}

extern "C" void syscall_entry(void);
__attribute__((constructor)) void init_syscall() {
    idt_set_int(0x80-MAP_BASE, int80);
    wrmsr(0x174, 28);
    wrmsr(0x176, (uint64_t)syscall_entry);
    // Enable syscall extensions
    wrmsr(0xc0000080, rdmsr(0xc0000080) | 1);
    wrmsr(0xc0000081, ((0x48-16) >> 48));
    wrmsr(0xC0000082, (uint64_t)syscall_entry);
    wrmsr(0xC0000100, 0x30);
    //wrmsr(0xC0000101, 0x48 | 3);
    wrmsr(0xC0000102, 0x30);
}