#include <idt.hpp>
#include <libc.h>

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
} task_t;

size_t next_pid;

task_t *root_task, *current_task;

bool SCHED_STARTED = false;

extern "C" void task_entry(void);

void dummy_task() {
    for (;;) asm volatile ("hlt"); // dummy task, does nothing.
}

const char *strdup(const char *in) {
    size_t l = strlen(in);
    void *a = malloc(l+1);
    memset(a, 0, l+1);
    strcpy((char *)a, in);
    return (const char *)a;
}

void sched_init() {
    root_task = new task_t;
    root_task->regs.rip = (uint64_t)task_entry;
    root_task->regs.rax = (uint64_t)dummy_task;
    root_task->next = root_task;
    root_task->name = strdup("Dummy task");
    current_task = root_task;
    root_task->state = TASK_READY;
}

void create_task(void (*task)(), const char *name) {
    task_t *ntask = new task_t;
    ntask->state = TASK_CREATE;
    ntask->regs.rip = (uint64_t)task_entry;
    ntask->regs.rax = (uint64_t)task;
    ntask->name = strdup(name);
    ntask->next = root_task;
    task_t *ctask = current_task;
    while (ctask->next != root_task) {
        ctask = ctask->next;
    }
    ctask->next = ntask;
    ntask->state = TASK_READY;
}

void save_regs(idt_regs *regs) {
    // Save registers
    memcpy(&current_task->regs, regs, sizeof(idt_regs));
}

void load_regs(task_t *src, idt_regs *regs) {
    memcpy(regs, &src->regs, sizeof(idt_regs));
}

void sched_handl(idt_regs *regs) {
    if (!SCHED_STARTED) {
        root_task->regs.cr2 = regs->cr2;
        root_task->regs.cs = regs->cs;
        root_task->regs.es = regs->es;
        root_task->regs.fs = regs->fs;
        root_task->regs.gs = regs->gs;
        root_task->regs.ds = regs->ds;
        root_task->regs.ss = regs->ss;
    }
    current_task->state = TASK_READY;
    save_regs(regs);
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