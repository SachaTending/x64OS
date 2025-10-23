#include <logging.hpp>
#include <arch/arch.hpp>
#include <arch/vmm.h>

static Logger log("LAPIC");
uint64_t lapic_base = 0; // TODO: grab lapic base from per-cpu struct
static inline uint32_t lapic_read(uint32_t reg) {
    return *((volatile uint32_t *)((uintptr_t)lapic_base + VMM_HIGHER_HALF + reg));
}

static inline void lapic_write(uint32_t reg, uint32_t val) {
    *((volatile uint32_t *)((uintptr_t)lapic_base + VMM_HIGHER_HALF + reg)) = val;
}
#define LAPIC_REG_SPURIOUS 0x0f0

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

void Arch::x86::ACPI::LAPICSetup(){
    wrmsr(IA32_APIC_BASE_MSR, (rdmsr(IA32_APIC_BASE_MSR) & 0xfffff0000) | IA32_APIC_BASE_MSR_ENABLE);
    log.debug("LAPIC base: 0x%lx\n",lapic_base);
    vmm_map_range(krnl_page, lapic_base, 1, PTE_WRITABLE | PTE_PRESENT);
    log.debug("LAPIC has been mapped.\n");
    // Configure spurious IRQ
    lapic_write(LAPIC_REG_SPURIOUS, lapic_read(LAPIC_REG_SPURIOUS) | (1 << 8) | 0xff);
    log.info("LAPIC has beem configured.\n");
}