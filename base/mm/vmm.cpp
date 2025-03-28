#include <libc.h>
#include <vmm.h>
#include <limine.h>
#include <logging.hpp>

static Logger log("VMM");

extern pagemap *krnl_page;

extern limine_hhdm_request hhdm2;

//#define debug(a, ...) printf(a, __VA_ARGS__)

#define PAGE_SIZE 4096

#define PTE_PRESENT (1ull << 0ull)
#define PTE_WRITABLE (1ull << 1ull)
#define PTE_USER (1ull << 2ull)
#define PTE_NX (1ull << 63ull)


#define INVALID_PHYS ((uint64_t)0xffffffffffffffff)

#define VMM_HIGHER_HALF hhdm2.response->offset
extern "C" uint64_t *get_next_level(uint64_t *top_level, size_t idx, bool allocate);
void vmm_map_range(pagemap *pgm, uint64_t start, size_t count, uint64_t flags=PTE_PRESENT) {
    uint64_t start2 = ALIGN_DOWN(start, 4096);
    uint64_t end = ALIGN_UP(start+count, 4096);
    size_t pages = (end/4096)-(start/4096);
    pages += 1;
    //printf("start: 0x%lx, end: 0x%lx, pages to map: %lu\n", start2, end, pages);
    for (size_t i=0;i<pages;i++) {
        vmm_map_page(pgm, start2+(i*4096), start2+(i*4096), flags);
        vmm_map_page(pgm, (start2+(i*4096))+VMM_HIGHER_HALF, start2+(i*4096), flags);
        //printf("map: 0x%lx -> 0x%lx\n", start2+(i*4096), start2+(i*4096));
    }
}


struct pagemap *vmm_new_pagemap(void) {
    struct pagemap *pagemap = new struct pagemap;
    if (pagemap == NULL) {
        //errno = ENOMEM;
        goto cleanup;
    }

    pagemap->lock = (spinlock_t)SPINLOCK_INIT;
    pagemap->top_level = pmm_alloc(1);
    if (pagemap->top_level == NULL) {
        //errno = ENOMEM;
        goto cleanup;
    }

    pagemap->top_level = (uint64_t)((void *)pagemap->top_level + VMM_HIGHER_HALF);
    if (krnl_page != 0) {
        for (size_t i = 256; i < 512; i++) {
            pagemap->top_level[i] = krnl_page->top_level[i];
        }
    }
    return pagemap;

cleanup:
    if (pagemap != NULL) {
        free(pagemap);
    }

    return NULL;
}

static void destroy_level(uint64_t *pml, size_t start, size_t end, int level) {
    if (level == 0) {
        return;
    }

    for (size_t i = start; i < end; i++) {
        uint64_t *next_level = get_next_level(pml, i, false);
        if (next_level == NULL) {
            continue;
        }

        destroy_level(next_level, 0, 512, level - 1);
    }

    pmm_free((void *)pml - VMM_HIGHER_HALF, 1);
}

void vmm_destroy_pagemap(struct pagemap *pagemap) {
    //spinlock_acquire(&pagemap->lock);

    while (pagemap->mmap_ranges.size() > 0) {
        struct mmap_range_local *local_range = pagemap->mmap_ranges[0];

        //munmap(pagemap, local_range->base, local_range->length);
    }

    spinlock_acquire(&pagemap->lock);

    destroy_level(pagemap->top_level, 0, 256, 4);
    free(pagemap);
}
#define MAP_SHARED	0x01
void *pmm_alloc_nozero(size_t pages);
struct pagemap *vmm_fork_pagemap(struct pagemap *pagemap) {
    spinlock_acquire(&pagemap->lock);

    struct pagemap *new_pagemap = vmm_new_pagemap();
    if (new_pagemap == NULL) {
        goto cleanup;
    }

