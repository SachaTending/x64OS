#include <drv_subsys.hpp>
#include <logging.hpp>
#include <fs/resource.h>
#include <fs/devtmpfs.h>
#include <drivers/part_parser.hpp>

// This source code was ported from lyre os project

static Logger log("NVME");

int nvme_controller_num = 0;

struct nvme_bar {
    uint64_t capabilities; // 0x00-0x07
    uint32_t version; // 0x08-0x0B
    uint32_t intms; // 0x0C-0x0F (interrupt mask set)
    uint32_t intmc; // 0x10-0x13 (interrupt mask clear)
    uint32_t conf; // 0x14-0x17
    uint32_t unused1;
    uint32_t status; // 0x1C-0x1F
    uint32_t unused2;
    uint32_t aqa; // 0x24-0x27 (admin queue attrs)
    uint64_t asq; // 0x28-0x2F (admin submit queue)
    uint64_t acq; // 0x30-0x37 (admin completion queue)
};

struct nvme_id {
    uint16_t vid;
    uint16_t ssvid;
    char sn[20];
    char mn[40];
    char fr[8];
    uint8_t rab;
    uint8_t ieee[3];
    uint8_t mic;
    uint8_t mdts;
    uint16_t ctrlid;
    uint32_t version;
    uint32_t unused1[43];
    uint16_t oacs;
    uint8_t acl;
    uint8_t aerl;
    uint8_t fw; // firmware5
    uint8_t lpa;
    uint8_t elpe;
    uint8_t npss;
    uint8_t avscc;
    uint8_t apsta;
    uint16_t wctemp;
    uint16_t cctemp;
    uint16_t unused2[121];
    uint8_t sqes;
    uint8_t cqes;
    uint16_t unused3;
    uint32_t nn;
    uint16_t oncs;
    uint16_t fuses;
    uint8_t fna;
    uint8_t vwc;
    uint16_t awun;
    uint16_t awupf;
    uint8_t nvscc;
    uint8_t unused4;
    uint16_t acwu;
    uint16_t unused5;
    uint32_t sgls;
    uint32_t unused6[1401];
    uint8_t vs[1024];
};

struct nvme_cmd {
    union {
        struct {
            uint8_t opcode;
            uint8_t flags;
            uint16_t cid;
            uint32_t nsid;
            uint32_t cdw1[2];
            uint64_t metadata;
            uint64_t prp1;
            uint64_t prp2;
            uint32_t cdw2[6];
        } common; // generic command
        struct {
            uint8_t opcode;
            uint8_t flags;
            uint16_t cid;
            uint32_t nsid;
            uint64_t unused;
            uint64_t metadata;
            uint64_t prp1;
            uint64_t prp2;
            uint64_t slba;
            uint16_t len;
            uint16_t control;
            uint32_t dsmgmt;
            uint32_t ref;
            uint16_t apptag;
            uint16_t appmask;
        } rw; // read or write
        struct {
            uint8_t opcode;
            uint8_t flags;
            uint16_t cid;
            uint32_t nsid;
            uint64_t unused1;
            uint64_t unused2;
            uint64_t prp1;
            uint64_t prp2;
            uint32_t cns;
            uint32_t unused3[5];
        } identify; // identify
        struct {
            uint8_t opcode;
            uint8_t flags;
            uint16_t cid;
            uint32_t nsid;
            uint64_t unused1;
            uint64_t unused2;
            uint64_t prp1;
            uint64_t prp2;
            uint32_t fid;
            uint32_t dword;
            uint64_t unused[2];
        } features;
        struct {
            uint8_t opcode;
            uint8_t flags;
            uint16_t cid;
            uint32_t unused1[5];
            uint64_t prp1;
            uint64_t unused2;
            uint16_t cqid;
            uint16_t size;
            uint16_t cqflags;
            uint16_t irqvec;
            uint64_t unused3[2];
        } createcompq;
        struct {
            uint8_t opcode;
            uint8_t flags;
            uint16_t cid;
            uint32_t unused1[5];
            uint64_t prp1;
            uint64_t unused2;
            uint16_t sqid;
            uint16_t size;
            uint16_t sqflags;
            uint16_t cqid;
            uint64_t unused3[2];
        } createsubq;
        struct {
            uint8_t opcode;
            uint8_t flags;
            uint16_t cid;
            uint32_t unused1[9];
            uint16_t qid;
            uint16_t unused2;
            uint32_t unused3[5];
        } deleteq;
        struct {
            uint8_t opcode;
            uint8_t flags;
            uint16_t cid;
            uint32_t unused1[9];
            uint16_t sqid;
            uint16_t cqid;
            uint32_t unused2[5];
        } abort;
    };
};

