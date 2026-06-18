// The following code was ported from lyre os project
#include <stdint.h>
#include <fs/resource.h>
#include <logging.hpp>
#include <fs/devtmpfs.h>

static Logger log("Partition parser");

struct mbr_entry {
    uint8_t status;
    uint8_t start[3];
    uint8_t type;
    uint8_t end[3];
    uint32_t startsect;
    uint32_t sectors;
};

#define GPT_IMPORTANT 1
#define GPT_DONTMOUNT 2
#define GPT_LEGACY 4

struct gpt_header {
    char sig[8];
    uint32_t rev;
    uint32_t len;
    uint32_t crc32;
    uint32_t unused;

    uint64_t lba;
    uint64_t altlba;
    uint64_t first;
    uint64_t last;

    uint64_t guidlow;
    uint64_t guidhi;

    uint64_t partlba;
    uint32_t entries;
    uint32_t entrysize;
    uint32_t crc32arr;
};

struct gpt_entry {
    uint64_t typelow;
    uint64_t typehi;

    uint64_t unilow;
    uint64_t unihi;

    uint64_t start;
    uint64_t end;

    uint64_t attr;

    uint16_t name[36]; // 36 UTF-16 unicode chars
};

struct partition_device : resource_t {
    uint64_t start;
    uint64_t sectors;
    uint16_t blksize;
    struct resource *root; // root block device
};

// wrappers for partition offset read/writes
static ssize_t readpart(struct resource *_this, struct f_description *description, void *buf, off_t loc, size_t count) {
    struct partition_device *thiz = (struct partition_device *)_this;
    if ((size_t)loc >= thiz->sectors * thiz->blksize) {
        return -1;
    }
    return thiz->root->read(thiz->root, description, buf, loc + thiz->start * thiz->blksize, count);
}

static ssize_t writepart(struct resource *_this, struct f_description *description, const void *buf, off_t loc, size_t count) {
    struct partition_device *thiz = (struct partition_device *)_this;
    if ((size_t)loc >= thiz->sectors * thiz->blksize) {
        return -1;
    }
    return thiz->root->write(thiz->root, description, buf, loc + thiz->start * thiz->blksize, count);
}

void partition_enum(struct resource *root, const char *rootname, uint16_t blocksize, const char *convention) {
    struct gpt_header header;
    off_t loc = 512;
    root->read(root, NULL, &header, loc, sizeof(header));
    loc += sizeof(header);

        if (strncmp(header.sig, "EFI PART", 8)) {
        goto mbr; // not GPT either
    }
    if (header.len < 92) {
        goto mbr; // incorrect length
    }
    if (header.len > root->stat.st_size) {
        goto mbr; // segmented header
    }
    if (header.lba != 1) {
        goto mbr; // should be first
    }
    if (header.first > header.last) {
        goto mbr; // borked header
    }
    log.info("%s uses gpt scheme\n", rootname);

    struct gpt_entry entry;
    loc = header.partlba * 512;
    for (size_t i = 0; i < header.entries; i++) {
        root->read(root, NULL, &entry, loc, sizeof(entry));
        loc += sizeof(entry);

        if (entry.unilow == 0 && entry.unihi == 0) {
            continue;
        }

        if (entry.attr & (GPT_DONTMOUNT | GPT_LEGACY)) {
            continue; // skip this
        }

        log.info("GPT: p%u start: %u (+%u)\n", i + 1, entry.start, entry.end - entry.start);
        struct partition_device *part_res = (partition_device *)Resource::Create(sizeof(struct partition_device));
        part_res->root = root;
        part_res->blksize = blocksize;
        part_res->start = entry.start;
        part_res->sectors = (entry.end - entry.start) / blocksize;

        part_res->stat.st_blksize = blocksize;
        part_res->stat.st_size = entry.end - entry.start;
        part_res->stat.st_blocks = (entry.end - entry.start) / blocksize;
        part_res->stat.st_rdev = resource_create_dev_id();
        part_res->stat.st_mode = 0666 | S_IFBLK;
        part_res->can_mmap = false;
        part_res->write = writepart;
        part_res->read = readpart;
        part_res->ioctl = resource_default_ioctl;
        char partname[64];
        snprintf(partname, sizeof(partname) - 1, convention, rootname, i + 1);
        devtmpfs_add_device((struct resource *)part_res, partname);
    }
    return;

mbr:
    log.info("%s probably uses mbr partition scheme\n", rootname);
    uint16_t hint;
    loc = 444;
    root->read(root, NULL, &hint, loc, 2);
    loc += 2;
    if (hint && hint != 0x5a5a) {
        return;
    }
    struct mbr_entry entries[4];
    root->read(root, NULL, entries, loc, sizeof(struct mbr_entry) * 4);

    for (size_t i = 0; i < 4; i++) {
        if (!entries[i].type) {
            continue;
        }
        log.info("MBR: p%u start: %u (+%u)\n", i + 1, entries[i].startsect, entries[i].sectors);

        struct partition_device *part_res = (partition_device *)Resource::Create(sizeof(struct partition_device));
        part_res->root = root;
        part_res->blksize = blocksize;
        part_res->start = entries[i].startsect;
        part_res->sectors = entries[i].sectors;

        part_res->stat.st_blksize = blocksize;
        part_res->stat.st_size = entries[i].sectors * blocksize;
        part_res->stat.st_blocks = entries[i].sectors;
        part_res->stat.st_rdev = resource_create_dev_id();
        part_res->stat.st_mode = 0666 | S_IFBLK;
        part_res->can_mmap = false;
        part_res->write = writepart;
        part_res->read = readpart;
        part_res->ioctl = resource_default_ioctl;
        char partname[64];
        snprintf(partname, sizeof(partname) - 1, convention, rootname, i + 1);
        devtmpfs_add_device((struct resource *)part_res, partname);
    }
    return;
}