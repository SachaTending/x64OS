#include <stddef.h>
#include <stdint.h>
#include <limine.h>
#include <libc.h>

extern limine_hhdm_request hhdm;

static int alloc_id;

struct alloc_struct
{
    bool allocated;
    int id;
    size_t size;
    char magic[7];
    alloc_struct *prev;
    alloc_struct *next;
};

int avaible_mem, total_mem;

void debug_null(const char *m, ...) {
    (void)m;
}

#define debug debug_null

static limine_memmap_entry big;
extern int kernel_start;
extern int kernel_end;
static alloc_struct *root;
void printdbg(const char *c);
void pmm_setup(limine_memmap_response *mmap, uint64_t hhdm_off) {
    #if 1
    for (uint64_t e = 0;e<mmap->entry_count;e++) {
        if (mmap->entries[e]->type == LIMINE_MEMMAP_USABLE) {
            if (mmap->entries[e]->length > big.length) {
                big.length = mmap->entries[e]->length;
                big.base = mmap->entries[e]->base;
                big.type = mmap->entries[e]->type;
            }
        }
    }
    if (big.base == (uint64_t)&kernel_start) {
        big.base = (uint64_t)&kernel_end;
        big.length = big.length - (&kernel_end - &kernel_start);
    }
    #else
    big.base = (uint64_t)mem_heap;
    big.length = sizeof(mem_heap);
    #endif
    if (big.base < hhdm_off) big.base += hhdm_off;
    debug("big.base: 0x%016lx\n", big.base);
    root = (alloc_struct *)(big.base);
    root->allocated = false;
    strcpy((char *)&(root->magic), "FLOPPA");
    root->allocated = true;
    root->next = root;
    root->prev = root;
    root->id = alloc_id;
    root->size = 0;
    alloc_id++;
}

static bool check_magic(alloc_struct *entr) {
    return (entr->magic[0] == 'F' && entr->magic[1] == 'L' && entr->magic[2] == 'O' && entr->magic[3] == 'P' && entr->magic[4] == 'P' && entr->magic[5] == 'A');
}


static void set_signature(alloc_struct *alloc) {
    strcpy((char *)&alloc->magic, "FLOPPA");
}
extern "C" {
    void * malloc(size_t size) {
        alloc_struct *d = (alloc_struct *)(big.base + sizeof(alloc_struct));
        alloc_struct *prev;
        debug("Searching block size: %u\n", size);
        while ((uintptr_t)d < (big.length + big.base)) {
            debug("Addr: 0x%x\n", d);
            if (check_magic(d)) {
                // This is allocated block
                debug("Found block\n");
                if (d->allocated == false && d->size >= size) {
                    // Found free block, set allocated to true and return it
                    debug("Block is unallocated");
                    d->allocated = true;
                    return d += sizeof(alloc_struct);
                } else {
                    debug("Block allocated.\n");
                    prev = d;
                    d += d->size + sizeof(alloc_struct);
                }
            } else {
                // Found unallocated space
                if (((uintptr_t)d + sizeof(alloc_struct) + size) > (big.base + big.length)) {
                    // bruh
                    return (void *)-1;
                } else {
                    // Allocate space
                    if ((uintptr_t)d < hhdm.response->offset) {
                        d += hhdm.response->offset;
                    }
                    debug("Allocating block at 0x%016lx size %08lu...\n", d, size);
                    d->allocated = true;
                    if ((uintptr_t)prev < hhdm.response->offset) {
                        prev += hhdm.response->offset;
                    }
                    d->prev = prev;
                    prev->next = d;
                    set_signature(d);
                    d->size = size;
                    d->id = alloc_id;
                    alloc_id++;
                    return (void *)((uintptr_t *)d + sizeof(alloc_struct));
                }
            }
        }
        debug("No free block found.\n");
        //printf("WE FUCKED UP, NO MEMORY.\n");
        return (void *)-1;
    }
    void free(void *mem) {
        alloc_struct *d = (alloc_struct *)((uint64_t *)mem - sizeof(alloc_struct));
        if (check_magic(d)) {
            if (d->allocated) {
                d->allocated = false;
            }
        }
    }
    void *realloc(void *ptr, size_t l) {
        alloc_struct *d = (alloc_struct *)((uint64_t *)ptr - sizeof(alloc_struct));
        if (check_magic(d)) {
            if (d->allocated) {
                if (d->size < l) {
                    void *ptr_new = malloc(d->size);
                    memcpy(ptr_new, (const void *)ptr, d->size);
                    memset(ptr, 0, d->size);
                    free(ptr);
                    return ptr_new;
                }
            }
        }
        return ptr;
    }
}