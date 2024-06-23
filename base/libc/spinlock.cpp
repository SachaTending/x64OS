#include <libc.h>
#include <spinlock.h>

extern "C" {
    __attribute__((noinline)) void spinlock_acquire(spinlock_t *lock) {
        if (spinlock_test_and_acq(lock)) return;
        volatile size_t deadlock_counter = 0;
        for (;;) {
            if (spinlock_test_and_acq(lock)) {
                break;
            }
            if (++deadlock_counter >= 100000000) {
                goto deadlock;
            }
    #if defined (__x86_64__)
            asm volatile ("pause");
    #endif
        }
        lock->last_acquirer = __builtin_return_address(0);
        return;

    deadlock:
        PANIC("spinlock: Deadlock occurred at 0x%lx on lock 0x%lx whose last acquirer was 0x%lx", __builtin_return_address(0), lock, lock->last_acquirer);
    }

    __attribute__((noinline)) void spinlock_acquire_no_dead_check(spinlock_t *lock) {
        for (;;) {
            if (spinlock_test_and_acq(lock)) {
                break;
            }
    #if defined (__x86_64__)
            asm volatile ("pause");
    #endif
        }
        lock->last_acquirer = __builtin_return_address(0);
    }
}