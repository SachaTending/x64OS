#include <stdint.h>
#include <stddef.h>
#include <libc.h>
#include <arch/interrupts.h>
#include <logging.hpp>
#include <arch/vmm.h>
#include <krnl.hpp>

static Logger log("IDT");

struct InterruptDescriptor64 {
   uint16_t offset_1;        // offset bits 0..15
   uint16_t selector;        // a code segment selector in GDT or LDT
   uint8_t  ist;             // bits 0..2 holds Interrupt Stack Table offset, rest of bits zero.
   uint8_t  type_attributes; // gate type, dpl, and p fields
   uint16_t offset_2;        // offset bits 16..31
   uint32_t offset_3;        // offset bits 32..63
   uint32_t zero;            // reserved
};

InterruptDescriptor64 idt[512]; // 512 interrupts, why not?

static void idt_set_gate_internal(uint64_t handler, size_t number) {
    InterruptDescriptor64 *gate = &idt[number];
    gate->offset_1 = handler & 0xFFFF;
    gate->offset_2 = (handler >> 16) & 0xffff;
    gate->offset_3 = handler >> 32;
    gate->selector = 0x28;
    gate->type_attributes = 0x8E;
}

extern "C" uint64_t int_lst[256];
static inline void lidt(void* base, uint16_t size)
{
    // This function works in 32 and 64bit mode
    struct {
        uint16_t length;
        void*    base;
    } __attribute__((packed)) IDTR = { size, base };
 
    asm ( "lidt %0" : : "m"(IDTR) );  // let the compiler choose an addressing mode
}
void arch_idt_init() {
    for (int i=0;i<256;i++) {
        idt_set_gate_internal(int_lst[i], i);
    }
	lidt((void *)&idt, sizeof(idt)-1);
    asm volatile ("sti");
}
void lapic_eoi();
extern "C" cpu_ctx *idt_main_handler(cpu_ctx *ctx) {
    if (ctx->int_vector == 0xe) {
        log.debug("got pagefault\n");
        log.debug("addr: 0x%lx err: 0x%lx, rip: 0x%lx\n", ctx->cr2, ctx->err, ctx->rip);
        if (ctx->cr2 > VMM_HIGHER_HALF) {
            log.debug("gonna fix that\n");
            vmm_map_page(krnl_page, ctx->cr2, ctx->cr2-VMM_HIGHER_HALF, PTE_PRESENT);
            return;
        }
    }
    //printf("GOT INTERRUPT 0x%lx(%lu)!!!\n", ctx->int_vector, ctx->int_vector);
    //printf("RSP: 0x%lx\n", ctx->rsp);
    if (ctx->int_vector < 32) {
        printf("CR2: 0x%lx\n", ctx->cr2);
        printf("RIP: 0x%lx\n", ctx->rip);
        printf("ERR: 0x%lx\n", ctx->err);
        printf("CS: 0x%lx SS: 0x%lx DS: 0x%lx ES: 0x%lx\n", ctx->cs, ctx->ss, ctx->ds, ctx->es);
        while (1);
    } else {
        Kernel::DispatchInterrupt(ctx, ctx->int_vector);
    }
    lapic_eoi();
    return ctx;
}


void idt_set_global_ist(uint8_t ist) {
    for (int i=0;i<256;i++) {
        //if (idte[i].IST != 0) continue;
        idt[i].ist = ist;
    }
}