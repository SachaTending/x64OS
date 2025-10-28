#include <stdint.h>
#include <krnl.hpp>
#include <libc.h>
#include <sched/sched.hpp>

#define MAGIC_TEXT "This program is only for x64OS. Please use x64OS to run this program.\n"

void Kernel::HandleSyscall(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    Scheduler::thread_t *thr = Scheduler::GetCurrentThread();
    switch (thr->syscall) {
        case SYSCALL_SET_LINUX:
            switch (syscall_num) {
                case 1:
                    if (arg3 == sizeof(MAGIC_TEXT) && !memcmp(MAGIC_TEXT, arg2, sizeof(MAGIC_TEXT))) {
                        printf("Process %s requested for syscall set change.\n", thr->name);
                        thr->syscall = SYSCALL_SET_TRANS;
                    } else {
                        if (arg1 == 1) {
                            for (int i=0;i<arg3;i++) printf("%c", ((char *)arg2)[i]);
                        }
                        //printf("%s a", arg2);
                    }
                    break;
                case 60:
                    // TOOD: Kill
                    break;
                default:
                    printf("Unknown syscall %lu\n", syscall_num);
                    break;
            }
            break;
        case SYSCALL_SET_TRANS:
            if (syscall_num == 60 && arg1 == -1) {
                thr->syscall = SYSCALL_SET_X64OS;
                printf("Process %s now uses x64os's syscall set.\n", thr->name);
            }
            break;
        case SYSCALL_SET_X64OS:
            switch (syscall_num)
            {
                case 512:
                    printf("DEBUG: %s", arg1);
                    break;
                
                default:
                    printf("Unknown syscall %lu\n", syscall_num);
                    break;
            }
    }
}