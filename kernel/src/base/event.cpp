#include <event.h>
#include <sched/sched.hpp>
#include <arch/arch.hpp>
#include <krnl.hpp>

static ssize_t check_for_pending(struct event **events, size_t num_events) {
    for (size_t i = 0; i < num_events; i++) {
        if (events[i]->pending > 0) {
            events[i]->pending--;
            return i;
        }
    }
    return -1;
}

static void attach_listeners(struct event **events, size_t num_events, struct thread *thread) {
    thread->attached_events_i = 0;

    for (size_t i = 0; i < num_events; i++) {
        struct event *event = events[i];
        if (event->listeners_i == EVENT_MAX_LISTENERS) {
            //panic(NULL, true, "Event listeners exhausted");
            PANIC("Event listeners exhausted");
        }

        struct event_listener *listener = &event->listeners[event->listeners_i++];
        listener->thread = thread;
        listener->which = i;

        if (thread->attached_events_i == MAX_EVENTS) {
            //panic(NULL, true, "Listening on too many events");
            PANIC("Listening on too many events");
        }

        thread->attached_events[thread->attached_events_i++] = event;
    }
}

static void detach_listeners(struct thread *thread) {
    for (size_t i = 0; i < thread->attached_events_i; i++) {
        struct event *event = thread->attached_events[i];

        for (size_t j = 0; j < event->listeners_i; j++) {
            struct event_listener *listener = &event->listeners[j];
            if (listener->thread != thread) {
                continue;
            }

            event->listeners[j] = event->listeners[--event->listeners_i];
            break;
        }
    }

    thread->attached_events_i = 0;
}

static void lock_events(struct event **events, size_t num_events) {
    for (size_t i = 0; i < num_events; i++) {
        spinlock_acquire(&events[i]->lock);
    }
}

static void unlock_events(struct event **events, size_t num_events) {
    for (size_t i = 0; i < num_events; i++) {
        spinlock_release(&events[i]->lock);
    }
}

size_t event_await(struct event **events, size_t num_events, bool block) {
    ssize_t ret = -1;

    struct thread *thread = Scheduler::GetCurrentThread();

    bool old_ints = interrupt_toggle(false);
    lock_events(events, num_events);

    size_t i = check_for_pending(events, num_events);
    if (i != -1) {
        ret = i;
        unlock_events(events, num_events);
        goto cleanup;
    }

    if (!block) {
        unlock_events(events, num_events);
        goto cleanup;
    }

    attach_listeners(events, num_events, thread);
    //sched_dequeue_thread(thread);
    unlock_events(events, num_events);
    Scheduler::Yield(true);

    interrupt_toggle(false);

    if (thread->enqueued_by_signal) {
        goto cleanup2;
    }

    ret = thread->which_event;

cleanup2:
    lock_events(events, num_events);
    detach_listeners(thread);
    unlock_events(events, num_events);

cleanup:
    interrupt_toggle(old_ints);
    return ret;
}


size_t event_trigger(struct event *event, bool drop) {
    bool old_state = interrupt_toggle(false);

    spinlock_acquire(&event->lock);

    size_t ret = 0;
    if (event->listeners_i == 0) {
        if (!drop) {
            event->pending++;
        }
        ret = 0;
        goto cleanup;
    }

    for (size_t i = 0; i < event->listeners_i; i++) {
        struct event_listener *listener = &event->listeners[i];
        struct thread *thread = listener->thread;

        thread->which_event = listener->which;
        //sched_enqueue_thread(thread, false);
    }

    ret = event->listeners_i;
    event->listeners_i = 0;

cleanup:
    spinlock_release(&event->lock);
    interrupt_toggle(old_state);
    return ret;
}