struct nvme_cmdcomp {
    uint32_t res;
    uint32_t unused;
    uint16_t sqhead;
    uint16_t sqid;
    uint16_t cid;
    uint16_t status;
};

struct nvme_queue {
    volatile struct nvme_cmd *submit;
    volatile struct nvme_cmdcomp *completion;
    volatile uint32_t *submitdb;
    volatile uint32_t *completedb;
    uint16_t elements; // elements in queue
    uint16_t cqvec;
    uint16_t sqhead;
    uint16_t sqtail;
    uint16_t cqhead;
    uint8_t cqphase;
    uint16_t qid; // queue id
    uint32_t cmdid; // command id
    uint64_t *physregpgs; // pointer to the PRPs
};

struct nvme_device {
    struct resource;
    volatile struct nvme_bar *bar;
    size_t stride;
    size_t queueslots;
    struct nvme_queue adminqueue;
    size_t maxtransshift;
    size_t namespaces;
};

#define NVME_WAITCACHE 0 // cache is not ready
#define NVME_READYCACHE 1 // cache is ready
#define NVME_DIRTYCACHE 2 // cache is damaged/dirty

// cache (helps speed up disk reads by hitting cache)
struct cachedblock {
    uint8_t *cache; // pointer to the cache we have for this block
    uint64_t block;
    uint64_t end;
    int status;
};

struct nvme_lbaf {
    uint16_t ms;
    uint8_t ds;
    uint8_t rp;
};

struct nvme_nsid {
    uint64_t size;
    uint64_t capabilities;
    uint64_t nuse;
    uint8_t features;
    uint8_t nlbaf;
    uint8_t flbas;
    uint8_t mc;
    uint8_t dpc;
    uint8_t dps;
    uint8_t nmic;
    uint8_t rescap;
    uint8_t fpi;
    uint8_t unused1;
    uint16_t nawun;
    uint16_t nawupf;
    uint16_t nacwu;
    uint16_t nabsn;
    uint16_t nabo;
    uint16_t nabspf;
    uint16_t unused2;
    uint64_t nvmcap[2];
    uint64_t unusued3[5];
    uint8_t nguid[16];
    uint8_t eui64[8];
    struct nvme_lbaf lbaf[16];
    uint64_t unused3[24];
    uint8_t vs[3712];
};

struct nvme_nsdevice : resource {
    struct nvme_queue queue;
    struct nvme_device *controller;
    size_t nsid;
    size_t lbasize;
    size_t lbacount;
    size_t maxphysrpgs;
    size_t overwritten;
    size_t cacheblocksize;
    struct cachedblock *cache;
};

#define NVME_CAPMQES(cap)      ((cap) & 0xffff)
#define NVME_CAPSTRIDE(cap)    (((cap) >> 32) & 0xf)
#define NVME_CAPMPSMIN(cap)    (((cap) >> 48) & 0xf)
#define NVME_CAPMPSMAX(cap)    (((cap) >> 52) & 0xf)

// generic
#define NVME_OPFLUSH 0x00
#define NVME_OPWRITE 0x01
#define NVME_OPREAD 0x02
// admin
#define NVME_OPCREATESQ 0x01
#define NVME_OPDELCQ 0x04
#define NVME_OPCREATECQ 0x05
#define NVME_OPIDENTIFY 0x06
#define NVME_OPABORT 0x08
#define NVME_OPSETFT 0x09
#define NVME_OPGETFT 0x0a

static void nvme_createaqueue(struct nvme_device *ctrl, struct nvme_queue *queue, uint64_t slots, uint64_t id) {
    queue->submit = (volatile nvme_cmd *)malloc(sizeof(struct nvme_cmd) * slots); // command queue
    queue->submitdb = (uint32_t *)((uint64_t)ctrl->bar + PAGE_SIZE + (2 * id * (4 << ctrl->stride)) + VMM_HIGHER_HALF);
    queue->sqhead = 0;
    queue->sqtail = 0;
    queue->completion = (volatile nvme_cmdcomp *)malloc(sizeof(struct nvme_cmdcomp) * slots); // command result queue
    queue->completedb = (uint32_t *)((uint64_t)ctrl->bar + PAGE_SIZE + ((2 * id + 1) * (4 << ctrl->stride)) + VMM_HIGHER_HALF);
    queue->cqvec = 0;
    queue->cqhead = 0;
    queue->cqphase = 1;
    queue->elements = slots;
    queue->qid = id;
    queue->cmdid = 0;
    queue->physregpgs = NULL;
}

