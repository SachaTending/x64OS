#pragma once
#include <vec.h>
#include <stdint.h>
#include <spinlock.h>
#include <stdbool.h>
// Source code was taken from lyre os project, not wrote by me(TendingStream73)
struct pagemap {
    spinlock_t lock;
    uint64_t *top_level;
    VECTOR_TYPE(struct mmap_range_local *) mmap_ranges;
};

#ifdef __cplusplus
extern "C" {
#endif

bool vmm_map_page(struct pagemap *pagemap, uintptr_t virt, uintptr_t phys, uint64_t flags);

void vmm_switch_to(struct pagemap *pagemap);

#define PTE_PRESENT (1ull << 0ull)
#define PTE_WRITABLE (1ull << 1ull)
#define PTE_USER (1ull << 2ull)

#ifdef __cplusplus
}
#endif