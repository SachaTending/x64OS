#include <vfs.hpp>
#include <libc.h>
#include <frg/std_compat.hpp>
#include <frg/vector.hpp>

typedef struct devtmpfs_file {
    const char *path;
} devtmpfs_file_t;

typedef struct devtmpfs_file_priv {
    void *res_priv;
} devtmpfs_file_priv_t;

typedef frg::vector<devtmpfs_file_t *, frg::stl_allocator> devtmpfs_file_vec_t;

typedef struct devtmpfs_mnt {
    
};

devtmpfs_file_vec_t devtmpfs_vec = devtmpfs_file_vec_t();

bool devtmpfs_check_block(vfs_fs_t *fs, const char *block) {
    (void)fs;
    (void)block;
    return true;
}

vfs_fs_t devtmpfs = {
    .name = "devtmpfs",
    .priv = NULL,
    .probe_fs = devtmpfs_check_block
};