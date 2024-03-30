#include <stddef.h>
#include <frg/std_compat.hpp>
#include <frg/vector.hpp>
#include <stdint.h>
#include <libc.h>
#include <new>

typedef void (*get_file_t)(struct vfs_mnt *mnt);

typedef struct vfs_mnt {
    const char *path;
    void *priv;
    get_file_t get_file;
} vfs_mnt_t;

typedef bool (*probe_fs_t)(struct vfs_fs *fs, const char *block);
typedef vfs_mnt_t *(*mnt_fs_t)(struct vfs_fs *fs, const char *block);

typedef struct vfs_fs {
    const char *name;
    void *priv;
    probe_fs_t probe_fs;
    mnt_fs_t mnt_fs;
} vfs_fs_t;

struct liballoc {
	void *allocate(size_t size) {
		return malloc(size);
	}

	void deallocate(void *ptr, size_t size) {
		free(ptr);
	}

	void free(void *ptr) {
		free(ptr);
	}
};

typedef frg::vector<uintptr_t, frg::stl_allocator> vfs_vec;

#define vec_get(ind) ((vfs_mnt_t *)vec[ind])

vfs_vec vec = vfs_vec();

void vfs_mount(const char *block, const char *mnt_path, vfs_fs_t *fs) {
    if (!fs) {
        return; // safety first
    }
    if (!fs->probe_fs(fs, block)) {
        return; // fs probe failed
    }
    for (size_t ind=0;ind<vec.size();ind++) {
        if (strlen(mnt_path) == strlen((vec_get(ind))->path)) {
            if (!strcmp(mnt_path, vec_get(ind)->path)) {
                return;
            }
        }
    }
    vfs_mnt_t *mnt = fs->mnt_fs(fs, block);
    mnt->path = strdup(mnt_path);
    vec.push_back((uintptr_t)mnt);
}

void vfs_get_file(const char *path) {
    size_t big_size = 0;
    size_t path_len = strlen(path);
    vfs_mnt_t *m;
    for (size_t i=0;i<vec.size();i++) {
        if (path_len < strlen(vec_get(i)->path)) {
            continue;
        }
        if (!strcmp(vec_get(i)->path, path)) {
            big_size = strlen(vec_get(i)->path);
            m = vec_get(i);
        }
    }
}

void vfs_init() {
    
}