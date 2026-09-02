// This code was ported from Lyre OS
#include <stdint.h>
#include <stddef.h>
#include <fs/resource.h>
#include <logging.hpp>
#include <libc.h>
#include <krnl.hpp>

static Logger log("EXT2");

struct ext2fs_superblock {
    uint32_t inodecnt;
    uint32_t blockcnt;
    uint32_t sbrsvd;
    uint32_t unallocb;
    uint32_t unalloci;
    uint32_t sb;
    uint32_t blksize;
    uint32_t fragsize;
    uint32_t blockspergroup;
    uint32_t fragspergroup;
    uint32_t inodespergroup;
    uint32_t lastmnt; // unix epoch for last mount
    uint32_t lastwritten; // unix epoch for last write
    uint16_t mountcnt;
    uint16_t mountallowed; // are we allowed to mount this filesystem?
    uint16_t sig;
    uint16_t fsstate;
    uint16_t errorresp;
    uint16_t vermin;
    uint32_t lastfsck; // last time we cleaned the filesystem
    uint32_t forcedfsck;
    uint32_t osid;
    uint32_t vermaj;
    uint16_t uid;
    uint16_t gid;

    uint32_t first;
    uint16_t inodesize;
    uint16_t sbbgd;
    uint32_t optionalfts;
    uint32_t reqfts;
    uint32_t readonlyfts;
    char uuid[16]; // filesystem uuid
    char name[16];
    char lastmountedpath[64]; // last path we had when mounted
} __attribute__((packed));

struct ext2fs_blockgroupdesc {
    uint32_t addrblockbmp;
    uint32_t addrinodebmp;
    uint32_t inodetable;
    uint16_t unallocb;
    uint16_t unalloci;
    uint16_t dircnt;
    uint16_t unused[7];
} __attribute__((packed));

struct ext2fs_inode {
    uint16_t perms;
    uint16_t uid;
    uint32_t sizelo;
    uint32_t accesstime;
    uint32_t creationtime;
    uint32_t modifiedtime;
    uint32_t deletedtime;
    uint16_t gid;
    uint16_t hardlinkcnt;
    uint32_t sectors;
    uint32_t flags;
    uint32_t osd1;
    uint32_t blocks[15];
    uint32_t gennum;
    uint32_t eab;
    uint32_t sizehi;
    uint32_t fragaddr;
    uint32_t osd2[3];
} __attribute__((packed));

struct ext2fs_direntry {
    uint32_t inodeidx;
    uint16_t entsize;
    uint8_t namelen;
    uint8_t dirtype;
    char name[];
} __attribute__((packed));

struct ext2fs : vfs_filesystem {

    uint64_t devid;

    struct vfs_node *backing; // block device this filesystem exists on
    struct ext2fs_inode root;
    struct ext2fs_superblock sb;

    size_t blksize;
    size_t fragsize;
    size_t bgdcnt;
};

struct ext2fs_resource : resource_t {
    struct ext2fs *fs;
};


#define EXT2FS_INODESIZE(a) ({ (uint64_t)((uint64_t)(a)->sizelo | ((uint64_t)(a)->sizehi << 32)); })

static ssize_t ext2fs_bgdreadentry(struct ext2fs_blockgroupdesc *bgd, struct ext2fs *fs, uint32_t idx) {
    off_t off = fs->blksize >= 2048 ? fs->blksize : fs->blksize * 2;

    //ASSERT_MSG(fs->backing->resource->read(fs->backing->resource, NULL, bgd, off + sizeof(struct ext2fs_blockgroupdesc) * idx, sizeof(struct ext2fs_blockgroupdesc)), "ext2fs: unable to read bgd entry");
    
    int ret = fs->backing->resource->read(fs->backing->resource, NULL, bgd, off + sizeof(struct ext2fs_blockgroupdesc) * idx, sizeof(struct ext2fs_blockgroupdesc));
    if (ret == 0) {
        PANIC("EXT2: Failed to bgd entry\n");
    }
    
    return 0;
}

