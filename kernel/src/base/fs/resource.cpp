#include <fs/resource.h>
#include <libc.h>

int resource_default_ioctl(struct resource *this2, struct f_description *description, uint64_t request, uint64_t arg) {
    (void)this2;
    (void)description;
    (void)arg;

    //switch (request) {
    //    case TCGETS:
    //    case TCSETS:
    //    case TIOCSCTTY:
    //    case TIOCGWINSZ:
    //        //errno = ENOTTY;
    //        return -1;
    //}

    //errno = EINVAL;
    return -1;
}


dev_t resource_create_dev_id(void) {
    static dev_t dev_id_counter = 1;
    static spinlock_t lock = (spinlock_t)SPINLOCK_INIT;
    spinlock_acquire(&lock);
    dev_t ret = dev_id_counter++;
    spinlock_release(&lock);
    return ret;
}

static ssize_t stub_read(struct resource *this2, struct f_description *description, void *buf, off_t offset, size_t count) {
    (void)this2;
    (void)description;
    (void)buf;
    (void)offset;
    (void)count;
    //errno = ENOSYS;
    return -1;
}

static ssize_t stub_write(struct resource *this2, struct f_description *description, const void *buf, off_t offset, size_t count) {
    (void)this2;
    (void)description;
    (void)buf;
    (void)offset;
    (void)count;
    //errno = ENOSYS;
    return -1;
}

static void *stub_mmap(struct resource *this2, size_t file_page, int flags) {
    (void)this2;
    (void)file_page;
    (void)flags;
    //errno = ENOSYS;
    return NULL;
}

static bool stub_msync(struct resource *this2, size_t file_page, void *phys, int flags) {
    (void)this2;
    (void)file_page;
    (void)phys;
    (void)flags;
    //errno = ENOSYS;
    return false;
}

static bool stub_chmod(struct resource *this2, mode_t mode) {
    this2->stat.st_mode &= ~0777;
    this2->stat.st_mode |= mode & 0777;
    return true;
}

static bool stub_ref(struct resource *this2, struct f_description *description) {
    (void)description;
    this2->refcount++;
    return true;
}

static bool stub_unref(struct resource *this2, struct f_description *description) {
    (void)this2;
    (void)description;
    this2->refcount--;
    return true;
}

static bool stub_truncate(struct resource *this2, struct f_description *description, size_t length) {
    (void)this2;
    (void)description;
    (void)length;
    //errno = ENOSYS;
    return false;
}

void *Resource::Create(size_t size) {
    struct resource *res = (resource *)malloc(size);
    if (res == NULL) {
        return NULL;
    }

    res->res_size = size;
    res->read = stub_read;
    res->write = stub_write;
    res->ioctl = resource_default_ioctl;
    res->mmap = stub_mmap;
    res->msync = stub_msync;
    res->chmod = stub_chmod;
    res->ref = stub_ref;
    res->unref = stub_unref;
    res->truncate = stub_truncate;
    return res;
}


struct f_descriptor *fd_create_from_resource(struct resource *res, int flags) {
    struct f_description *description = new struct f_description;
    struct f_descriptor *fd;
    if (description == NULL) {
        goto fail;
    }

    description->refcount = 1;
    description->flags = flags & FILE_STATUS_FLAGS_MASK;
    description->lock = (spinlock_t)SPINLOCK_INIT;
    description->res = res;

    fd = new struct f_descriptor;
    if (fd == NULL) {
        goto fail;
    }
    res->ref(res, description);
    fd->description = description;
    fd->flags = flags & FILE_DESCRIPTOR_FLAGS_MASK;
    return fd;

fail:
    if (description != NULL) {
        free(description);
    }
    return NULL;
}
#include <sched/sched.hpp>
bool fdnum_close(thread_t *proc, int fdnum, bool lock) {
    struct f_descriptor *fd;
    if (proc == NULL) {
        proc = Scheduler::GetCurrentThread();
    }

    bool ok = false;

    if (lock) {
        spinlock_acquire(&proc->fds_lock);
    }

    if (fdnum < 0 || fdnum >= MAX_FDS) {
        //errno = EBADF;
        goto cleanup;
    }

    fd = (struct f_descriptor *)proc->fds[fdnum];
    if (fd == NULL) {
        //errno = EBADF;
        goto cleanup;
    }

    fd->description->res->unref(fd->description->res, fd->description);

    if (fd->description->refcount-- == 1) {
        free(fd->description);
    }

    free(fd);

    ok = true;
    proc->fds[fdnum] = NULL;

cleanup:
    if (lock) {
        spinlock_release(&proc->fds_lock);
    }
    return ok;
}

