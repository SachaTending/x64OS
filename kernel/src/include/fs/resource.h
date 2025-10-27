#pragma once
#include <stdint.h>
#include <stddef.h>
#include <spinlock.h>
#include <sys/stat.h>

struct process;
struct f_description;
typedef size_t ssize_t;

struct resource {
    int res_size;
    int status;
    //struct event event;
    size_t refcount;
    spinlock_t lock;
    struct stat stat;
    bool can_mmap;

    ssize_t (*read)(struct resource *this2, struct f_description *description, void *buf, off_t offset, size_t count);
    ssize_t (*write)(struct resource *this2, struct f_description *description, const void *buf, off_t offset, size_t count);
    int (*ioctl)(struct resource *this2, struct f_description *description, uint64_t request, uint64_t arg);
    void *(*mmap)(struct resource *this2, size_t file_page, int flags);
    bool (*msync)(struct resource *this2, size_t file_page, void *phys, int flags);
    bool (*chmod)(struct resource *this2, mode_t mode);
    bool (*unref)(struct resource *this2, struct f_description *description);
    bool (*ref)(struct resource *this2, struct f_description *description);
    bool (*truncate)(struct resource *this2, struct f_description *description, size_t length);
};

struct f_description {
    size_t refcount;
    off_t offset;
    bool is_dir;
    int flags;
    spinlock_t lock;
    struct resource *res;
    struct vfs_node *node;
};

struct f_descriptor {
    struct f_description *description;
    int flags;
};

namespace Resource
{
    void *Create(size_t size);
} // namespace Resource
