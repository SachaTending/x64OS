#include <arch/vmm.h>
#include <logging.hpp>

static Logger log("VMM");
bool p = false;

#define INVLPG(addr) asm volatile("invlpg (%0)" :: "r"(addr) : "memory");

extern "C" {
    void *pmm_alloc(size_t pages);
    uint64_t *get_next_level(uint64_t *top_level, size_t idx, bool allocate);
    struct pagemap *vmm_new_pagemap(void) {
        struct pagemap *pagemap = new struct pagemap;
        if (pagemap == NULL) {
            //errno = ENOMEM;
            goto cleanup;
        }

        pagemap->lock = (spinlock_t)SPINLOCK_INIT;
        pagemap->top_level = (uint64_t *)pmm_alloc(1);
        pagemap->mmap_ranges.clear();
        if (pagemap->top_level == NULL) {
            //errno = ENOMEM;
            goto cleanup;
        }

        pagemap->top_level = (uint64_t *)((void *)pagemap->top_level + VMM_HIGHER_HALF);
        memset((void *)pagemap->top_level, 0, 4096);
        if (krnl_page != 0) {
            for (size_t i = 256; i < 512; i++) {
                pagemap->top_level[i] = krnl_page->top_level[i];
            }
        }
        vmm_map_page(pagemap, 0x00000000037fc024, 0x00000000037fc024, PTE_PRESENT | PTE_WRITABLE);
        vmm_map_page(pagemap, 0x00000000037fd024, 0x00000000037fd024, PTE_WRITABLE | PTE_PRESENT);
        return pagemap;

    cleanup:
        if (pagemap != NULL) {
            free(pagemap);
        }

        return NULL;
    }

    bool vmm_map_page(struct pagemap *pagemap, uintptr_t virt, uintptr_t phys, uint64_t flags) {
        INVLPG(virt);
        if (pagemap == NULL) return false;
        spinlock_acquire(&(pagemap->lock));
        flags |= PTE_USER;

        bool ok = false;
        size_t pml4_entry = (virt & (0x1ffull << 39)) >> 39;
        size_t pml3_entry = (virt & (0x1ffull << 30)) >> 30;
        size_t pml2_entry = (virt & (0x1ffull << 21)) >> 21;
        size_t pml1_entry = (virt & (0x1ffull << 12)) >> 12;

        uint64_t *pml4 = pagemap->top_level;
        if ((uint64_t)pml4 < VMM_HIGHER_HALF) pml4 += VMM_HIGHER_HALF;
        uint64_t *pml3 = 0;
        uint64_t *pml2 = 0;
        uint64_t *pml1 = 0;
        pml3 = get_next_level(pml4, pml4_entry, true);
        if (pml3 == NULL) {
            log.error("Failed to get pml3\n");
            goto cleanup;
        }
        //log.debug("got pml3 %lp ", pml3);
        //debug("got pml3 %lp ", pml3);
        pml2 = get_next_level(pml3, pml3_entry, true);
        if (pml2 == NULL) {
            log.error("Failed to get pml2\n");
            goto cleanup;
        }
        pml1 = get_next_level(pml2, pml2_entry, true);
        if (pml1 == NULL) {
            log.error("Failed to get pml1\n");
            goto cleanup;
        }

        if ((pml1[pml1_entry] & PTE_PRESENT) != 0) {
            //if (p) log.error("Entry for addr 0x%lx already present.\n", virt);
            //log.debug("Entry for addr 0x%lx already present.\n", virt);
            //goto cleanup;
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
        return PTE_GET_ADDR(*pte) + virt % 4096;
    }
    void vmm_map_range(pagemap *pgm, uint64_t start, size_t count, uint64_t flags) {
        uint64_t start2 = ALIGN_DOWN(start, 4096);
        uint64_t end = ALIGN_UP(start+count, 4096);
        size_t pages = (end/4096)-(start/4096);
        pages += 1;
        printf("start: 0x%lx, end: 0x%lx, pages to map: %lu\n", start2, end, pages);
        for (size_t i=0;i<pages;i++) {
            vmm_map_page(pgm, start2+(i*4096), start2+(i*4096), flags);
            vmm_map_page(pgm, (start2+(i*4096))+VMM_HIGHER_HALF, start2+(i*4096), flags);
            //printf("map: 0x%lx -> 0x%lx\n", start2+(i*4096), start2+(i*4096));
        }
    }

    void vmm_map_range_no_krnl_map(pagemap *pgm, uint64_t start, size_t count, uint64_t flags) {
        uint64_t start2 = ALIGN_DOWN(start, 4096);
        uint64_t end = ALIGN_UP(start+count, 4096);
        size_t pages = (end/4096)-(start/4096);
        pages += 1;
        printf("start: 0x%lx, end: 0x%lx, pages to map: %lu\n", start2, end, pages);
        for (size_t i=0;i<pages;i++) {
            vmm_map_page(pgm, start2+(i*4096), start2+(i*4096), flags);
            //printf("map: 0x%lx -> 0x%lx\n", start2+(i*4096), start2+(i*4096));
        }
    }
    void vmm_switch_to(struct pagemap *pagemap) {
        asm volatile (
            "mov %0, %%cr3"
            :
            : "r" ((void *)((uint64_t)pagemap->top_level - VMM_HIGHER_HALF))
            : "memory"
        );
    }

    bool vmm_unmap_page(struct pagemap *pagemap, uintptr_t virt, bool already_locked) {
        if (!already_locked) {
            spinlock_acquire(&pagemap->lock);
        }

        bool ok = false;
        size_t pml4_entry = (virt & (0x1ffull << 39)) >> 39;
        size_t pml3_entry = (virt & (0x1ffull << 30)) >> 30;
        size_t pml2_entry = (virt & (0x1ffull << 21)) >> 21;
        size_t pml1_entry = (virt & (0x1ffull << 12)) >> 12;

        uint64_t *pml4 = pagemap->top_level;
        uint64_t *pml3 = get_next_level(pml4, pml4_entry, false);
        uint64_t *pml2;
        uint64_t *pml1;
        if (pml3 == NULL) {
            goto cleanup;
        }
        pml2 = get_next_level(pml3, pml3_entry, false);
        if (pml2 == NULL) {
            goto cleanup;
        }
        pml1 = get_next_level(pml2, pml2_entry, false);
        if (pml1 == NULL) {
            goto cleanup;
        }

        if ((pml1[pml1_entry] & PTE_PRESENT) == 0) {
            //errno = EINVAL;
            goto cleanup;
        }

        ok = true;
        pml1[pml1_entry] = 0;

        asm volatile (
            "invlpg (%0)"
            :
            : "r" (virt)
            : "memory"
        );

    cleanup:
        if (!already_locked) {
            spinlock_release(&pagemap->lock);
        }
        return ok;
    }
}