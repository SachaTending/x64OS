#include <frg/std_compat.hpp>
#include <frg/vector.hpp>
#include <new>
#include <logging.hpp>
#include <prg_loader.hpp>
#include <vfs.hpp>
#include <libc.h>
#include <vmm.h>

static Logger log("Program loader");

typedef frg::vector<prg_loader_t *, frg::stl_allocator> prg_vector_t;

prg_vector_t prg_vector;

void register_loader(prg_loader_t *prg) {
    prg_vector.push_back(prg);
    log.info("Registered loader: %s\n", prg->loader_name);
}

#define BUF_LEN 512

LOADER_ERROR load_program(const char *path, pagemap *pgm) {
    vfs_node_t *file = vfs_get_node(path);
    if (file == NULL) {
        return LOADER_FILE_NOT_FOUND;
    }
    char buf[BUF_LEN]; 
    file->read(file, (void *)&buf, BUF_LEN); // Read program header
    for (size_t i=0;i<(prg_vector.size()/sizeof(prg_loader_t *));i++) {
        prg_loader_t *loader = prg_vector[i];
        if (loader->magic_len > BUF_LEN) continue; // Check if magic extends buffer length
        if (!memcmp((const void *)&buf, (const void *)loader->magic, loader->magic_len)) { // Check magic
            file->seek(0);
            LOADER_ERROR err = loader->check(loader, file); // Check if program valid
            if (err == LOADER_OK) {
                file->seek(0);
                err = loader->load(loader, file, pgm);
                return err;
            }
        }
    }
}