#include <arch/arch.hpp>
#include <arch/io.h>

#define cli() asm volatile ("cli")
#define sti() asm volatile ("sti")

void set_pit_count(unsigned count) {
	// Disable interrupts
	cli();
 
	// Set low byte
    outb(0x43, 0x34);
	outb(0x40,count&0xFF);		// Low byte
	outb(0x40,(count&0xFF00)>>8);	// High byte
    sti();
	return;
}

#define PIT_DIVIDEND ((uint64_t)1193182)
static void pit_set_frequency(uint64_t frequency) {
    uint64_t new_divisor = PIT_DIVIDEND / frequency;
    if (PIT_DIVIDEND % frequency > frequency / 2) {
        new_divisor++;
    }
    set_pit_count((uint16_t)new_divisor);
}
extern uint64_t smp_bsp_lapic;
void io_apic_set_irq_redirect(uint32_t lapic_id, uint8_t vector, uint8_t irq, bool status);
void Arch::x86::InitPIC() {
    pit_set_frequency(1000); // Set frequency to 1 kHz
    io_apic_set_irq_redirect(smp_bsp_lapic, 32, 0, true);
    ///io_apic_set_irq_redirect(smp_bsp_lapic, 32, 0, true);
}