#pragma once
#include <arch/sched.hpp>

enum thread_state {
    STATE_NEW, // Thread is just created
    STATE_RUNNING, // Thread is currently running
    STATE_SLEEP, // Thread is currently sleeping and waiting for new context switch
    STATE_STALLED, // Thread is currently stalled, so it won't be running again
    STATE_ZOMBIE, // Thread is currently shutting down
};

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
    } thread_t;
} // namespace Scheduler
