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
size_t mgmt_syscall_read(size_t handle, void *buf, size_t count);
void mgmt_syscall_write(size_t handle, void *buf, size_t count);
void *mgmt_mmap(uintptr_t hint, size_t length, uint64_t flags, int fdnum, size_t offset);
int mgmt_set_tcb(void *ptr);
void mgmt_syscall_getcwd(char *buffer, size_t len);
size_t mgmt_syscall_seek(size_t handle, size_t seek_pos, int whence);
void mgmt_syscall_ioctl(size_t handle, uint64_t req, void *s);
size_t mgmt_syscall_exec(const char *path, char **argv, char **env);
int sched_fork();
int sched_fork(idt_regs *regs);
void sys_print(const char *txt, uint64_t len) {
    printf("");
    for (uint64_t i=0;i<len;i++) putchar_(txt[i]);
}
uint8_t ps2_recv_dev();

void tasks() {
    task_t *t = root_task;
    log.info("Task list:\n");
    do {
        if (t->fork_parent == 0) log.info("Task: %s PID: %u\n", t->name, t->pid);
        else log.info("Task: %s PID: %u PPID: %u\n", t->name, t->pid, t->fork_parent->pid);
        t = t->next;
    } while (t != root_task);
}

void int80(idt_regs *regs, void *_) {
    (void)_;
    log.debug("SYSCALL: %u RIP: 0x%lx RSP: 0x%lx\n", regs->rax, regs->rcx, regs->rsp);
    switch (regs->rax)
    {
        case 0:
            regs->rax = mgmt_syscall_read(regs->rdi, (void *)regs->rsi, (size_t)regs->rdx);
            break;
        case 1:
            regs->rax = 0;
            mgmt_syscall_write(regs->rdi, (void *)regs->rsi, regs->rdx);
            break;
        case 2:
            regs->rax = mgmt_syscall_open((const char *)regs->rdi);
            break;
        case 4:
            regs->rax = (uint64_t)mgmt_mmap(regs->rdi, regs->rsi, regs->rdx, regs->r8, regs->r9);
            break;
        case 7: // Set tcb
            regs->rax = mgmt_set_tcb((void *)regs->rdi);
            break;
        case 8:
            regs->rax = 0;
            mgmt_syscall_getcwd((char *)regs->rdi, (size_t)regs->rsi);
            break;
        case 9:
            regs->rax = mgmt_syscall_seek((size_t)regs->rdi, (size_t)regs->rsi, (int)regs->rdx);
            break;
        case 10:
            regs->rax = 0;
            mgmt_syscall_ioctl((size_t)regs->rdi, regs->rsi, (void *)regs->rdx);
            break;
        case 11:
            // TODO: Implement unmap
            regs->rax = 0;
            break;
        case 12:
            regs->rax = mgmt_syscall_exec((const char *)regs->rdi, (char **)regs->rsi, (char **)regs->rdx);
            break;
        case 13:
            regs->rax = sched_fork(regs);
            break;
        case 14:
            log.info("Simulating waitpid.\n");
            for(;;) asm volatile ("hlt");
            break;
        case 39:
            regs->rax = getpid();
            break;
        case 60: // exit
            sched_kill_pid(getpid());
            break;
        case 512:
            //printf("debug from process: %s\n", regs->rdi);
            printf("debug: %s\n", regs->rdi);
            break;
        case 1024:
            regs->rax = 0;
            tasks();
            break;
        default:
            log.debug("unknown syscall: %u\n", regs->rax);
            regs->rax = 0;
            break;
    }
    //regs->cs = 8*8 | 3;
    //regs->ss = 7*8 | 3;
}

extern "C" idt_regs *syscall_c_entry(idt_regs *regs) {
    //printf("SYSCALL: %u\n", regs->rax);
    int80(regs, nullptr);
    //regs->cs = (8*8) | 3;
    //regs->ss = (7*8) | 3;
    return regs;
}

extern "C" void syscall_entry(void);
__attribute__((constructor)) void init_syscall() {
    idt_set_int(0x80-MAP_BASE, int80);
    //wrmsr(0x174, 28);
    //wrmsr(0x176, (uint64_t)syscall_entry);
    // Enable syscall extensions
    wrmsr(0xc0000080, rdmsr(0xc0000080) | 1);
    uint64_t a = ((uint64_t)6*8) << 48;
    a |= (uint64_t)((uint64_t)0x28 << 32);
    wrmsr(0xc0000081, a);
    //printf("msr 0xc0000081 = 0x%lx", a);
    wrmsr(0xC0000082, (uint64_t)syscall_entry);
    wrmsr(0xC0000100, 0xff);
    wrmsr(0xc0000100, 0xff);
    wrmsr(0xC0000102, 0xff);
}
