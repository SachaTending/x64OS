#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef struct vfs_stat {
    size_t size;
    bool is_dir;
}vfs_stat_t;

typedef int (*write_t)(struct vfs_node *node, void *data, size_t len);
typedef int (*read_t)(struct vfs_node *node, void *buf, size_t len);
typedef size_t (*seek_t)(struct vfs_node *node, size_t offset, int seek_type);
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
typedef vfs_stat_t *(*stat_t)(struct vfs_mnt *mnt, const char *path);

typedef struct vfs_mnt {
    const char *path;
    void *priv;
    get_file_t get_file;
    create_fite_t create_file;
    umount_t umount;
    stat_t stat;
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
    VFS_OK = 0,
    VFS_INVALID_FS = -1,
    VFS_INVALID_BLOCK = -2,
    VFS_INVALID_OPTIONS = -3,
    VFS_ALREADY_MOUNTED = -4,
    VFS_FILE_NOT_FOUND = -5
};

int vfs_mount(const char *block, const char *mnt_path, vfs_fs_t *fs);
vfs_node_t *vfs_create_file(const char *path, bool is_dir);
vfs_node_t *vfs_get_node(const char *path);
vfs_stat_t *vfs_stat(const char *path);