#pragma once
#include <mmap.h>
#include <stdint.h>
#include <spinlock.h>
#include <stdbool.h>
#define PAGE_SIZE 4096
typedef frg::vector<struct mmap_range_local *, frg::stl_allocator> mmap_ranges_t;
// Source code was taken from lyre os project, but modified by me(TendingStream73)
struct pagemap {
    spinlock_t lock;
    uint64_t *top_level;
    mmap_ranges_t mmap_ranges;
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

void vmm_map_range(pagemap *pgm, uint64_t start, size_t count, uint64_t flags=PTE_PRESENT);
struct pagemap *vmm_new_pagemap(void);