    for (auto *it: pagemap->mmap_ranges) {
        struct mmap_range_local *local_range = it;
        struct mmap_range_global *global_range = local_range->global;

        struct mmap_range_local *new_local_range = new struct mmap_range_local;
        if (new_local_range == NULL) {
            goto cleanup;
        }

        *new_local_range = *local_range;
        // NOTE: Not present in vinix, in case of weird VMM bugs keep this line in mind :^)
        new_local_range->pagemap = new_pagemap;

        //if (global_range->res != NULL) {
        //    global_range->res->refcount++;
        //}

        if ((local_range->flags & MAP_SHARED) != 0) {
            global_range->locals.push_back(new_local_range);
            for (uintptr_t i = local_range->base; i < local_range->base + local_range->length; i += PAGE_SIZE) {
                uint64_t *old_pte = vmm_virt2pte(pagemap, i, false);
                if (old_pte == NULL) {
                    continue;
                }

                uint64_t *new_pte = vmm_virt2pte(new_pagemap, i, true);
                if (new_pte == NULL) {
                    goto cleanup;
                }
                *new_pte = *old_pte;
            }
        } else {
            struct mmap_range_global *new_global_range = new struct mmap_range_global;
            if (new_global_range == NULL) {
                goto cleanup;
            }

            new_global_range->shadow_pagemap = vmm_new_pagemap();
            if (new_global_range->shadow_pagemap == NULL) {
                goto cleanup;
            }

            new_global_range->base = global_range->base;
            new_global_range->length = global_range->length;
            //new_global_range->res = global_range->res;
            new_global_range->offset = global_range->offset;

            new_global_range->locals.push_back(new_local_range);

            if ((local_range->flags & MAP_ANONYMOUS) != 0) {
                for (uintptr_t i = local_range->base; i < local_range->base + local_range->length; i += PAGE_SIZE) {
                    uint64_t *old_pte = vmm_virt2pte(pagemap, i, false);
                    if (old_pte == NULL || (PTE_GET_FLAGS(*old_pte) & PTE_PRESENT) == 0) {
                        continue;
                    }

                    uint64_t *new_pte = vmm_virt2pte(new_pagemap, i, true);
                    if (new_pte == NULL) {
                        goto cleanup;
                    }

                    uint64_t *new_spte = vmm_virt2pte(new_global_range->shadow_pagemap, i, true);
                    if (new_spte == NULL) {
                        goto cleanup;
                    }

                    void *old_page = (void *)PTE_GET_ADDR(*old_pte);
                    void *page = pmm_alloc_nozero(1);
                    if (page == NULL) {
                        goto cleanup;
                    }

                    memcpy(page + VMM_HIGHER_HALF, old_page + VMM_HIGHER_HALF, PAGE_SIZE);
                    *new_pte = PTE_GET_FLAGS(*old_pte) | (uint64_t)page;
                    *new_spte = *new_pte;
                }
            } else {
                PANIC("Not an anonymous fork.");
            }
        }

        new_pagemap->mmap_ranges.push_back(new_local_range);
    }
    
    spinlock_release(&pagemap->lock);
    return new_pagemap;

cleanup:
    spinlock_release(&pagemap->lock);
    if (new_pagemap != NULL) {
        vmm_destroy_pagemap(new_pagemap);
    }
    return NULL;
}

