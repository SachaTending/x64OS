#include <vfs.hpp>
#include <libc.h>
#include <logging.hpp>
#include <limine.h>

static Logger log("Initrd unpacker");

extern limine_module_request modules;

typedef struct ustar_header {
    char name[100];             /* File name.  Null-terminated if room. */
    char mode[8];               /* Permissions as octal string. */
    char uid[8];                /* User ID as octal string. */
    char gid[8];                /* Group ID as octal string. */
    char size[12];              /* File size in bytes as octal string. */
    char mtime[12];             /* Modification time in seconds
                                   from Jan 1, 1970, as octal string. */
    char chksum[8];             /* Sum of octets in header as octal string. */
    char typeflag;              /* An enum ustar_type value. */
    char linkname[100];         /* Name of link target.
                                   Null-terminated if room. */
    char magic[6];              /* "ustar\0" */
    char version[2];            /* "00" */
    char uname[32];             /* User name, always null-terminated. */
    char gname[32];             /* Group name, always null-terminated. */
    char devmajor[8];           /* Device major number as octal string. */
    char devminor[8];           /* Device minor number as octal string. */
    char prefix[155];           /* Prefix to file name.
                                   Null-terminated if room. */
    char padding[12];           /* Pad to 512 bytes. */
} __attribute__((packed)) ustar_header_t; // Taken from https://github.com/wookayin/pintos/blob/master/src/lib/ustar.c

int oct2bin(unsigned char *str, int size) {
    int n = 0;
    unsigned char *c = str;
    while (size-- > 0) {
        n *= 8;
        n += *c - '0';
        c++;
    }
    return n;
}

/* returns file size and pointer to file data in out */
uint8_t *tar_lookup(unsigned char *archive, char *filename) {
    unsigned char *ptr = archive;

    while (!memcmp(ptr + 257, "ustar", 5)) {
        int filesize = oct2bin(ptr + 0x7c, 11);
        if (!memcmp(ptr, filename, strlen(filename) + 1)) {
            return ptr+512;
        }
        ptr += (((filesize + 511) / 512) + 1) * 512;
    }
    return 0;
}


void unpack_initrd() {
    if (modules.response == 0) {
        PANIC("No ramdisks found!\n");
    }
    for (int i=0;i<modules.response->module_count;i++) {
        ustar_header_t *archive = (ustar_header_t *)modules.response->modules[i]->address;
        if (!memcmp(archive->magic, "ustar", 5)) {
            while (!memcmp(archive->magic, "ustar", 5)) {
                int size = oct2bin((unsigned char *)archive->size, 11);
                if (!memcmp(archive->name, ".", 1)) {
                    memcpy(archive->name, archive->name+1, 99);
                }
                if ((!memcmp(archive->name, "/.", 2) or !memcmp(archive->name, "/..", 3)) && archive->typeflag == '5') {
                    archive+=512;
                    continue;
                }
                if(vfs_get_node(archive->name) == NULL) {
                    vfs_node_t *n = NULL;
                    switch (archive->typeflag)
                    {
                        case 0:
                        case '0':
                            log.info("Unpacking %s, size: %d...\n", archive->name, size);
                            n = vfs_create_file(archive->name, false);
                            if (n != NULL) {
                                uint8_t *ptr = tar_lookup((uint8_t *)archive, archive->name);
                                n->write(n, ptr, size);
                                n->close(n);
                            } else {
                                log.info("Failed to unpack %s: failed to create file\n", archive->name);
                            }
                            break;
                        case '5':
                            log.info("Creating directory %s...\n", archive->name);
                            n = vfs_create_file(archive->name, true);
                            if (n != NULL) {
                                n->close(n);
                            } else {
                                log.info("Failed to create directory %s: failed to create directory.\n", archive->name);
                            }
                            break;
                        default:
                            log.info("Failed to unpack %s: unknown type, %s\n", archive->name, archive->typeflag);
                            break;
                    }

                }
                archive += (((size + 511) / 512) + 1) * 512;
            }
        }
    }
    log.info("All ramdisks unpacked!\n");
}