#include <stdint.h>
#include <idt.hpp>
#include <libc.h>
#include <logging.hpp>
#include <cpu.h>
#include <sched.hpp>

static Logger log("Interrupts");

typedef struct idt_entry // and again copied code.
{
	// bytes 0 and 1
	uint64_t OffsetLow  : 16;
	// bytes 2 and 3
	uint64_t SegmentSel : 16;
	// byte 4
	uint64_t IST        : 3;
	uint64_t Reserved0  : 5;
	// byte 5
	uint64_t GateType   : 4;
	uint64_t Reserved1  : 1;
	uint64_t DPL        : 2;
	uint64_t Present    : 1;
	// bytes 6, 7, 8, 9, 10, 11
	uint64_t OffsetHigh : 48;
	// bytes 12, 13, 14, 15
	uint64_t Reserved2  : 32;
} __attribute__((packed)) idt_entry_t;

idt_entry_t idte[256];
extern "C" uint64_t fetch_cr0(void);
extern "C" uint64_t fetch_cr3(void);
void idt_regs_dump(idt_regs *regs) {
    printf("R08: 0x%016lx R09: 0x%016lx R10: 0x%016lx R11: 0x%016lx R12: 0x%016lx R13: 0x%016lx R14: 0x%016lx R15: 0x%016lx\n", regs->r8, regs->r9, regs->r10, regs->r11, regs->r12, regs->r13, regs->r14, regs->r15);
    printf("RIP: 0x%016lx RSP: 0x%016lx RBP: 0x%016lx RAX: 0x%016lx\n", regs->rip, regs->rsp, regs->rbp, regs->rax);
    printf("RBX: 0x%016lx RCX: 0x%016lx RDI: 0x%016lx RDX: 0x%016lx\n", regs->rbx, regs->rcx, regs->rdi, regs->rdx);
    printf("RSI: 0x%016lx RFLAGS: 0x%016lx\n", regs->rsi, regs->rflags);
    printf("CS: 0x%lx SS: 0x%lx DS: 0x%lx ES: 0x%lx FS: 0x%lx GS: 0x%lx\n", regs->cs, regs->ss, regs->ds, regs->es, regs->fs, regs->gs);
    printf("SFRA: 0x%lx CR2: 0x%lx\n", regs->sfra, regs->cr2);
    printf("CR0: 0x%lx CR3: 0x%016lx\n", fetch_cr0(), fetch_cr3());
    printf("MSR 0xc0000100: 0x%lx\n", rdmsr(0xc0000100));
}
void stacktrace(uintptr_t *s);
extern "C" uint64_t int_lst[256];

struct idt_h
{
    idt_handl func;
    void *priv;
    size_t count;
};


