#include <vfs.hpp>
#include <libc.h>
#include <frg/vector.hpp>
#include <frg/std_compat.hpp>
#include <new>
#include <logging.hpp>

static Logger log("tmpfs");

struct tmpfs_file {
    const char *fpath;
    uint64_t size;
    void *data;
    bool is_dir;
};

struct tmpfs_node {
    tmpfs_file *file;
    struct tmpfs_mnt *mnt;
};

typedef frg::vector<tmpfs_file *, frg::stl_allocator> files_vector;

struct tmpfs_mnt {
    files_vector *vect;
};

void tmpfs_umount(vfs_mnt_t *mnt) {
    tmpfs_mnt *context = (tmpfs_mnt *)mnt->priv;
    for (size_t i=0;i<context->vect->size();i++) {
        tmpfs_file *file = context->vect->data()[i];
        if (file->is_dir == false or file->size == 0) free((void *)file->data);
        free((void *)file->fpath);
        free((void *)file);
    }
    context->vect->clear();
    delete context->vect;
    delete context;
    delete mnt;
}

int tmpfs_read(vfs_node_t *node, void *buf, size_t len) {
    log.debug("tmpfs_read(0x%lx, 0x%lx, %lu);\n", node, buf, len);
    tmpfs_node *node2 = (tmpfs_node *)node->data;
    if (node2->file->size == 0 or node2->file->is_dir == true) return 0;
    while ((node->seek_pos + len) > node2->file->size) {
        len--;
    }
    memcpy(buf, node2->file->data+node->seek_pos, len);
    node->seek_pos += len;
    log.debug("seek_pos: %lu\n", node->seek_pos);
    return len;
}

size_t tmpfs_tell(vfs_node_t *node) {
    return node->seek_pos;
}

size_t tmpfs_seek(vfs_node_t *node, size_t seek_pos, int seek_type) {
    tmpfs_node *node2 = (tmpfs_node *)node->data;
    switch (seek_type)
    {
        case SEEK_SET:
            while (seek_pos > node2->file->size) seek_pos--;
            break;
        case SEEK_END:
            while (seek_pos > node2->file->size) {
                seek_pos--;
            }
            seek_pos = node2->file->size - seek_pos;
        case SEEK_CUR:
            while ((seek_pos + node->seek_pos) > node2->file->size) {
                seek_pos--;
            }
            seek_pos += node->seek_pos;
        default:
            break;
    }
    node->seek_pos = seek_pos;
    return seek_pos;
}

int tmpfs_write(vfs_node_t *node, void *data, size_t len) {
    tmpfs_node *node2 = (tmpfs_node *)node->data;
    if ((len+node->seek_pos) > node2->file->size) {
        node2->file->data = realloc(node2->file->data, len+node->seek_pos);
        node2->file->size = len+node->seek_pos;
    }
    memcpy(node2->file->data+node->seek_pos, data, len);
    node->seek_pos += len;
    return (int)len;
}

int tmpfs_ioctl(vfs_node_t *node, int action, void *data) {
    (void)node;
    (void)action;
    (void)data;
    return -1; // ioctl in tmpfs u said?
}

int tmpfs_close(vfs_node_t *node) {
    tmpfs_node *node2 = (tmpfs_node *)node->data;
    delete node2;
    delete node;
    return 0;
}

bool tmpfs_file_exists(struct tmpfs_mnt *context, const char *path) {
    for (size_t i=0;i<context->vect->size();i++) {
        tmpfs_file *file = context->vect->data()[i];
        if (!strcmp(path, file->fpath)) {
            if (strlen(path) != strlen(file->fpath)) continue;
            return true;
        }
    }
    return false;
}

vfs_node_t *tmpfs_create_file(struct vfs_mnt *mnt, const char *path, bool is_dir) {
    //printf("tmpfs: create file %s, is dir: %d\n", path, is_dir);
    tmpfs_mnt *context = (tmpfs_mnt *)mnt->priv;
    if (tmpfs_file_exists(context, path) == true) {
        log.debug("tmpfs_create_file(0x%lx, %s, %d): file already exists.\n", mnt, path, is_dir);
        return NULL; // File already exists
    }
    tmpfs_file *file = new tmpfs_file;
    file->size = 0;
    file->is_dir = is_dir;
    file->fpath = strdup(path);
    context->vect->push_back(file);
    return mnt->get_file(mnt, path);
}

vfs_node_t *tmpfs_get_file(struct vfs_mnt *mnt, const char *path) {
    tmpfs_mnt *context = (tmpfs_mnt *)mnt->priv;
    for (size_t i=0;i<context->vect->size();i++) {
        tmpfs_file *file = context->vect->data()[i];
        if (!strcmp(path, file->fpath)) {
            if (strlen(path) != strlen(file->fpath)) continue;
            log.debug("tmpfs_get_file(0x%lx, %s): found file %s\n", mnt, path, file->fpath);
            vfs_node_t *n = new vfs_node_t;
            tmpfs_node *n2 = new tmpfs_node;
            n2->file = file;
            n2->mnt = context;
            n->data = (void *)n2;
            n->read = tmpfs_read;
            n->tell = tmpfs_tell;
            n->seek = tmpfs_seek;
            n->write = tmpfs_write;
            n->ioctl = tmpfs_ioctl;
            n->close = tmpfs_close;
            return n;
        }
    }
    log.debug("get_file(%s) = NULL(not found.)\n", path);
    return NULL; // no file found
}

vfs_stat_t *tmpfs_stat(vfs_mnt_t *mnt, const char *path) {
    tmpfs_mnt *context = (tmpfs_mnt *)mnt->priv;
    if (tmpfs_file_exists(context, path) == false) { // is file exists?
        // File doesn't exists
        return NULL;
    }
    vfs_node_t *node = mnt->get_file(mnt, path);
    tmpfs_node *n2 = (tmpfs_node *)node->data;
    vfs_stat_t *out = new vfs_stat_t;
    out->is_dir = n2->file->is_dir;
    if (n2->file->is_dir == false) out->size = n2->file->size;
    node->close(node);
    return out;
}

vfs_mnt_t *create_tmpfs() {
    vfs_mnt_t *mnt = new vfs_mnt_t;
    tmpfs_mnt *context = new tmpfs_mnt;
    context->vect = new files_vector;
    tmpfs_file *root = new tmpfs_file;
    root->data = malloc(1);
    root->size = 1;
    root->is_dir = true;
    root->fpath = strdup("/");
    context->vect->push_back(root);
    mnt->priv = (void *)context;
    mnt->umount = tmpfs_umount;
    mnt->create_file = tmpfs_create_file;
    mnt->get_file = tmpfs_get_file;
    mnt->stat = tmpfs_stat;
    printf("tmpfs: mounted.\n");
    return mnt;
}

vfs_mnt_t *tmpfs_mnt_fs(vfs_fs_t *fs, const char *block) {
    (void)fs;
    (void)block;
    return create_tmpfs();
}

bool tmpfs_check_block(vfs_fs_t *fs, const char *block) {
    (void)fs;
    (void)block;
    return true;
}

vfs_fs_t *tmpfs_create_fs() {
    vfs_fs_t *fs = new vfs_fs_t;
    fs->mnt_fs = tmpfs_mnt_fs;
    fs->name = "tmpfs";
    fs->probe_fs = tmpfs_check_block;
    return fs;
}