static void nvme_submitcmd(struct nvme_queue *queue, struct nvme_cmd &cmd) {
    uint16_t tail = queue->sqtail; // tail of the submit queue
    memcpy((void *)&(queue->submit[tail]), &cmd, sizeof(cmd)); // TendingStream73: after some debuging i found that simply writing to queue causes exception 0xD. Why? IDK
    //queue->submit[tail] = cmd; // add to tail (end of queue)
    tail++;
    if (tail == queue->elements) {
        tail = 0;
    }
    *(queue->submitdb) = tail; // set to tail
    queue->sqtail = tail; // update tail so now it'll point to the element after (nothing until we submit a new command)
}

static uint16_t nvme_awaitsubmitcmd(struct nvme_queue *queue, struct nvme_cmd &cmd) {
    uint16_t head = queue->cqhead;
    uint16_t phase = queue->cqphase;
    cmd.common.cid = queue->cmdid++;
    nvme_submitcmd(queue, cmd);
    uint16_t status = 0;

    while (true) {
        status = queue->completion[queue->cqhead].status;
        if ((status & 0x01) == phase) {
            break;
        }
    }

    status >>= 1;
    if (status) {
        log.error("NVME cmd error, status: %x\n", status);
    }

    head++;
    if (head == queue->elements) {
        head = 0;
        queue->cqphase = !queue->cqphase; // flip phase
    }

    *(queue->completedb) = head;
    queue->cqhead = head;
    return status;
}

static ssize_t nvme_identify(struct nvme_device *ctrl, struct nvme_id *id) {
    uint64_t len = sizeof(struct nvme_id);
    struct nvme_cmd cmd;
    memset(&cmd, 0, sizeof(nvme_cmd));
    cmd.identify.opcode = NVME_OPIDENTIFY;
    cmd.identify.nsid = 0;
    cmd.identify.cns = 1;
    cmd.identify.prp1 = (uint64_t)id - VMM_HIGHER_HALF;
    ssize_t off = (uint64_t)id & (PAGE_SIZE - 1);
    len -= (PAGE_SIZE - off);
    if (len <= 0) {
        cmd.identify.prp2 = 0;
    } else {
        uint64_t addr = (uint64_t)id + (PAGE_SIZE - off);
        cmd.identify.prp2 = addr;
    }
    uint16_t status = nvme_awaitsubmitcmd(&(ctrl->adminqueue), cmd);
    if (status != 0) {
        return -1;
    }

    size_t shift = 12 + NVME_CAPMPSMIN(ctrl->bar->capabilities);
    size_t maxtransshift;
    if (id->mdts) {
        maxtransshift = shift + id->mdts;
    } else {
        maxtransshift = 20;
    }
    ctrl->maxtransshift = maxtransshift;
    return 0;
}

static ssize_t nvme_setqueuecount(struct nvme_device *ctrl, int count) {
    struct nvme_cmd cmd;
    cmd.features.opcode = NVME_OPSETFT;
    cmd.features.prp1 = 0;
    cmd.features.fid = 0x07; // number of queues (feature)
    cmd.features.dword = (count - 1) | ((count - 1) << 16);
    uint16_t status = nvme_awaitsubmitcmd(&ctrl->adminqueue, cmd);
    if (status != 0) {
        return -1;
    }
    return 0;
}

static void nvme_createqueue(struct nvme_nsdevice *ns, struct nvme_queue *queue, uint64_t slots, uint64_t id) {
    queue->submit = (nvme_cmd *)malloc(sizeof(struct nvme_cmd) * slots); // command queue
    queue->submitdb = (uint32_t *)((uint64_t)ns->controller->bar + PAGE_SIZE + (2 * id * (4 << ns->controller->stride)) + VMM_HIGHER_HALF);
    queue->sqhead = 0;
    queue->sqtail = 0;
    queue->completion = (nvme_cmdcomp *)malloc(sizeof(struct nvme_cmdcomp) * slots); // command result queue
    queue->completedb = (uint32_t *)((uint64_t)ns->controller->bar + PAGE_SIZE + ((2 * id + 1) * (4 << ns->controller->stride)) + VMM_HIGHER_HALF);
    queue->cqvec = 0;
    queue->cqhead = 0;
    queue->cqphase = 1;
    queue->elements = slots;
    queue->qid = id;
    queue->cmdid = 0;
    queue->physregpgs = (uint64_t *)malloc(ns->maxphysrpgs * slots * sizeof(uint64_t));
}

