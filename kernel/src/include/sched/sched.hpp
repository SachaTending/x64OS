#pragma once
#include <arch/sched.hpp>
#include <arch/vmm.h>
#include <vfs.hpp>
#include <spinlock.h>

enum thread_state {
    STATE_NEW, // Thread is just created
    STATE_RUNNING, // Thread is currently running
    STATE_SLEEP, // Thread is currently sleeping and waiting for new context switch
    STATE_STALLED, // Thread is currently stalled, so it won't be running again
    STATE_ZOMBIE, // Thread is currently shutting down
};

struct auxval {
    uint64_t at_entry;
    uint64_t at_phdr;
    uint64_t at_phent;
    uint64_t at_phnum;
};

enum syscall_set {
    SYSCALL_SET_LINUX,
    SYSCALL_SET_TRANS,
    SYSCALL_SET_X64OS
};
#define MAX_FDS 256
namespace Scheduler
{
    typedef struct thread {
        arch_specific_cpu_state_t cpu_state;
        const char *name;
        int pid;
        thread_state state = STATE_NEW;
        struct thread *next_thread;
        struct thread *prev_thread;
        uint64_t initial_stack;
        struct pagemap *pgm;
        uint64_t mmap_anon_base;
        syscall_set syscall;
        struct vfs_node *cwd;
        spinlock_t fds_lock;
        struct f_descriptor *fds[MAX_FDS];
        struct {
            uint64_t set_tid_addr;
        } linux_specific;
    } thread_t;
    void Init();
    void CreateThread(const char *name, void (*entry)(), bool usermode, struct pagemap *pgm, const char **argv=0, const char **envp=0, auxval *aux=0);
    void Start();
    void Stop();
    thread_t *GetCurrentThread();
} // namespace Scheduler
