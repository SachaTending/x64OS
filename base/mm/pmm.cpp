#include <limine.h>
#include <spinlock.h>
#include <libc.h>
#include <printf/printf.h>
#include <logging.hpp>
#include <vmm.h>

static Logger log("PMM(From lyre os)");

#define PAGE_SIZE 4096

extern limine_memmap_request m;
#define memmap_request m

extern limine_hhdm_request hhdm2;
extern pagemap *krnl_page;
#define hhdm_request hhdm2
#define SIZEOF_ARRAY(ARRAY) (sizeof(ARRAY) / sizeof(ARRAY[0]))
#define MIN(A, B) ({ \
    typeof(A) MIN_a = A; \
    typeof(B) MIN_b = B; \
    MIN_a < MIN_b ? MIN_a : MIN_b; \
})

#define MAX(A, B) ({ \
    typeof(A) MAX_a = A; \
    typeof(B) MAX_b = B; \
    MAX_a > MAX_b ? MAX_a : MAX_b; \
})

#define DIV_ROUNDUP(VALUE, DIV) ({ \
    typeof(VALUE) DIV_ROUNDUP_value = VALUE; \
    typeof(DIV) DIV_ROUNDUP_div = DIV; \
    (DIV_ROUNDUP_value + (DIV_ROUNDUP_div - 1)) / DIV_ROUNDUP_div; \
})

#define ALIGN_UP(VALUE, ALIGN) ({ \
    typeof(VALUE) ALIGN_UP_value = VALUE; \
    typeof(ALIGN) ALIGN_UP_align = ALIGN; \
    DIV_ROUNDUP(ALIGN_UP_value, ALIGN_UP_align) * ALIGN_UP_align; \
})

#define ALIGN_DOWN(VALUE, ALIGN) ({ \
    typeof(VALUE) ALIGN_DOWN_value = VALUE; \
    typeof(VALUE) ALIGN_DOWN_align = ALIGN; \
    (ALIGN_DOWN_value / ALIGN_DOWN_align) * ALIGN_DOWN_align; \
})

#define VMM_HIGHER_HALF hhdm2.response->offset

static spinlock_t lock = SPINLOCK_INIT;
static uint8_t *bitmap = NULL;
static uint64_t highest_page_index = 0;
static uint64_t last_used_index = 0;
static uint64_t usable_pages = 0;
extern uint64_t used_pages = 0;
static uint64_t reserved_pages = 0;

extern size_t used_ram;

static inline bool bitmap_test(void *bitmap, size_t bit) {
    uint8_t *bitmap_u8 = (uint8_t *)bitmap;
    return bitmap_u8[bit / 8] & (1 << (bit % 8));
}

static inline void bitmap_set(void *bitmap, size_t bit) {
    uint8_t *bitmap_u8 = (uint8_t *)bitmap;
    bitmap_u8[bit / 8] |= (1 << (bit % 8));
}

static inline void bitmap_reset(void *bitmap, size_t bit) {
    uint8_t *bitmap_u8 = (uint8_t *)bitmap;
    bitmap_u8[bit / 8] &= ~(1 << (bit % 8));
}

void *pmm_alloc_nozero(size_t pages);
void slab_init();
void pmm_init(void) {
    log.name = "PMM(From lyre os)";
    // TODO: Check if memmap and hhdm responses are null and panic
    struct limine_memmap_response *memmap = memmap_request.response;
    struct limine_hhdm_response *hhdm = hhdm_request.response;
    struct limine_memmap_entry **entries = memmap->entries;

    uint64_t highest_addr = 0;

    // Calculate how big the memory map needs to be.
    for (size_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = entries[i];

        //printf("pmm: Memory map entry: base=%lx, length=%lx, type=%lx\n",
        //    entry->base, entry->length, entry->type);

        switch (entry->type) {
            case LIMINE_MEMMAP_USABLE:
                usable_pages += DIV_ROUNDUP(entry->length, PAGE_SIZE);
                highest_addr = MAX(highest_addr, entry->base + entry->length);
                break;
            case LIMINE_MEMMAP_RESERVED:
            case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
            case LIMINE_MEMMAP_ACPI_NVS:
            case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
            case LIMINE_MEMMAP_KERNEL_AND_MODULES:
                reserved_pages += entry->length;
                break;
        }
    }

    // Calculate the needed size for the bitmap in bytes and align it to page size.
    highest_page_index = highest_addr / PAGE_SIZE;
    uint64_t bitmap_size = ALIGN_UP(highest_page_index / 8, PAGE_SIZE);

    log.debug("pmm: Highest address: %lx\n", highest_addr);
    log.debug("pmm: Bitmap size: %lu bytes\n", bitmap_size);

    // Find a hole for the bitmap in the memory map.
    for (size_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = entries[i];

        if (entry->type != LIMINE_MEMMAP_USABLE) {
            continue;
        }

        if (entry->length >= bitmap_size) {
            bitmap = (uint8_t *)(entry->base + hhdm->offset);

            // Initialise entire bitmap to 1 (non-free)
            memset(bitmap, 0xff, bitmap_size);

            entry->length -= bitmap_size;
            entry->base += bitmap_size;

            break;
        }
    }

    // Populate free bitmap entries according to the memory map.
    for (size_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = entries[i];

        if (entry->type != LIMINE_MEMMAP_USABLE) {
            continue;
        }

        for (uint64_t j = 0; j < entry->length; j += PAGE_SIZE) {
            bitmap_reset(bitmap, (entry->base + j) / PAGE_SIZE);
        }
    }

    log.debug("pmm: Usable memory: %luMiB\n", (usable_pages * 4096) / 1024 / 1024);
    log.debug("pmm: Reserved memory: %luMiB\n", (reserved_pages * 4096) / 1024 / 1024);
    slab_init();
}