static ssize_t nvme_createqueues(struct nvme_device *ctrl, struct nvme_nsdevice *ns, uint16_t qid) {

    nvme_createqueue(ns, &ns->queue, ctrl->queueslots, qid);

    struct nvme_cmd cmd1;
    memset(&cmd1, 0, sizeof(nvme_cmd));
    cmd1.createcompq.opcode = NVME_OPCREATECQ;
    cmd1.createcompq.prp1 = (uint64_t)ns->queue.completion - VMM_HIGHER_HALF; // any reference to something within the kernel must be subtracted by our higher half (this is omitted here as the queue initialisation code already handles this)
    cmd1.createcompq.cqid = qid;
    cmd1.createcompq.size = ctrl->queueslots - 1;
    cmd1.createcompq.cqflags = (1 << 0); // queue phys
    cmd1.createcompq.irqvec = 0;
    uint16_t status = nvme_awaitsubmitcmd(&ctrl->adminqueue, cmd1);
    if (status != 0) {
        return -1;
    }

    struct nvme_cmd cmd2;
    memset(&cmd2, 0, sizeof(nvme_cmd));
    cmd2.createsubq.opcode = NVME_OPCREATESQ;
    cmd2.createsubq.prp1 = (uint64_t)ns->queue.submit - VMM_HIGHER_HALF;
    cmd2.createsubq.sqid = qid;
    cmd2.createsubq.cqid = qid;
    cmd2.createsubq.size = ctrl->queueslots - 1;
    cmd2.createsubq.sqflags = (1 << 0) | (2 << 1); // queue phys + medium priority
    status = nvme_awaitsubmitcmd(&ctrl->adminqueue, cmd2);
    if (status != 0) {
        return -1;
    }
    return 0;
}

static ssize_t nvme_nsid(struct nvme_nsdevice *ns, struct nvme_nsid *nsid) {
    struct nvme_cmd cmd;
    memset(&cmd, 0, sizeof(nvme_cmd));
    cmd.identify.opcode = NVME_OPIDENTIFY;
    cmd.identify.nsid = ns->nsid; // differentiate from normal identify by passing name space id
    cmd.identify.cns = 0;
    cmd.identify.prp1 = (uint64_t)nsid - VMM_HIGHER_HALF;
    uint16_t status = nvme_awaitsubmitcmd(&ns->controller->adminqueue, cmd);
    if (status != 0) {
        return -1;
    }
    return 0;
}

static ssize_t nvme_rwlba(struct nvme_nsdevice *ns, void *buf, uint64_t start, uint64_t count, uint8_t write) {
    if(start + count >= ns->lbacount) count -= (start + count) - ns->lbacount;
    size_t pageoff = (uint64_t)buf & (PAGE_SIZE - 1);
    int shoulduseprp = 0;
    int shoulduseprplist = 0;
    uint32_t cid = ns->queue.cmdid;
    if ((count * ns->lbasize) > PAGE_SIZE) {
        if ((count * ns->lbasize) > (PAGE_SIZE * 2)) {
            size_t prpcount = ((count - 1) * ns->lbasize) / PAGE_SIZE;
            if (prpcount > ns->maxphysrpgs) {
                log.error("Exceeded phyiscal region pages\n");
            }
            for (size_t i = 0; i < prpcount; i++) {
                ns->queue.physregpgs[i + cid * ns->maxphysrpgs] = ((uint64_t)(buf - VMM_HIGHER_HALF - pageoff) + PAGE_SIZE + i * PAGE_SIZE);
            }
            shoulduseprp = 0;
            shoulduseprplist = 1;
        } else {
            shoulduseprp = 1;
        }
    }
    struct nvme_cmd cmd;
    memset(&cmd, 0, sizeof(nvme_cmd));
    cmd.rw.opcode = write ? NVME_OPWRITE : NVME_OPREAD;
    cmd.rw.flags = 0;
    cmd.rw.nsid = ns->nsid;
    cmd.rw.control = 0;
    cmd.rw.dsmgmt = 0;
    cmd.rw.ref = 0;
    cmd.rw.apptag = 0;
    cmd.rw.appmask = 0;
    cmd.rw.metadata = 0;
    cmd.rw.slba = start;
    cmd.rw.len = count - 1;
    if (shoulduseprplist) {
        cmd.rw.prp1 = (uint64_t)buf - VMM_HIGHER_HALF;
        cmd.rw.prp2 = (uint64_t)(&ns->queue.physregpgs[cid * ns->maxphysrpgs]) - VMM_HIGHER_HALF;
    } else if (shoulduseprp) {
        cmd.rw.prp2 = (uint64_t)((uint64_t)buf + PAGE_SIZE - VMM_HIGHER_HALF);
    } else {
        cmd.rw.prp1 = (uint64_t)buf - VMM_HIGHER_HALF;
    }

    uint16_t status = nvme_awaitsubmitcmd(&ns->queue, cmd);
    if (status) {
        log.error("Failed to read/write with status %u at %lu with count %lu, is_write:%u\n", status, start, count, write);
        return status;
    }
    return 0;
}