static ssize_t ext2fs_inodereadentry(struct ext2fs_inode *inode, struct ext2fs *fs, uint32_t inodeidx) {
    size_t tableidx = (inodeidx - 1) % fs->sb.inodespergroup;
    size_t bgdidx = (inodeidx - 1) / fs->sb.inodespergroup;

    struct ext2fs_blockgroupdesc bgd;
    memset(&bgd, 0, sizeof(ext2fs_blockgroupdesc));
    ext2fs_bgdreadentry(&bgd, fs, bgdidx);

    //ASSERT(fs->backing->resource->read(fs->backing->resource, NULL, inode, bgd.inodetable * fs->blksize + fs->sb.inodesize * tableidx, sizeof(struct ext2fs_inode)), "ext2fs: failed to read inode entry");
    int ret = fs->backing->resource->read(fs->backing->resource, NULL, inode, bgd.inodetable * fs->blksize + fs->sb.inodesize * tableidx, sizeof(struct ext2fs_inode));
    if (ret == 0) {
        PANIC("EXT2: Failed to read inode entry\n");
    }
    return 0;
}

static uint32_t ext2fs_inodegetblock(struct ext2fs_inode *inode, struct ext2fs *fs, uint32_t iblock) {
    uint32_t blockidx = 0;
    uint32_t blocklvl = fs->blksize / 4;

    if (iblock < 12) {
        blockidx = inode->blocks[iblock];
        return blockidx;
    }

    iblock -= 12;

    if (iblock >= blocklvl) {
        iblock -= blocklvl;

        uint32_t singleidx = iblock / blocklvl;
        off_t indirectoff = iblock % blocklvl;
        uint32_t indirectblock = 0;

        if (singleidx >= blocklvl) {
            iblock -= blocklvl * blocklvl; // square

            uint32_t doubleindirect = iblock / blocklvl;
            indirectoff = iblock % blocklvl;
            uint32_t singleindirectidx = 0;
            fs->backing->resource->read(fs->backing->resource, NULL, &singleindirectidx, inode->blocks[14] * fs->blksize + doubleindirect * 4, sizeof(uint32_t));
            fs->backing->resource->read(fs->backing->resource, NULL, &indirectblock, doubleindirect * fs->blksize + singleindirectidx * 4, sizeof(uint32_t));
            fs->backing->resource->read(fs->backing->resource, NULL, &blockidx, indirectblock * fs->blksize + indirectoff * 4, sizeof(uint32_t));

            return blockidx;
        }

        fs->backing->resource->read(fs->backing->resource, NULL, &indirectblock, inode->blocks[13] * fs->blksize + singleidx * 4, sizeof(uint32_t));
        fs->backing->resource->read(fs->backing->resource, NULL, &blockidx, indirectblock * fs->blksize + indirectoff * 4, sizeof(uint32_t));

        return blockidx;
    }

    fs->backing->resource->read(fs->backing->resource, NULL, &blockidx, inode->blocks[12] * fs->blksize + iblock * 4, sizeof(uint32_t));

    return blockidx;
}

static ssize_t ext2fs_inoderead(struct ext2fs_inode *inode, struct ext2fs *fs, void *buf, off_t off, size_t count) {
    if (off > (off_t)EXT2FS_INODESIZE(inode)) {
        return 0;
    }

    if ((off + count) > EXT2FS_INODESIZE(inode)) {
        count = EXT2FS_INODESIZE(inode) - off;
    }

    for (size_t head = 0; head < count;) { // force reads to be block-wise
        size_t iblock = (off + head) / fs->blksize;

        size_t size = count - head;
        off = (off + head) % fs->blksize;

        if (size > (fs->blksize - off)) {
            size = fs->blksize - off;
        }

        uint32_t block = ext2fs_inodegetblock(inode, fs, (uint32_t)iblock);
        if (fs->backing->resource->read(fs->backing->resource, NULL, (void *)((uint64_t)buf + head), block * fs->blksize + off, size) == -1) {
            return -1;
        }

        head += size;
    }

    return count;
}

static ssize_t ext2fs_resread(struct resource *_this, struct f_description *description, void *buf, off_t loc, size_t count) {
    (void)description;
    struct ext2fs_resource *thiz = (struct ext2fs_resource *)_this;
    spinlock_acquire(&thiz->lock);

    struct ext2fs_inode curinode;
    memset(&curinode, 0, sizeof(ext2fs_inode));

    ext2fs_inodereadentry(&curinode, thiz->fs, thiz->stat.st_ino);

    if ((off_t)(loc + count) > thiz->stat.st_size) {
        count = count - ((loc + count) - thiz->stat.st_size); // reading will only ever read total size!
    }

    //this->stat.st_atim = time_realtime;
    //curinode.accesstime = this->stat.st_atim.tv_sec;
    //ext2fs_inodewriteentry(&curinode, this->fs, this->stat.st_ino);

    ssize_t ret = ext2fs_inoderead(&curinode, thiz->fs, buf, loc, count);
    spinlock_release(&thiz->lock);
    return ret;
}

