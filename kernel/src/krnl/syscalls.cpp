#include <stdint.h>
#include <krnl.hpp>
#include <libc.h>
#include <sched/sched.hpp>
#include <sys/uio.h>
#include <errno.h>
#include <asm/termios.h>
#include <asm/prctl.h>
#include <sys/utsname.h>

#define USERSPACE_ADDR_TO_KRNL(addr) (vmm_virt2phys(Scheduler::GetCurrentThread()->pgm, (uint64_t)addr))

struct iovec2 {
    void *iov_base;
    uint64_t iov_len;
} __attribute__((packed));
ssize_t syscall_write(int fdnum, const void *buf, size_t count);
int sys_linux_writev(int fd, const struct iovec2 *iov, int iovcnt) {
    //return -1;
    //printf("writev(%d, 0x%lx, %d);\n", fd, iov, iovcnt);
    int a=0;
    uint64_t i2 = 0;
    iov = (iovec2 *)(USERSPACE_ADDR_TO_KRNL(iov)+VMM_HIGHER_HALF);
    for (int i=0;i<iovcnt;i++) {
        char *iov_real_base = (char *)(USERSPACE_ADDR_TO_KRNL(iov[i].iov_base)+VMM_HIGHER_HALF);
        //printf("buf: 0x%lx, size: %lu\n", iov_real_base, iov[i].iov_len);
        //if (iov[i].iov_len == 18446744071562237456) continue; // skip
        //for (i2=0;i2<iov[i].iov_len;i2++) {
        //    printf("%c", ((char *)iov_real_base)[i2]);
        //}
        syscall_write(fd, (void*)iov_real_base, iov[i].iov_len);
        a+=iov[i].iov_len;
    }
    return a;
}

int sys_linux_uname(struct utsname *uname_struct) {
    uname_struct = (struct utsname *)(USERSPACE_ADDR_TO_KRNL(uname_struct)+VMM_HIGHER_HALF);
    #define COPY_STR(src, dest) memcpy((void *)dest, (const void *)src, sizeof(src));
    COPY_STR("x64OS", &uname_struct->sysname);
    COPY_STR("v0.idk", &uname_struct->release);
    COPY_STR("localhost", &uname_struct->nodename);
    COPY_STR("idk", &uname_struct->version);
    COPY_STR("x86_64", &uname_struct->machine);
    return 0;
}

#define MAGIC_TEXT "This program is only for x64OS. Please use x64OS to run this program.\n"
int syscall_openat(int dir_fdnum, const char *path, int flags, int mode);
ssize_t syscall_read(int fdnum, void *buf, size_t count);

#define SYS_read      0
#define SYS_write     1
#define SYS_open      2
#define SYS_set_tls   3
#define SYS_mmap      4
#define SYS_exit      5
#define SUS_clock_get 6
#define SYS_tlb_set   7
#define SYS_getcwd    8
#define SYS_seek      9
#define SYS_ioctl     10
#define SYS_unmap     11
#define SYS_exec      12
#define SYS_fork      13
#define SYS_waitpid   14

void *mmap(struct pagemap *pagemap, uintptr_t addr, size_t length, int prot,
           int flags, vfs_node_t *node, size_t offset);
bool munmap(struct pagemap *pagemap, uintptr_t addr, size_t length);

int syscall_exec_PROTO(const char *path, const char **argv, const char **envp);

uint64_t sys_linux_mmap(
           void *addr, size_t length, int prot, int flags,
           int fd, off_t offset);
