// THIS CODE IS COPIED FROM Lyre OS by mintsuki AND MODIFIED BY ME TO USE C++
#include <vfs.hpp>
#include <libc.h>
#include <spinlock.h>
#include <fs/resource.h>
#include <sys/stat.h>
#include <logging.hpp>
#include <sched/sched.hpp>

static Logger log("VFS");

spinlock_t vfs_lock = SPINLOCK_INIT;

struct vfs_node *VFS::CreateNode(struct vfs_filesystem *fs, struct vfs_node *parent,
                                 const char *name, bool dir) {
    struct vfs_node *node = new vfs_node_t;
    //log.debug("VFS::CreateNode(0x%lx, 0x%lx, \"%s\", %d);\n", fs, parent, name, dir);
    node->name = (char *)strdup(name);

    node->parent = parent;
    node->filesystem = fs;

    if (dir) {
        node->children = (typeof(node->children))HASHMAP_INIT(256);
    }
    //log.info("VFS::CreateNode(0x%lx, 0x%lx, \"%s\", %d); ret=0x%lx\n", fs, parent, name, dir, node);
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
    log.info("Registered new filesystem: %s\n", identifier);

    spinlock_release(&vfs_lock);
}
void tmpfs_init(void);
void devtmpfs_init(void);
void VFS::Init(void) {
    log.debug("VFS::Init();\n");
    vfs_root = VFS::CreateNode(NULL, NULL, "", false);

    filesystems = (typeof(filesystems))HASHMAP_INIT(256);
    tmpfs_init();
    devtmpfs_init();
    log.debug("VFS::Init(); done\n");
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
    EEXIST,
    ELOOP
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
    //printf("path2node(0x%lx, %s); called\n", parent, path);
    if (parent == 0) parent = vfs_root;
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
        elem_str[elem_len] = '\0';
        //log.debug("path2node: elem=%s\n", elem_str);

        current_node = reduce_node(current_node, false);

        struct vfs_node *new_node;

        // XXX put a lock around this guy
        // XXX page fault here (seemingly random)
        if (!HASHMAP_SGET(&current_node->children, new_node, elem_str)) {
            errno = ENOENT;
            log.debug("path2node: returning ENOENT\n");
            if (last) {
                return (struct path2node_res){current_node, NULL, elem_str};
            }
            free(elem_str);
            return (struct path2node_res){NULL, NULL, NULL};
        }

        new_node = reduce_node(new_node, false);
        if (!populate(new_node)) {
            log.debug("path2node: populate failed\n");
            free(elem_str);
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
                free(elem_str);
                return (struct path2node_res){NULL, NULL, NULL};
            }
            current_node = r.target;
        }

        if (!S_ISDIR(current_node->resource->stat.st_mode)) {
            errno = ENOTDIR;
            free(elem_str);
            return (struct path2node_res){NULL, NULL, NULL};
        }
        free(elem_str);
    }

    errno = ENOENT;
    log.debug("path2node(0x%lx, %s): ENOENT\n");
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
    //log.info("Created node %s on node %s mode %d, is dir: %d\n", name, parent->name, mode, S_ISDIR(mode));

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

struct f_descriptor *fd_from_fdnum(thread_t *proc, int fdnum) {
    if (proc == NULL) {
        proc = Scheduler::GetCurrentThread();
    }

    struct f_descriptor *ret = NULL;
    spinlock_acquire(&proc->fds_lock);

    if (fdnum < 0 || fdnum >= MAX_FDS) {
        printf("fd_from_fdnum(0x%016lx, %d): invalid fd\n", proc, fdnum);
        //errno = EBADF;
        goto cleanup;
    }

    ret = (struct f_descriptor *)proc->fds[fdnum];
    if (ret == NULL) {
        printf("fd_from_fdnum(0x%016lx, %d): invalid fd, it just doesn't exist\n", proc, fdnum);
        //errno = EBADF;
        goto cleanup;
    }

    ret->description->refcount++;

cleanup:
    spinlock_release(&proc->fds_lock);
    return ret;
}

static struct vfs_node *get_parent_dir(int dir_fdnum, const char *path) {
    thread_t *thr = Scheduler::GetCurrentThread();
    log.debug("get_parent_dir(%d, %s): thread's cwd: 0x%lx\n", dir_fdnum, path, thr->cwd);

    if (path != NULL && *path == '/') {
        return vfs_root;
    }

    if (dir_fdnum == AT_FDCWD) {
        return thr->cwd;
    }

    struct f_descriptor *fd = fd_from_fdnum(thr, dir_fdnum);
    if (fd == NULL) {
        return NULL;
    }

    struct f_description *description = fd->description;
    if (!S_ISDIR(description->res->stat.st_mode)) {
        errno = ENOTDIR;
        return NULL;
    }

    return description->node;
}

