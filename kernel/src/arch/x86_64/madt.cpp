#include <arch/arch.hpp>
#include <uacpi/uacpi.h>
#include <uacpi/tables.h>
#include <libc.h>
#include <logging.hpp>
#include <frg/std_compat.hpp>
#include <new>
#include <frg/vector.hpp>

static Logger log("MADT Parser");

static madt_header *madt;

madt_lapic_vec_t madt_lapic_vec; 
madt_io_apic_vec_t madt_io_apic_vec;
madt_io_apic_irq_map_vec_t madt_io_apic_irq_map_vec;
extern uint64_t lapic_base;
void Arch::x86::ACPI::MadtSetup() {
    uacpi_table madt_addr;
    uacpi_status r = uacpi_table_find_by_signature("APIC", &madt_addr);
    if (uacpi_unlikely_error(r)) {
        log.error("Failed to init MADT, probably table not found: %s\n", uacpi_status_to_string(r));
        return;
    }
    log.info("Found MADT table at addr 0x%lx\n", madt_addr.virt_addr);
    madt = (madt_header *)madt_addr.virt_addr;
    log.info("Length: %lu\n", madt->length);
    log.info("LAPIC Base: 0x%lx\n", madt->lapic_addr);
    lapic_base = madt->lapic_addr;
    size_t entries = 0;
    size_t offset = 0;
    for (;;) {
        if (madt->length - sizeof(struct madt_header) - offset < 2) {
            break;
        }
        struct madt_entry_header *header = (struct madt_entry_header *)(madt->entries + offset);
        madt_lapic *lapic = (madt_lapic *)header;
        madt_io_apic *io_apic = (madt_io_apic *)header;
        madt_io_apic_irq_map *irq_map = (madt_io_apic_irq_map *)header;
        switch (header->type)
        {
            case 0:
                log.info("Entry: LAPIC, ACPI Processor id: %u, APIC ID: %u, Flags: %lu\n", lapic->acpi_cpu_id, lapic->apic_id, lapic->flags);
                madt_lapic_vec.push(lapic);
                break;
            case 1:
                log.info("Entry: IOAPIC, IO APIC's id: %u, IO APIC's addr: 0x%lx, GSI base: 0x%lx(%lu)\n", io_apic->ioapic_id, io_apic->ioapic_addr, io_apic->gsi_base, io_apic->gsi_base);
                madt_io_apic_vec.push(io_apic);
                break;
            case 2:
                log.info("Entry: IOAPIC IRQ map, Bus src: %u, IRQ src: %lu, GSI: %lu, Flags: %lu\n", irq_map->bus_src, irq_map->irq_src, irq_map->gsi, irq_map->flags);
                madt_io_apic_irq_map_vec.push(irq_map);
                break;
            default:
                log.warn("Found unknown entry %u with size %u\n", header->type, header->length);
                break;
        }
        entries += 1;
        offset += MAX(header->length, 2);
    }
    log.info("Found %lu MADT entries\n", entries);
}