extern "C" {
    void *pmm_alloc(size_t pages);
    uint64_t *get_next_level(uint64_t *top_level, size_t idx, bool allocate);
    bool vmm_map_page(struct pagemap *pagemap, uintptr_t virt, uintptr_t phys, uint64_t flags) {
        spinlock_acquire(&pagemap->lock);
        flags |= PTE_USER;

        bool ok = false;
        size_t pml4_entry = (virt & (0x1ffull << 39)) >> 39;
        size_t pml3_entry = (virt & (0x1ffull << 30)) >> 30;
        size_t pml2_entry = (virt & (0x1ffull << 21)) >> 21;
        size_t pml1_entry = (virt & (0x1ffull << 12)) >> 12;

        uint64_t *pml4 = pagemap->top_level;
        uint64_t *pml3 = 0;
        uint64_t *pml2 = 0;
        uint64_t *pml1 = 0;
        pml3 = get_next_level(pml4, pml4_entry, true);
        if (pml3 == NULL) {
            goto cleanup;
        }
        //debug("got pml3 %lp ", pml3);
        pml2 = get_next_level(pml3, pml3_entry, true);
        if (pml2 == NULL) {
            goto cleanup;
        }
        //debug("got pml2, ", 1);
        pml1 = get_next_level(pml2, pml2_entry, true);
        if (pml1 == NULL) {
            goto cleanup;
        }
        //debug("got pml1, ", 1);

        if ((pml1[pml1_entry] & PTE_PRESENT) != 0) {
            goto cleanup;
        }

        ok = true;
        //printf("map: ok, ");
        pml1[pml1_entry] = phys | flags;

    cleanup:
        spinlock_release(&pagemap->lock);
        return ok;
    }

    uint64_t *get_next_level(uint64_t *top_level, size_t idx, bool allocate) {
        if ((uint64_t)top_level < VMM_HIGHER_HALF)top_level += VMM_HIGHER_HALF;
        //debug("next lvl: 0x%016lx %u %01d, ", top_level, idx, allocate);
        //if (!allocate) log.debug("top_level: 0x%lx idx: %lu allocate: %d\n", top_level, idx, allocate);
        if ((top_level[idx] & PTE_PRESENT) != 0) {
            //if (!allocate) log.debug("%lu: 0x%lx\n", idx, (uint64_t *)(PTE_GET_ADDR(top_level[idx]) + VMM_HIGHER_HALF));
            return (uint64_t *)(PTE_GET_ADDR(top_level[idx]) + VMM_HIGHER_HALF);
        }

        if (!allocate) {
            return NULL;
        }
        //debug("allocate %u, ", idx);
        void *next_level = pmm_alloc(1);
        if (next_level == NULL) {
            return NULL;
        }

        top_level[idx] = (uint64_t)next_level | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
        return (uint64_t *)((uint64_t)next_level + VMM_HIGHER_HALF);
    }
    void vmm_switch_to(struct pagemap *pagemap) {
        asm volatile (
            "mov %0, %%cr3"
            :
            : "r" ((void *)((uint64_t)pagemap->top_level - VMM_HIGHER_HALF))
            : "memory"
        );
    }
    uint64_t *vmm_virt2pte(struct pagemap *pagemap, uintptr_t virt, bool allocate) {
        size_t pml4_entry = (virt & (0x1ffull << 39)) >> 39;
        size_t pml3_entry = (virt & (0x1ffull << 30)) >> 30;
        size_t pml2_entry = (virt & (0x1ffull << 21)) >> 21;
        size_t pml1_entry = (virt & (0x1ffull << 12)) >> 12;

        uint64_t *pml4 = pagemap->top_level;
        uint64_t *pml3 = get_next_level(pml4, pml4_entry, allocate);
        if (pml3 == NULL) {
            log.debug("pml3 == NULL\n");
            return NULL;
        }
        uint64_t *pml2 = get_next_level(pml3, pml3_entry, allocate);
        if (pml2 == NULL) {
            log.debug("pml2 == NULL\n");
            return NULL;
        }
        uint64_t *pml1 = get_next_level(pml2, pml2_entry, allocate);
        if (pml1 == NULL) {
            log.debug("pm1 == NULL\n");
            return NULL;
        }

        return &pml1[pml1_entry];
    }
    uintptr_t vmm_virt2phys(struct pagemap *pagemap, uintptr_t virt) {
        uint64_t *pte = vmm_virt2pte(pagemap, virt, false);
        //log.debug("pte: 0x%lx\n", pte);
        if (pte == NULL || (PTE_GET_FLAGS(*pte) & PTE_PRESENT) == 0) {
            log.debug("%d\n", pte == NULL || (PTE_GET_FLAGS(*pte) & PTE_PRESENT) == 0);
            return INVALID_PHYS;
        }
        //log.debug("0x%lx\n",  PTE_GET_ADDR(*pte));
        return PTE_GET_ADDR(*pte);
    }
}