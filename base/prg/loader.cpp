#include <stddef.h>
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

LOADER_ERROR load_program(const char *path, pagemap *pgm, char **ld_path, uint64_t *entry, uint64_t *tls, auxval *aux, uint64_t load_base=0) {
    vfs_node_t *file = vfs_get_node(path);
    //printf("get node: 0x%lx\n", file);
    if (file == NULL) {
        return LOADER_FILE_NOT_FOUND;
    }
    char *buf = new char[BUF_LEN]; 
    if (buf == NULL) {
        file->close(file);
        return LOADER_ALLOC_FAIL;
    }
    //printf("buf: 0x%lx\n", buf);
    file->read(file, buf, BUF_LEN); // Read program header
    //printf("header read.\n");
    size_t vector_size = prg_vector.size();
    //printf("vector size: %lu\n", vector_size);
    for (size_t i=0;i<vector_size;i++) {
        //printf("loader %lu: %s\n", i, prg_vector[i]->loader_name);
        prg_loader_t *loader = prg_vector[i];
        if (loader->magic_len > BUF_LEN) continue; // Check if magic extends buffer length
        if (!memcmp(buf, (const void *)loader->magic, loader->magic_len)) { // Check magic
            file->seek(file, 0, SEEK_SET);
            LOADER_ERROR err = loader->check(loader, file); // Check if program valid
            if (err == LOADER_OK) {
                file->seek(file, 0, SEEK_SET);
                err = loader->load(loader, file, pgm, ld_path, entry, tls, aux, load_base);
                return err;
            }
        }
    }
}