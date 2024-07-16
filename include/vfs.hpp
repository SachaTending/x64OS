#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef int (*write_t)(struct vfs_node *node, void *data, size_t len);
typedef int (*read_t)(struct vfs_node *node, void *buf, size_t len);
typedef int (*seek_t)(struct vfs_node *node, size_t offset, int seek_type);
typedef size_t (*tell_t)(struct vfs_node *node);
typedef int (*close_t)(struct vfs_node *node);
typedef int (*ioctl_t)(struct vfs_node *node, int action, void *data);

typedef struct vfs_node {
    write_t write;
    read_t read;
    seek_t seek;
    tell_t tell;
    ioctl_t ioctl;
    close_t close;
    size_t seek_pos;
    void *data;
} vfs_node_t;

typedef vfs_node_t *(*get_file_t)(struct vfs_mnt *mnt, const char *path);
typedef vfs_node_t *(*create_fite_t)(struct vfs_mnt *mnt, const char *path, bool is_dir);
typedef void (*umount_t)(struct vfs_mnt *mnt);

typedef struct vfs_mnt {
    const char *path;
    void *priv;
    get_file_t get_file;
    create_fite_t create_file;
    umount_t umount;
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