static ssize_t nvme_findblock(struct nvme_nsdevice *ns, uint64_t block) {
    for (size_t i = 0; i < 512; i++) {
        if ((ns->cache[i].block == block) && (ns->cache[i].status)) {
            return i;
        }
    }
    return -1;
}

static ssize_t nvme_cacheblock(struct nvme_nsdevice *ns, uint64_t block) {
    // only called with this block isn't already cached
    int ret, target;

    for (target = 0; target < 512; target++) {
        if (!ns->cache[target].status) {
            goto found; // find a free cache block
        }
    }

    // no free caches left, overwrite an existing cache (we needn't worry about overwriting anything cache-wise as they just exist to speed up repitition of recent block reads)
    if (ns->overwritten == 512) {
        ns->overwritten = 0;
        target = ns->overwritten;
    } else {
        target = ns->overwritten++;
    }

    goto notfound;

found:
    ns->cache[target].cache = (uint8_t *)malloc(ns->cacheblocksize); // intialise cache
notfound:
    ret = nvme_rwlba(ns, ns->cache[target].cache, (ns->cacheblocksize / ns->lbasize) * block, ns->cacheblocksize / ns->lbasize, 0); // dump data from block into cache (this will be used immediately afterwards for reads (and as a basis for writes))
    if (ret == -1) {
        return ret;
    }

    ns->cache[target].block = block;
    ns->cache[target].status = NVME_READYCACHE;

    return target;
}

// read `count` bytes at `loc` into `buf`
static ssize_t nvme_read(struct resource *_this, struct f_description *description, void *buf, off_t loc, size_t count) {
    (void)description;
    spinlock_acquire(&_this->lock);
    struct nvme_nsdevice *thiz = (struct nvme_nsdevice *)_this;

    for (size_t progress = 0; progress < count;) {
        uint64_t sector = (loc + progress) / thiz->cacheblocksize;
        int slot = nvme_findblock(thiz, sector); // find a cache associated with this block
        if (slot == -1) {
            slot = nvme_cacheblock(thiz, sector); // request a cache so next time we can just hit that for this block
            if (slot == -1) {
                spinlock_release(&thiz->lock);
                return -1;
            }
        }

        uint64_t chunk = count - progress;
        size_t off = (loc + progress) % thiz->cacheblocksize;
        if (chunk > thiz->cacheblocksize - off) {
            chunk = thiz->cacheblocksize - off;
        }
        memcpy(buf + progress, &thiz->cache[slot].cache[off], chunk); // copy data chunk into buffer
        progress += chunk;
    }

    spinlock_release(&thiz->lock);
    return count;
}

