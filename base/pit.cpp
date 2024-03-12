#include <io.h>
#include <idt.hpp>
#include <stddef.h>

extern size_t global_ticks;

void timer_irq(idt_regs regs) {
    (void)regs;
    global_ticks++;
}

#define cli() asm volatile ("cli")
#define sti() asm volatile ("sti")

void set_pit_count(unsigned count) {
	// Disable interrupts
	cli();
 
	// Set low byte
	outb(0x40,count&0xFF);		// Low byte
	outb(0x40,(count&0xFF00)>>8);	// High byte
    sti();
	return;
}
#define PIT_DIVIDEND ((uint64_t)1193182)
void pit_set_frequency(uint64_t frequency) {
    uint64_t new_divisor = PIT_DIVIDEND / frequency;
    if (PIT_DIVIDEND % frequency > frequency / 2) {
        new_divisor++;
    }
    set_pit_count((uint16_t)new_divisor);
}

void init_pit() {
    pit_set_frequency(1);
    //set_pit_count(0xff);
}