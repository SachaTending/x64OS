#include <libc.h>
#include <vmm.h>
#include <limine.h>

extern limine_hhdm_request hhdm2;

#define debug(a, ...) printf(a, __VA_ARGS__)

#define PAGE_SIZE 4096

#define PTE_PRESENT (1ull << 0ull)
#define PTE_WRITABLE (1ull << 1ull)
#define PTE_USER (1ull << 2ull)
#define PTE_NX (1ull << 63ull)

#define PTE_ADDR_MASK 0x000ffffffffff000
#define PTE_GET_ADDR(VALUE) ((VALUE) & PTE_ADDR_MASK)

#define VMM_HIGHER_HALF hhdm2.response->offset

extern "C" {
    void *pmm_alloc(size_t pages);
    uint64_t *get_next_level(uint64_t *top_level, size_t idx, bool allocate);
    bool vmm_map_page(struct pagemap *pagemap, uintptr_t virt, uintptr_t phys, uint64_t flags) {
        spinlock_acquire(&pagemap->lock);

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
        if ((top_level[idx] & PTE_PRESENT) != 0) {
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
}