#include <io.h>
#include <logging.hpp>

static Logger log("PIC");

void io_wait() {
    outb(0x80, 0);
}

#define PIC1		0x20		/* IO base address for master PIC */
#define PIC2		0xA0		/* IO base address for slave PIC */
#define PIC1_COMMAND	PIC1
#define PIC1_DATA	(PIC1+1)
#define PIC2_COMMAND	PIC2
#define PIC2_DATA	(PIC2+1)


void PIC_remap(int offset1, int offset2);

void init_pic() {
    log.info("Using PIC as interrupt controller.\n");
    log.info("Remapping...\n");
    outb(PIC1_DATA, 0xff);
    outb(PIC2_DATA, 0xff); // mask ints to awoit pic triggering
    asm volatile ("cli");
    PIC_remap(32, 32+8);
    log.info("Unmasking all interrupts\n");
    outb(PIC1_DATA, 0);
    outb(PIC2_DATA, 0);
    asm volatile ("sti");
}
#define PIC_EOI		0x20		/* End-of-interrupt command code */
 
void eoi(uint8_t irq)
{
	if(irq >= 8)
		outb(PIC2_COMMAND,PIC_EOI);
 
	outb(PIC1_COMMAND,PIC_EOI);
}