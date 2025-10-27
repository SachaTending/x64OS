#include <limine.h>
#include <logging.hpp>
#include <libc.h>
#include <sys/time.h>
#include <fs/resource.h>
#include <vfs.hpp>

static Logger log("Initrd unpacker");

extern struct limine_module_request module_request;

#define TAR_FILE_TYPE_NORMAL '0'
#define TAR_FILE_TYPE_HARD_LINK '1'
#define TAR_FILE_TYPE_SYMLINK '2'
#define TAR_FILE_TYPE_CHAR_DEV '3'
#define TAR_FILE_TYPE_BLOCK_DEV '4'
#define TAR_FILE_TYPE_DIRECTORY '5'
#define TAR_FILE_TYPE_FIFO '6'
#define TAR_FILE_TYPE_GNU_LONG_PATH 'L'

struct tar {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char type;
    char link_name[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char dev_major[8];
    char dev_minor[8];
    char prefix[155];
};

static inline uint64_t oct2int(const char *str, size_t len) {
    uint64_t value = 0;
    while (*str && len > 0) {
        value = value * 8 + (*str++ - '0');
        len--;
    }
    return value;
}
extern "C" void pmm_free(void *addr, size_t pages);
static void unpack_addr(void *file_addr) {
    struct tar *current_file = (struct tar *)file_addr;
    char *name_override = NULL;
    while (strncmp(current_file->magic, "ustar", 5) == 0) {
        char *name = current_file->name;
        char *link_name = current_file->link_name;
        if (name_override != NULL) {
            name = name_override;
            name_override = NULL;
        }

        if (strcmp(name, "./") == 0) {
            continue;
        }

        uint64_t mode = oct2int(current_file->mode, sizeof(current_file->mode));
        uint64_t size = oct2int(current_file->size, sizeof(current_file->size));
        uint64_t mtime = oct2int(current_file->mtime, sizeof(current_file->mtime));
        // uint64_t uid = oct2int(current_file->uid, sizeof(current_file->uid));
        // uint64_t gid = oct2int(current_file->gid, sizeof(current_file->gid));

        struct vfs_node *node = NULL;
        switch (current_file->type) {
            case TAR_FILE_TYPE_NORMAL: {
                node = VFS::Create(vfs_root, name, mode | S_IFREG);
                if (node == NULL) {
                    //panic(NULL, true, "Failed to allocate an initramfs node");
                }

                struct resource *resource = node->resource;
                //ASSERT(resource->write(resource, NULL, (void *)current_file + 512, 0, size) == (ssize_t)size);
                resource->write(resource, NULL, (void *)current_file + 512, 0, size);
                break;
            }
            case TAR_FILE_TYPE_SYMLINK: {
                //node = vfs_symlink(vfs_root, link_name, name);
                //if (node == NULL) {
                //    panic(NULL, true, "Failed to allocate an initramfs node");
                //}
                break;
            }
            case TAR_FILE_TYPE_DIRECTORY: {
                node = VFS::Create(vfs_root, name, mode | S_IFDIR);
                if (node == NULL) {
                    //panic(NULL, true, "Failed to allocate an initramfs node");
                }
                break;
            }
            case TAR_FILE_TYPE_GNU_LONG_PATH:
                name_override = (void *)current_file + 512;
                name_override[size] = 0;
                break;
        }

        if (node != NULL) {
            //node->resource->stat.st_mtim = (struct timespec){.tv_sec = mtime, .tv_nsec = 0};
        }

        pmm_free((void *)current_file - VMM_HIGHER_HALF, (512 + ALIGN_UP(size, 512)) / PAGE_SIZE);

        current_file = (tar *)(void *)current_file + 512 + ALIGN_UP(size, 512);
    }
}

void unpack_initrd() {
    for (int i=0;i<module_request.response->module_count;i++) {
        log.info("Module %d: %s\n", i, module_request.response->modules[i]->path);
        unpack_addr(module_request.response->modules[i]->address);
    }
    log.info("All modules has been unpacked!\n");
}