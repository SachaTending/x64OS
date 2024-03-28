#include <acpi.hpp>
#include <vec.h>
#include <logging.hpp>
#include <libc.h>
#include <madt.hpp>

static Logger log("APIC");

enum APIC_FLAGS {
    MASK_PIC,
    DUAL_8259_PICS
};

struct madt {
    ACPISDTHeader hdr;
    uint32_t lapic_addr;
    uint32_t flags;
    char entries_data[];
};

#define BIT(x) (1 << x)

extern "C" {
    typeof(madt_lapics) madt_lapics = (typeof(madt_lapics))VECTOR_INIT;
    typeof(madt_io_apics) madt_io_apics = (typeof(madt_io_apics))VECTOR_INIT;
    typeof(madt_isos) madt_isos = (typeof(madt_isos))VECTOR_INIT;
    typeof(madt_nmis) madt_nmis = (typeof(madt_nmis))VECTOR_INIT;
} // fix for ld arguing about undefined variables

uint32_t cpuReadIoApic(void *ioapicaddr, uint32_t reg)
{
    uint32_t volatile *ioapic = (uint32_t volatile *)ioapicaddr;
    ioapic[0] = (reg & 0xff);
    return ioapic[4];
}

#define MAX(A, B) ({ \
    typeof(A) MAX_a = A; \
    typeof(B) MAX_b = B; \
    MAX_a > MAX_b ? MAX_a : MAX_b; \
})

madt *madt_table = 0;
extern uint32_t lapic_addr;
void madt_init() {
    log.info("Searching APIC table...\n");
    madt_table = (madt *)find_table("APIC", 0);
    if (!madt_table) {
        log.error("No APIC table found.\n");
        return;
    }
    log.debug("APIC Data:\n");
    log.info("LAPIC Location: 0x%04lx\n", madt_table->lapic_addr);
    log.debug("Flags:\n");
    if (madt_table->flags & BIT(MASK_PIC)) {
        log.debug("Need to mask PIC\n");
    } if (madt_table->flags & BIT(DUAL_8259_PICS)) {
        log.debug("Dual 8259 legacy PICs installed.\n");
    }
    lapic_addr = madt_table->lapic_addr;
    size_t offset = 0;
    for (;;) {
        if (madt_table->hdr.Length - sizeof(struct madt) - offset < 2) {
            break;
        }
        struct madt_header *header = (struct madt_header *)(madt_table->entries_data + offset);
        switch (header->id) {
            case 0:
                //log.info("Found local APIC #%lu\n", madt_lapics.length); // Half of code copied from lyre os
                VECTOR_PUSH_BACK(&madt_lapics, (struct madt_lapic *)header);
                break;
            case 1:
                //log.info("Found IO APIC #%lu\n", madt_io_apics.length);
                VECTOR_PUSH_BACK(&madt_io_apics, (struct madt_io_apic *)header);
                break;
            case 2:
                //log.info("Found ISO #%lu\n", madt_isos.length);
                VECTOR_PUSH_BACK(&madt_isos, (struct madt_iso *)header);
                break;
            case 4:
                //log.info("Found NMI #%lu\n", madt_nmis.length);
                VECTOR_PUSH_BACK(&madt_nmis, (struct madt_nmi *)header);
                break;
        }

        offset += MAX(header->length, 2);
    }
}