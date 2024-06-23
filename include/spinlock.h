#pragma once
#include <stddef.h>

// Source code was taken from lyre os project, not wrote by me(TendingStream73)

#define CAS __sync_bool_compare_and_swap

typedef struct {
    int lock;
    void *last_acquirer;
} spinlock_t;

#define SPINLOCK_INIT {0, NULL}

static inline bool spinlock_test_and_acq(spinlock_t *lock) {
    if (lock->lock) return 0;
    lock->lock = 1;
    return 1;
}
#ifdef __cplusplus
extern "C" {
#endif
void spinlock_acquire(spinlock_t *lock);
void spinlock_acquire_no_dead_check(spinlock_t *lock);
#ifdef __cplusplus
}
#endif
static inline void spinlock_release(spinlock_t *lock) {
    lock->last_acquirer = NULL;
    lock->lock = 0;
}
