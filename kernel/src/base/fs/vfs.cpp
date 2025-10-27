#include <vfs.hpp>
#include <libc.h>
#include <spinlock.h>
#include <fs/resource.h>
#include <sys/stat.h>
#include <logging.hpp>

static Logger log("VFS");

static spinlock_t vfs_lock = SPINLOCK_INIT;

struct vfs_node *VFS::CreateNode(struct vfs_filesystem *fs, struct vfs_node *parent,
                                 const char *name, bool dir) {
    struct vfs_node *node = new vfs_node_t;

    node->name = (char *)strdup(name);

    node->parent = parent;
    node->filesystem = fs;

    if (dir) {
        node->children = (typeof(node->children))HASHMAP_INIT(256);
    }

    return node;
}
vfs_node_t *vfs_root;

void vfs_create_dotentries(struct vfs_node *node, struct vfs_node *parent) {
    struct vfs_node *dot = VFS::CreateNode(node->filesystem, node, ".", false);
    struct vfs_node *dotdot = VFS::CreateNode(node->filesystem, node, "..", false);

    dot->redir = node;
    dotdot->redir = parent;

    HASHMAP_SINSERT(&node->children, ".", dot);
    HASHMAP_SINSERT(&node->children, "..", dotdot);
}


static HASHMAP_TYPE(fs_mount_t) filesystems;

void VFS::AddFilesystem(fs_mount_t fs_mount, const char *identifier) {
    spinlock_acquire(&vfs_lock);

    HASHMAP_SINSERT(&filesystems, identifier, fs_mount);

    spinlock_release(&vfs_lock);
}
void tmpfs_init(void);
void VFS::Init(void) {
    vfs_root = VFS::CreateNode(NULL, NULL, "", false);

    filesystems = (typeof(filesystems))HASHMAP_INIT(256);
    tmpfs_init();
}
struct path2node_res {
    struct vfs_node *target_parent;
    struct vfs_node *target;
    char *basename;
};
int errno;
enum e {
    ENOENT,
    ENOTDIR,
    EISDIR,
    ENODEV,
    EEXIST
};
static struct vfs_node *reduce_node(struct vfs_node *node, bool follow_symlinks);
static bool populate(struct vfs_node *node) {
    if (node->filesystem && node->filesystem->populate && node->populated == false && node->resource && S_ISDIR(node->resource->stat.st_mode)) {
        node->filesystem->populate(node->filesystem, node);
        return node->populated;
    }
    return true;
}
static struct path2node_res path2node(struct vfs_node *parent, const char *path) {
    if (path == NULL || strlen(path) == 0) {
        errno = ENOENT;
        return (struct path2node_res){NULL, NULL, NULL};
    }

    size_t path_len = strlen(path);

    bool ask_for_dir = path[path_len - 1] == '/';

    size_t index = 0;
    struct vfs_node *current_node = reduce_node(parent, false);
    if (!populate(current_node)) {
        return (struct path2node_res){NULL, NULL, NULL};
    }

    if (path[index] == '/') {
        current_node = reduce_node(vfs_root, false);
        while (path[index] == '/') {
            if (index == path_len - 1) {
                return (struct path2node_res){current_node, current_node, (char *)strdup("/")};
            }
            index++;
        }
    }

    for (;;) {
        const char *elem = &path[index];
        size_t elem_len = 0;

        while (index < path_len && path[index] != '/') {
            elem_len++, index++;
        }

        while (index < path_len && path[index] == '/') {
            index++;
        }

        bool last = index == path_len;

        char *elem_str = (char *)strdup(elem);

        current_node = reduce_node(current_node, false);

        struct vfs_node *new_node;

        // XXX put a lock around this guy
        // XXX page fault here (seemingly random)
        if (!HASHMAP_SGET(&current_node->children, new_node, elem_str)) {
            errno = ENOENT;
            if (last) {
                return (struct path2node_res){current_node, NULL, elem_str};
            }
            return (struct path2node_res){NULL, NULL, NULL};
        }

        new_node = reduce_node(new_node, false);
        if (!populate(new_node)) {
            return (struct path2node_res){NULL, NULL, NULL};
        }

        if (last) {
            if (ask_for_dir && !S_ISDIR(new_node->resource->stat.st_mode)) {
                errno = ENOTDIR;
                return (struct path2node_res){current_node, NULL, elem_str};
            }
            return (struct path2node_res){current_node, new_node, elem_str};
        }

        current_node = new_node;

        if (S_ISLNK(current_node->resource->stat.st_mode)) {
            struct path2node_res r = path2node(current_node->parent, current_node->symlink_target);
            if (r.target == NULL) {
                return (struct path2node_res){NULL, NULL, NULL};
            }
            current_node = r.target;
        }

        if (!S_ISDIR(current_node->resource->stat.st_mode)) {
            errno = ENOTDIR;
            return (struct path2node_res){NULL, NULL, NULL};
        }
    }

