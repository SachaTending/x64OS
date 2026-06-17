#pragma once
#include <stdint.h>
#include <frg/vector.hpp>
#include <frg/std_compat.hpp>
#include <new>
namespace Arch
{
    void Init();
    void InitStage2();
    void InitACPI(); // SHOULD BE CALLED AFTER UACPI INI
    void InitTImer();
    namespace x86
    {
        void InitPIC();
        namespace ACPI
        {
            void MadtSetup();
            void LAPICSetup();
        } // namespace ACPI
        
    } // namespace x86
} // namespace Arch

struct description_table_header
{
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oemid[6];
    uint64_t oem_tableid;
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed));

struct madt_header : public description_table_header {
    uint32_t lapic_addr;
    uint32_t flags;
    char entries[];
} __attribute__((packed));

struct madt_entry_header {
    uint8_t type;
    uint8_t length;
} __attribute__((packed));

struct madt_lapic : public madt_entry_header {
    uint8_t acpi_cpu_id;
    uint8_t apic_id;
    uint32_t flags;
} __attribute__((packed));

struct madt_io_apic : public madt_entry_header {
    uint8_t ioapic_id;
    uint8_t res1;
    uint32_t ioapic_addr;
    uint32_t gsi_base;
} __attribute__((packed));

struct madt_io_apic_irq_map : public madt_entry_header {
    uint8_t bus_src;
    uint8_t irq_src;
    uint32_t gsi;
    uint16_t flags;
} __attribute__((packed));

typedef frg::vector<madt_lapic *, frg::stl_allocator> madt_lapic_vec_t;
typedef frg::vector<madt_io_apic *, frg::stl_allocator> madt_io_apic_vec_t;
typedef frg::vector<madt_io_apic_irq_map *, frg::stl_allocator> madt_io_apic_irq_map_vec_t;
extern madt_lapic_vec_t madt_lapic_vec; 
extern madt_io_apic_vec_t madt_io_apic_vec;
extern madt_io_apic_irq_map_vec_t madt_io_apic_irq_map_vec;

#define MAXIMUM_INTS 256

#define ASM asm volatile

#define STOP_INTERRUPTS ASM ("cli")
#define START_INTERRUPTS ASM ("sti")

#define HCF STOP_INTERRUPTS; ASM ("1: hlt; jmp 1") // aka halt and catch fire