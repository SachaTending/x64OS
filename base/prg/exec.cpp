#include <sched.hpp>
#include <libc.h>
#include <prg_loader.hpp>
#include <krnl.hpp>
#include <sched.hpp>
extern pagemap *krnl_page;

typedef int (*task_entry_t)();

int exec(const char *path) {
    pagemap *pgm = new pagemap;
    pgm->lock = SPINLOCK_INIT;
    pgm->top_level = (uint64_t *)pmm_alloc(1);
    uint64_t t = (uint64_t)pgm->top_level;
    t += VMM_HIGHER_HALF;
    pgm->top_level = (uint64_t *)t;
    printf("top level: 0x%lx\n", pgm->top_level);
    for (size_t i=256;i<512;i++) {
        pgm->top_level[i] = krnl_page->top_level[i];
    }
    char *ld_path;
    uint64_t entry;
    LOADER_ERROR ret = load_program(path, pgm, &ld_path, &entry);
    printf("program loader return: ld_path: %s(0x%lx), entry: 0x%lx, ret: %d\n", ld_path, ld_path, entry, ret);
    if (ret != LOADER_OK) {
        delete pgm->top_level;
        delete pgm;
        return -1;
    }
    // TODO: Add linker loading
    create_task((task_entry_t)entry, path, true, pgm);
    return 0;
}