static struct vfs_node *ext2fs_create(struct vfs_filesystem *_this, struct vfs_node *parent, const char *name, int mode) {
    log.warn("ext2fs_create(0x%016lx, 0x%016lx, %s, %d): STUB\n", _this, parent, name, mode);
    return 0;
}

static void ext2fs_populate(struct vfs_filesystem *_this, struct vfs_node *node) {
    struct ext2fs *fs = (struct ext2fs *)_this;
    struct ext2fs_inode parent;
    memset(&parent, 0, sizeof(ext2fs_inode));
    ext2fs_inodereadentry(&parent, fs, node->resource->stat.st_ino);
    void *buf = malloc(EXT2FS_INODESIZE(&parent));
    ext2fs_inoderead(&parent, fs, buf, 0, EXT2FS_INODESIZE(&parent));
    log.debug("inode size: %lu\n", EXT2FS_INODESIZE(&parent));
    for (size_t i = 0; i < EXT2FS_INODESIZE(&parent);) {
        struct ext2fs_direntry *direntry = (struct ext2fs_direntry *)((uint64_t)buf + i);

        struct vfs_node *fnode = 0;
        struct ext2fs_resource *fres = 0;
        uint16_t mode = 0;
        struct ext2fs_inode inode;

        char *namebuf = (char *)malloc(direntry->namelen + 1);
        strncpy(namebuf, direntry->name, direntry->namelen);

        log.debug("file name: %s\n", namebuf);
        if (direntry->inodeidx == 0) {
            goto next;
        }

        if (!strcmp(namebuf, ".") || !strcmp(namebuf, "..")) {
            i += direntry->entsize; // vfs already handles creating these
            continue;
        }
        memset(&inode, 0, sizeof(struct ext2fs_inode));
        ext2fs_inodereadentry(&inode, fs, direntry->inodeidx);

        mode = (inode.perms & 0xFFF) |
            (direntry->dirtype == 1 ? S_IFREG :
             direntry->dirtype == 2 ? S_IFDIR :
             direntry->dirtype == 3 ? S_IFCHR :
             direntry->dirtype == 4 ? S_IFBLK :
             direntry->dirtype == 5 ? S_IFIFO :
             direntry->dirtype == 6 ? S_IFSOCK :
             S_IFLNK);
        fnode = VFS::CreateNode((struct vfs_filesystem *)fs, node, namebuf, S_ISDIR(mode));
        fres = CREATE_RESOURCE(struct ext2fs_resource);

        if (S_ISREG(mode)) {
            fres->can_mmap = true;
        }

        // TODO
        fres->read = ext2fs_resread;
        //fres->write = ext2fs_reswrite;
        //fres->truncate = ext2fs_restruncate;
        //fres->mmap = ext2fs_resmmap;
        //fres->msync = ext2fs_resmsync;
        //fres->chmod = ext2fs_reschmod;
        //fres->unref = ext2fs_resunref;

        fres->stat.st_uid = inode.uid;
        fres->stat.st_gid = inode.gid;
        fres->stat.st_mode = mode;
        fres->stat.st_ino = direntry->inodeidx;
        fres->stat.st_size = EXT2FS_INODESIZE(&inode);
        fres->stat.st_nlink = inode.hardlinkcnt;
        fres->refcount = 1; // exclude dot entries in directory inode links (our vfs relies on its own implementation)
        fres->stat.st_blksize = fs->blksize;
        fres->stat.st_blocks = fres->stat.st_size / fs->blksize;

        fres->stat.st_atim = (struct timespec) { .tv_sec = inode.accesstime, .tv_nsec = 0 };
        fres->stat.st_ctim = (struct timespec) { .tv_sec = inode.creationtime, .tv_nsec = 0 };
        fres->stat.st_mtim = (struct timespec) { .tv_sec = inode.modifiedtime, .tv_nsec = 0 };

        fres->fs = fs;

        fnode->resource = (struct resource *)fres;
        fnode->populated = false;

        HASHMAP_SINSERT(&fnode->parent->children, namebuf, fnode);

        if (S_ISDIR(mode)) {
            //vfs_create_dotentries(fnode, node); // set up for correct directory structure
        }

        if (S_ISLNK(mode)) {
            //char *linkbuffer = alloc(EXT2FS_INODESIZE(&inode));
            //ext2fs_readlink(&inode, fs, linkbuffer); // implicit acceptance of link buffer length being the same as the inode
            //fnode->symlink_target = strdup(linkbuffer);
            //free(linkbuffer); // free current context
        }
next:
        free(namebuf);
        i += direntry->entsize;
    }

    node->populated = true; // we already populated this node with all existing files
    free(buf);
}
static inline struct vfs_filesystem *ext2fs_instantiate(void) {
    struct ext2fs *new_fs = new struct ext2fs;
    if (new_fs == NULL) {
        return NULL;
    }

    new_fs->create = ext2fs_create;
    new_fs->populate = ext2fs_populate;
    //new_fs->symlink = ext2fs_symlink;
    //new_fs->link = ext2fs_link;

    return (struct vfs_filesystem *)new_fs;
}

