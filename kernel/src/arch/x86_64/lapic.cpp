#include <logging.hpp>
#include <arch/arch.hpp>
#include <arch/vmm.h>
#include <arch/io.h>

static Logger log("LAPIC");
uint64_t lapic_base = 0; // TODO: grab lapic base from per-cpu struct
static inline uint32_t lapic_read(uint32_t reg) {
    return *((volatile uint32_t *)((uintptr_t)lapic_base + VMM_HIGHER_HALF + reg));
}

static inline void lapic_write(uint32_t reg, uint32_t val) {
    *((volatile uint32_t *)((uintptr_t)lapic_base + VMM_HIGHER_HALF + reg)) = val;
}

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t edx = 0, eax = 0;
    asm volatile (
        "rdmsr\n\t"
        : "=a" (eax), "=d" (edx)
        : "c" (msr)
        : "memory"
    );
    return ((uint64_t)edx << 32) | eax;
}

static inline uint64_t wrmsr(uint32_t msr, uint64_t val) {
    uint32_t eax = (uint32_t)val;
    uint32_t edx = (uint32_t)(val >> 32);
    asm volatile (
        "wrmsr\n\t"
        :
        : "a" (eax), "d" (edx), "c" (msr)
        : "memory"
    );
    return ((uint64_t)edx << 32) | eax;
}



#define IA32_APIC_BASE_MSR 0x1B
#define IA32_APIC_BASE_MSR_ENABLE 0x800
#define LAPIC_REG_ID 0x20 // LAPIC ID
#define LAPIC_REG_EOI 0x0b0 // End of interrupt
#define LAPIC_REG_SPURIOUS 0x0f0
#define LAPIC_REG_CMCI 0x2f0 // LVT Corrected machine check interrupt
#define LAPIC_REG_ICR0 0x300 // Interrupt command register
#define LAPIC_REG_ICR1 0x310
#define LAPIC_REG_LVT_TIMER 0x320
#define LAPIC_REG_TIMER_INITCNT 0x380 // Initial count register
#define LAPIC_REG_TIMER_CURCNT 0x390 // Current count register
#define LAPIC_REG_TIMER_DIV 0x3e0
#define LAPIC_EOI_ACK 0x00
void lapic_eoi(void) {
    lapic_write(LAPIC_REG_EOI, LAPIC_EOI_ACK);
}
void lapic_timer_stop(void) {
    lapic_write(LAPIC_REG_TIMER_INITCNT, 0);
    lapic_write(LAPIC_REG_LVT_TIMER, 1 << 16);
}

#define PIT_DIVIDEND ((uint64_t)1193182)
uint16_t pit_get_current_count(void) {
    outb(0x43, 0x00);
    uint8_t lo = inb(0x40);
    uint8_t hi = inb(0x40);
    return ((uint16_t)hi << 8) | lo;
}
uint64_t lapic_freq = 0;
void set_pit_count(unsigned count);
void lapic_timer_calibrate(void) {
    lapic_timer_stop();

    // Initialize PIT
    lapic_write(LAPIC_REG_LVT_TIMER, (1 << 16) | 0xff); // Vector 0xff, masked
    lapic_write(LAPIC_REG_TIMER_DIV, 0);

    set_pit_count(0xffff); // Reset PIT

    uint64_t samples = 0xfffff;

    uint16_t initial_tick = pit_get_current_count();

    lapic_write(LAPIC_REG_TIMER_INITCNT, (uint32_t)samples);
    while (lapic_read(LAPIC_REG_TIMER_CURCNT) != 0);

    uint16_t final_tick = pit_get_current_count();

    uint64_t total_ticks = initial_tick - final_tick;
    lapic_freq = (samples / total_ticks) * PIT_DIVIDEND;
    log.debug("LAPIC Frequency: %lu Hz\n", lapic_freq);

    lapic_timer_stop();
}

void lapic_send_ipi(uint32_t lapic_id, uint32_t vec) {
    lapic_write(LAPIC_REG_ICR1, lapic_id << 24);
    lapic_write(LAPIC_REG_ICR0, vec);
}

void Arch::x86::ACPI::LAPICSetup(){
    //wrmsr(IA32_APIC_BASE_MSR, (rdmsr(IA32_APIC_BASE_MSR) & 0xfffff0000) | IA32_APIC_BASE_MSR_ENABLE);
    log.debug("LAPIC base: 0x%lx\n",lapic_base);
    vmm_map_range(krnl_page, lapic_base, 1, PTE_WRITABLE | PTE_PRESENT);
    log.debug("LAPIC has been mapped.\n");
    lapic_timer_calibrate();
    // Configure spurious IRQ
    lapic_write(LAPIC_REG_SPURIOUS, lapic_read(LAPIC_REG_SPURIOUS) | (1 << 8) | 0xff);
    log.info("LAPIC has beem configured.\n");
}