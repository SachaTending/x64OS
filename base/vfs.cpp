#include <stddef.h>
#include <frg/std_compat.hpp>
#include <frg/vector.hpp>
#include <stdint.h>
#include <libc.h>
#include <new>
#include <vfs.hpp>

typedef frg::vector<uintptr_t, frg::stl_allocator> vfs_vec;

#define vec_get(ind) ((vfs_mnt_t *)vec[ind])

vfs_vec vec = vfs_vec();

int vfs_mount(const char *block, const char *mnt_path, vfs_fs_t *fs) {
    if (!fs && !block && mnt_path) {
        return VFS_INVALID_OPTIONS; // safety first
    }
    if (!fs->probe_fs(fs, block)) {
        return VFS_INVALID_BLOCK; // fs probe failed
    }
    for (size_t ind=0;ind<vec.size();ind++) {
        if (strlen(mnt_path) == strlen((vec_get(ind))->path)) {
            if (!strcmp(mnt_path, vec_get(ind)->path)) {
                return VFS_ALREADY_MOUNTED;
            }
        }
    }
    vfs_mnt_t *mnt = fs->mnt_fs(fs, block);
    mnt->path = strdup(mnt_path);
    vec.push_back((uintptr_t)mnt);
    return 0;
}

vfs_mnt_t *vfs_get_mnt(const char *path) {
    size_t big_size = 0;
    size_t path_len = strlen(path);
    vfs_mnt_t *m = NULL;
    for (size_t i=0;i<vec.size();i++) {
        if (path_len < big_size) {
            continue;
        }
        if (!strcmp(vec_get(i)->path, path)) {
            big_size = strlen(vec_get(i)->path);
            m = vec_get(i);
        }
    }
    return m;
}

vfs_node_t *vfs_get_node(const char *path) {
    vfs_mnt_t *mnt = vfs_get_mnt(path);
    if (mnt == NULL) {
        return NULL;
    }
    size_t plen = strlen(mnt->path);
    vfs_node_t *node = mnt->get_file(mnt, path+plen);
    return node;
}

void vfs_init() {
    
}