static struct vfs_node *ext2fs_mount(struct vfs_node *parent, const char *name, struct vfs_node *source) {

    struct ext2fs *new_fs = (struct ext2fs *)ext2fs_instantiate();

    if (new_fs == NULL) {
        return NULL; // failed to instantiate filesystem
    }

    source->resource->read(source->resource, NULL, &new_fs->sb, source->resource->stat.st_blksize * 2, sizeof(struct ext2fs_superblock));

    if (new_fs->sb.sig != 0xef53) {
        log.error("Failed to mount %s: Invalid signature\n", name);
        free(new_fs);
        return NULL; // signature is not correct
    }

    if (new_fs->sb.vermaj < 1) {
        log.error("Failed to mount %s: Version not supported\n", name);
        free(new_fs);
        return NULL; // only supports newer revisions of the filesystem
    }

    new_fs->backing = source;
    new_fs->blksize = 1024 << new_fs->sb.blksize;
    new_fs->fragsize = 1024 << new_fs->sb.fragsize;
    new_fs->bgdcnt = new_fs->sb.blockcnt / new_fs->sb.blockspergroup;

    log.info("EXT2 Info on %s:\n", name);
    log.info("  - Block size: %lu\n", new_fs->blksize);

    if (ext2fs_inodereadentry(&new_fs->root, new_fs, 2)) {
        log.error("Unable to read root inode, aborted\n");
        free(new_fs);
        return NULL;
    }

    log.info("Creating node...\n");
    struct vfs_node *node = VFS::CreateNode((struct vfs_filesystem *)new_fs, parent, name, true);
    if (node == NULL) {
        free(new_fs);
        return NULL;
    }

    log.info("Creating resource...\n");
    struct ext2fs_resource *resource = CREATE_RESOURCE(struct ext2fs_resource);
    if (resource == NULL) {
        // TODO: Free node
        free(new_fs);
        return NULL;
    }

    resource->stat.st_size = new_fs->root.sizelo | ((uint64_t)new_fs->root.sizehi >> 32);
    resource->stat.st_blksize = new_fs->blksize;
    resource->stat.st_blocks = resource->stat.st_size / resource->stat.st_blksize;
    resource->stat.st_dev = source->resource->stat.st_rdev; // assign to device id of source device
    resource->stat.st_mode = 0644 | S_IFDIR;
    resource->stat.st_nlink = new_fs->root.hardlinkcnt;
    resource->stat.st_ino = 2; // root inode

    //resource->stat.st_atim = time_realtime;
    //resource->stat.st_ctim = time_realtime;
    //resource->stat.st_mtim = time_realtime;

    resource->fs = new_fs;

    node->resource = (struct resource *)resource;

    log.info("ext2fs_mount(parent=0x%016lx, name=%s, source=%s) ret=0x%016lx\n", parent, name, source, node);
    return node; // root node (will become child of parent)
}

void ext2fs_init(void) {
    VFS::AddFilesystem(ext2fs_mount, "ext2fs");
}
