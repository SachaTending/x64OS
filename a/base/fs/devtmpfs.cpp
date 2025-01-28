#include <vfs.hpp>
#include <libc.h>
#include <frg/std_compat.hpp>
#include <frg/vector.hpp>
#include <new>
#include <logging.hpp>
#include <fs/devtmpfs.hpp>

static Logger log("devtmpfs");

typedef struct devtmpfs_file {
    const char *path;
    devtmpfs_node_t *file;
} devtmpfs_file_t;

typedef frg::vector<devtmpfs_file_t *, frg::stl_allocator> devtmpfs_file_vec_t;

typedef struct devtmpfs_mnt {
    
};

devtmpfs_file_vec_t devtmpfs_vec = devtmpfs_file_vec_t();

static void devtmpfs_umount(vfs_mnt_t *mnt) {
    delete (devtmpfs_mnt *)mnt->priv;
    delete mnt;
}
static int devtmpfs_read(vfs_node_t *node, void *buf, size_t len) {
    devtmpfs_node_t *n = (devtmpfs_node_t *)node->data;
    if (n->read != NULL) return n->read(n, buf, len);
    return -1;
}

static int devtmpfs_write(vfs_node_t *node, void *buf, size_t len) {
    devtmpfs_node_t *n = (devtmpfs_node_t *)node->data;
    if (n->write != NULL) return n->write(n, buf, len);
    return -1;
}

static int devtmpfs_ioctl(vfs_node_t *node, int action, void *data) {
    devtmpfs_node_t *n = (devtmpfs_node_t *)node->data;
    if (n->ioctl != NULL) {
        return n->ioctl(n, action, data);
    }
    return -1;
}

static int devtmpfs_close(vfs_node_t *node) {
    devtmpfs_node_t *n = (devtmpfs_node_t *)node->data;
    int ret = 0;
    if (n->close != NULL) ret = n->close(n);
    delete node;
    return ret;
}

static size_t devtmpfs_seek(vfs_node_t *node, size_t seek_pos, int seek_type) {
    devtmpfs_node_t *n = (devtmpfs_node_t *)node->data;
    if (n->seek != NULL) return n->seek(n, seek_pos, seek_type);
    return -1;
}

static size_t devtmpfs_tell(vfs_node_t *node) {
    devtmpfs_node_t *n = (devtmpfs_node_t *)node->data;
    if (n->tell != NULL) return n->tell(n);
    return -1;
}

static vfs_node_t *devtmpfs_get_file(struct vfs_mnt *mnt, const char *path) {
    log.debug("get_file(%s)\n", path);
    for (size_t i=0;i<devtmpfs_vec.size();i++) {
        if (!strcmp(path, devtmpfs_vec[i]->path)) {
            vfs_node_t *node = new vfs_node_t;
            node->data = (void *)devtmpfs_vec[i]->file;
            // TODO: Write function pointers to vfs_node_t
            node->read = devtmpfs_read;
            node->write = devtmpfs_write;
            node->ioctl = devtmpfs_ioctl;
            node->close = devtmpfs_close;
            node->seek = devtmpfs_seek;
            node->tell = devtmpfs_tell;
            return node;
        }
    }
    log.debug("get_file(%s) = NULL(not found.)\n", path);
    return NULL;
}

static vfs_node_t *devtmpfs_create_file(struct vfs_mnt *mnt, const char *path, bool is_dir) {
    log.debug("attempted to create file %s, is_dir: %d\n", path, is_dir);
    return NULL;
}

static vfs_mnt_t *create_devtmpfs() {
    vfs_mnt_t *mnt = new vfs_mnt_t;
    devtmpfs_mnt *context = new devtmpfs_mnt;
    mnt->priv = (void *)context;
    mnt->umount = devtmpfs_umount;
    mnt->create_file = devtmpfs_create_file;
    mnt->get_file = devtmpfs_get_file;
    //mnt->stat = tmpfs_stat;
    log.debug("mounted.\n");
    return mnt;
}

void devtmpfs_register_file(devtmpfs_node_t *n, const char *path) {
    devtmpfs_file_t *file = new devtmpfs_file_t;
    file->file = n;
    file->path = path;
    devtmpfs_vec.push(file);
    log.debug("Registered file %s\n", path);
}

static vfs_mnt_t *devtmpfs_mnt_fs(vfs_fs_t *fs, const char *block) {
    (void)fs;
    (void)block;
    return create_devtmpfs();
}

bool devtmpfs_check_block(vfs_fs_t *fs, const char *block) {
    (void)fs;
    (void)block;
    return true;
}

vfs_fs_t devtmpfs = {
    .name = "devtmpfs",
    .priv = NULL,
    .probe_fs = devtmpfs_check_block,
    .mnt_fs = devtmpfs_mnt_fs
};
void console_init();
void devtmpfs_init() {
    vfs_register_fs(&devtmpfs);
    console_init();
}