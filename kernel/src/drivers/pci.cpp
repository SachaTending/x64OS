// This source code was ported from lyre os project

#include <krnl.hpp>
#include <libc.h>
#include <logging.hpp>
#include <drivers/pci.hpp>
#include <frg/vector.hpp>
#include <frg/std_compat.hpp>
#include <uacpi/uacpi.h>
#include <uacpi/tables.h>
#include <uacpi/acpi.h>
#include <arch/vmm.h>
#include <drv_subsys.hpp>
#include <new>

static Logger log("PCI");

uint32_t (*pci_read)(struct pci_device *dev, uint32_t offset, int access_size);
void (*pci_write)(struct pci_device *dev, uint32_t offset, uint32_t value, int access_size);

struct mcfg_entry {
    uint64_t mmio_base;
    uint16_t segment;
    uint8_t start;
    uint8_t end;
    uint32_t reserved;
};

typedef frg::vector<struct mcfg_entry, frg::stl_allocator> mcfg_entries_t;
typedef frg::vector<struct pci_device *, frg::stl_allocator> pci_devices_t;

mcfg_entries_t mcfg_entries;
pci_devices_t pci_devices;

uint32_t mcfg_read(struct pci_device *dev, uint32_t offset, int access_size) {
    for (auto ent : mcfg_entries) {
        if (dev->seg != ent.segment) {
            continue;
        } else if ((dev->bus > ent.end) || (dev->bus < ent.start)) {
            continue;
        }
        uintptr_t target = ((dev->bus - ent.start) << 20) | (dev->slot << 15) | (dev->func << 12);
        target += ent.mmio_base + offset + VMM_HIGHER_HALF;
        uint32_t out = 0;
        switch (access_size) {
            case 1:
                out = *(volatile uint8_t *)target;
                break;
            case 2:
                out = *(volatile uint16_t *)target;
                break;
            case 4:
                out = *(volatile uint32_t *)target;
                break;
            default:
                log.info("unknown access_size: %d\n", access_size);
        }
        return out;
    }
    log.warn("Failed to find MCFG for device %02x:%02x:%02x\n", dev->bus, dev->slot, dev->func);
    return 0;
}

void mcfg_write(struct pci_device *dev, uint32_t offset, uint32_t value, int access_size) {
    for (auto ent : mcfg_entries) {
        if (dev->seg != ent.segment) {
            continue;
        } else if ((dev->bus > ent.end) || (dev->bus < ent.start)) {
            continue;
        }
        uintptr_t target = ((dev->bus - ent.start) << 20) | (dev->slot << 15) | (dev->func << 12);
        target += ent.mmio_base + offset + VMM_HIGHER_HALF;
        switch (access_size) {
            case 1:
                *(volatile uint8_t *)target = value;
                break;
            case 2:
                *(volatile uint16_t *)target = value;
                break;
            case 4:
                *(volatile uint32_t *)target = value;
                break;
        }
        return;
    }
    log.warn("Failed to find MCFG for device %02x:%02x:%02x\n", dev->seg, dev->bus, dev->slot);
}

static void scan_function(uint8_t bus, uint8_t slot, uint8_t func);
static void scan_bus(uint8_t bus) {
    for (int slot = 0; slot < 32; slot++) {
        for (int func = 0; func < 8; func++) {
            scan_function(bus, slot, func);
        }
    } 
}
// Returns true if device exists in pci_devices
static bool pci_check_if_exists(uint8_t bus, uint8_t slot, uint8_t func) {
    for (auto dev : pci_devices) {
        if (dev->bus == bus && dev->slot == slot && dev->func == func) return true;
    }
    return false;
}

