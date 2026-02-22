#include <sched/sched.hpp>
#include <logging.hpp>
#include <libc.h>
#include <arch/sched.hpp>
#include <krnl.hpp>
#include <prg_loading.hpp>
#include <elf.h>

#define STACK_SIZE 128*1024

static Logger log("Scheduler");

using namespace Scheduler;

thread_t *root_thread;
thread_t *current_thread;

// How does this variable work: Each time when kernel stop schedulers, 1 adds to this variable, when kernel starts scheduler, 1 is substracted from this variable
static char sched_run = 0;

static int next_pid;

static int allocate_pid() {
    int out = next_pid;
    next_pid++;
    return out;
}
int force_openat(int dir_fdnum, const char *path, int flags, int mode, thread_t *thread);
int fdnum_create_from_resource(thread_t *proc, struct resource *res, int flags,
                               int old_fdnum, bool specific);
void Scheduler::CreateThread(const char *name, void (*entry)(), bool usermode, pagemap *pgm, const char **argv, const char **envp, auxval *aux) {
    //sched_run++;
    Stop();
    //thread_t *thr = new thread_t;
    thread_t *thr = (thread_t*)((uint64_t)pmm_alloc(4)+VMM_HIGHER_HALF);
    memset(thr, 0, 4*4096);
    thr->name = strdup(name);
    thr->pid = allocate_pid();
    thr->mmap_anon_base = 0x80000000000;
    thr->initial_stack = (uint64_t)pmm_alloc(STACK_SIZE/PAGE_SIZE);
    thr->syscall = SYSCALL_SET_LINUX;
    memset((void *)thr->initial_stack, 0, STACK_SIZE);
    thr->pgm = pgm;
    thr->cwd = vfs_root;
    if (usermode) vmm_map_range_no_krnl_map(thr->pgm, thr->initial_stack, STACK_SIZE, PTE_PRESENT | PTE_USER | PTE_WRITABLE);
    else vmm_map_range(thr->pgm, thr->initial_stack, STACK_SIZE, PTE_PRESENT | PTE_USER | PTE_WRITABLE);
    Arch::Scheduler::SetupSchedState(&thr->cpu_state, usermode, (uint64_t)entry, thr->initial_stack+STACK_SIZE);
    if (argv != 0) {
        uintptr_t *stack = (uintptr_t *)(thr->initial_stack+STACK_SIZE);
        uintptr_t old_rsp = (uintptr_t)stack;
        void *stack_top = (void *)(thr->initial_stack);

        //log.info("stack: 0x%lx\n", stack);
        //log.info("stack_top: 0x%lx\n", stack_top);
        //log.info("old_rsp: 0x%lx\n", old_rsp);
        int envp_len;
        for (envp_len = 0; envp[envp_len] != NULL; envp_len++) {
            size_t length = strlen(envp[envp_len]);
            stack = (uintptr_t *)((uint64_t)stack - length - 1);
            memcpy(stack, envp[envp_len], length+1);
            //printf("0x%lx: %s, strlen: %d\n", stack, stack, strlen(envp[envp_len]));
        }

        log.info("stack: 0x%lx\n", stack);
        int argv_len;
        //printf("ARGV WRITE START\n");
        for (argv_len = 0; argv[argv_len] != NULL; argv_len++) {
            size_t length = strlen(argv[argv_len]);
            stack = (uintptr_t *)((uint64_t)stack - length - 1);
            memcpy(stack, argv[argv_len], length);
            //printf("0x%lx: %s\n", stack, stack);
        }

        //log.info("stack: 0x%lx\n", stack);
        stack = (uintptr_t *)ALIGN_DOWN((uintptr_t)stack, 16);
        if (((argv_len + envp_len + 1) & 1) != 0) {
            stack--;
        }

        //log.info("stack: 0x%lx\n", stack);

        // Auxilary vector
        *(--stack) = 0, *(--stack) = 0;
        stack -= 2; stack[0] = AT_SECURE, stack[1] = 0;
        stack -= 2; stack[0] = AT_ENTRY, stack[1] = aux->at_entry;
        stack -= 2; stack[0] = AT_PHDR,  stack[1] = aux->at_phdr;
        stack -= 2; stack[0] = AT_PHENT, stack[1] = aux->at_phent;
        stack -= 2; stack[0] = AT_PHNUM, stack[1] = aux->at_phnum;

        // Environment variables
        *(--stack) = 0;
        stack -= envp_len;
        for (int i = 0; i < envp_len; i++) {
            old_rsp -= strlen(envp[i]) + 1;
            stack[i] = old_rsp;
            //printf("wrote %d envp addr 0x%lx, envp[%d]=%s\n", i, old_rsp, i, envp[i]);
        }

        // Arguments
        *(--stack) = 0;
        stack -= argv_len;
        for (int i = 0; i < argv_len; i++) {
            old_rsp -= strlen(argv[i]) + 1;
            //log.info("0x%lx\n", old_rsp);
            //printf("wrote %d argv addr 0x%lx\n", i, old_rsp);
            stack[i] = old_rsp;
        }

        *(--stack) = argv_len;
        //printf("old stack: 0x%lx\n", thr->cpu_state.regs.rsp);
        //thr->cpu_state.regs.rsp -= ((uint64_t)stack) - ((uint64_t)stack_top);
        thr->cpu_state.regs.rsp = ((uint64_t)stack);
        //printf("new stack: 0x%lx\n", thr->cpu_state.regs.rsp);
        //force_openat(AT_FDCWD, "/dev/console", O_RDWR, 30777, thr); // populate fd with console
        vfs_node *console_node = VFS::GetNode(vfs_root, "/dev/console", true);
        fdnum_create_from_resource(thr, console_node->resource, 0, 0, true);
        fdnum_create_from_resource(thr, console_node->resource, 0, 1, true);
        fdnum_create_from_resource(thr, console_node->resource, 0, 2, true);
        thr->cpu_state.regs.rsp += VMM_HIGHER_HALF;
    }
    // Find latest thread
    thread_t *tmp = root_thread;
    while (tmp->next_thread != root_thread) {
        tmp = tmp->next_thread;
    }
    log.debug("last thread: %s\n", tmp->name);
    // Add thread to list
    thr->next_thread = tmp->next_thread;
    tmp->next_thread = thr;
    thr->prev_thread = tmp;
    root_thread->prev_thread = thr;
    Start();
}

