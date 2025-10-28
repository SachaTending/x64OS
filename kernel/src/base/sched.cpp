#include <sched/sched.hpp>
#include <logging.hpp>
#include <libc.h>
#include <arch/sched.hpp>
#include <krnl.hpp>

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

void Scheduler::CreateThread(const char *name, void (*entry)(), bool usermode=false, pagemap *pgm) {
    sched_run++;
    thread_t *thr = new thread_t;
    thr->name = strdup(name);
    thr->pid = allocate_pid();
    thr->initial_stack = (uint64_t)new char[STACK_SIZE];
    thr->pgm = pgm;
    Arch::Scheduler::SetupSchedState(&thr->cpu_state, usermode, (uint64_t)entry, thr->initial_stack+STACK_SIZE);
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
    sched_run--;
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
    thread_t *prev = thr->prev_thread;
    thread_t *next = thr->next_thread;

    prev->next_thread = next;
    next->prev_thread = prev;
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
    Scheduler::CreateThread("TEST2", test2);
    Scheduler::CreateThread("TEST3", test3);

    Scheduler::CreateThread("IDK", test4);
    Scheduler::CreateThread("IDK2", test5, true);
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
    if (sched_run >= 1) return;
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
    //if (regs->rip < VMM_HIGHER_HALF) printf("rip: 0x%lx\n", regs->rip);
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