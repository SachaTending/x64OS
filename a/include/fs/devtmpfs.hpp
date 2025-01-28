#pragma once
#include <stddef.h>
typedef int (*devtmpfs_read_t)(struct devtmpfs_node *, void *buf, size_t len);
typedef int (*devtmpfs_write_t)(struct devtmpfs_node *, void *buf, size_t len);
typedef int (*devtmpfs_ioctl_t)(struct devtmpfs_node *, int action, void *data);
typedef int (*devtmpfs_close_t)(struct devtmpfs_node *node);
typedef size_t (*devtmpfs_tell_t)(struct devtmpfs_node *node);
typedef size_t (*devtmpfs_seek_t)(struct devtmpfs_node *node, size_t seek_pos, int seek_type);

typedef struct devtmpfs_node {
    void *priv;
    devtmpfs_read_t read;
    devtmpfs_write_t write;
    devtmpfs_ioctl_t ioctl;
    devtmpfs_close_t close;
    devtmpfs_seek_t seek;
    devtmpfs_tell_t tell;
} devtmpfs_node_t;

void devtmpfs_register_file(devtmpfs_node_t *n, const char *path);