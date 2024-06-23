#include <idt.hpp>
#include <libc.h>
#include <sched.hpp>
#include <spinlock.h>

size_t next_pid;

task_t *root_task, *current_task;

bool SCHED_STARTED = false;
bool SCHED_READY = false;

extern "C" void task_entry(void);

void dummy_task() {
    for (;;) asm volatile ("hlt"); // dummy task, does nothing.
}

void sched_init() {
    root_task = new task_t;
    root_task->regs.rip = (uint64_t)task_entry;
    root_task->regs.rax = (uint64_t)dummy_task;
    root_task->next = root_task;
    root_task->name = strdup("Dummy task");
    root_task->pid = next_pid;
    root_task->last_task = true;
    next_pid++;
    void *stack = malloc(64*1024);
    root_task->regs.rsp = ((uint64_t)stack)+64*1024;
    current_task = root_task;
    root_task->state = TASK_READY;
}

void start_sched() {
    SCHED_READY = true;
}

bool SCHED_STOP = false;
spinlock_t sched_lock = SPINLOCK_INIT;

void create_task(int (*task)(), const char *name) {
    SCHED_STOP = true;
    spinlock_acquire(&sched_lock);
    task_t *new_task = new task_t;
    new_task->name = strdup(name);
    new_task->next = root_task;
    new_task->pid = next_pid;
    next_pid++;
    new_task->state = TASK_CREATE;
    new_task->regs.rip = (uint64_t)task_entry;
    new_task->regs.rax = (uint64_t)task;
    new_task->regs.rsp = (uint64_t)malloc(64*1024)+64*1024;
    task_t *task_p = root_task;
    do {
        task_p = task_p->next;
    } while(task_p->last_task != true); 
    task_p->last_task = false;
    new_task->state = TASK_READY;
    task_p->next = new_task; 
    new_task->last_task = true;
    spinlock_release(&sched_lock);
    SCHED_STOP = false;
}

void save_regs(idt_regs *regs) {
    // Save registers
    memcpy(&current_task->regs, regs, sizeof(idt_regs));
}

void load_regs(task_t *src, idt_regs *regs) {
    memcpy(regs, &src->regs, sizeof(idt_regs));
}

void sched_handl(idt_regs *regs) {
    if (SCHED_STOP) return; // scheduler stopped
    if (!SCHED_READY) return;
    if (!SCHED_STARTED) {
        root_task->regs.cr2 = regs->cr2;
        root_task->regs.cs = regs->cs;
        root_task->regs.es = regs->es;
        root_task->regs.fs = regs->fs;
        root_task->regs.gs = regs->gs;
        root_task->regs.ds = regs->ds;
        root_task->regs.ss = regs->ss;
        SCHED_STARTED = true;
    }
    else save_regs(regs);
    current_task->state = TASK_READY;
    task_t *ntask = current_task->next;
    ntask->regs.cr2 = regs->cr2;
    ntask->regs.cs = regs->cs;
    ntask->regs.es = regs->es;
    ntask->regs.fs = regs->fs;
    ntask->regs.gs = regs->gs;
    ntask->regs.ds = regs->ds;
    ntask->regs.ss = regs->ss;
    load_regs(ntask, regs);
    ntask->state = TASK_RUNNING;
    current_task = ntask;
}