int fdnum_create_from_fd(struct thread *_proc, struct f_descriptor *fd, int old_fdnum, bool specific) {
    thread_t *proc = (thread_t *)_proc;
    if (proc == NULL) {
        proc = Scheduler::GetCurrentThread();
    }

    int res = -1;
    spinlock_acquire(&proc->fds_lock);

    if (old_fdnum < 0 || old_fdnum >= MAX_FDS) {
        //errno = EBADF;
        goto cleanup;
    }

    if (!specific) {
        for (int i = old_fdnum; i < MAX_FDS; i++) {
            if (proc->fds[i] == NULL) {
                proc->fds[i] = (f_descriptor*)fd; // From TendingStream73: for some reason c compiler thinks that scheduler has f_descriptor
                res = i;
                goto cleanup;
            }
        }
    } else {
        fdnum_close(proc, old_fdnum, false);
        proc->fds[old_fdnum] = (f_descriptor*)fd;
        res = old_fdnum;
    }

cleanup:
    spinlock_release(&proc->fds_lock);
    return res;
}


int fdnum_create_from_resource(thread_t *proc, struct resource *res, int flags,
                               int old_fdnum, bool specific) {
    struct f_descriptor *fd = fd_create_from_resource(res, flags);
    if (fd == NULL) {
        return -1;
    }

    return fdnum_create_from_fd(proc, fd, old_fdnum, specific);
}

struct f_descriptor *fd_from_fdnum(thread_t *proc, int fdnum);

ssize_t syscall_write(int fdnum, const void *buf, size_t count) {

    //DEBUG_SYSCALL_ENTER("write(%d, %lx, %lu)", fdnum, buf, count);

    ssize_t ret = -1;

    thread_t *thread = Scheduler::GetCurrentThread();
    struct f_description *description;
    struct resource *res;
    struct f_descriptor *fd = fd_from_fdnum(thread, fdnum);
    if (fd == NULL) {
        goto cleanup;
    }

    description = fd->description;
    res = description->res;

    ret = res->write(res, description, buf, description->offset, count);
    if (ret < 0) {
        ret = -1;
        goto cleanup;
    }

    description->offset += ret;

cleanup:
    //DEBUG_SYSCALL_LEAVE("%lld", ret);
    return ret;
}

int syscall_ioctl(int fdnum, uint64_t request, uint64_t arg) {

    //DEBUG_SYSCALL_ENTER("ioctl(%d, %lu, %lx)", fdnum, request, arg);

    int ret = -1;
    struct f_description *description;
    struct resource *res;

    thread_t *proc = Scheduler::GetCurrentThread();

    struct f_descriptor *fd = fd_from_fdnum(proc, fdnum);
    if (fd == NULL) {
        goto cleanup;
    }

    description = fd->description;
    res = description->res;
    ret = res->ioctl(res, description, request, arg);

cleanup:
    //DEBUG_SYSCALL_LEAVE("%d", ret);
    return ret;
}

ssize_t syscall_read(int fdnum, void *buf, size_t count) {

    //DEBUG_SYSCALL_ENTER("read(%d, %lx, %lu)", fdnum, buf, count);

    ssize_t ret = -1;

    thread_t *proc = Scheduler::GetCurrentThread();
    struct f_description *description;
    struct resource *res;
    struct f_descriptor *fd = fd_from_fdnum(proc, fdnum);
    if (fd == NULL) {
        goto cleanup;
    }
    
    description = (f_description *) fd->description;
    res = description->res;

    ret = res->read(res, description, buf, description->offset, count);
    if (ret < 0) {
        ret = -1;
        goto cleanup;
    }

    description->offset += ret;

cleanup:
    //DEBUG_SYSCALL_LEAVE("%lld", ret);
    return ret;
}

ssize_t syscall_pread(int fdnum, void *buf, size_t count, off_t offset) {
    ssize_t ret = -1;

    thread_t *proc = Scheduler::GetCurrentThread();
    struct f_description *description;
    struct resource *res;
    struct f_descriptor *fd = fd_from_fdnum(proc, fdnum);
    if (fd == NULL) {
        goto cleanup;
    }
    
    description = (f_description *) fd->description;
    res = description->res;

    ret = res->read(res, description, buf, offset, count);
    if (ret < 0) {
        ret = -1;
        goto cleanup;
    }

cleanup:
    //DEBUG_SYSCALL_LEAVE("%lld", ret);
    return ret;
}