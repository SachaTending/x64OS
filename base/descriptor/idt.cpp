#include <stdint.h>
#include <idt.hpp>
#include <libc.h>
#include <logging.hpp>

static Logger log("idt");

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
void idt_regs_dump(cpu_ctx *regs) {
    printf("R08: 0x%016lx R09: 0x%016lx R10: 0x%016lx R11: 0x%016lx R12: 0x%016lx R13: 0x%016lx R14: 0x%016lx R15: 0x%016lx\n", regs->r8, regs->r9, regs->r10, regs->r11, regs->r12, regs->r13, regs->r14, regs->r15);
    printf("RIP: 0x%016lx RSP: 0x%016lx RBP: 0x%016lx RAX: 0x%016lx\n", regs->rip, regs->rsp, regs->rbp, regs->rax);
    printf("RBX: 0x%016lx RCX: 0x%016lx RDI: 0x%016lx RDX: 0x%016lx\n", regs->rbx, regs->rcx, regs->rdi, regs->rdx);
    printf("RSI: 0x%016lx RFLAGS: 0x%016lx\n", regs->rsi, regs->rflags);
    printf("CS: 0x%lx SS: 0x%lx DS: 0x%lx ES: 0x%lx\n", regs->cs, regs->ss, regs->ds, regs->es);
    printf("CR0: 0x%lx CR3: 0x%016lx\n", fetch_cr0(), fetch_cr3());
}
void stacktrace(uintptr_t *s);
extern "C" uint64_t int_lst[256];
idt_handl idt_handls[256];
void lapic_eoi(void);
void eoi(uint8_t irq);
#define BIT(b) (1 << b)
#define errcode_13_shift (regs->ErrorCode >> 1)
extern "C" cpu_ctx *syscall_c_entry(cpu_ctx *);
bool mmap_pf(cpu_ctx *regs);
void on_page_fault(cpu_ctx *regs) {
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
}
idt_exc_handl idt_excpetion_handler[32];

void idt_register_exception_handler(int vector, idt_exc_handl handl) {
    printf("int %u = 0x%lx\n", vector, handl);
    idt_excpetion_handler[vector] = handl;
}
void sched_handl(cpu_ctx *regs);
extern "C" cpu_ctx *idt_handler2(cpu_ctx *regs) {
    //if (regs->IntNumber != 32) log.debug("Interrupt: %u\n", regs->IntNumber);
    if (regs->IntNumber < 32) {
        //printf("before\n");
        //idt_regs_dump(regs);
        if (idt_excpetion_handler[regs->IntNumber]) {
            //printf("handler for int %u exists\n", regs->IntNumber);
            if (idt_excpetion_handler[regs->IntNumber](regs)) {
                //printf("after\n");
                //idt_regs_dump(regs);
                //sched_handl(regs);
                return regs;
            }
        } else {
            //printf("handler for int %u does not exists\n", regs->IntNumber);
        }
    }
    if (regs->IntNumber == 1024) {
        // Special interrupt, syscall
        syscall_c_entry(regs);
        return regs;
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
        stacktrace(regs->rbp);
        for(;;);
    }
    //printf("int: 0x%x\n", regs->IntNumber);
    if (idt_handls[regs->IntNumber-MAP_BASE]) {
        idt_handls[regs->IntNumber-MAP_BASE](regs);
    } else {
        printf("WARNING: Unknown int %u\n", regs->IntNumber);
        stacktrace(0);
    }
    sched_handl(regs);
    eoi(regs->IntNumber);
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

void idt_set_ist(int vector, uint8_t ist) {
    idte[vector].IST = ist;
}

void idt_set_global_ist(uint8_t ist) {
    for (int i=0;i<256;i++) {
        //if (idte[i].IST != 0) continue;
        idte[i].IST = ist;
    }
}

void idt_set_int(uint64_t vec, idt_handl handl) {
    idt_handls[vec] = handl;
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