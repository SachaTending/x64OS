#pragma once
#include <stdint.h>

typedef struct vfs_node {

} vfs_node_t;

typedef vfs_node_t *(*get_file_t)(struct vfs_mnt *mnt, const char *path);

typedef struct vfs_mnt {
    const char *path;
    void *priv;
    get_file_t get_file;
} vfs_mnt_t;

typedef bool (*probe_fs_t)(struct vfs_fs *fs, const char *block);
typedef vfs_mnt_t *(*mnt_fs_t)(struct vfs_fs *fs, const char *block);

typedef struct vfs_fs {
    const char *name;
    void *priv;
    probe_fs_t probe_fs;
    mnt_fs_t mnt_fs;
} vfs_fs_t;

enum VFS_ERRORS {
    VFS_OK,
    VFS_INVALID_FS,
    VFS_INVALID_BLOCK,
    VFS_INVALID_OPTIONS,
    VFS_ALREADY_MOUNTED
};