    errno = ENOENT;
    return (struct path2node_res){NULL, NULL, NULL};
}

static struct vfs_node *reduce_node(struct vfs_node *node, bool follow_symlinks) {
    if (node->redir != NULL) {
        return reduce_node(node->redir, follow_symlinks);
    }
    if (node->mountpoint != NULL) {
        return reduce_node(node->mountpoint, follow_symlinks);
    }
    if (node->symlink_target != NULL && follow_symlinks == true) {
        struct path2node_res r = path2node(node->parent, node->symlink_target);
        if (r.target == NULL) {
            return NULL;
        }
        return reduce_node(r.target, follow_symlinks);
    }
    return node;
}

bool VFS::Mount(struct vfs_node *parent, const char *source, const char *target,
               const char *fs_name) {
    spinlock_acquire(&vfs_lock);

    bool ret = false;
    struct path2node_res r = {0};

    fs_mount_t fs_mount;
    if (!HASHMAP_SGET(&filesystems, fs_mount, fs_name)) {
        errno = ENODEV;
        if (r.basename != NULL) {
            free(r.basename);
        }
        spinlock_release(&vfs_lock);
        return ret;
    }

    struct vfs_node *source_node = NULL;
    if (source != NULL && strlen(source) != 0) {
        struct path2node_res rr = path2node(parent, source);
        source_node = rr.target;
        if (rr.basename != NULL) {
            free(rr.basename);
        }
        if (source_node == NULL) {

            if (r.basename != NULL) {
                free(r.basename);
            }
            spinlock_release(&vfs_lock);
            return ret;
        }
        if (S_ISDIR(source_node->resource->stat.st_mode)) {
            errno = EISDIR;
            if (r.basename != NULL) {
                free(r.basename);
            }
            spinlock_release(&vfs_lock);
            return ret;
        }
    }

    r = path2node(parent, target);

    bool mounting_root = r.target == vfs_root;

    if (r.target == NULL) {
        if (r.basename != NULL) {
            free(r.basename);
        }
        spinlock_release(&vfs_lock);
        return ret;
    }

    if (!mounting_root && !S_ISDIR(r.target->resource->stat.st_mode)) {
        //errno = EISDIR;
        if (r.basename != NULL) {
            free(r.basename);
        }
        spinlock_release(&vfs_lock);
        return ret;
    }

    struct vfs_node *mount_node = fs_mount(r.target_parent, r.basename, source_node);
    if (mount_node == NULL) {
        goto cleanup; // failed to mount
    }
    r.target->mountpoint = mount_node;

    vfs_create_dotentries(mount_node, r.target_parent);

    if (source != NULL && strlen(source) != 0) {
        log.info("Mounted `%s` on `%s` with filesystem `%s`\n", source, target, fs_name);
    } else {
        log.info("Mounted %s on `%s`\n", fs_name, target);
    }

    ret = true;

cleanup:
    if (r.basename != NULL) {
        free(r.basename);
    }
    spinlock_release(&vfs_lock);
    return ret;
}


struct vfs_node *VFS::Create(struct vfs_node *parent, const char *name, int mode) {
    spinlock_acquire(&vfs_lock);

    struct vfs_node *ret = NULL;

    struct path2node_res r = path2node(parent, name);
    struct vfs_filesystem *target_fs;
    struct vfs_node *target_node;

    if (r.target_parent == NULL) {
        goto cleanup;
    }

    if (r.target != NULL) {
        errno = EEXIST;
        goto cleanup;
    }

    target_fs = r.target_parent->filesystem;
    target_node = target_fs->create(target_fs, r.target_parent, r.basename, mode);

    HASHMAP_SINSERT(&r.target_parent->children, r.basename, target_node);

    if (S_ISDIR(target_node->resource->stat.st_mode)) {
        vfs_create_dotentries(target_node, r.target_parent);
    }

    ret = target_node;
    log.info("Created node %s on node %s mode %d, is dir: %d\n", name, parent->name, mode, S_ISDIR(mode));

cleanup:
    if (r.basename != NULL) {
        free(r.basename);
    }
    spinlock_release(&vfs_lock);
    return ret;
}

struct vfs_node *VFS::GetNode(struct vfs_node *parent, const char *path, bool follow_links) {
    spinlock_acquire(&vfs_lock);

    struct vfs_node *ret = NULL;

    struct path2node_res r = path2node(parent, path);
    if (r.target == NULL) {
        goto cleanup;
    }

    if (follow_links) {
        ret = reduce_node(r.target, true);
        goto cleanup;
    }

    ret = r.target;

cleanup:
    if (r.basename != NULL) {
        free(r.basename);
    }
    spinlock_release(&vfs_lock);
    return ret;
}