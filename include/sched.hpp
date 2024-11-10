#pragma once
#include <idt.hpp>
#include <stddef.h>
#include <vmm.h>

extern pagemap *krnl_page;

void sched_init();
void start_sched();
void stop_sched();
void create_task(int (*task)(), const char *name, bool usermode=false, pagemap *pgm=krnl_page, uint64_t tls=0);

int getpid();
void sched_kill_pid(int pid);

void sched_handl(idt_regs *regs);

enum TASK_STATE {
    TASK_CREATE, // task creating
    TASK_READY, // task prepared
    TASK_RUNNING, // task running
    TASK_ZOMBIE // task killed
};

typedef struct task {
    struct task *next;
    idt_regs regs;
    const char *name;
    enum TASK_STATE state;
    size_t pid;
    bool last_task;
    pagemap *pgm;
    void *stack_addr;
    uint64_t tls;
} task_t;

extern task_t *root_task, *current_task;