thread_t *find_thread_by_pid(int pid) {
    thread_t *tmp = root_thread;
    while (tmp->next_thread != root_thread) {
        if (tmp->pid == pid) return tmp;
        tmp = tmp->next_thread;
    }
    return 0;
}

void remove_and_destroy_thread(int pid) {
    log.debug("attemptingt to kill pid %d\n", pid);
    sched_run++;
    thread_t *thr = find_thread_by_pid(pid);
    if (thr == 0) {
        log.debug("tried to kill pid %d but it's not found\n", pid);
        sched_run--;
        return;
    }
    if (root_thread->prev_thread == thr) root_thread->prev_thread = thr->prev_thread;
    thread_t *prev = thr->prev_thread;
    thread_t *next = thr->next_thread;

    prev->next_thread = next;
    next->prev_thread = prev;
    sched_run--;
}
void sched_tick(cpu_ctx *regs, void *_);

void test1() {
    printf("Thread1 started.\n");
    while (1);
    while (1) printf("i'm thr1\n");
}

void test2() {
    printf("Thread2 started.\n");
    while (1);
    while (1) printf("i'm thr2\n");
}

void test3() {
    printf("Thread3 started.\n");
    while (1);
    while (1) printf("i'm thr3\n");
}

void test4() {
    printf("Thread4 started.\n");
    while (1);
    while (1) printf("i'm thr4\n");
}

void test5() {
    //printf("Thread5 started.\n");

	asm volatile("syscall"
	             : 
	             : "a"(512), "D"("Hello, world! This syscall is made from kernel itself, code running in usermode\n")
	             : "memory", "rcx", "r11");
    asm volatile("syscall"
	         : 
	         : "a"(512), "D"("я забыл то что ядро может выводить utf-8 текст, ну вобщем теперь надо добавлять vfs, tmpfs и распаковку initrd\n")
	         : "memory", "rcx", "r11");
    while (1);
    while (1) printf("i'm thr5\n");
}

void TEST_init_sched() {
    thread_t *root = new thread_t;
    root->name = "TEST1";
    root->pid = allocate_pid();
    root->initial_stack = (uint64_t)new char[STACK_SIZE];
    Arch::Scheduler::SetupSchedState(&root->cpu_state, false, (uint64_t)test1, root->initial_stack);
    root_thread = root;
    current_thread = root;
    root_thread->prev_thread = root_thread;
    root_thread->next_thread = root_thread;
    Scheduler::CreateThread("TEST2", test2, false, krnl_page);
    Scheduler::CreateThread("TEST3", test3, false, krnl_page);

    Scheduler::CreateThread("IDK", test4, false, krnl_page);
    Scheduler::CreateThread("IDK2", test5, true, krnl_page);
    //remove_and_destroy_thread(2);
    Kernel::RegisterInterruptHandler(TIMER_INTERRUPT, sched_tick, 0);
    //log.info("Scheduler has been started.\n");
}