void pmm_on_vmm_enabled() {
    log.info("Mapping memory...\n");
    struct limine_memmap_response *memmap = memmap_request.response;
    struct limine_hhdm_response *hhdm = hhdm_request.response;
    struct limine_memmap_entry **entries = memmap->entries;
    for (size_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = entries[i];
        vmm_map_range(krnl_page, entry->base, entry->length, PTE_PRESENT | PTE_WRITABLE);
        log.debug("entry %lu mapped.\n", i);
    }
}

static void *inner_alloc(size_t pages, uint64_t limit) {
    size_t p = 0;

    while (last_used_index < limit) {
        if (!bitmap_test(bitmap, last_used_index++)) {
            if (++p == pages) {
                size_t page = last_used_index - pages;
                for (size_t i = page; i < last_used_index; i++) {
                    bitmap_set(bitmap, i);
                }
                return (void *)(page * PAGE_SIZE);
            }
        } else {
            p = 0;
        }
    }
    log.debug("inner_alloc(pages=%lu, limit=%lu): Allocation failed, no free pages(no free ram)\n");
    return NULL;
}

extern "C" void *pmm_alloc(size_t pages) {
    void *ret = pmm_alloc_nozero(pages);
    if (ret != NULL) {
        memset(ret + VMM_HIGHER_HALF, 0, pages * PAGE_SIZE);
    }

    return ret;
}

void *pmm_alloc_nozero(size_t pages) {
    spinlock_acquire(&lock);

    size_t last = last_used_index;
    void *ret = inner_alloc(pages, highest_page_index);

    if (ret == NULL) {
        last_used_index = 0;
        ret = inner_alloc(pages, last);
    }

    // TODO: Check if ret is null and panic
    used_pages += pages;

    spinlock_release(&lock);
    return ret;
}

extern "C" void pmm_free(void *addr, size_t pages) {
    spinlock_acquire(&lock);

    size_t page = (uint64_t)addr / PAGE_SIZE;
    for (size_t i = page; i < page + pages; i++) {
        bitmap_reset(bitmap, i);
    }
    used_pages -= pages;

    spinlock_release(&lock);
}


struct slab {
    spinlock_t lock;
    void **first_free;
    size_t ent_size;
};

struct slab_header {
    struct slab *slab;
};

struct alloc_metadata {
    size_t pages;
    size_t size;
};

static struct slab slabs[10];
void slab_free(void *addr);
static inline struct slab *slab_for(size_t size) {
    for (size_t i = 0; i < SIZEOF_ARRAY(slabs); i++) {
        struct slab *slab = &slabs[i];
        if (slab->ent_size >= size) {
            return slab;
        }
    }
    return NULL;
}

static void create_slab(struct slab *slab, size_t ent_size) {
    slab->lock = (spinlock_t)SPINLOCK_INIT;
    slab->first_free = (void **)(pmm_alloc_nozero(1) + VMM_HIGHER_HALF);
    slab->ent_size = ent_size;

    size_t header_offset = ALIGN_UP(sizeof(struct slab_header), ent_size);
    size_t available_size = PAGE_SIZE - header_offset;

    struct slab_header *slab_ptr = (struct slab_header *)slab->first_free;
    slab_ptr->slab = slab;
    slab->first_free = (void **)((void *)slab->first_free + header_offset);

    void **arr = (void **)slab->first_free;
    size_t max = available_size / ent_size - 1;
    size_t fact = ent_size / sizeof(void *);

    for (size_t i = 0; i < max; i++) {
        arr[i * fact] = &arr[(i + 1) * fact];
    }
    arr[max * fact] = NULL;
}

