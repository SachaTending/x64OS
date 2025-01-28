#pragma once
#include <stdint.h>
#include <stddef.h>
#include <frg/std_compat.hpp>
#include <frg/vector.hpp>
#include <new>
#include <vfs.hpp>
#include <vfs.hpp>

typedef frg::vector<struct mmap_range_local *, frg::stl_allocator> locals_vec_t;

struct mmap_range_global {
    struct pagemap *shadow_pagemap;
    locals_vec_t locals;
    vfs_node_t *node;
    uintptr_t base;
    size_t length;
    size_t offset;
};

struct mmap_range_local {
    struct pagemap *pagemap;
    struct mmap_range_global *global;
    uintptr_t base;
    size_t length;
    size_t offset;
    int prot;
    int flags;
};

#define MAP_PRIVATE   0b001
#define MAP_FIXED     0b010
#define MAP_ANONYMOUS 0b100

#define PROT_WRITE 0b001
#define PROT_READ  0b010
#define PROT_EXEC  0b100

void *mmap(struct pagemap *pagemap, uintptr_t addr, size_t length, int prot,
           int flags, vfs_node_t *node, size_t offset);