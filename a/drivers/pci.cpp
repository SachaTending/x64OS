#include <io.h>
#include <logging.hpp>
#include <drivers/pci.hpp>
#include <spinlock.h>
#include <frg/std_compat.hpp>
#include <frg/vector.hpp>
#include <new>

typedef frg::vector<pci_driver_t *, frg::stl_allocator> driver_vec_t;

driver_vec_t drivers_vector = driver_vec_t();

static Logger log("PCI");

static spinlock_t pci_spinlock = SPINLOCK_INIT;

uint32_t pci_size_map[100];
pci_dev_t dev_zero= {0};

// This code has been stealed from https://github.com/szhou42/osdev/blob/master/src/kernel/drivers/pci.c and modified by me

uint32_t pci_read(pci_dev_t dev, uint32_t field) {
    spinlock_acquire(&pci_spinlock);
	// Only most significant 6 bits of the field
	dev.field_num = (field & 0xFC);
	dev.enable = 1;
	outl(PCI_CONFIG_ADDR, (1 << 31) | (field & ~3) | (dev.function_num << 8) | (dev.device_num << 11) | (dev.bus_num << 16));
    //log.debug("read: field=0x%08x bus=%u dev=%u func=%u\n", field, dev.bus_num, dev.device_num, dev.function_num);
	// What size is this field supposed to be ?
	uint32_t size = pci_size_map[field];
    uint32_t t = ind(0xcfc);
    t >>= (field & 0x3) * 8;
    switch (size)
    {
        case 1:
            // Get the first byte only, since it's in little endian, it's actually the 3rd byte
            spinlock_release(&pci_spinlock);
            return (uint8_t)t;
            break;
        case 2:
            spinlock_release(&pci_spinlock);
		    return (uint16_t)t;
            break;
        case 4:
            // Read entire 4 bytes
            //t = inl(PCI_CONFIG_DATA);
            spinlock_release(&pci_spinlock);
            return t;
        default:
            return 0xFFFF;
            break;
    }
}

uint64_t pci_get_bar(int bar, pci_dev_t *dev) {
    uint32_t b = PCI_BAR0;
    switch (bar)
    {
        case 1:
            b = PCI_BAR1;
            break;
        case 2:
            b = PCI_BAR2;
            break;
        case 3:
            b = PCI_BAR3;
            break;
        case 4:
            b = PCI_BAR4;
            break;
        case 5:
            b = PCI_BAR5;
            break;
        default:
            break;
    }
    uint32_t base_low = pci_read(*dev, b);
    pci_write(*dev, b, ~0);
    uint32_t size_low = pci_read(*dev, b);
    pci_write(*dev, b, base_low);
    if (base_low & 1) {
        return base_low & ~0b11;
    } else {
        int type = (base_low >> 1) & 3;
        uint32_t base_high = pci_read(*dev, b+4);
        uint64_t result = base_low & 0xfffffff0;
        if (type == 2) {
            result |= ((uint64_t)base_high << 32);
        }
        return result;
    }
}

void pci_write(pci_dev_t dev, uint32_t field, uint32_t value) {
    spinlock_acquire(&pci_spinlock);
    dev.field_num = (field & 0xFC) >> 2;
    dev.enable = 1;
    outl(PCI_CONFIG_ADDR, (1 << 31) | (field & ~3) | (dev.function_num << 8) | (dev.device_num << 11) | (dev.bus_num << 16));
    outl(PCI_CONFIG_DATA, value);
    spinlock_release(&pci_spinlock);
}

bool pci_is_end(pci_dev_t dev) {
    return !pci_read(dev, PCI_HEADER_TYPE);
}

pci_id_t null_id = {0, 0};
bool id_is_null(pci_id_t id) {
    return !(id.dev_id and id.vendor_id);
}

