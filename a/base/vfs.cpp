#include <stddef.h>
#include <frg/std_compat.hpp>
#include <frg/vector.hpp>
#include <stdint.h>
#include <libc.h>
#include <new>
#include <vfs.hpp>
#include <logging.hpp>

static Logger log("VFS");

typedef frg::vector<uintptr_t, frg::stl_allocator> vfs_vec;
typedef frg::vector<vfs_fs_t *, frg::stl_allocator> vfs_fs_vec;

#define vec_get(ind) ((vfs_mnt_t *)vec[ind])

vfs_vec vec = vfs_vec();
vfs_fs_vec fs_vec = vfs_fs_vec();

int vfs_mount_by_name(const char *block, const char *mnt_path, const char *fs_name) {
    for (size_t i=0;i<fs_vec.size();i++) {
        if (!strcmp(fs_vec[i]->name, fs_name)) {
            return vfs_mount(block, mnt_path, fs_vec[i]);
        }
    }
    return VFS_INVALID_FS;
}

int vfs_mount(const char *block, const char *mnt_path, vfs_fs_t *fs) {
    if (!fs && !block && mnt_path) {
        return VFS_INVALID_OPTIONS; // safety first
    }
    if (!fs->probe_fs(fs, block)) {
        return VFS_INVALID_BLOCK; // fs probe failed
    }
    // Check if mnt_path exists
    vfs_stat_t *_t = vfs_stat(mnt_path);
    if (!_t) {
        if (strlen(mnt_path) != 1) { 
            log.debug("tried to mount %s on %s, but %s doesn't exists, _t=0x%lx.\n", fs->name, mnt_path, mnt_path, _t);
            return VFS_FILE_NOT_FOUND; // mnt_path not found
        }
    }
    else {
        delete _t;
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
#define AUTOCORRECT_PATH(path) if (path[0] == '.') path += 1
vfs_node_t *vfs_get_node(const char *path) {
    AUTOCORRECT_PATH(path);
    vfs_mnt_t *mnt = vfs_get_mnt(path);
    if (mnt == NULL) {
        log.debug("vfs_get_node(%s) = NULL(bcz no mnt found)\n", path);
        //printf("vfs: no mnt for %s found\n", path);
        return NULL;
    }
    size_t plen = strlen(mnt->path);
    log.debug("mnt->path(%s) length = %lu, path+plen=%s\n", mnt->path, plen, path+plen);
    if (plen == 1) {
        plen -= 1;
    }
    vfs_node_t *node = mnt->get_file(mnt, path+plen);
    log.debug("vfs_get_node(%s) = 0x%lx\n", path, node);
    return node;
}

vfs_node_t *vfs_create_file(const char *path, bool is_dir) {
    log.debug("create_file(%s, %d)\n", path, is_dir);
    AUTOCORRECT_PATH(path);
    vfs_mnt_t *mnt = vfs_get_mnt(path);
    if (mnt == NULL) {
        return NULL;
    }
    size_t plen = strlen(mnt->path);
    vfs_node_t *node = mnt->create_file(mnt, path+plen-1, is_dir);
    return node;
}

vfs_stat_t *vfs_stat(const char *path) {
    AUTOCORRECT_PATH(path);
    vfs_mnt_t *mnt = vfs_get_mnt(path);
    if (mnt == NULL) {
        return NULL;
    }
    return mnt->stat(mnt, path+strlen(mnt->path)-1);
}

void vfs_register_fs(vfs_fs_t *fs) {
    fs_vec.push(fs);
    log.debug("registered fs: %s\n", fs->name);
}

void vfs_init() {
    
}