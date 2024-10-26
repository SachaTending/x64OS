#include <stdint.h>
#include <idt.hpp>
#include <libc.h>

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
}
void stacktrace(uintptr_t *s);
extern "C" uint64_t int_lst[256];
idt_handl idt_handls[256];
void lapic_eoi(void);
void eoi(uint8_t irq);
#define BIT(b) (1 << b)
#define errcode_13_shift (regs->ErrorCode >> 1)
void syscall_c_entry(idt_regs *);

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
}

extern "C" void idt_handler2(idt_regs *regs) {
    if (regs->IntNumber == 1024) {
        // Special interrupt, syscall
        syscall_c_entry(regs);
        return;
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
                printf("IDT(0b11)");
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
        for(;;);
    }
    if (idt_handls[regs->IntNumber-MAP_BASE]) {
        idt_handls[regs->IntNumber-MAP_BASE](regs);
    } else {
        printf("WARNING: Unknown int %u\n", regs->IntNumber);
        stacktrace(0);
    }
    eoi(regs->IntNumber);
}

void idt_set_desc(uint64_t addr, int index, int gtype=0xE) {
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
        if (idte[i].IST != 0) continue;
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
        idt_set_desc(int_lst[i], i);
    }
	lidt((void *)&idte, sizeof(idte)-1);
}