bool vfs_fdnum_path_to_node(int dir_fdnum, const char *path, bool empty_path, bool enoent_error,
                            struct vfs_node **parent, struct vfs_node **node, char **basename) {
    log.debug("vfs_fdnum_path_to_node(%d, %s, %s, %d, 0x%lx, 0x%lx, 0x%lx);\n", dir_fdnum, path, empty_path, enoent_error, parent, node, basename);
    if (!empty_path && (path == NULL || strlen(path) == 0)) {
        errno = ENOENT;
        return false;
    }

    struct vfs_node *parent_node = get_parent_dir(dir_fdnum, path);
    if (parent == NULL) {
        return false;
    }

    log.debug("vfs_fdnum_path_to_node(%d, %s, %s, %d, 0x%lx, 0x%lx, 0x%lx); parent_node=0x%lx\n", dir_fdnum, path, empty_path, enoent_error, parent, node, basename, parent_node);

    struct path2node_res res = path2node(parent_node, path);
    if (res.target == NULL && (errno == ENOENT && enoent_error)) {
        log.debug("vfs_fdnum_path_to_node(%d, %s, %s, %d, 0x%lx, 0x%lx, 0x%lx); returning false bcz path2node returnet ENOENT\n", dir_fdnum, path, empty_path, enoent_error, parent, node, basename);
        return false;
    }
    log.debug("vfs_fdnum_path_to_node: res.target_parent=0x%lx\n", res.target_parent);

    if (parent != NULL) {
        *parent = res.target_parent;
    }

    if (node != NULL) {
        *node = res.target;
    }

    if (basename != NULL) {
        *basename = res.basename;
    } else {
        if (res.basename != NULL) {
            free(res.basename);
        }
    }
    log.debug("vfs_fdnum_path_to_node: ret: parent=0x%lx, node=0x%lx, basename=0x%lx\n", *parent, *node, *basename);

    return true;
}

int force_openat(int dir_fdnum, const char *path, int flags, int mode, thread_t *thread) {
    int ret = -1;

    struct vfs_node *parent = NULL;
    char *basename = NULL;

    int create_flags, follow_links;
    struct vfs_node *node;
    struct f_descriptor *fd;

        if (!vfs_fdnum_path_to_node(dir_fdnum, path, false, false, &parent, NULL, &basename)) {
        goto cleanup;
    }
    log.debug("%s's parent: 0x%lx\n", path, parent);

    if (parent == NULL) {
        errno = ENOENT;
        goto cleanup;
    }

    create_flags = flags & FILE_CREATION_FLAGS_MASK;
    follow_links = (flags & O_NOFOLLOW) == 0;

    node = VFS::GetNode(parent, basename, follow_links);
    if (node == NULL) {
        if ((create_flags & O_CREAT) != 0) {
            //node = VFS::Create(parent, basename, (mode & ~proc->umask) | S_IFREG);
            node = VFS::Create(parent, basename, (mode) | S_IFREG);
        } else {
            errno = ENOENT;
            goto cleanup;
        }
    }

    if (node == NULL) {
        goto cleanup;
    }

    if (S_ISLNK(node->resource->stat.st_mode)) {
        errno = ELOOP;
        goto cleanup;
    }

    node = reduce_node(node, true);
    if (node == NULL) {
        goto cleanup;
    }

    if (!S_ISDIR(node->resource->stat.st_mode) && (flags & O_DIRECTORY) != 0) {
        errno = ENOTDIR;
        goto cleanup;
    }

    fd = fd_create_from_resource(node->resource, flags);
    if (fd == NULL) {
        goto cleanup;
    }

    if ((flags & O_TRUNC) != 0 && S_ISREG(node->resource->stat.st_mode)) {
        node->resource->truncate(node->resource, fd->description, 0);
    }

    fd->description->node = node;
    ret = fdnum_create_from_fd((struct thread *)thread, fd, 0, false);

cleanup:
    if (basename != NULL) {
        free(basename);
    }

    //DEBUG_SYSCALL_LEAVE("%d", ret);
    return ret;
} 

int syscall_openat(int dir_fdnum, const char *path, int flags, int mode) {
    //DEBUG_SYSCALL_ENTER("openat(%d, %s, %x, %o)", dir_fdnum, path, flags, mode);

    int ret = -1;

    auto *thread = Scheduler::GetCurrentThread();

    struct vfs_node *parent = NULL;
    char *basename = NULL;

    int create_flags, follow_links;
    struct vfs_node *node;
    struct f_descriptor *fd;

    if (!vfs_fdnum_path_to_node(dir_fdnum, path, false, false, &parent, NULL, &basename)) {
        goto cleanup;
    }
    log.debug("%s's parent: 0x%lx\n", path, parent);

    if (parent == NULL) {
        errno = ENOENT;
        goto cleanup;
    }

    create_flags = flags & FILE_CREATION_FLAGS_MASK;
    follow_links = (flags & O_NOFOLLOW) == 0;

    node = VFS::GetNode(parent, basename, follow_links);
    if (node == NULL) {
        if ((create_flags & O_CREAT) != 0) {
            //node = VFS::Create(parent, basename, (mode & ~proc->umask) | S_IFREG);
            node = VFS::Create(parent, basename, (mode) | S_IFREG);
        } else {
            errno = ENOENT;
            goto cleanup;
        }
    }

    if (node == NULL) {
        goto cleanup;
    }

    if (S_ISLNK(node->resource->stat.st_mode)) {
        errno = ELOOP;
        goto cleanup;
    }

    node = reduce_node(node, true);
    if (node == NULL) {
        goto cleanup;
    }

    if (!S_ISDIR(node->resource->stat.st_mode) && (flags & O_DIRECTORY) != 0) {
        errno = ENOTDIR;
        goto cleanup;
    }

    fd = fd_create_from_resource(node->resource, flags);
    if (fd == NULL) {
        goto cleanup;
    }

    if ((flags & O_TRUNC) != 0 && S_ISREG(node->resource->stat.st_mode)) {
        node->resource->truncate(node->resource, fd->description, 0);
    }

    fd->description->node = node;
    ret = fdnum_create_from_fd((struct thread *)thread, fd, 0, false);

cleanup:
    if (basename != NULL) {
        free(basename);
    }

    //DEBUG_SYSCALL_LEAVE("%d", ret);
    return ret;
}