static void scan_function(uint8_t bus, uint8_t slot, uint8_t func) {
    if (pci_check_if_exists(bus, slot, func)) return;
    struct pci_device *dev = new pci_device;
    dev->bus = bus;
    dev->slot = slot;
    dev->func = func;

    if (PCI_READD(dev, 0) == (uint32_t)-1) {
        delete dev;
        return;
    }

    uint32_t reg_0 = PCI_READD(dev, 0);
    uint32_t reg_2 = PCI_READD(dev, 2 * sizeof(uint32_t));

    dev->device_id = (uint16_t)(reg_0 >> 16);
    dev->vendor_id = (uint16_t)reg_0;
    dev->rev_id = (uint8_t)reg_2;
    dev->subclass = (uint8_t)(reg_2 >> 16);
    dev->pci_class = (uint8_t)(reg_2 >> 24);
    dev->prog_if = (uint8_t)(reg_2 >> 8);

    if (dev->pci_class == 6 && dev->subclass == 4) {
        // Check if there are more devices hidden behind this bridge
        uint32_t reg_6 = PCI_READD(dev, 6 * sizeof(uint32_t));
        scan_bus((reg_6 >> 8) & 0xff);
    }

    uint16_t sreg = PCI_READW(dev, 6);
    if (sreg & (1 << 4)) {
        // Traverse the caps list, looking for PCI capabilities
        uint8_t next_off = PCI_READB(dev, 0x34);

        while (next_off) {
            uint8_t ident = PCI_READB(dev, next_off);

            switch (ident) {
                case 5: // MSI compatible
                    dev->msi_supported = true;
                    dev->msi_offset = next_off;
                    break;
                case 16: // PCIe compatible
                    dev->pcie_supported = true;
                    dev->pcie_offset = next_off;
                    break;
                case 17: // MSI-X compatible
                    dev->msix_supported = true;
                    dev->msix_offset = next_off;
                    break;
            }

            next_off = PCI_READB(dev, next_off + 1);
        }
    }

    pci_devices.push_back(dev);

    //VECTOR_PUSH_BACK(&devlist, dev);
}

static void scan_root_bus() {
    struct pci_device *root_dev = new pci_device;
    
    if (!(PCI_READD(root_dev, 0xC) & 0x800000)) {
        // Only one PCI host controller
        log.info("This machine has only one root bus\n");
        scan_bus(0);
    } else {
        log.info("This maachine has multiple root busses\n");
        scan_bus(0);
        for (int i = 0; i < 8; i++) {
            root_dev->func = i;

            if (PCI_READD(root_dev, 0x0) == (uint32_t)-1) {
                continue;
            }

            scan_bus(i);
        }
    }
    delete root_dev;
    log.info("Detected PCI devices:\n");
    for (auto device : pci_devices) {
        pci_device *dev = device;
        log.info("  - %02d:%02d:%02d %04x:%04x %02d:%02d:%02d\n", 
            dev->bus, dev->slot, dev->func,
            dev->vendor_id, dev->device_id,
            dev->pci_class, dev->subclass, dev->prog_if);
    }
}

static void dispatch_drivers(void) {
    DRIVER_FOR_EACH(DRIVER_PCI, dev,
        struct pci_driver *p = dev->pci_dev;
        log.info("driver: %s\n", p->name);
        for (auto dev : pci_devices) {
            struct pci_device *d = dev; 
      
            if (d->claimed) {
                continue;
            }

            if (p->match & PCI_MATCH_DEVICE) {
                // XXX: Terrible way to do this
                if (d->vendor_id != p->vendor) {
                    continue;
                }
                bool v = false;
                for (size_t i = 0; i < p->devcount; i++) {
                    if ((d->device_id == p->devices[i])) {
                        v = true;
                    }
                }

                if (!v) {
                    continue;
                }
                log.info("PCI_MATCH_DEVICE triggered for device %04x:%04x at %02x:%02x:%02x\n", d->vendor_id, d->device_id, d->seg, d->bus, d->slot);
                p->init(d);
                d->claimed = true;
            } else if (p->match & PCI_MATCH_VENDOR) {
                if ((d->vendor_id != p->vendor)) {
                    continue;
                }
                log.info("PCI_MATCH_VENDOR triggered for device %04x:%04x at %02x:%02x:%02x\n", d->vendor_id, d->device_id, d->seg, d->bus, d->slot);
                p->init(d);
                d->claimed = true;
            } else {
                if ((p->match & PCI_MATCH_CLASS) && (d->pci_class != p->pci_class)) {
                    continue;
                } else {
                    log.info("PCI_MATCH_CLASS triggered for device %04x:%04x at %02x:%02x:%02x\n", d->vendor_id, d->device_id, d->seg, d->bus, d->slot);
                }

                if ((p->match & PCI_MATCH_SUBCLASS) && (d->subclass != p->subclass)) {
                    continue;
                } else {
                    log.info("PCI_MATCH_SUBCLASS triggered for device %04x:%04x at %02x:%02x:%02x\n", d->vendor_id, d->device_id, d->seg, d->bus, d->slot);
                }

                if ((p->match & PCI_MATCH_PROG_IF) && (d->prog_if != p->prog_if)) {
                    continue;
                } else {
                    log.info("PCI_MATCH_PROG_IF triggered for device %04x:%04x at %02x:%02x:%02x\n", d->vendor_id, d->device_id, d->seg, d->bus, d->slot);
                }

                p->init(d);
                d->claimed = true;
            }
        }
    );
}

