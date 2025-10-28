#include <krnl.hpp>
#include <vfs.hpp>
#include <libc.h>
#include <arch/vmm.h>
#include <sched/sched.hpp>
#include <prg_loading.hpp>

static Logger *log = new Logger("Kernel");

void unpack_initrd();
void load_lol(resource *res, pagemap *pgm, uint64_t *entry);
typedef void (*c)();
void Kernel::Main() {
    log->info("idk what to put here, but this is a Kernel::Main\n");
    VFS::Init();
    VFS::Mount(vfs_root, NULL, "/", "tmpfs");
    VFS::Create(vfs_root, "/dev", 0755 | S_IFDIR);
    unpack_initrd();
    log->info("Trying to read file from VFS...\n");
    vfs_node_t *node = VFS::GetNode(vfs_root, "/hi.txt", true);
    if (node == false) {
        log->error("Failed to get file, is it unpacked?\n");
    } else {
        log->info("Successfully opened file /hi.txt!\n");
        log->info("File contents: ");
        char buf[16384];
        memset(buf, 0, 200);
        node->resource->read(node->resource, NULL, buf, 0, 180);
        printf(buf);
        printf("\n");
        log->info("File has been read successfully\n");
    }
    //log->info("Легро, где арты?\n");
    #define PRG "/linux_compat_layer_test"
    node = VFS::GetNode(vfs_root, PRG, true);
    if (node != NULL) {
        pagemap *pgm = vmm_new_pagemap();
        auxval aux;
        const char *ld;
        bool ret = elf_load(pgm, node->resource, 0x0, &aux, &ld);
        if (ret == false) {
            log->error("Failed to load %s as elf program.\n", PRG);
        } else {
            log->info("%s info:\n", PRG);
            log->info("entry: 0x%lx\n", aux.at_entry);
            if (ld) {
                log->info("interpreter: %s\n", ld);
            } else {
                log->info("No interpreter.\n");
            }
            Scheduler::Stop();
            Scheduler::CreateThread(PRG, aux.at_entry, true, pgm);
            Scheduler::Start();
        }
    } else {

    }
    while (1);
}