int syscall_ioctl(int fdnum, uint64_t request, uint64_t arg);
uint64_t Kernel::HandleSyscall(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6, cpu_ctx *ctx) {
    thread_t *thr = Scheduler::GetCurrentThread();
    int ret;
    vmm_switch_to(krnl_page);
    printf("SYSCALL %d START\n", syscall_num);
    //thr->syscall = SYSCALL_SET_X64OS;
    switch (thr->syscall) {
        case SYSCALL_SET_LINUX:
            switch (syscall_num) {
                case 0:
                    return syscall_read(arg1, (void *)arg2, arg3);
                case 1:
                    printf("write(%d, 0x%lx, %d);\n", arg1, arg2, arg3);
                    arg2 = USERSPACE_ADDR_TO_KRNL(arg2)+VMM_HIGHER_HALF;
                    if (arg3 == sizeof(MAGIC_TEXT) && !strcmp(MAGIC_TEXT, (const char *)arg2)) {
                        printf("magic: %s\n", arg2);
                        printf("Process %s requested for syscall set change.\n", thr->name);
                        thr->syscall = SYSCALL_SET_TRANS;
                        return 0;
                    } else {
                        //printf("write(%d, 0x%lx, %d\n): unknown fd %d\n", arg1, arg2, arg3, arg1);
                        //printf("%s a", arg2);
                        return syscall_write((int)arg1, (const void *)USERSPACE_ADDR_TO_KRNL(arg2), arg3);
                    }
                    break;
                case 2:
                    printf("before: 0x%lx\n", arg1);
                    arg1 = USERSPACE_ADDR_TO_KRNL(arg1)+VMM_HIGHER_HALF;
                    printf("after: 0x%lx\n", arg1);
                    printf("open(%s, %lu, %lu)\n", arg1, arg2, arg3);
                    ret = syscall_openat(AT_FDCWD, (const char *)arg1, arg2, arg3);
                    printf("open(%s, %lu, %lu) ret=%d\n", arg1, arg2, arg3, ret);
                    return ret;
                case 9:
                    return sys_linux_mmap((void *)arg1, arg2, (int)arg3, (int)arg4, (int)arg5, arg6);
                case 11:
                    return munmap(thr->pgm, arg1, arg2);
                case 12:
                    printf("brk(0x%lx)\n", arg1);
                    return -ENOSYS;
                case 16:
                    printf("ioctl(?)(%d, %d, 0x%lx)\n", arg1, arg2, arg3);
                    if (arg2 == TIOCGWINSZ) {
                        struct winsize *size = (winsize *)arg3;
                        size->ws_col = 100;
                        size->ws_row = 100;
                    } else {
                        return syscall_ioctl((int)arg1, arg2, arg3);
                    }
                    return 0;
                case 20:
                    return sys_linux_writev(arg1, (struct iovec2*)arg2, arg3);
                case 60:
                    return 0;
                    // TOOD: Kill
                    break;
                case 63:
                    return sys_linux_uname((struct utsname *)arg1);
                case 102:
                    // getuid
                    return 0; // TODO
                case 158:
                    printf("arch_prctl(%d, 0x%lx)\n", arg1, arg2);
                    switch (arg1) {
                        case ARCH_SET_FS:
                            thr->cpu_state.tls = arg2;
                            Arch::Scheduler::LoadState(0, &thr->cpu_state);
                            return 0;
                        default:
                            printf("unknown op: %d\n", arg1);
                            return 0;
                    }
                case 218:
                    printf("set_tid_address(0x%lx)\n", arg1);
                    thr->linux_specific.set_tid_addr = arg1;
                    return thr->pid;
                case 257:
                    return syscall_openat(arg1, (const char *)arg2, (int)arg3, 777);
                default:
                    printf("LINUX COMPATIBILITY MODE: Unknown syscall %lu\n", syscall_num);
                    return -ENOSYS;
                    break;
            }
            break;
        case SYSCALL_SET_TRANS:
            if (syscall_num == 60 && (int)arg1 == (int)-1) {
                thr->syscall = SYSCALL_SET_X64OS;
                printf("Process %s now uses x64os's syscall set.\n", thr->name);
                return 0;
            }
            break;
        case SYSCALL_SET_X64OS:
            switch (syscall_num)
            {
                case SYS_read:
                    return syscall_read(arg1, (void *)arg2, arg3);
                case SYS_write:
                    return syscall_write((int)arg1, (const void *)USERSPACE_ADDR_TO_KRNL(arg2), arg3);
                    //printf("%s a", arg2);
                    break;
                // TODO: Write
                // Requirements: /dev/console, devtmpfs
                case SYS_open:
                    printf("open(%s, %lu, %lu)\n", arg1, arg2, arg3);
                    return syscall_openat(AT_FDCWD, (const char *)arg1, arg2, arg3);
                case SYS_tlb_set:
                    printf("set_tls(0x%lx)\n", arg1);
                    thr->cpu_state.tls = arg1;
                    Arch::Scheduler::LoadState(0, &thr->cpu_state);
                    return 0;
                case SYS_mmap:
                    printf("mmap(addr=0x%lx, length=%lu) offs: 0x%lx\n", arg1, arg2, arg4);
                    return (uint64_t)mmap(Scheduler::GetCurrentThread()->pgm, arg1, arg2, (int)(arg3 >> 32), (int)arg3, 0, arg4);
                case SYS_exec:
                    ret = syscall_exec_PROTO((const char *)vmm_virt2phys(thr->pgm, arg1)+VMM_HIGHER_HALF, (const char**)arg2, (const char**)arg3);
                    if (ret == 0) {
                        Arch::Scheduler::LoadState(ctx, &thr->cpu_state);
                    }
                    return ret;
                // TODO: mmap
                // Actually there is a lot of TODOs
                case 512:
                    printf("DEBUG: %s", vmm_virt2phys(thr->pgm, arg1)+VMM_HIGHER_HALF);
                    return 0;
                    break;              
                default:
                    printf("Unknown syscall %lu\n", syscall_num);
                    return -1;
                    break;
            }    
    }
    vmm_switch_to(thr->pgm);
}