static bool cmp_id(pci_id_t *i1, pci_id_t *i2) {
    //log.debug("cmp: %04x:%04x ? %04x:%04x\n", i1->vendor_id, i1->dev_id, i2->vendor_id, i2->dev_id);
    return (i1->dev_id == i2->dev_id) and (i2->vendor_id == i1->vendor_id);
}

pci_driver_t *pci_get_driver(pci_id_t *id, int dnum) {
    //log.debug("dnum: %d\n", dnum);
    for (int i=0;i<drivers_vector.size();i++) {
        pci_driver_t *drv = drivers_vector[i];
        int id2=0;
        bool compatible = false;
        while (!id_is_null(drv->supported_ids[id2])) {
            if (cmp_id(&drv->supported_ids[id2], id)) {
                compatible = true;
            }
            id2++;
        }
        if (compatible) {
            if (dnum == 0) {
                return drv;
            }
            dnum--;
        }
    }
    return NULL;
}

void init_pci() {
    // Populate pci_size_map(contains size of various headers)
    pci_size_map[PCI_VENDOR_ID]         = 2;
	pci_size_map[PCI_DEVICE_ID]         = 2;
	pci_size_map[PCI_COMMAND]	        = 2;
	pci_size_map[PCI_STATUS]	        = 2;
	pci_size_map[PCI_SUBCLASS]	        = 1;
	pci_size_map[PCI_CLASS]		        = 1;
	pci_size_map[PCI_CACHE_LINE_SIZE]	= 1;
	pci_size_map[PCI_LATENCY_TIMER]		= 1;
	pci_size_map[PCI_HEADER_TYPE]       = 1;
	pci_size_map[PCI_BIST]              = 1;
	pci_size_map[PCI_BAR0]              = 4;
	pci_size_map[PCI_BAR1]              = 4;
	pci_size_map[PCI_BAR2]              = 4;
	pci_size_map[PCI_BAR3]              = 4;
	pci_size_map[PCI_BAR4]              = 4;
	pci_size_map[PCI_BAR5]              = 4;
    pci_size_map[PCI_BAR5+4]            = 4;
	pci_size_map[PCI_INTERRUPT_LINE]	= 1;
	pci_size_map[PCI_SECONDARY_BUS]		= 1;
    pci_dev_t pdev;
    size_t pci_device_count = 0;
    uint16_t vendor = 0;
    uint16_t devid = 0;
    pci_id_t id;
    for (uint8_t bus=0;bus<32;bus++) {
        pdev.bus_num = bus;
        for (uint8_t dev=0;dev<DEVICE_PER_BUS;dev++) {
            pdev.device_num = dev;
            for (uint8_t func=0;func<FUNCTION_PER_DEVICE;func++) {
                pdev.function_num = func;
                //if (pci_is_end(pdev)) continue;
                vendor = pci_read(pdev, PCI_VENDOR_ID);
                if (vendor == 0xFFFF) continue;
                devid = pci_read(pdev, PCI_DEVICE_ID);
                id.vendor_id = vendor;
                id.dev_id = devid;
                int dnum = 0;
                log.info("Found device: bus: %u device: %u function: %u vendor: 0x%04x devid: 0x%04x\n", bus, dev, func, vendor, devid);
                while (1) {
                    pci_driver_t *drv = pci_get_driver(&id, dnum);
                    if (drv == NULL) {
                        //log.debug("%04x:%04x: driver not found.\n", vendor, devid);
                        break;
                    }
                    bool probe_result = drv->probe(drv, &pdev);
                    if (probe_result) {
                        void *s = drv->init(drv, &pdev);
                        break;
                    }
                    //log.info("%04x:%04x: driver: %s, probe result: %d\n", vendor, devid, drv->name, probe_result);
                    dnum++;
                }
                pci_device_count++;
            }
        }
    }
    log.info("Found %lu devices", pci_device_count);
}

void pci_register_driver(pci_driver_t *driver) {
    drivers_vector.push_back(driver);
    log.info("Registered driver: %s\n", driver->name);
}