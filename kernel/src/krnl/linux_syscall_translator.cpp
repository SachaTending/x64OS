#include <logging.hpp>
#include <vfs.hpp>
#include <mmap.h>

static Logger log("Linux syscall translation");
void *mmap(struct pagemap *pagemap, uintptr_t addr, size_t length, int prot,
           int flags, vfs_node_t *node, size_t offset);

#define MMAP_LINUX_PROT_NONE      0
#define MMAP_LINUX_PROT_READ      1
#define MMAP_LINUX_PROT_WRITE     2
#define MMAP_LINUX_PROT_EXEC      4
#define MMAP_LINUX_PROT_GROWSDOWN 0x01000000
#define MMAP_LINUX_PROT_GROWSUP   0x02000000

#define MMAP_LINUX_MAP_SHARED     0x01
#define MMAP_LINUX_MAP_PRIVATE    0x02
#define MMAP_LINUX_MAP_SHARED_VALIDATE 0x03
#define MMAP_LINUX_MAP_TYPE       0x0f
#define MMAP_LINUX_MAP_FIXED      0x10
#define MMAP_LINUX_MAP_ANON       0x20
#define MMAP_LINUX_MAP_ANONYMOUS  MMAP_LINUX_MAP_ANON
#define MMAP_LINUX_MAP_NORESERVE  0x4000
#define MMAP_LINUX_MAP_GROWSDOWN  0x0100
#define MMAP_LINUX_MAP_DENYWRITE  0x0800
#define MMAP_LINUX_MAP_EXECUTABLE 0x1000
#define MMAP_LINUX_MAP_LOCKED     0x2000
#define MMAP_LINUX_MAP_POPULATE   0x8000
#define MMAP_LINUX_MAP_NONBLOCK   0x10000
#define MMAP_LINUX_MAP_STACK      0x20000
#define MMAP_LINUX_MAP_HUGETLB    0x40000
#define MMAP_LINUX_MAP_SYNC       0x80000
#define MMAP_LINUX_MAP_FIXED_NOREPLACE 0x100000
#define MMAP_LINUX_MAP_FILE       0

uint64_t sys_linux_mmap(
           void *addr, size_t length, int prot, int flags,
           int fd, off_t offset) {
    log.info("mmap(0x%lx, %lu, %d, %d, %d, %lu)\n", addr, length, prot, flags, fd, offset);
    // Convert linux's prot to our prot
    int prot2 = 0;
    if (prot & MMAP_LINUX_PROT_READ) prot2 |= PROT_READ;
    if (prot & MMAP_LINUX_PROT_WRITE) prot2 |= PROT_WRITE;
    if (prot & MMAP_LINUX_PROT_EXEC) prot2 |= PROT_EXEC;
    // Same for flags
    int flags2 = 0;
    if (flags & MMAP_LINUX_MAP_FIXED) flags2 |= MAP_FIXED;
    if (flags & MMAP_LINUX_MAP_ANONYMOUS) flags2 |= MAP_ANONYMOUS;
    if (flags & MMAP_LINUX_MAP_PRIVATE) flags2 |= MAP_PRIVATE;
    return (uint64_t)mmap(Scheduler::GetCurrentThread()->pgm, (uint64_t)addr, length, prot2, flags2, 0, offset);
}