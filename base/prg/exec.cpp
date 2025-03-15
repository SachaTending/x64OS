#include <sched.hpp>
#include <libc.h>
#include <prg_loader.hpp>
#include <krnl.hpp>
#include <sched.hpp>
#include <tss.h>
#include <sched.hpp>
void resume_sched();
extern tss_entry_t *tss;
extern pagemap *krnl_page;

typedef int (*task_entry_t)();

int exec(const char *path, int argc, char **argv, char **envp) {
    stop_sched();
    pagemap *pgm = vmm_new_pagemap();
    vmm_map_range(pgm, (uint64_t)tss-VMM_HIGHER_HALF, sizeof(tss_entry_t));
    char *ld_path = 0;
    uint64_t entry;
    uint64_t tls;
    auxval *aux = new auxval;
    LOADER_ERROR ret = load_program(path, pgm, &ld_path, &entry, &tls, aux);
    printf("===================\nat_entry: 0x%lx\nat_phdr: 0x%lx\nat_phent: 0x%lx\nat_phnum: 0x%lx\n", aux->at_entry, aux->at_phdr, aux->at_phent, aux->at_phnum);
    //printf("program loader return: ld_path: %s(0x%lx), entry: 0x%lx, ret: %d\n", ld_path, ld_path, entry, ret);
    auxval *taux = 0;
    if (ld_path != NULL) {
        taux = new auxval;
        printf("Executable uses linker, (EXPERIMENTAL) loading %s...\n", ld_path);
        //auxval *taux = new auxval;
        LOADER_ERROR ret = load_program(ld_path, pgm, &ld_path, &entry, &tls, taux, 0x40000000);
        printf("===================\nat_entry: 0x%lx\nat_phdr: 0x%lx\nat_phent: 0x%lx\nat_phnum: 0x%lx\n", aux->at_entry, aux->at_phdr, aux->at_phent, aux->at_phnum);
    }
    if (ret != LOADER_OK) {
        if (ret == LOADER_FILE_NOT_FOUND) {
            printf("/init not found.\n");
        }
        printf("ret=%u\n", ret);
        delete pgm->top_level;
        delete pgm;
        start_sched();
        return -1;
    }
    uint64_t pid=0;
    // TODO: Add linker loading
    if (taux != 0) create_task((task_entry_t)taux->at_entry, path, true, pgm, tls, argv, envp, aux, &pid, argc);
    else create_task((task_entry_t)aux->at_entry, path, true, pgm, tls, argv, envp, aux, &pid, argc);
    if (taux != 0) delete taux;
    resume_sched();
    return (int)pid;
}