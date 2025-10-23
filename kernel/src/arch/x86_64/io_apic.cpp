#include <arch/arch.hpp>
#include <logging.hpp>
#include <arch/vmm.h>

static Logger log("IOAPIC");
void io_apic_set_gsi_redirect(uint32_t lapic_id, uint8_t vector, uint8_t gsi,
                              uint16_t flags, bool status);
static uint32_t io_apic_read(struct madt_io_apic *io_apic, uint32_t reg) {
    uint64_t base = (uint64_t)io_apic->ioapic_addr + VMM_HIGHER_HALF;
    vmm_map_page(krnl_page, base, io_apic->ioapic_addr, PTE_WRITABLE | PTE_PRESENT);
    *(volatile uint32_t *)base = reg;
    return *(volatile uint32_t *)(base + 16);
}

static void io_apic_write(struct madt_io_apic *io_apic, uint32_t reg, uint32_t value) {
    uint64_t base = (uint64_t)io_apic->ioapic_addr + VMM_HIGHER_HALF;
    vmm_map_page(krnl_page, base, io_apic->ioapic_addr, PTE_WRITABLE | PTE_PRESENT);
    *(volatile uint32_t *)base = reg;
    *(volatile uint32_t *)(base + 16) = value;
}

static size_t io_apic_gsi_count(struct madt_io_apic *io_apic) {
    return (io_apic_read(io_apic, 1) & 0xff0000) >> 16;
}

static struct madt_io_apic *io_apic_from_gsi(uint32_t gsi) {
    for (madt_io_apic *io_apic : madt_io_apic_vec) {
        if (gsi >= io_apic->gsi_base && gsi < io_apic->gsi_base + io_apic_gsi_count(io_apic)) {
            return io_apic;
        }
    }

    //panic(NULL, true, "Cannot determine IO APIC from GSI %lu", gsi);
    log.debug("io_apic_from_gsi(%lu): failed to find io apic\n", gsi);
    return -1;
}

void io_apic_set_irq_redirect(uint32_t lapic_id, uint8_t vector, uint8_t irq, bool status) {
    for (madt_io_apic_irq_map *iso : madt_io_apic_irq_map_vec) {
        if (iso->irq_src != irq) {
            continue;
        }

        io_apic_set_gsi_redirect(lapic_id, vector, iso->gsi, iso->flags, status);
        return;
    }

    io_apic_set_gsi_redirect(lapic_id, vector, irq, 0, status);
}

void io_apic_set_gsi_redirect(uint32_t lapic_id, uint8_t vector, uint8_t gsi,
                              uint16_t flags, bool status) {
    struct madt_io_apic *io_apic = io_apic_from_gsi(gsi);

    uint64_t redirect = vector;
    if ((flags & (1 << 1)) != 0) {
        redirect |= (1 << 13);
    }

    if ((flags & (1 << 3)) != 0) {
        redirect |= (1 << 15);
    }

    if (!status) {
        redirect |= (1 << 16);
    }

    redirect |= (uint64_t)lapic_id << 56;

    uint32_t io_redirect_table = (gsi - io_apic->gsi_base) * 2 + 16;
    io_apic_write(io_apic, io_redirect_table, (uint32_t)redirect);
    io_apic_write(io_apic, io_redirect_table + 1, (uint32_t)(redirect >> 32));
}