static ssize_t nvme_write(struct resource *_this, struct f_description *description, const void *buf, off_t loc, size_t count) {
    (void)description;
    spinlock_acquire(&_this->lock);
    struct nvme_nsdevice *thiz = (struct nvme_nsdevice *)_this;

    for (size_t progress = 0; progress < count;) {
        uint64_t sector = (loc + progress) / thiz->cacheblocksize;
        int slot = nvme_findblock(thiz, sector);
        if (slot == -1) {
            slot = nvme_cacheblock(thiz, sector);
            if (slot == -1) {
                spinlock_release(&thiz->lock);
                return -1;
            }
        }

        uint64_t chunk = count - progress;
        size_t off = (loc + progress) % thiz->cacheblocksize;
        if (chunk > thiz->cacheblocksize - off) {
            chunk = thiz->cacheblocksize - off;
        }

        // copy buffer into cache (for writing)
        memcpy(&thiz->cache[slot].cache[off], buf + progress, chunk);
        thiz->cache[slot].status = NVME_READYCACHE; // in usage (allow for cache hits)
        int ret = nvme_rwlba(thiz, thiz->cache[slot].cache, (thiz->cacheblocksize / thiz->lbasize) * thiz->cache[slot].block, thiz->cacheblocksize / thiz->lbasize, 1);
        if (ret == -1) {
            spinlock_release(&thiz->lock);
            return -1;
        }
        progress += chunk;
    }

    spinlock_release(&thiz->lock);
    return count;
}

static void nvme_initnamespace(size_t id, struct nvme_device *controller) {
    struct nvme_nsdevice *nsdev_res = (nvme_nsdevice *)Resource::Create(sizeof(struct nvme_nsdevice));
    nsdev_res->controller = controller;
    nsdev_res->nsid = id;
    struct nvme_nsid *nsid = new struct nvme_nsid;
    if (nvme_nsid(nsdev_res, nsid)) {
        log.error("Failed to send identify to namespace %d\n", id);
        delete nsdev_res;
        delete nsid;
        return;
    }

    uint64_t formattedlba = nsid->flbas & 0x0f;
    uint64_t lbashift = nsid->lbaf[formattedlba].ds;
    uint64_t maxlbas = 1 << (controller->maxtransshift - lbashift);
    nsdev_res->maxphysrpgs = (maxlbas * (1 << lbashift)) / PAGE_SIZE;

    if (nvme_createqueues(controller, nsdev_res, id)) {
        log.error("Failed to create IO queues for namespace %d\n", id);
        delete nsdev_res;
        delete nsid;
        return;
    }

    nsdev_res->cache = (cachedblock *)malloc(sizeof(struct cachedblock) * 512); // set up our cache
    nsdev_res->lbasize = 1 << nsid->lbaf[formattedlba].ds;
    nsdev_res->cacheblocksize = nsdev_res->lbasize * 4; // cache disk blocks in each cache block (less time spent dealing with block reads from disk and overwrites)
    nsdev_res->lbacount = nsid->size;

    log.info("Namespace %d lba count: %d\n", id, nsdev_res->lbacount);
    log.info("Namespace size(in gigabytes): %d\n", (nsdev_res->lbacount * nsdev_res->lbasize) / 1024 / 1024 /1024);

    nsdev_res->can_mmap = false;
    nsdev_res->read = nvme_read;
    nsdev_res->write = nvme_write;
    nsdev_res->ioctl = resource_default_ioctl;

    // adding all this size information to stat means we can easily see size information about the block device without anything other than a quick stat
    nsdev_res->stat.st_size = nsid->size * nsdev_res->lbasize; // total size
    nsdev_res->stat.st_blocks = nsid->size; // blocks are just part of this
    nsdev_res->stat.st_blksize = nsdev_res->lbasize; // block sizes are the lba size
    nsdev_res->stat.st_rdev = resource_create_dev_id();
    nsdev_res->stat.st_mode = 0666 | S_IFBLK;

    char devname[32];
    snprintf(devname, sizeof(devname) - 1, "nvme%lun%lu", nvme_controller_num, id);
    devtmpfs_add_device((struct resource *)nsdev_res, devname);
    log.info("Registered namespace in dev: %s\n", devname);
    //char buf[512];
    //nvme_read((resource *)nsdev_res, NULL, buf, 0, 512);
    //log.info("Read from lba 0 result: %s\n", buf);
    //snprintf(buf+strlen(buf), 512-strlen(buf), "write test from x64os!");
    //nvme_write((resource *)nsdev_res, NULL, buf, 0, 512);
    //log.info("Wrote 512 bytes \n");

    partition_enum((struct resource *)nsdev_res, devname, nsdev_res->lbasize, "%sp%u");
}

