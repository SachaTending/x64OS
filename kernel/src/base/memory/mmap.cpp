#include <stdint.h>
#include <vfs.hpp>
#include <sched/sched.hpp>
#include <libc.h>
#include <spinlock.h>
#include <mmap.h>
#include <logging.hpp>
#include <krnl.hpp>

static Logger log("mmap");
uint64_t base = 0;
bool munmap(struct pagemap *pagemap, uintptr_t addr, size_t length);

void *mmap(struct pagemap *pagemap, uintptr_t addr, size_t length, int prot,
           int flags, vfs_node_t *node, size_t offset) {
    Scheduler::Stop();
    asm volatile("sti");
    struct mmap_range_global *global_range = NULL;
    struct mmap_range_local *local_range = NULL;

    length = ALIGN_UP(length, PAGE_SIZE);

    thread_t *task = Scheduler::GetCurrentThread();
    log.debug("task: 0x%lx\n", task);
    log.debug("task name: 0x%lx %s\n", task->name, task->name);
    log.debug("pagemap: 0x%lx\n", pagemap);
    if (pagemap == 0) pagemap = task->pgm;
    log.debug("pagemap: 0x%lx 0x%lx\n", pagemap, task->pgm);
    log.debug("mmap_anon_base: 0x%lx\n", task->mmap_anon_base);
    if ((flags & MAP_FIXED) != 0) {
        // Not supported.
        //printf("got MAP_FIXED\n");
        //PANIC("Tried to mmap with MAP_FIXED, but this is not implemented.\n"); // now supported
        if (!munmap(pagemap, addr, length)) {
            goto cleanup;
        }
    } else {
        //printf("normal mmap\n");
        base = task->mmap_anon_base;
        log.debug("base: 0x%lx\n", base); 
        task->mmap_anon_base += length + PAGE_SIZE;
    }
    
    global_range = (mmap_range_global *)malloc(sizeof(mmap_range_global));
    log.debug("gr 0x%lx\n", global_range);

    global_range->shadow_pagemap = vmm_new_pagemap();
    log.debug("spgm 0x%lx\n", global_range->shadow_pagemap);

    global_range->base = base;
    global_range->length = length;
    if (node) {
        global_range->res = node->resource;
        log.debug("resource: 0x%016lx\n"), global_range->res;
    }
    global_range->offset = offset;

    local_range = new struct mmap_range_local;
    log.debug("lr 0x%lx\n", local_range);

    local_range->pagemap = pagemap;
    local_range->global = global_range;
    local_range->base = base;
    local_range->length = length;
    local_range->prot = prot;
    local_range->flags = flags;
    local_range->offset = offset;

    global_range->locals.push_back(local_range);
    //printf("push1\n");

    spinlock_acquire(&pagemap->lock);
    log.debug("sp acq\n");
    log.debug("pgm: 0x%lx\n", pagemap);
    pagemap->mmap_ranges.push_back(local_range);
    log.debug("push2\n");

cleanup:
    spinlock_release(&pagemap->lock);

    Scheduler::Start();
    asm volatile("sti");
    log.info("mmap ret=0x%lx\n", base);
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
#define PTE_NX (1ull << 63ull)
bool mmap_page_in_range(struct mmap_range_global *global, uintptr_t virt,
                            uintptr_t phys, int prot) {
    //log.debug("mmap_page_in_range(0x%016lx, 0x%016lx, 0x%016lx, %d);\n", global, virt, phys, prot);
    uint64_t pt_flags = PTE_PRESENT | PTE_USER;

    if ((prot & PROT_WRITE) != 0) {
        pt_flags |= PTE_WRITABLE;
    }
    if ((prot & PROT_EXEC) == 0) {
        pt_flags |= PTE_NX;
    }
    //log.debug("pt_flags: %d\n", pt_flags);

    if (!vmm_map_page(global->shadow_pagemap, virt, phys, pt_flags)) {
        log.error("Failed to vmm_map_page\n");
        return false;
    }
    //log.debug("page mapped in shadow pagemap.\n");
    //return true;
    for (size_t i=0;i<global->locals.size();i++) {
        struct mmap_range_local *local_range = global->locals[i];
        if (virt < local_range->base || virt >= local_range->base + local_range->length) {
            continue;
        }
        //log.debug("selected pagemap: base: 0x%016lx, length: %d\n", local_range->base, local_range->length);
        //log.debug("pagemap: 0x%lx\n", local_range->pagemap);
        if (!vmm_map_page(local_range->pagemap, virt, phys, pt_flags)) {
            //log.debug("page mapped.\n");
            return false;
        }
    };

    return true;
}

bool mmap_pf(cpu_ctx *regs) {
    if ((regs->err & 0x1) != 0) {
    //if (false) {
        log.debug("got PF with PRESENT\n");
        return false;
        if (regs->rip < hhdm) {
            log.debug("mmap_pf: STRANGE, got PRESENT page fault from userspace, gonna handle\n");
            goto handle_pf;
        }
        return false;
        log.debug("mmap_pf: cr2=0x%lx, not our case, gonna handle anyway, err=0x%08x\n", regs->cr2, regs->err);
        log.debug("cs: 0x%02x (ring: %d)\n", regs->cs, regs->cs & 3);
        log.debug("rip: 0x%016lx\n", regs->rip);
        log.debug("is it a kernel pagemap? %d\n", Scheduler::GetCurrentThread()->pgm == krnl_page);
        //return false;
    }
        if (regs->rip > (uint64_t)Kernel::Main) {
        log.debug("wtf, got mmap_pf from krnl\n");
        return false;
    }
    handle_pf:

    uint64_t cr2 = regs->cr2;
    log.debug("mmap_pf, addr: 0x%016lx, err: 0x%04x\n", cr2, regs->err);
    pagemap *pgm = Scheduler::GetCurrentThread()->pgm;
    log.debug("thread: %d %s\n", Scheduler::GetCurrentThread()->pid, Scheduler::GetCurrentThread()->name);
    spinlock_acquire(&pgm->lock);

    struct addr2range range = addr2range(pgm, cr2);
    struct mmap_range_local *local_range = range.range;

    spinlock_release(&pgm->lock);

    if (local_range == NULL) {
        log.debug("mmap_pf: addr 0x%016lx not found in local ranges in thread %s(%d), not allocating.\n", cr2, Scheduler::GetCurrentThread()->name, Scheduler::GetCurrentThread()->pid);
        return false;
    }

    void *page = NULL;
    if ((local_range->flags & MAP_ANONYMOUS) != 0) {
        log.info("gonna allocate page for addr 0x%lx\n", cr2);
        page = pmm_alloc(1);
        log.debug("new page allocated: 0x%lx for addr 0x%lx\n", page, cr2);
    } else {
        struct resource *res = local_range->global->res;
        log.debug("mmap_pf: mmaping res 0x%016lx\n", res);
        log.debug("res->mmap: 0x%016lx\n", res->mmap);
        page = res->mmap(res, range.file_page, local_range->flags);
        log.debug("new file page allocated: 0x%lx for addr 0x%lx\n", page, cr2);
        //PANIC("mmaping files is (sadly) not supported.");
    }

    if (page == NULL) {
        return false;
    }
    bool ret = mmap_page_in_range(local_range->global, range.memory_page * PAGE_SIZE, (uintptr_t)page, local_range->prot);

    return ret;
}

bool munmap(struct pagemap *pagemap, uintptr_t addr, size_t length) {
    if (length == 0) {
        //errno = EINVAL;
        return false;
    }
    length = ALIGN_UP(length, PAGE_SIZE);

    for (uintptr_t i = addr; i < addr + length; i += PAGE_SIZE) {
        struct addr2range range = addr2range(pagemap, i);
        if (range.range == NULL) {
            continue;
        }

        struct mmap_range_local *local_range = range.range;
        struct mmap_range_global *global_range = local_range->global;

        uintptr_t snip_begin = i;
        for (;;) {
            i += PAGE_SIZE;
            if (i >= local_range->base + local_range->length || i >= addr + length) {
                break;
            }
        }

        uintptr_t snip_end = i;
        size_t snip_length = snip_end - snip_begin;

        spinlock_acquire(&pagemap->lock);

        if (snip_begin > local_range->base && snip_end < local_range->base + local_range->length) {
            struct mmap_range_local *postsplit_range = new struct mmap_range_local;
            if (postsplit_range == NULL) {
                // FIXME: Page map is in inconsistent state at this point!
                //errno = ENOMEM;
                spinlock_release(&pagemap->lock);
                return false;
            }

            postsplit_range->pagemap = local_range->pagemap;
            postsplit_range->global = global_range;
            postsplit_range->base = snip_end;
            postsplit_range->length = (local_range->base + local_range->length) - snip_end;
            postsplit_range->offset = local_range->offset + (off_t)(snip_end - local_range->base);
            postsplit_range->prot = local_range->prot;
            postsplit_range->flags = local_range->flags;

            pagemap->mmap_ranges.push_back(postsplit_range);

            local_range->length -= postsplit_range->length;
        }

        for (uintptr_t j = snip_begin; j < snip_end; j += PAGE_SIZE) {
            vmm_unmap_page(pagemap, j, true);
        }

        if (snip_length == local_range->length) {
            //VECTOR_REMOVE_BY_VALUE(&pagemap->mmap_ranges, local_range);
            // To developers of frigg runtime: pls add a way to remove value from vector
            mmap_ranges_t *shadow_vec = new mmap_ranges_t;
            for (mmap_range_local *l : pagemap->mmap_ranges) {
                if (l == local_range) continue;
                shadow_vec->push_back(l);
            }
            pagemap->mmap_ranges.clear();
            for (mmap_range_local *l: *shadow_vec) {
                pagemap->mmap_ranges.push_back(l);
            }
        }

        spinlock_release(&pagemap->lock);

        if (snip_length == local_range->length && global_range->locals.size() == 1) {
            if ((local_range->flags & MAP_ANONYMOUS) != 0) {
                for (uintptr_t j = global_range->base; j < global_range->base + global_range->length; j += PAGE_SIZE) {
                    uintptr_t phys = vmm_virt2phys(global_range->shadow_pagemap, j);
                    if (phys == INVALID_PHYS) {
                        continue;
                    }

                    if (!vmm_unmap_page(global_range->shadow_pagemap, j, true)) {
                        // FIXME: Page map is in inconsistent state at this point!
                        //errno = EINVAL;
                        return false;
                    }
                    pmm_free((void *)phys, 1);
                }
            } else {
                // TODO: res->unmap();
            }

            free(local_range);
        } else {
            if (snip_begin == local_range->base) {
                local_range->offset += snip_length;
                local_range->base = snip_end;
            }
            local_range->length -= snip_length;
        }
    }
    return true;
}

void vmm_destroy_pagemap(struct pagemap *pagemap);
bool mmap_range(struct pagemap *pagemap, uintptr_t virt, uintptr_t phys,
                size_t length, int prot, int flags) {
    flags |= MAP_ANONYMOUS;

    uintptr_t aligned_virt = ALIGN_DOWN(virt, PAGE_SIZE);
    size_t aligned_length = ALIGN_UP(length + (virt - aligned_virt), PAGE_SIZE);

    struct mmap_range_global *global_range = NULL;
    struct mmap_range_local *local_range = NULL;

    global_range = new mmap_range_global;
    if (global_range == NULL) {
        log.error("Failed to malloc global range\n");
        goto cleanup;
    }

    global_range->shadow_pagemap = vmm_new_pagemap();
    if (global_range->shadow_pagemap == NULL) {
        log.error("Failed to create pagemap for shadow pagemap\n");
        goto cleanup;
    }

    global_range->base = aligned_virt;
    global_range->length = aligned_length;

    local_range = new mmap_range_local;
    if (local_range == NULL) {
        log.error("Failed to malloc local range.\n");
        goto cleanup;
    }

    local_range->pagemap = pagemap;
    local_range->global = global_range;
    local_range->base = aligned_virt;
    local_range->length = aligned_length;
    local_range->prot = prot;
    local_range->flags = flags;

    global_range->locals.push_back(local_range);

    spinlock_acquire(&pagemap->lock);

    pagemap->mmap_ranges.push(local_range);

    spinlock_release(&pagemap->lock);

    for (size_t i = 0; i < aligned_length; i += PAGE_SIZE) {
        if (!mmap_page_in_range(global_range, aligned_virt + i, phys + i, prot)) {
            // FIXME: Page map is in inconsistent state at this point!
            log.error("mmap_page_in_range failed.\n");
            goto cleanup;
        }
    }

    return true;

cleanup:
    if (local_range != NULL) {
        free(local_range);
    }
    if (global_range != NULL) {
        if (global_range->shadow_pagemap != NULL) {
            //vmm_destroy_pagemap(global_range->shadow_pagemap);
        }

        //free(global_range);
        delete global_range;
    }
    return false;
}