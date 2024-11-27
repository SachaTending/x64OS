#include <mmap.h>
#include <vmm.h>
#include <vfs.hpp>
#include <libc.h>
#include <sched.hpp>
#include <idt.hpp>

void *mmap(struct pagemap *pagemap, uintptr_t addr, size_t length, int prot,
           int flags, vfs_node_t *node, size_t offset) {
    stop_sched();
    struct mmap_range_global *global_range = NULL;
    struct mmap_range_local *local_range = NULL;

    length = ALIGN_UP(length, PAGE_SIZE);

    task_t *task = get_current_task();

    uint64_t base = 0;
    if ((flags & MAP_FIXED) != 0) {
        // Not supported.
        PANIC("Tried to mmap with MAP_FIXED, but this is not implemented.\n");
        //if (!munmap(pagemap, addr, length)) {
        //    goto cleanup;
        //}
        base = addr;
    } else {
        base = task->mmap_anon_base;
        task->mmap_anon_base += length + PAGE_SIZE;
    }
    global_range = new mmap_range_global;

    global_range->shadow_pagemap = vmm_new_pagemap();

    global_range->base = base;
    global_range->length = length;
    global_range->node = node;
    global_range->offset = offset;

    local_range = new struct mmap_range_local;

    local_range->pagemap = pagemap;
    local_range->global = global_range;
    local_range->base = base;
    local_range->length = length;
    local_range->prot = prot;
    local_range->flags = flags;
    local_range->offset = offset;

    global_range->locals.push_back(local_range);

    spinlock_acquire(&pagemap->lock);

    pagemap->mmap_ranges.push_back(local_range);

    spinlock_release(&pagemap->lock);

    start_sched();
    return (void *)base;
}

struct addr2range {
    struct mmap_range_local *range;
    size_t memory_page;
    size_t file_page;
};

struct addr2range addr2range(struct pagemap *pagemap, uintptr_t virt) {
    for (size_t i=0;i<pagemap->mmap_ranges.size();i++) {
        struct mmap_range_local *local_range = pagemap->mmap_ranges[i];
        if (virt < local_range->base || virt >= local_range->base + local_range->length) {
            continue;
        }
        
        size_t memory_page = virt / PAGE_SIZE;
        size_t file_page = local_range->offset / PAGE_SIZE + (memory_page - local_range->base / PAGE_SIZE);
        return (struct addr2range){.range = local_range, .memory_page = memory_page, .file_page = file_page};
    };
    return (struct addr2range){.range = NULL, .memory_page = 0, .file_page = 0};
}

bool mmap_page_in_range(struct mmap_range_global *global, uintptr_t virt,
                            uintptr_t phys, int prot) {
    uint64_t pt_flags = PTE_PRESENT | PTE_USER;

    if ((prot & PROT_WRITE) != 0) {
        pt_flags |= PTE_WRITABLE;
    }
    if ((prot & PROT_EXEC) == 0) {
        // i forgot to add NX.
    }

    if (!vmm_map_page(global->shadow_pagemap, virt, phys, pt_flags)) {
        return false;
    }
    return true;
    for (size_t i=0;i<global->locals.size();i++) {
        struct mmap_range_local *local_range = global->locals[i];
        if (virt < local_range->base || virt >= local_range->base + local_range->length) {
            continue;
        }

        if (!vmm_map_page(local_range->pagemap, virt, phys, pt_flags)) {
            return false;
        }
    };

    return true;
}

bool mmap_pf(idt_regs *regs) {
    if ((regs->ErrorCode & 0x1) != 0) {
        return false;
    }
    uint64_t cr2 = regs->cr2;
    pagemap *pgm = get_current_task()->pgm;
    spinlock_acquire(&pgm->lock);

    struct addr2range range = addr2range(pgm, cr2);
    struct mmap_range_local *local_range = range.range;

    spinlock_release(&pgm->lock);

    if (local_range == NULL) {
        return false;
    }

    void *page = NULL;
    if ((local_range->flags & MAP_ANONYMOUS) != 0) {
        page = pmm_alloc(1);
    } else {
        //struct resource *res = page = local_range->global->res;
        //page = res->mmap(res, range.file_page, local_range->flags);
        PANIC("mmaping files is (sadly) not supported.");
    }

    if (page == NULL) {
        return false;
    }
    return mmap_page_in_range(local_range->global, range.memory_page * PAGE_SIZE, (uintptr_t)page, local_range->prot);
}