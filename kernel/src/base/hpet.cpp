#include <uacpi/uacpi.h>
#include <uacpi/tables.h>
#include <logging.hpp>
#include <arch/vmm.h>

static Logger log("HPET support");

#define timer_configuration(n) (0x100 + 0x20 * n)
#define timer_comparator(N) (0x108 + 0x20 * N)

struct address_structure
{
    uint8_t address_space_id;    // 0 - system memory, 1 - system I/O
    uint8_t register_bit_width;
    uint8_t register_bit_offset;
    uint8_t reserved;
    uint64_t address;
} __attribute__((packed));

struct description_table_header
{
    char signature[4];    // 'HPET' in case of HPET table
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oemid[6];
    uint64_t oem_tableid;
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed));

struct hpet : public description_table_header
{
    uint8_t hardware_rev_id;
    uint8_t comparator_count:5;
    uint8_t counter_size:1;
    uint8_t reserved:1;
    uint8_t legacy_replacement:1;
    uint16_t pci_vendor_id;
    address_structure address;
    uint8_t hpet_number;
    uint16_t minimum_tick;
    uint8_t page_protection;
} __attribute__((packed));

static hpet *hpet_table;
static uint64_t minimal_tick;

static void write_reg_64(uint64_t offset, uint64_t value) {
    if (hpet_table == 0) return;
    uint64_t *mmio = (uint64_t *)(hpet_table->address.address + offset);
    //mmio += VMM_HIGHER_HALF;
    *mmio = value;
}
static uint64_t read_reg_64(uint64_t offset) {
    if (hpet_table == 0) return 0;
    uint64_t *mmio = (uint64_t *)(hpet_table->address.address + offset);
    //mmio += VMM_HIGHER_HALF;
    return *mmio;
}
void io_apic_set_irq_redirect(uint32_t lapic_id, uint8_t vector, uint8_t irq, bool status);
bool try_to_init_hpet() {
    uacpi_table hpet_addr;
    uacpi_status r = uacpi_table_find_by_signature("HPET", &hpet_addr);
    if (uacpi_unlikely_error(r)) {
        log.error("Failed to find HPET table: %s\n", uacpi_status_to_string(r));
        return false;
    }
    log.info("Found HPET table at addr 0x%lx\n", hpet_addr.virt_addr);
    hpet_table = (hpet *)hpet_addr.virt_addr;
    minimal_tick = hpet_table->minimum_tick;
    log.info("HPET's minimal tick: %lu\n", minimal_tick);
    log.info("HPET's Registers address: 0x%lx, located in %s IO space.\n", hpet_table->address.address, hpet_table->address.address_space_id ? "Port" : "Memory");
    log.info("HPET has %lu comparators\n", hpet_table->comparator_count);
    if (hpet_table->address.address_space_id != 0) {
        log.error("Currently only Memory IO access is supported, sorry.\n");
        return false;
    }
    vmm_map_range(krnl_page, hpet_table->address.address, 1, PTE_PRESENT | PTE_WRITABLE);
    write_reg_64(0x10, read_reg_64(0x10) | (1 << 1)); // Enable legacy mapping
    // TODO: Init comparators
    uint64_t tconf = read_reg_64(timer_configuration(1));
    for (int i=0;i<32;i++) {
        log.debug("IRQ%lu Allowed: %s\n", i, tconf & (1 << (i + 32)) ? "yes": "no");
    }
    write_reg_64(timer_configuration(1), (1 << 9) | (1 << 2));
    write_reg_64(timer_comparator(1), read_reg_64(0x0F0) + 10000);
    io_apic_set_irq_redirect(0, 0, 4, true);
    io_apic_set_irq_redirect(1, 0, 0, true);
    write_reg_64(0x10, read_reg_64(0x10) | (1 << 0)); // ENABLE_CNF
    return true;
}