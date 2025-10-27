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
