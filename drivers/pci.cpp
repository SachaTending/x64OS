#include <io.h>
#include <logging.hpp>
#include <drivers/pci.hpp>
#include <spinlock.h>

static Logger log("PCI");

static spinlock_t pci_spinlock = SPINLOCK_INIT;

uint32_t pci_size_map[100];
pci_dev_t dev_zero= {0};

// This code has been stealed from https://github.com/szhou42/osdev/blob/master/src/kernel/drivers/pci.c and modified by me

uint32_t pci_read(pci_dev_t dev, uint32_t field) {
    spinlock_acquire(&pci_spinlock);
	// Only most significant 6 bits of the field
	dev.field_num = (field & 0xFC) >> 2;
	dev.enable = 1;
	outl(PCI_CONFIG_ADDR, dev.bits);
    //log.debug("read: field=0x%08x bus=%u dev=%u func=%u\n", field, dev.bus_num, dev.device_num, dev.function_num);
	// What size is this field supposed to be ?
	uint32_t size = pci_size_map[field];
    uint32_t t;
    switch (size)
    {
        case 1:
            // Get the first byte only, since it's in little endian, it's actually the 3rd byte
            t = inb(PCI_CONFIG_DATA + (field & 3));
            spinlock_release(&pci_spinlock);
            return t;
            break;
        case 2:
            t = inw(PCI_CONFIG_DATA + (field & 2));
            spinlock_release(&pci_spinlock);
		    return t;
            break;
        case 4:
            // Read entire 4 bytes
            t = inl(PCI_CONFIG_DATA);
            spinlock_release(&pci_spinlock);
            return t;
        default:
            return 0xFFFF;
            break;
    }
}

void pci_write(pci_dev_t dev, uint32_t field, uint32_t value) {
    spinlock_acquire(&pci_spinlock);
    dev.field_num = (field & 0xFC) >> 2;
    dev.enable = 1;
    outl(PCI_CONFIG_ADDR, dev.bits);
    outl(PCI_CONFIG_DATA, value);
    spinlock_release(&pci_spinlock);
}

bool pci_is_end(pci_dev_t dev) {
    return !pci_read(dev, PCI_HEADER_TYPE);
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
	pci_size_map[PCI_INTERRUPT_LINE]	= 1;
	pci_size_map[PCI_SECONDARY_BUS]		= 1;
    pci_dev_t pdev;
    size_t devices = 0;
    for (uint8_t bus=0;bus<32;bus++) {
        pdev.bus_num = bus;
        for (uint8_t dev=0;dev<DEVICE_PER_BUS;dev++) {
            pdev.device_num = dev;
            for (uint8_t func=0;func<FUNCTION_PER_DEVICE;func++) {
                pdev.function_num = func;
                //if (pci_is_end(pdev)) continue;
                uint16_t vendor = pci_read(pdev, PCI_VENDOR_ID);
                if (vendor == 0xFFFF) continue;
                uint16_t devid = pci_read(pdev, PCI_DEVICE_ID);
                log.debug("Found device: bus: %u device: %u function: %u vendor: 0x%04x devid: 0x%04x\n", bus, dev, func, vendor, devid);
                devices++;
            }
        }
    }
    log.info("Found %lu devices\n", devices);
}