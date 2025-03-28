#include <stddef.h>
#include <stdint.h>
#include <krnl.hpp>
#include <libc.h>
#include <io.h>
#include <logging.hpp>
#include <msr.h>
#include <idt.hpp>
#include <vmm.h>
#include <krnl.hpp>

#define LAPIC_ID  0x20
#define LAPIC_VER 0x30
#define LAPIC_EOI 0xB0
#define LAPIC_SPU 0xF0


#define IA32_APIC_BASE_MSR 0x1B
#define IA32_APIC_BASE_MSR_BSP 0x100 // Processor is a BSP
#define IA32_APIC_BASE_MSR_ENABLE 0x800

/* Set the physical address for local APIC registers */
void cpu_set_apic_base(uintptr_t apic) {
    uint32_t edx = 0;
    uint32_t eax = (apic & 0xfffff0000) | IA32_APIC_BASE_MSR_ENABLE;
    edx = (apic >> 32) & 0x0f;
    asm volatile (
        "wrmsr\n\t"
        :
        : "a" (eax), "d" (edx), "c" (IA32_APIC_BASE_MSR)
        : "memory"
    );
}

/**
 * Get the physical address of the APIC registers page
 * make sure you map it to virtual memory ;)
 */
uintptr_t cpu_get_apic_base() {
    uint32_t edx = 0, eax = 0;
    asm volatile (
        "rdmsr\n\t"
        : "=a" (eax), "=d" (edx)
        : "c" (IA32_APIC_BASE_MSR)
        : "memory"
    );
    return (eax & 0xfffff000) | (((uint64_t)edx & 0x0f) << 32);
}

static Logger log("LAPIC");
uint32_t lapic_addr = 0;
static uint32_t read_reg(uint32_t reg) {
    return *(volatile uint32_t *)(lapic_addr+reg);
}

static void write_reg(uint32_t reg, uint32_t val) {
    *(volatile uint32_t *)(lapic_addr+reg) = val;
}

#define PIC1		0x20		/* IO base address for master PIC */
#define PIC2		0xA0		/* IO base address for slave PIC */
#define PIC1_COMMAND	PIC1
#define PIC1_DATA	(PIC1+1)
#define PIC2_COMMAND	PIC2
#define PIC2_DATA	(PIC2+1)

bool is_lapic_enabled = false;

void lapic_eoi() {
    write_reg(LAPIC_EOI, 0);
}

void PIC_remap(int offset1, int offset2);
bool lapic_init(void) {
    return false;
    vmm_map_page(krnl_page, lapic_addr, lapic_addr, PTE_WRITABLE | PTE_NOCACHE);
    log.debug("MSR: 0x%lx\n", rdmsr(0x1b) & 0xfffff000);
    cpu_set_apic_base(cpu_get_apic_base());
    log.info("LAPIC Base address: 0x%lx\n", lapic_addr);
    log.info("LAPIC ID: %u\n", read_reg(LAPIC_ID));
    log.info("LAPIC Version: %u\n", read_reg(LAPIC_VER));
    write_reg(LAPIC_SPU, read_reg(LAPIC_SPU) | (1 << 8) | 0xff); // Enable LAPIC
    PIC_remap(MAP_BASE, MAP_BASE+8);
    outb(PIC1_DATA, 0xff);
    outb(PIC2_DATA, 0xff);
    is_lapic_enabled = true;
    log.debug("PIC Masked.\n");
    asm volatile ("sti");
    return true;
}