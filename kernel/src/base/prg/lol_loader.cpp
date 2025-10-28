#include <vfs.hpp>
#include <stdint.h>
#include <libc.h>
#include <arch/vmm.h>

struct lol {
    char hdr[3];
    uint64_t load_addr;
    uint64_t entry;
    uint64_t size;
} __attribute__((packed));

void load_lol(resource *res, pagemap *pgm, uint64_t *entry) {
    lol hdr;
    res->read(res, NULL, &hdr, 0, sizeof(lol));
    printf("load_addr: 0x%lx, entry: 0x%lx\n", hdr.load_addr, hdr.entry);
    void *a = pmm_alloc(1);
    res->read(res, NULL, a+VMM_HIGHER_HALF, 0, hdr.size);
    vmm_map_page(pgm, hdr.load_addr, a, PTE_WRITABLE | PTE_PRESENT | PTE_USER);
    *entry = hdr.entry;
}