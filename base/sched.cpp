#include <idt.hpp>
#include <libc.h>
#include <sched.hpp>
#include <spinlock.h>
#include <vmm.h>
#include <krnl.hpp>
#include <logging.hpp>
#include <cpu.h>
#include <elf.h>
#include <msr.h>

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
    root_task->usermode = false;
    root_task->cr3 = root_task->pgm->top_level-VMM_HIGHER_HALF;
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
    start_sched();
}
static inline uint64_t read_cr3(void) {
    uint64_t ret;
    asm volatile ("mov %%cr3, %0" : "=r"(ret) :: "memory");
    return ret;
}
spinlock_t sched_lock = SPINLOCK_INIT;
#define STACK_SIZE 64*1024
bool _sched_stop_internal = false;
static inline void write_cr3(uint64_t value) {
    //log.debug("write_cr3(0x%lx);\n", value);
    asm volatile ("mov %0, %%cr3" :: "r"(value) : "memory");
}

size_t strlen_pgm(const char *txt) {
    size_t o = 0;
    o = 0;
    if ((uint64_t)txt == 0x747365745f636374) return 0;
    //log.debug("txt: 0x%lx\n", txt);
    //txt = (const char *)vmm_virt2phys(get_current_task()->pgm, (uint64_t)txt);
    //log.debug("txt: 0x%lx\n", txt);
    //if ((uint64_t)txt == 0xffffffffffffffff) return 0;
    //txt += VMM_HIGHER_HALF;
    //log.debug("txt: 0x%lx\n", txt);
    while (*txt++) o++;
    return o;
}
#if CONFIG_DEBUG == 'y'
#define DBG_PRINT(...) printf(__VA_ARGS__)
#else
#define DBG_PRINT(...)
#endif
void create_task(int (*task)(), 
    const char *name, 
    bool usermode=false, 
    pagemap *pgm=krnl_page, 
    uint64_t tls=0, 
    const char **argv=0, 
    const char **envp=0,
    auxval* aux=0,
    uint64_t *out_pid=0,
    int argc=0
) {
    _sched_stop_internal = true;
    spinlock_acquire(&sched_lock);
    task_t *new_task = new task_t;
    new_task->name = strdup(name);
    new_task->next = root_task;
    size_t pid = next_pid;
    new_task->pid = pid;
    next_pid++;
    new_task->state = TASK_CREATE;
    new_task->regs.rip = (uint64_t)task_entry;
    new_task->regs.rax = (uint64_t)task;
    new_task->tls = tls;
    new_task->mmap_anon_base = 0x80000000000;
    //new_task->regs.rflags = 0x202;
    new_task->regs.rflags |= (1 << 9);
    volatile uintptr_t *stack;
    new_task->usermode = usermode;
    //#define stack (new_task->regs.rsp)
    if (usermode == true) {
        new_task->regs.rip = (uint64_t)task;
        new_task->regs.rax = 0;
        //new_task->regs.rcx = (uint64_t)task;
        //new_task->regs.ds = new_task->regs.es = new_task->regs.ss = (7*8) | 3;
        new_task->regs.ss = 0x53;
        new_task->regs.es = new_task->regs.ds = (10*8) | 3;
        new_task->regs.cs = 0x4B;
        uint64_t rsp = (uint64_t)pmm_alloc(STACK_SIZE/4096);
        new_task->stack_addr = (void *)rsp;
        vmm_map_range(pgm, rsp, STACK_SIZE, PTE_PRESENT | PTE_USER | PTE_WRITABLE);
        //vmm_map_range(pgm, (uint64_t)task_entry-VMM_HIGHER_HALF, 8192, PTE_PRESENT | PTE_USER);
        new_task->regs.rsp = (rsp+STACK_SIZE);
        uint64_t old_p = read_cr3();
        DBG_PRINT("old_cr3: 0x%lx\n", old_p);
        //write_cr3((uint64_t)krnl_page->top_level-VMM_HIGHER_HALF);
        if (argv != NULL && envp != NULL && aux != NULL) {
            //vmm_switch_to(pgm);
            DBG_PRINT("pushing argc, argv, envp, aux...\n");
            DBG_PRINT("rsp: 0x%lx\n", new_task->regs.rsp);
            stack = (uintptr_t *)new_task->regs.rsp;
            //stack += VMM_HIGHER_HALF;
            DBG_PRINT("stack: 0x%lx\n", stack);
            #define GET_REAL_STACK ((uint64_t *)(vmm_virt2phys(pgm, (uint64_t)stack)+VMM_HIGHER_HALF))
            void *stack_top = new_task->stack_addr+STACK_SIZE;
            int envp_len;
            for (envp_len = 0; envp[envp_len] != NULL; envp_len += 1) {
                size_t len = strlen_pgm(envp[envp_len]);
                DBG_PRINT("len: %d\n", len);
                stack = (uintptr_t *)((uint64_t)stack - len - 1);
                memcpy((void *)GET_REAL_STACK, envp[envp_len], len);
                DBG_PRINT("pushed env: %s\n", envp[envp_len]);
            }
            //printf("pushed all env\n");
            //int argv_len;
            //printf("argv: 0x%lx\n", argv);
//
            //for (argv_len = 0; argv[argv_len] != NULL; argv_len++) {
            //    printf("argv_len=%d\n", argv_len);
            //    printf("strlen(0x%lx(%d))\n", argv[argv_len], argv_len);
            //    size_t len = strlen_pgm(argv[argv_len]);
            //    if (len == 0) {
            //        argv_len--;
            //        break;
            //    }
            //    printf("len: %d\n", len);
            //    stack = (uintptr_t *)((uint64_t)stack - len - 1);
            //    printf("stack: 0x%lx\n", stack);
            //    memcpy((void *)GET_REAL_STACK, argv[argv_len], len);
            //    printf("pushed arg: %s\n", argv[argv_len]);
            //}
            int argv_len;
            for (argv_len = 0; argv_len < argc; argv_len++) {
                size_t length = strlen_pgm(argv[argv_len]);
                stack = (void *)stack - length - 1;
                //printf("memcpy(0x%lx, 0x%lx, %lu)\n", stack, argv[argv_len]);
                memcpy(GET_REAL_STACK, argv[argv_len], length);
            }

            //printf("argv_len=%d\n", argv_len);
            stack = (uintptr_t *)ALIGN_DOWN((uintptr_t)stack, 16);
            //printf("stack: 0x%lx\n", stack);
            if (((argv_len + envp_len + 1) & 1) != 0) {
                stack--;
            }

            // Auxilary vector
            //*(--stack) = 0, *(--stack) = 0;
            stack--;
            *GET_REAL_STACK = 0;
            stack--;
            *GET_REAL_STACK = 0;
            //printf("stack: 0x%lx\n", stack);
            stack -= 2; GET_REAL_STACK[0] = AT_SECURE, GET_REAL_STACK[1] = 0;
            stack -= 2; GET_REAL_STACK[0] = AT_ENTRY, GET_REAL_STACK[1] = aux->at_entry;
            //printf("at_entry=0x%lx\n", aux->at_entry);
            stack -= 2; GET_REAL_STACK[0] = AT_PHDR,  GET_REAL_STACK[1] = aux->at_phdr;
            stack -= 2; GET_REAL_STACK[0] = AT_PHENT, GET_REAL_STACK[1] = aux->at_phent;
            stack -= 2; GET_REAL_STACK[0] = AT_PHNUM, GET_REAL_STACK[1] = aux->at_phnum;

            uintptr_t old_rsp = new_task->regs.rsp;

            stack--;
            *GET_REAL_STACK = 0;
            stack -= envp_len;
            for (int i = 0; i < envp_len; i++) {
                //printf("old_rsp -= strlen(0x%lx(%d))\n", envp[i], i);
                old_rsp -= strlen_pgm(envp[i]) + 1;
                GET_REAL_STACK[i] = old_rsp;
            }

            // Arguments
            //*(--stack) = 0;
            stack--;
            *GET_REAL_STACK = 0;
            stack -= argv_len;
            for (int i = 0; i < argv_len; i++) {
                //printf("old_rsp -= strlen(0x%lx(%d))\n", argv[i], i);
                old_rsp -= strlen_pgm(argv[i]) + 1;
                GET_REAL_STACK[i] = old_rsp;
            }
            //printf("stack: 0x%lx\n", stack);
            //*(--stack) = argv_len;
            stack--;
            *GET_REAL_STACK = argv_len;
            //printf("argv_len: %lu\n", argv_len);
            //printf("pushed: %lu\n", *stack);
            //*(--stack) = (uint64_t)stack;
            //new_task->regs.rdi = (uint64_t)stack;
            new_task->regs.rsp = (uint64_t)stack;
            //new_task->regs.rsp -= (uint64_t)stack_top - (uint64_t)stack;
            //printf("rsp: 0x%lx\n", new_task->regs.rsp);
        }
        write_cr3(old_p);
    } else {
        new_task->stack_addr = ((void *)pmm_alloc(STACK_SIZE/4096));
        new_task->regs.rsp = ((uint64_t)new_task->stack_addr)+STACK_SIZE+VMM_HIGHER_HALF;
        new_task->regs.cs = 5*8;
        new_task->regs.es = new_task->regs.ds = new_task->regs.ss = 6*8;
    }
    //memset(new_task->stack_addr, 0, STACK_SIZE);
    task_t *task_p = root_task;
    do {
        task_p = task_p->next;
    } while(task_p->last_task != true); 
    task_p->last_task = false;
    new_task->state = TASK_READY;
    task_p->next = new_task; 
    new_task->last_task = true;
    new_task->pgm = pgm;
    new_task->cr3 = (uint64_t)(pgm->top_level)-VMM_HIGHER_HALF;
    new_task->mmap_anon_base = 0x80000000000;
    mgmt_on_new_program(new_task->pid);
    spinlock_release(&sched_lock);
    _sched_stop_internal = false;
    if (out_pid) *out_pid=pid;
    //log.debug("Task with name %s created, entrypoint: 0x%lx, usermode: %d, pagemap: 0x%lx, pid: %d\n", name, task, usermode, pgm, new_task->pid);
}
bool is_sched_started() {
    return SCHED_STARTED;
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
        //root_task->regs.cr2 = regs->cr2;
        //if (root_task->regs.cs == 0) {
        //    root_task->regs.cs = regs->cs;
        //    root_task->regs.es = regs->es;
        //    //root_task->regs.fs = root_task->tls;
        //    root_task->regs.gs = regs->gs;
        //    root_task->regs.ds = regs->ds;
        //    root_task->regs.ss = regs->ss;
        //}
        root_task->regs.rflags |= (1 << 9);
        SCHED_STARTED = true;
    }
    else save_regs(regs);
    task_t *ntask;
    current_task->cr3 = read_cr3();
    vmm_switch_to(krnl_page);
    if (current_task->state == TASK_ZOMBIE) {
        ntask = current_task->next;
    }
    else {
        current_task->state = TASK_READY;
        ntask = current_task->next;
    }
    //ntask = sched_kill_task_by_state(ntask);
    ntask->regs.cr2 = regs->cr2;
    //vmm_switch_to(ntask->pgm);
    if (ntask->regs.cs == 0) {
        ntask->regs.cs = regs->cs;
        ntask->regs.es = regs->es;
        ntask->regs.fs = ntask->tls;
        ntask->regs.gs = regs->gs;
        ntask->regs.ds = regs->ds;
        ntask->regs.ss = regs->ss;
    }
    //ntask->regs.gs = regs->gs;
    if (ntask)
    load_regs(ntask, regs);
    if (ntask->usermode) {
        wrmsr(0xc0000100, ntask->tls);
        //log.debug("task %s(%u), msr 0xC0000100: 0x%lx\n", ntask->name, ntask->pid, rdmsr(0xC0000100));
    }
    ntask->state = TASK_RUNNING;
    regs->fs = ntask->tls;
    //regs->rflags |= (1 << 9);
    current_task = ntask;
    write_cr3(ntask->cr3);
    //if (current_task->usermode == true) log.debug("%u rsp: 0x%lx\n", current_task->pid, current_task->regs.rsp);
    //log.debug("Switched to task %s, pid: %d, RIP: 0x%lx\n", current_task->name, current_task->pid, current_task->regs.rip);
}
void vmm_map_range2(pagemap *pgm, uint64_t start, uint64_t phys,size_t count, uint64_t flags=PTE_PRESENT) {
    uint64_t start2 = ALIGN_DOWN(start, 4096);
    uint64_t end = ALIGN_UP(start+count, 4096);
    size_t pages = (end/4096)-(start/4096);
    pages += 1;
    //printf("start: 0x%lx, end: 0x%lx, pages to map: %lu\n", start2, end, pages);
    for (size_t i=0;i<pages;i++) {
        vmm_map_page(pgm, start2+(i*4096), phys+(i*4096), flags);
        vmm_map_page(pgm, (start2+(i*4096))+VMM_HIGHER_HALF, phys+(i*4096), flags);
        //printf("map: 0x%lx -> 0x%lx\n", start2+(i*4096), start2+(i*4096));
    }
}
struct pagemap *vmm_fork_pagemap(struct pagemap *pagemap);
int sched_fork(idt_regs *regs) {
    task_t *current_task2 = current_task;
    if (!current_task->fork_parent) {
        _sched_stop_internal = true;
        task *new_task = new task;
        memcpy(new_task, current_task, sizeof(task));
        new_task->fork_parent = current_task;
        new_task->pid = next_pid;
        next_pid++;
        memcpy(&new_task->regs, regs, sizeof(idt_regs));
        printf("new_task->regs.rip: 0x%lx\nregs->rip: 0x%lx\n", new_task->regs.rip, regs->rip);
        new_task->regs.ss = (10*8) | 3;
        new_task->regs.es = new_task->regs.ds = (10*8) | 3;
        new_task->regs.cs = (9*8) | 3;
        //new_task->regs.rip = new_task->regs.rcx;
        new_task->pgm = vmm_fork_pagemap(current_task2->pgm);
        uint64_t stack = (uint64_t)new_task->stack_addr;
        printf("stack: 0x%lx\n", stack);
        task_t *task_p = root_task;
        do {
            task_p = task_p->next;
        } while(task_p->last_task != true); 
        task_p->last_task = false;
        new_task->state = TASK_READY;
        task_p->next = new_task; 
        new_task->last_task = true;
        new_task->next = root_task;
        new_task->regs.rax = new_task->regs.rbx = 0;
        new_task->stack_addr = pmm_alloc(STACK_SIZE);
        for (int i=256;i<512;i++) {
            new_task->pgm->top_level[i] = current_task2->pgm->top_level[i];
        }
        memcpy(new_task->stack_addr+VMM_HIGHER_HALF, current_task->stack_addr+VMM_HIGHER_HALF, STACK_SIZE);
        vmm_map_range2(new_task->pgm, (current_task->stack_addr), (new_task->stack_addr), STACK_SIZE, PTE_USER | PTE_WRITABLE);
        vmm_map_range(new_task->pgm, (new_task->stack_addr), STACK_SIZE, PTE_USER | PTE_WRITABLE);
        new_task->cr3 = (uint64_t)((void *)new_task->pgm->top_level - VMM_HIGHER_HALF);
        printf("cr3[256]=0x%lx\n", new_task->pgm->top_level[256]);
        printf("new_task->cr3=0x%lx\n", new_task->cr3);
        _sched_stop_internal = false;
        return new_task->pid;
    }
    return 0;
}