static void *alloc_from_slab(struct slab *slab) {
    spinlock_acquire(&slab->lock);

    if (slab->first_free == NULL) {
        create_slab(slab, slab->ent_size);
    }

    void **old_free = slab->first_free;
    slab->first_free = (void **)*old_free;
    memset(old_free, 0, slab->ent_size);

    spinlock_release(&slab->lock);
    return old_free;
}

static void free_in_slab(struct slab *slab, void *addr) {
    spinlock_acquire(&slab->lock);
    void **new_head;
    if (addr == NULL) {
        goto cleanup;
    }

    new_head = (void **)addr;
    *new_head = slab->first_free;
    slab->first_free = new_head;

cleanup:
    spinlock_release(&slab->lock);
}

void slab_init(void) {
    create_slab(&slabs[0], 8);
    create_slab(&slabs[1], 16);
    create_slab(&slabs[2], 24);
    create_slab(&slabs[3], 32);
    create_slab(&slabs[4], 48);
    create_slab(&slabs[5], 64);
    create_slab(&slabs[6], 128);
    create_slab(&slabs[7], 256);
    create_slab(&slabs[8], 512);
    create_slab(&slabs[9], 1024);
}

void *slab_alloc(size_t size) {
    struct slab *slab = slab_for(size);
    if (slab != NULL) {
        return alloc_from_slab(slab);
    }

    size_t page_count = DIV_ROUNDUP(size, PAGE_SIZE);
    void *ret = pmm_alloc(page_count + 1);
    if (ret == NULL) {
        return NULL;
    }

    ret += VMM_HIGHER_HALF;
    struct alloc_metadata *metadata = (struct alloc_metadata *)ret;

    metadata->pages = page_count;
    metadata->size = size;

    used_ram += size;

    return ret + PAGE_SIZE;
}

void *slab_realloc(void *addr, size_t new_size) {
    if (addr == NULL) {
        return slab_alloc(new_size);
    }

    if (((uintptr_t)addr & 0xfff) == 0) {
        struct alloc_metadata *metadata = (struct alloc_metadata *)(addr - PAGE_SIZE);
        if (DIV_ROUNDUP(metadata->size, PAGE_SIZE) == DIV_ROUNDUP(new_size, PAGE_SIZE)) {
            metadata->size = new_size;
            return addr;
        }

        void *new_addr = slab_alloc(new_size);
        if (new_addr == NULL) {
            return NULL;
        }

        if (metadata->size > new_size) {
            memcpy(new_addr, addr, new_size);
        } else {
            memcpy(new_addr, addr, metadata->size);
        }

        slab_free(addr);
        return new_addr;
    }

    struct slab_header *slab_header = (struct slab_header *)((uintptr_t)addr & ~0xfff);
    struct slab *slab = slab_header->slab;

    if (new_size > slab->ent_size) {
        void *new_addr = slab_alloc(new_size);
        if (new_addr == NULL) {
            return NULL;
        }

        memcpy(new_addr, addr, slab->ent_size);
        free_in_slab(slab, addr);
        return new_addr;
    }

    return addr;
}

void slab_free(void *addr) {
    if (addr == NULL) {
        return;
    }

    if (((uintptr_t)addr & 0xfff) == 0) {
        struct alloc_metadata *metadata = (struct alloc_metadata *)(addr - PAGE_SIZE);
        used_ram -= metadata->size;
        pmm_free((void *)metadata - VMM_HIGHER_HALF, metadata->pages + 1);
        return;
    }

    struct slab_header *slab_header = (struct slab_header *)((uintptr_t)addr & ~0xfff);
    free_in_slab(slab_header->slab, addr);
}

extern "C" {
    void *malloc(size_t size) {
        void *out = slab_alloc(size);
        return out;
    }
    void free(void *ptr) {
        slab_free(ptr);
    }
    void *realloc(void *ptr, size_t l) {
        if (!ptr) {
            void *ptr = malloc(l);
            memset(ptr, 0, l);
            return ptr;
        }
        return slab_realloc(ptr, l);
    }
}

size_t used_ram=0;