#include <idt.hpp>
#include <sched.hpp>
#include <logging.hpp>
#include <printf/printf.h>
#include <msr.h>
#include <libc.h>
#include <sys/syscall.h>
#include <sched.hpp>
#include <vfs.hpp>
#include <frg/std_compat.hpp>
#include <frg/vector.hpp>
#include <new>

struct proc_handle {
    size_t id;
    vfs_node_t *node;
};

typedef frg::vector<proc_handle *, frg::stl_allocator> proc_handles_t;

struct proc_info {
    size_t pid;
    proc_handles_t *handles;
};

static Logger log("Syscall handler");
size_t mgmt_syscall_open(const char *path);
void mgmt_syscall_read(size_t handle, void *buf, size_t count);
void *mgmt_mmap(uintptr_t hint, size_t length, uint64_t flags, int fdnum, size_t offset);
void sys_print(const char *txt, uint64_t len) {
    printf("");
    for (uint64_t i=0;i<len;i++) putchar_(txt[i]);
}
uint8_t ps2_recv_dev();

__attribute__((section(".data"))) extern "C" {
    __attribute__((section(".data")))  void *syscall_stack = 0;
}
void int80(cpu_ctx *regs) {
    //log.debug("SYSCALL: %u\n", regs->rax);
    switch (regs->rax)
    {
        case 0:
            mgmt_syscall_read(regs->rdi, (void *)regs->rsi, (size_t)regs->rdx);
            break;
        case 1:
            if (regs->rdi == 1) { 
                sys_print((const char *)regs->rsi, regs->rdx);
            }
            break;
        case 2:
            regs->rax = mgmt_syscall_open((const char *)regs->rdi);
            break;
        case 4:
            regs->rax = (uint64_t)mgmt_mmap(regs->rdi, regs->rsi, regs->rdx, regs->r8, regs->r9);
            break;
        case 39:
            regs->rax = getpid();
            break;
        case 60: // exit
            sched_kill_pid(getpid());
            break;
        case 512:
            log.debug("debug from process: %s\n", regs->rdi);
            break;
        default:
            log.debug("unknown syscall: %u\n", regs->rax);
            break;
    }
}

extern "C" cpu_ctx *syscall_c_entry(cpu_ctx *regs) {
    //printf("SYSCALL: %u\n", regs->rax);
    int80(regs);
    return regs;
}
bool mmap_pf(cpu_ctx *regs);
extern "C" void syscall_entry(void);
__attribute__((constructor)) void init_syscall() {
    idt_set_int(0x80-MAP_BASE, int80);
    //wrmsr(0x174, 28);
    //wrmsr(0x176, (uint64_t)syscall_entry);
    // Enable syscall extensions
    wrmsr(0xc0000080, rdmsr(0xc0000080) | 1);
    wrmsr(0xc0000081, (((6*8)-16) >> 48 | (0x28 >> 32)));
    wrmsr(0xC0000082, (uint64_t)syscall_entry);
    wrmsr(0xC0000100, 0xff);
    wrmsr(0xC0000101, 0xff);
    wrmsr(0xC0000102, 0xff);
    // Register mmap page fault handler as int exception handler
    syscall_stack = malloc(128*1024)+128*1024;
    idt_register_exception_handler(14, mmap_pf);
    printf("amogus\n");
}
