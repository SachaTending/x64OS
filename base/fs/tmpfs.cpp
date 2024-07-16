#include <vfs.hpp>
#include <libc.h>
#include <frg/vector.hpp>
#include <frg/std_compat.hpp>
#include <new>

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
        if (file->is_dir == false) free((void *)file->data);
        free((void *)file->fpath);
        free((void *)file);
    }
    context->vect->clear();
    delete context->vect;
    delete context;
    delete mnt;
}

int tmpfs_read(vfs_node_t *node, void *buf, size_t len) {
    tmpfs_node *node2 = (tmpfs_node *)node->data;
    while ((node->seek_pos + len) > node2->file->size) {
        len--;
    }
    memcpy(buf, node2->file->data+node->seek_pos, len);
    return len;
}

vfs_node_t *tmpfs_create_file(struct vfs_mnt *mnt, const char *path, bool is_dir) {

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
    return mnt;
}