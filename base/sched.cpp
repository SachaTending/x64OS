#include <idt.hpp>
#include <libc.h>
#include <sched.hpp>
#include <spinlock.h>
#include <vmm.h>
#include <krnl.hpp>
#include <logging.hpp>
#include <cpu.h>
#include <elf.h>

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
void sched_init(int (*task)(), const char *name) {
    root_task = new task_t;
    root_task->regs.rip = (uint64_t)task_entry;
    root_task->regs.rax = (uint64_t)task;
    root_task->next = root_task;
    root_task->name = strdup(name);
    root_task->pid = next_pid;
    root_task->last_task = true;
    root_task->pgm = krnl_page;
    root_task->regs.cs = 5*8 | 0;
    root_task->regs.es = root_task->regs.ds = root_task->regs.ss = 6*8 | 0;
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

task_t *get_current_task() {
    return current_task;
}

void sched_kill_pid(int pid) {
    stop_sched();
    task_t *task = get_process_by_pid(pid);
    if (task == NULL) {
        return; // No task found
    }
    task->state = TASK_ZOMBIE;
    log.debug("Task %d's state switched to TASK_ZOMBIE\n", pid);
    resume_sched();
}

spinlock_t sched_lock = SPINLOCK_INIT;
#define STACK_SIZE 256*1024
bool _sched_stop_internal = false;
void create_task(int (*task)(), 
    const char *name, 
    bool usermode=false, 
    pagemap *pgm=krnl_page, 
    uint64_t tls=0, 
    int argc=0, 
    const char **argv=0, 
    const char **envp=0,
    auxval* aux=0) {
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
    new_task->mmap_anon_base = 0x80000000000;
    //new_task->regs.rflags = 0x202;
    new_task->regs.rflags |= (1 << 9);
    uintptr_t *stack;
    if (usermode == true) {
        new_task->regs.rip = (uint64_t)task;
        //new_task->regs.rcx = (uint64_t)task;
        new_task->regs.ds = new_task->regs.es = new_task->regs.ss = (7*8) | 3;
        new_task->regs.cs = (8*8) | 3;
        uint64_t rsp = (uint64_t)pmm_alloc(STACK_SIZE/4096);
        new_task->stack_addr = (void *)rsp;
        vmm_map_range(pgm, rsp, STACK_SIZE, PTE_PRESENT | PTE_USER | PTE_WRITABLE);
        //vmm_map_range(pgm, (uint64_t)task_entry-VMM_HIGHER_HALF, 8192, PTE_PRESENT | PTE_USER);
        new_task->regs.rsp = (rsp+STACK_SIZE);
        if (argc != NULL && argv != NULL && envp != NULL && aux != NULL) {
            stack = (uintptr_t *)new_task->regs.rsp;
            void *stack_top = stack;
            int envp_len;
            for (envp_len = 0; envp[envp_len] != NULL; envp_len += 1) {
                size_t len = strlen(envp[envp_len]);
                stack = (uintptr_t *)((uint64_t)stack - len - 1);
                memcpy((void *)stack, envp[envp_len], len);
            }

            int argv_len;
            for (argv_len = 0; argv[argv_len] != NULL; argv_len++) {
                size_t len = strlen(argv[argv_len]);
                stack = (uintptr_t *)((uint64_t)stack - len - 1);
                memcpy(stack, argv[argv_len], len);
            }

            stack = (uintptr_t *)ALIGN_DOWN((uintptr_t)stack, 16);
            if (((argv_len + envp_len + 1) & 1) != 0) {
                stack--;
            }

            // Auxilary vector
            *(--stack) = 0, *(--stack) = 0;
            stack -= 2; stack[0] = AT_SECURE, stack[1] = 0;
            stack -= 2; stack[0] = AT_ENTRY, stack[1] = aux->at_entry;
            stack -= 2; stack[0] = AT_PHDR,  stack[1] = aux->at_phdr;
            stack -= 2; stack[0] = AT_PHENT, stack[1] = aux->at_phent;
            stack -= 2; stack[0] = AT_PHNUM, stack[1] = aux->at_phnum;

            uintptr_t old_rsp = new_task->regs.rsp;

             *(--stack) = 0;
            stack -= envp_len;
            for (int i = 0; i < envp_len; i++) {
                old_rsp -= strlen(envp[i]) + 1;
                stack[i] = old_rsp;
            }

            // Arguments
            *(--stack) = 0;
            stack -= argv_len;
            for (int i = 0; i < argv_len; i++) {
                old_rsp -= strlen(argv[i]) + 1;
                stack[i] = old_rsp;
            }

            *(--stack) = argv_len;

            new_task->regs.rsp -= (uint64_t)stack_top - (uint64_t)stack;
        }
    } else {
        new_task->stack_addr = ((void *)pmm_alloc(STACK_SIZE/4096));
        new_task->regs.rsp = ((uint64_t)new_task->stack_addr)+STACK_SIZE+VMM_HIGHER_HALF;
        new_task->regs.cs = 5*8;
        new_task->regs.ds = new_task->regs.es = new_task->regs.ss = 6*8;
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
    new_task->mmap_anon_base = 0x80000000000;
    mgmt_on_new_program(new_task->pid);
    spinlock_release(&sched_lock);
    _sched_stop_internal = false;
    //log.debug("Task with name %s created, entrypoint: 0x%lx, usermode: %d, pagemap: 0x%lx, pid: %d\n", name, task, usermode, pgm, new_task->pid);
}

void save_regs(cpu_ctx *regs) {
    // Save registers
    memcpy(&current_task->regs, regs, sizeof(cpu_ctx));
}

void load_regs(task_t *src, cpu_ctx *regs) {
    memcpy(regs, &src->regs, sizeof(cpu_ctx));
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
void sched_handl(cpu_ctx *regs) {
    if (SCHED_STOP) return; // scheduler stopped
    if (_sched_stop_internal) return; // another flag
    if (!SCHED_READY) return;
    if (!SCHED_STARTED) {
        //if (root_task->regs.cs == 0) {
        //    root_task->regs.cs = 0x28;
        //    root_task->regs.es = 0x30;
        //    root_task->regs.ds = 0x30;
        //    root_task->regs.ss = 0x30;
        //}
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
    vmm_switch_to(ntask->pgm);
    //if (ntask->regs.cs == 0) {
    //    ntask->regs.cs = 0x28;
    //    ntask->regs.es = 0x30;
    //    ntask->regs.ds = 0x30;
    //    ntask->regs.ss = 0x30;
    //}
    //log.debug("next rip=0x%lx\n", regs->rip);
    //ntask->regs.gs = regs->gs;
    if (ntask->next != ntask )load_regs(ntask, regs);
    //wrmsr(0xC0000100, ntask->tls);
    ntask->state = TASK_RUNNING;
    //regs->rflags |= (1 << 9);
    current_task = ntask;
    //log.debug("Switched to task %s, pid: %d, RIP: 0x%lx\n", current_task->name, current_task->pid, current_task->regs.rip);
    return;
}