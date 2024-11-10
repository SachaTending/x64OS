#include <idt.hpp>
#include <libc.h>
#include <sched.hpp>
#include <spinlock.h>
#include <vmm.h>
#include <krnl.hpp>
#include <logging.hpp>
#include <cpu.h>

static Logger log("Scheduler");

size_t next_pid;

task_t *root_task, *current_task;

bool SCHED_STARTED = false;
bool SCHED_READY = false;

extern "C" {
    void task_entry(void);
    void task_user_mode_entry(void);
}
void dummy_task() {
    for (;;) asm volatile ("hlt"); // dummy task, does nothing.
}

int getpid() {
    return current_task->pid;
}
void mgmt_on_new_program(size_t pid);
void sched_init() {
    root_task = new task_t;
    root_task->regs.rip = (uint64_t)task_entry;
    root_task->regs.rax = (uint64_t)dummy_task;
    root_task->next = root_task;
    root_task->name = strdup("Dummy task");
    root_task->pid = next_pid;
    root_task->last_task = true;
    root_task->pgm = krnl_page;
    next_pid++;
    root_task->stack_addr = malloc(64*1024);
    root_task->regs.rsp = ((uint64_t)root_task->stack_addr)+64*1024;
    current_task = root_task;
    root_task->state = TASK_READY;
    mgmt_on_new_program(root_task->pid);
}

bool SCHED_STOP = false;

void start_sched() {
    SCHED_READY = true;
    SCHED_STARTED = true;
    //SCHED_STOP = false;
    //log.debug("Scheduler started.\n");
}

void resume_sched() {
    SCHED_STOP = false;
}

void stop_sched() {
    SCHED_STOP = false;
}

task_t *get_process_by_pid(int pid) {
    task_t *task = root_task;
    while (true) {
        if (task->pid == pid) {
            return task;
        }
        task = task->next;
        if (task == root_task) {
            return NULL;
        }
    }
}

void sched_kill_pid(int pid) {
    stop_sched();
    task_t *task = get_process_by_pid(pid);
    if (task == NULL) {
        return; // No task found
    }
    task->state = TASK_ZOMBIE;
    log.debug("Task %d's state switched to TASK_ZOMBIE\n", pid);
    start_sched();
}

spinlock_t sched_lock = SPINLOCK_INIT;
#define STACK_SIZE 128*1024
bool _sched_stop_internal = false;
void create_task(int (*task)(), const char *name, bool usermode=false, pagemap *pgm=krnl_page, uint64_t tls=0) {
    _sched_stop_internal = true;
    spinlock_acquire(&sched_lock);
    task_t *new_task = new task_t;
    new_task->name = strdup(name);
    new_task->next = root_task;
    new_task->pid = next_pid;
    next_pid++;
    new_task->state = TASK_CREATE;
    new_task->regs.rip = (uint64_t)task_entry;
    new_task->regs.rax = (uint64_t)task;
    new_task->tls = tls;
    //new_task->regs.rflags = 0x202;
    new_task->regs.rflags |= (1 << 9);
    if (usermode == true) {
        new_task->regs.rip = (uint64_t)task;
        //new_task->regs.rcx = (uint64_t)task;
        new_task->regs.ds = new_task->regs.es = new_task->regs.ss = (10*8) | 3;
        new_task->regs.cs = (9*8) | 3;
        uint64_t rsp = (uint64_t)pmm_alloc(STACK_SIZE/4096);
        new_task->stack_addr = (void *)rsp;
        vmm_map_range(pgm, rsp, STACK_SIZE, PTE_PRESENT | PTE_USER | PTE_WRITABLE);
        //vmm_map_range(pgm, (uint64_t)task_entry-VMM_HIGHER_HALF, 8192, PTE_PRESENT | PTE_USER);
        new_task->regs.rsp = (rsp+STACK_SIZE);
    } else {
        new_task->stack_addr = ((void *)pmm_alloc(STACK_SIZE/4096));
        new_task->regs.rsp = ((uint64_t)new_task->stack_addr)+STACK_SIZE+VMM_HIGHER_HALF;
    }
    memset(new_task->stack_addr, 0, STACK_SIZE);
    task_t *task_p = root_task;
    do {
        task_p = task_p->next;
    } while(task_p->last_task != true); 
    task_p->last_task = false;
    new_task->state = TASK_READY;
    task_p->next = new_task; 
    new_task->last_task = true;
    new_task->pgm = pgm;
    mgmt_on_new_program(new_task->pid);
    spinlock_release(&sched_lock);
    _sched_stop_internal = false;
    //log.debug("Task with name %s created, entrypoint: 0x%lx, usermode: %d, pagemap: 0x%lx, pid: %d\n", name, task, usermode, pgm, new_task->pid);
}

void save_regs(idt_regs *regs) {
    // Save registers
    memcpy(&current_task->regs, regs, sizeof(idt_regs));
}

void load_regs(task_t *src, idt_regs *regs) {
    memcpy(regs, &src->regs, sizeof(idt_regs));
}

task_t *find_prev_task(task_t *task) {
    task_t *task2 = task;
    while (true) {
        if (task2->next == task) {
            return task2;
        }
        task2 = task2->next;
    }
}
task_t *sched_kill_task_by_state(task_t *task) {
    task_t *ntask = task;
    if (task->state == TASK_ZOMBIE) {
        task_t *prv = find_prev_task(task);
        if (prv == task) {
            PANIC("Task %s(pid: %d) tried to exit(this is the only task)\n", task->name, task->pid);
        }
        task_t *nxt = task->next;
        task_t *current = task;
        prv->next = nxt;
        pmm_free(current->stack_addr, STACK_SIZE/4096);
        log.debug("Task %s(pid: %d) killed.\n", current->name, current->pid);
        delete current;
        ntask = nxt;
    }
    return ntask;
}
void sched_handl(idt_regs *regs) {
    if (SCHED_STOP) return; // scheduler stopped
    if (_sched_stop_internal) return; // another flag
    if (!SCHED_READY) return;
    if (!SCHED_STARTED) {
        root_task->regs.cr2 = regs->cr2;
        if (root_task->regs.cs == 0) {
            root_task->regs.cs = regs->cs;
            root_task->regs.es = regs->es;
            root_task->regs.fs = root_task->tls;
            root_task->regs.gs = regs->gs;
            root_task->regs.ds = regs->ds;
            root_task->regs.ss = regs->ss;
        }
        root_task->regs.rflags |= (1 << 9);
        SCHED_STARTED = true;
    }
    else save_regs(regs);
    task_t *ntask;
    if (current_task->state == TASK_ZOMBIE) {
        ntask = current_task->next;
    }
    else {
        current_task->state = TASK_READY;
        ntask = current_task->next;
    }
    //ntask = sched_kill_task_by_state(ntask);
    ntask->regs.cr2 = regs->cr2;
    vmm_switch_to(ntask->pgm);
    if (ntask->regs.cs == 0) {
        ntask->regs.cs = regs->cs;
        ntask->regs.es = regs->es;
        ntask->regs.fs = ntask->tls;
        ntask->regs.gs = regs->gs;
        ntask->regs.ds = regs->ds;
        ntask->regs.ss = regs->ss;
    }
    //ntask->regs.gs = regs->gs;
    load_regs(ntask, regs);
    wrmsr(0xC0000100, ntask->tls);
    ntask->state = TASK_RUNNING;
    //regs->rflags |= (1 << 9);
    current_task = ntask;
    //log.debug("Switched to task %s, pid: %d, RIP: 0x%lx\n", current_task->name, current_task->pid, current_task->regs.rip);
}