idt_h idt_handls[256];
void lapic_eoi(void);
void eoi(uint8_t irq);
#define BIT(b) (1 << b)
#define errcode_13_shift (regs->ErrorCode >> 1)
extern "C" void syscall_c_entry(idt_regs *);
bool mmap_pf(idt_regs *regs);
void on_page_fault(idt_regs *regs) {
    printf("Got Page fault, flags: ");
    #define is_bit_set(bit) (regs->ErrorCode & BIT(bit))
    if (is_bit_set(0)) {
        printf("Present ");
    } if (is_bit_set(1)) {
        printf("Write ");
    } if (is_bit_set(2)) {
        printf("User ");
    } if (is_bit_set(3)) {
        printf("Reserved write ");
    } if (is_bit_set(4)) {
        printf("Instruction fetch ");
    } if (is_bit_set(5)) {
        printf("Protection key");
    } if (is_bit_set(6)) {
        printf("Shadow stack ");
    } if (is_bit_set(15)) {
        printf("Software guard exception ");
    }
    printf("\n");
    printf("Address: 0x%lx\n", regs->cr2);
}
void start_debug_console();
void gdt_reload(void);
extern bool is_lapic_enabled;
void lapic_eoi();
extern "C" idt_regs *idt_handler2(idt_regs *regs) {
    if (regs->IntNumber != 32 and regs->IntNumber != 14) {
    //if (regs->IntNumber != 32) {
    //if (1) {
        log.debug("INT: %u, CS: 0x%lx CR2: 0x%lx, FS: 0x%lx, RIP: 0x%lx\n", regs->IntNumber, regs->cs,regs->cr2, regs->fs, regs->rip);
    }
    if (regs->IntNumber == 14) {
        if (mmap_pf(regs)) {
        //if (true) {
            //printf("regs->cs: 0x%lx\n", regs->cs);
            //printf("regs->ss; 0x%lx\n", regs->ss);
            regs->cs = 0x4b;
            regs->ss = (10*8) | 3;
            //regs->ss = 0x3b;
            //regs->cs = (8 * 8) | 3;
            //printf("ss: 0x%lx cs: 0x%lx fs: 0x%lx\n", regs->ss, regs->cs, regs->fs);
            //printf("addr=0x%lx\n", regs->cr2);
            return regs;
        }
    }
    if (regs->IntNumber == 14) {
        if (get_current_task()->usermode && regs->fs != get_current_task()->tls) {
            // Try to set MSR 0xc0000100 to current_task->tls
            //log.debug("BUG: regs->fs != get_current_task()->tls, fixing...\n");
            wrmsr(0xc0000100, get_current_task()->tls);
            //log.debug("set msr 0xc0000100 to 0x%lx\n", rdmsr(0xc0000100));
            //log.debug("current task: 0x%lx, %s, %lu, tls: 0x%lx\n", get_current_task(), get_current_task()->name, get_current_task()->pid, get_current_task()->tls);
            regs->cs = 0x4b;
            regs->ss = (10*8) | 3;
            return regs;
        }
    }
    if (regs->IntNumber == 1024) {
        // Special interrupt, syscall
        syscall_c_entry(regs);
        //gdt_reload();
        return regs;
    }
    if (regs->IntNumber == 13) {
        uint8_t erc = errcode_13_shift & 0b11;
        if ((errcode_13_shift >> 2) == 7 and erc == 0) {
            //gdt_reload();
            //regs->ss = (8*8);
            //regs->cs = (7*8);
            struct iretq_frame *frame = (struct iretq_frame *) regs->rsp;
            printf("frame->cs=0x%x\n", frame->cs);
            printf("frame->ss=0x%x\n", frame->ss);
            printf("frame->rip=0x%lx\n", frame->rip);
            printf("frame->rsp=0x%lx\n", frame->rsp);
            printf("regs->rip=0x%lx\n", regs->rip);
            printf("regs->cs=0x%x\n", regs->cs);
            printf("regs->ss=0x%x\n", regs->ss);
            printf("regs->ds=0x%x\n", regs->ds);
            printf("regs->es=0x%x\n", regs->es);
            printf("frame->frame->rip=0x%lx\n", frame->rsp->rip);
            printf("frame->frame->rsp=0x%lx\n", frame->rsp->rsp);
            printf("frame->frame->frame->rip=0x%lx\n", frame->rsp->rsp->rip);
            printf("frame->frame->frame->frame->rip=0x%lx\n", frame->rsp->rsp->rsp->rip);
            //regs->cs = 9*8 | 3;
            //regs->ss = 10*8 | 3;
            //regs->ds = regs->es = regs->ss;
            frame->cs = 0x4B;  // USER_CS
            frame->ss = 0x53;  // USER_SS
            regs->ds = regs->es = 0x53; // USER_DATA
            //regs->es = regs->ds = 7*8;
            return regs;
        }
    }
    if (regs->IntNumber < 32) {
        printf("OH NO: INT_%u ERR=0x%04x\n", regs->IntNumber, regs->ErrorCode);
        if (regs->IntNumber == 13) {
            printf("Got #GD: ");
            if (regs->ErrorCode & BIT(0)) {
                printf("External ");
            }
            uint8_t erc = errcode_13_shift & 0b11;
            if (erc == 3) {
                printf("IDT(0b11)\0");
            } else if (erc == 2) {
                printf("LDT");
            } else if (erc == 1) {
                printf("IDT");
            } else {
                printf("GDT");
            }
            printf(" erc=%u segnment=%u\n", erc, errcode_13_shift >> 2);
        } else if (regs->IntNumber == 14) {
            on_page_fault(regs);
        }
        printf("\n");
        printf("Registers dump:\n");
        idt_regs_dump(regs);
        stacktrace(0);
        start_debug_console();
        for(;;);
    }
    if (idt_handls[regs->IntNumber-MAP_BASE].func) {
        idt_handls[regs->IntNumber-MAP_BASE].func(regs, idt_handls[regs->IntNumber-MAP_BASE].priv);
        if (get_current_task()) {
            if (get_current_task()->fork_parent) {
                log.info("RIP: 0x%lx RSP: 0x%lx RIP: 0x%lx RSP: 0x%lx\n", get_current_task()->regs.rip, get_current_task()->regs.rsp, regs->rip, regs->rsp);
            }
        }
        idt_handls[regs->IntNumber-MAP_BASE].count++;
    } else {
        log.warn("WARNING: Unknown int %u\n", regs->IntNumber);
        stacktrace(0);
    }
    if (is_lapic_enabled) lapic_eoi();
    else eoi(regs->IntNumber);
    uint64_t a = ((uint64_t)6*8) << 48;
    a |= (uint64_t)((uint64_t)0x28 << 32);
    //wrmsr(0xc0000081, a);
    task_t *task = get_current_task();
    if (task == NULL or task->regs.cs == 0) {
        return regs;
    }
    return regs;
}

void idt_set_desc(uint64_t addr, int index, int gtype) {
    idt_entry_t *e = &idte[index];
    e->OffsetLow = addr & 0xFFFF;
    e->OffsetHigh = addr >> 16;
    e->SegmentSel = 0x28;
    e->IST = 0;
    e->DPL = 3;
    e->GateType = gtype;
    e->Present = true;
}

void idt_set_global_ist(uint8_t ist) {
    for (int i=0;i<256;i++) {
        //if (idte[i].IST != 0) continue;
        idte[i].IST = ist;
    }
}

void idt_set_int(uint64_t vec, idt_handl handl, void *priv=nullptr) {
    idt_handls[vec].func = handl;
    idt_handls[vec].priv = priv;
}

static inline void lidt(void* base, uint16_t size)
{
    // This function works in 32 and 64bit mode
    struct {
        uint16_t length;
        void*    base;
    } __attribute__((packed)) IDTR = { size, base };
 
    asm ( "lidt %0" : : "m"(IDTR) );  // let the compiler choose an addressing mode
}
void idt_init() {
    for (int i=0;i<256;i++) {
        if (i < 31) {
            idt_set_desc(int_lst[i], i, 0xE);
        }
        else {
            idt_set_desc(int_lst[i], i, 0xE);
        }
    }
	lidt((void *)&idte, sizeof(idte)-1);
}