static void nvme_initcontroller(struct pci_device *device) {
    log.info("Initializing nvme controller %d...\n", nvme_controller_num);
    struct pci_bar bar = pci_get_bar(device, 0);
    if (bar.is_mmio == false) {
        log.error("Failed to init controller: BAR0 is not MMIO mapped.\n");
        return;
    }
    log.info("Got BAR0\n");
    if ((PCI_READD(device, 0x10) & 0b111) != 0b100) {
        log.warn("Something in pci config space is not right.\n");
    }
    if (!pci_map_bar(bar)) {
        log.error("Failed to map BAR0.\n");
        return;
    }
    log.info("BAR0 mapped.\n");

    pci_set_privl(device, PCI_PRIV_MMIO | PCI_PRIV_BUSMASTER);

    log.info("bar0 base: 0x%016lx\n", bar.base);

    nvme_bar *contr_bar = (nvme_bar *)(bar.base+VMM_HIGHER_HALF);
    if (contr_bar->conf & (1 << 0)) { // controller enabled?
        contr_bar->conf &= ~(1 << 0); // disable controller
        log.info("Controller disabled.\n");
        //controller_res->bar->conf = conf;
    }

    while ((contr_bar->status) & (1 << 0)); // await controller ready

    log.info("Controller is now ready.\n");

    nvme_device *contr_res = (nvme_device *)Resource::Create(sizeof(nvme_device));
    contr_res->bar = (nvme_bar *)(((uintptr_t)contr_bar)-VMM_HIGHER_HALF);

    contr_res->stride = NVME_CAPSTRIDE(contr_bar->capabilities);
    contr_res->queueslots = NVME_CAPMQES(contr_bar->capabilities);
    nvme_createaqueue(contr_res, &contr_res->adminqueue, contr_res->queueslots, 0); // intialise first queue

    uint32_t aqa = contr_res->queueslots - 1;
    aqa |= aqa << 16;
    aqa |= aqa << 16;
    contr_res->bar->aqa = aqa;
    uint32_t conf = (0 << 4) | (0 << 11) | (0 << 14) | (6 << 16) | (4 << 20) | (1 << 0); // reinitialise config (along with enabling the controller again)
    contr_res->bar->asq = (uint64_t)contr_res->adminqueue.submit - VMM_HIGHER_HALF;
    contr_res->bar->acq = (uint64_t)contr_res->adminqueue.completion - VMM_HIGHER_HALF;
    contr_res->bar->conf = conf;

    while (true) {
        uint32_t status = contr_res->bar->status;
        if (status & (1 << 0)) {
            break; // ready
        }
        if ((status & (1 << 1))) {
            log.error("Controller status is fatal.\n");
            return;
        }
    }

    nvme_id *contr_id = new nvme_id;
    if(nvme_identify(contr_res, contr_id)) {
        log.error("Failed to identify NVME controller.\n");
        return;
    }
    log.info("Controller info:\n");
    log.info(" - Serial number: %.*s\n", 20, contr_id->sn);
    log.info(" - Model number: %.*s\n", 40, contr_id->mn);
    log.info(" - Firmware: %.*s\n", 8, contr_id->fr);
    log.info(" - Namespaces: %d\n", contr_id->nn);
    if (contr_id->nn == 1) {
        log.info("Found namespace 1\n");
        nvme_initnamespace(1, contr_res);
    } else {
        uint32_t *nsids = (uint32_t *)malloc(DIV_ROUNDUP(contr_id->nn * 4, PAGE_SIZE));

        struct nvme_cmd getns;
        memset(&getns, 0, sizeof(nvme_cmd));
        getns.identify.opcode = NVME_OPIDENTIFY;
        getns.identify.cns = 2;
        getns.identify.prp1 = (uint64_t)nsids - VMM_HIGHER_HALF;
        if (nvme_awaitsubmitcmd(&contr_res->adminqueue, getns)) {
            log.error("Failed to obtain namespaces for controller %d\n", nvme_controller_num);
            return;
        }

        nvme_setqueuecount(contr_res, 4);
        for (size_t i = 0; i < contr_id->nn; i++) {
            if (nsids[i] && nsids[i] < contr_id->nn) {
                log.info("Found namespace %lu\n", nsids[i]);
                nvme_initnamespace(nsids[i], contr_res);
            }
        }
        delete nsids;
    }
    nvme_controller_num++;
}
static struct pci_driver nvme_driver = {
    .name = "nvme",
    .match = PCI_MATCH_CLASS | PCI_MATCH_SUBCLASS | PCI_MATCH_PROG_IF,
    .init = nvme_initcontroller,
    .pci_class = 0x01,
    .subclass = 0x08,
    .prog_if = 0x02,
    .vendor = 0,
    .devcount = 0,
    .devices = { }
};

EXPORT_PCI_DRIVER(nvme_driver);