void idle() {
    for(;;) asm volatile ("hlt");
}

void Scheduler::Init() {
    thread_t *root = new thread_t;
    root->name = "Idle";
    root->pid = allocate_pid();
    root->initial_stack = (uint64_t)new char[STACK_SIZE];
    Arch::Scheduler::SetupSchedState(&root->cpu_state, false, (uint64_t)idle, root->initial_stack);
    root_thread = root;
    current_thread = root;
    root_thread->prev_thread = root_thread;
    root_thread->next_thread = root_thread;
    root_thread->pgm = krnl_page;
    sched_run++;
    Kernel::RegisterInterruptHandler(TIMER_INTERRUPT, sched_tick, 0);
}

void Scheduler::Start() {
    sched_run--;
}

extern size_t global_ticks;
static bool just_started = true;
void sched_tick(cpu_ctx *regs, void *_) {
    if (sched_run > 0) return;
    //log.debug("sched_run: %d\n", sched_run);
    //log.info("Current thread: %s\n", current_thread->name);
    //log.info("%s(%d) [ %s(%d) ] %s(%d)\n", current_thread->prev_thread->name, current_thread->prev_thread->pid, current_thread->name, current_thread->pid, current_thread->next_thread->name, current_thread->next_thread->pid);
    if (just_started) {
        just_started = false;
        current_thread = current_thread->prev_thread; // HACK: Without this only the next thread gets executed. This hack allows to execute first thread
    }
    else Arch::Scheduler::SaveState(regs, &current_thread->cpu_state);
    current_thread->state = STATE_SLEEP;
    current_thread = current_thread->next_thread;
    if (current_thread->state == STATE_STALLED or current_thread->state == STATE_ZOMBIE) {
        while (1)  {
            //log.info("%s's state: %d\n", current_thread->name, current_thread->state);
            if (current_thread->state == STATE_STALLED) {
                current_thread = current_thread->next_thread;
                continue;
            } else if (current_thread->state == STATE_ZOMBIE) {
                int pid_to_kill = current_thread->pid;
                current_thread = current_thread->next_thread;
                remove_and_destroy_thread(pid_to_kill);
                continue;
            }
            break;
        }
    }
    Arch::Scheduler::LoadState(regs, &current_thread->cpu_state);
    //if (regs->rip < VMM_HIGHER_HALF) log.debug("rip: 0x%lx\n", regs->rip);
    current_thread->state = STATE_RUNNING;
    global_ticks++;
    vmm_switch_to(current_thread->pgm);
}

void Scheduler::Stop() {
    sched_run++;
}

thread_t *Scheduler::GetCurrentThread() {
    return current_thread;
}

int syscall_exec_PROTO(const char *path, const char **argv, const char **envp) {
    Scheduler::Stop();
    printf("exec(%s, 0x%lx, 0x%lx)\n", path, argv, envp);
    thread_t *c = current_thread;

    struct pagemap *new_pagemap = vmm_new_pagemap();
    struct auxval auxv, ld_auxv;
    const char *ld_path = 0;
    uint64_t entry;

    struct vfs_node *ld_node;

    struct vfs_node *node = VFS::GetNode(c->cwd, path, true);
    if (node == NULL || !elf_load(new_pagemap, node->resource, 0x0, &auxv, &ld_path)) {
        goto fail;
    }

    if (ld_path) {
        printf("exec(%s, 0x%lx, 0x%lx): ld: %s\n", path, argv, envp, ld_path);
        ld_node = VFS::GetNode(vfs_root, ld_path, true);
        if (ld_node == NULL || !elf_load(new_pagemap, ld_node->resource, 0x40000000, &ld_auxv, NULL)) {
            printf("no ld\n");
            goto fail;
        }
    }

    c->pgm = new_pagemap;
    c->mmap_anon_base = 0x80000000000;
    printf("initial_stack: 0x%lx\n", c->initial_stack);
    memset((void *)c->initial_stack, 0, STACK_SIZE);

    entry = ld_path == NULL ? auxv.at_entry : ld_auxv.at_entry;

    Arch::Scheduler::SetupSchedState(&c->cpu_state, true, entry, c->initial_stack);
    printf("entry: 0x%lx\n", entry);

    c->syscall = SYSCALL_SET_LINUX;

    Scheduler::Start();
    return 0;
    fail:
    Scheduler::Start();
    return -1;
}