#include <libc.h>
#include <logging.hpp>
#include <limine.h>
#include <io.h>
#include <acpi.hpp>

extern limine_hhdm_request hhdm2;
#define VMM_HIGHER_HALF hhdm2.response->offset

struct XSDP_t {
    char Signature[8];
    uint8_t Checksum;
    char OEMID[6];
    uint8_t Revision;
    uint32_t RsdtAddress;      // deprecated since version 2.0
    
    uint32_t Length;
    uint64_t XsdtAddress;
    uint8_t ExtendedChecksum;
    uint8_t reserved[3];
} __attribute__ ((packed));

limine_rsdp_request limine_rsdp = {
    .id = LIMINE_RSDP_REQUEST,
    .revision = 0,
    .response = 0
};

static Logger log("ACPI");
static Logger lailog("LAI");
XSDP_t *rsdp;

enum ACPI_TYPE {
    RSDT,
    XSDT
};

ACPI_TYPE type = RSDT;

struct rsdt {
    ACPISDTHeader hdr;
    uint32_t tables[];
};

struct xsdt {
    ACPISDTHeader hdr;
    uint64_t tables[];
};

ACPISDTHeader *table = 0;
size_t tables;

static uint64_t getaddr(size_t toff) {
    rsdt *r;
    xsdt *x;
    if (type == XSDT) {
        x = (xsdt *)table;
        return x->tables[toff];
    } else {
        r = (rsdt *)table;
        return r->tables[toff];
    }
}

uint64_t find_table(const char *signature, int index) {
    int i2 = 0;
    size_t i = 0;
    if (!memcmp(signature, "DSDT", 4)) {
        FADT *fadt = (FADT *)find_table("FACP", 0);
        if (!fadt) {
            log.error("No fadt?\n");
            return 0;
        }
        if (type == XSDT) {
            return fadt->X_Dsdt;
        } else {
            return fadt->Dsdt;
        }
    }
    while (i < tables) {
        ACPISDTHeader *tab = (ACPISDTHeader *)getaddr(i);
        if (!memcmp(signature, &tab->Signature, 4)) {
            if (i2 == index) {
                return (uint64_t)tab;
            }
            i2++;
        }
        i++;
    }
    return 0;
}
extern "C" {
    void lai_create_namespace();
    void lai_set_acpi_revision(int rev);
}
#define TRACE
#ifdef TRACE
#define trace(x) {printf("TRACE: %s:%d:%s\n", __FILE__, __LINE__, ##x);x}
#else
#define trace(x) x
#endif
void uacpi_init();
void acpi_init() {
    rsdp = (XSDP_t *)limine_rsdp.response->address;
    log.info("RSDP Location: 0x%016lx\n", rsdp);
    log.info("RSDP Revision: %u\n", rsdp->Revision);
    type = RSDT;
    if (rsdp->Revision >= 2) {
        type = XSDT;
        table = (ACPISDTHeader *)rsdp->XsdtAddress;
    }
    else table = (ACPISDTHeader *)rsdp->RsdtAddress;
    if (rsdp->RsdtAddress) {
        table = (ACPISDTHeader *)rsdp->RsdtAddress;;
        type  = RSDT;
    }
    log.info("OEMID: ");
    for (int i=0;i<6;i++) {
        printf("%c", table->OEMID[i]);
    }
    printf("\n");
    uint8_t div = 4;
    if (type == XSDT) div = 8;
    log.debug("type=%d\n", type);
    log.debug("div=%u\n", div);
    log.info("Table type: %s\n", type?"XSDT":"RSDT");
    log.info("Table addr: 0x%lx\n", table);
    tables = (table->Length - sizeof(ACPISDTHeader)) / div;
    log.debug("tables=%u\n", tables);
#ifdef CONFIG_USE_LAI
    lai_set_acpi_revision(rsdp->Revision);
    lai_create_namespace();
#else 
    uacpi_init();
#endif
    log.info("ACPI Initialized!\n");
}

// Support for LAI

extern "C" {
    void *laihost_malloc(size_t size) {
        return malloc(size);
    }
    void laihost_free(void * ptr, size_t) {
        free(ptr);
    }
    void *laihost_realloc(void *ptr, size_t size, size_t) {
        return realloc(ptr, size);
    }
    void laihost_panic(const char *msg) {
        PANIC("LAI: %s\n", msg);
    }
    void laihost_log(int level, const char *msg) {
        if (level == 1) {
            lailog.info("%s\n", msg);
        } else if (level == 2) {
            lailog.warn("%s\n", msg);
        } else {
            lailog.info("%s\n", msg);
        }
    }
    void *laihost_map(size_t addr, size_t count) {
        (void)count;
        //log.info("LAI: MAP ADDR 0x%016lx SIZE %u\n", addr, count);
        return (void *)((uint64_t)addr+VMM_HIGHER_HALF);
    }
    void laihost_unmap(void *ptr, size_t count ){
        (void)ptr;
        (void)count;
        //log.info("LAI: UNMAP ADDR 0x%016lx SIZE %u\n", ptr, count);
    }
    void *laihost_scan(char *sig, size_t index) {
        return (void *)find_table((const char *)sig, index);
    }
    uint8_t laihost_inb(uint16_t port) {
        return inb(port);
    }
    uint16_t laihost_inw(uint16_t port) {
        return inw(port);
    }
    uint32_t laihost_ind(uint16_t port) {
        return ind(port);
    }

    void laihost_outb(uint16_t port, uint8_t data) {
        outb(port, data);
    }
    void laihost_outw(uint16_t port, uint16_t data) {
        outw(port, data);
    }
    void laihost_outd(uint16_t port, uint32_t data) {
        outd(port, data);
    }

    uint8_t laihost_pci_readb(uint16_t seg, uint8_t bus, uint32_t slot, uint32_t func, uint16_t offset) {
        (void)seg;
        outd(0xCF8, (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfffc) | 0x80000000);
        uint8_t v = inb(0xCFC + (offset & 3));
        return v;
    }
    uint16_t laihost_pci_readw(uint16_t seg, uint8_t bus, uint32_t slot, uint32_t func, uint16_t offset) {
        (void)seg;
        outd(0xCF8, (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfffc) | 0x80000000);
        uint16_t v = inw(0xCFC + (offset & 2));
        return v;
    }
    uint32_t laihost_pci_readd(uint16_t seg, uint8_t bus, uint32_t slot, uint32_t func, uint16_t offset) {
        (void)seg;
        outd(0xCF8, (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfffc) | 0x80000000);
        uint32_t v = ind(0xCFC);
        return v;
    }

    void laihost_pci_writeb(uint16_t seg, uint8_t bus, uint8_t dev, uint8_t fun, uint16_t off, uint8_t val) {
        (void)seg;
        outd(0xCF8, (bus << 16) | (dev << 11) | (fun << 8) | (off & 0xfffc) | 0x80000000);
        outb(0xCFC + (off & 3), val);
    }
    void laihost_pci_writew(uint16_t seg, uint8_t bus, uint8_t dev, uint8_t fun, uint16_t off, uint16_t val) {
        (void)seg;
        outd(0xCF8, (bus << 16) | (dev << 11) | (fun << 8) | (off & 0xfffc) | 0x80000000);
        outw(0xCFC + (off & 2), val);
    }
    void laihost_pci_writed(uint16_t seg, uint8_t bus, uint8_t dev, uint8_t fun, uint16_t off, uint32_t val) {
        (void)seg;
        outd(0xCF8, (bus << 16) | (dev << 11) | (fun << 8) | (off & 0xfffc) | 0x80000000);
        outd(0xCFC, val);
    }
}