void pci_init() {
    log.info("PCI Subsystem is starting up...\n");
    uacpi_table mcfg_addr;
    uacpi_status r = uacpi_table_find_by_signature("MCFG", &mcfg_addr);
    if (uacpi_unlikely_error(r)) {
        log.error("Failed to init MCFG, probably table not found: %s\n", uacpi_status_to_string(r));
        PANIC("bruh");
    }
    log.info("Found MCFG Table at 0x%016lx(virt=0x%016lx)\n", mcfg_addr.ptr, mcfg_addr.virt_addr);
    int entries_count = (mcfg_addr.hdr->length - 44) / 16;
    struct mcfg_entry *mcfg_base = (mcfg_entry *)((uintptr_t)mcfg_addr.virt_addr + sizeof(struct acpi_sdt_hdr) + sizeof(uint64_t));
    for (int i = 0; i < entries_count; i++) {
        struct mcfg_entry ent = mcfg_base[i];
        mcfg_entries.push_back(ent);
        vmm_map_range(krnl_page, ent.mmio_base, 4096*256, PTE_PRESENT | PTE_NOCACHE | PTE_WRITABLE);
        log.info("Found ECAM space for segment %d, bus range %d-%d\n", ent.segment, ent.start, ent.end);
    }
    log.info("Using MCFG as a configuration access mechanism.\n");
    pci_read = mcfg_read;
    pci_write = mcfg_write;
    scan_root_bus();
    dispatch_drivers();
    //while(1);
}

struct pci_bar pci_get_bar(struct pci_device *d, uint8_t index) {
    struct pci_bar result = {0};

    if (index > 5) {
        return result;
    }

    uint16_t offset = 0x10 + index * sizeof(uint32_t);
    uint32_t base_low = PCI_READD(d, offset);
    PCI_WRITED(d, offset, ~0);
    uint32_t size_low = PCI_READD(d, offset);
    PCI_WRITED(d, offset, base_low);

    if (base_low & 1) {
        result.base = base_low & ~0b11;
        result.len  = ~(size_low & ~0b11) + 1;
    } else {
        int type = (base_low >> 1) & 3;
        uint32_t base_high = PCI_READD(d, offset + 4);

        result.base = base_low & 0xfffffff0;
        if (type == 2) {
            result.base |= ((uint64_t)base_high << 32);
        }

        result.len = ~(size_low & ~0b1111) + 1;
        result.is_mmio = true;
    }

    return result;
}

bool pci_map_bar(struct pci_bar &bar) {
    if (bar.is_mmio == false) {
        return false;
    }

    uintptr_t pagebase = ALIGN_DOWN(bar.base, PAGE_SIZE);
    uintptr_t end = ALIGN_UP(bar.base + bar.len, PAGE_SIZE);
    bool mapped = true;

    for (uintptr_t offset = 0; offset < end - pagebase && mapped; offset += PAGE_SIZE) {
        if (vmm_virt2phys(krnl_page, pagebase + offset) == INVALID_PHYS) {
            mapped = false;
        }
    }

    if (mapped == false) {
        for (uintptr_t offset = 0; offset < end - pagebase; offset += PAGE_SIZE) {
            vmm_unmap_page(krnl_page, pagebase + offset, false); // without this vmm_map_page will fail if a part of the bar was mapped
            vmm_unmap_page(krnl_page, pagebase + offset + VMM_HIGHER_HALF, false);
            if (vmm_map_page(krnl_page, pagebase + offset, pagebase + offset, PTE_PRESENT | PTE_WRITABLE) == false ||
                vmm_map_page(krnl_page, pagebase + offset + VMM_HIGHER_HALF, pagebase + offset, PTE_PRESENT | PTE_WRITABLE) == false){
                return false;
            }
        }
    }

    return true;
}

void pci_set_privl(struct pci_device *d, uint16_t flags) {
    uint16_t priv = PCI_READW(d, 0x4);
    priv &= ~0b111;
    priv |= flags & 0b111;
    PCI_WRITEW(d, 0x4, priv);
}