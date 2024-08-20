#include <stdint.h>
#include <stddef.h>
#include <vfs.hpp>
#include <vmm.h>

enum LOADER_ERROR {
    LOADER_OK = 0,
    LOADER_INVALID_EXEC = -1, // executable is invalid(header)
    LOADER_INVALID_MACHINE = -2, // executable has invalid machine type
    LOADER_ALLOC_FAIL = -3, // malloc failed
    LOADER_FILE_NOT_FOUND = -4
};

typedef LOADER_ERROR (*check_t)(struct prg_loader *prg_loader, vfs_node_t *node);
typedef LOADER_ERROR (*load_t)(struct prg_loader *prg_loader, vfs_node_t *node, pagemap *pgm);

typedef struct prg_loader
{
    const char *magic;
    size_t magic_len;
    const char *loader_name;
    check_t check;
    load_t load;
    void *priv;
} prg_loader_t;

void register_loader(prg_loader_t *prg);
LOADER_ERROR load_program(const char *path, pagemap *pgm);