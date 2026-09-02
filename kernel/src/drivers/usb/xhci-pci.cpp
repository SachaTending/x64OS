#include <drivers/usb/xhci.hpp>
#include <drv_subsys.hpp>
#include <logging.hpp>
#include <libc.h>

static Logger log("XHCI");

struct cap_regs {
    uint8_t caplength;
    uint8_t res_1;
    uint16_t hciversion; // "Interface Version Number"
    uint32_t hcsparams1; // "Structural Parameters 1"
    uint32_t hcsparams2; // "Structural Parameters 2"
    uint32_t hcsparams3; // "Structural Parameters 3"
    uint32_t hccparams1; // "Capability parameters"
    uint32_t dboff; // Doorbell offset
    uint32_t rtsoff; // Runtime registers space offset
    uint32_t hccparams2; // "Capability parameters 2"
} __attribute__((packed));

// Some parts of code was copied from haiku os

#define HCS0_XECP(x)			(((x) >> 16) & 0xffff)
#define XECP_NEXT(x)			(((x) >> 8) & 0xff)

#define XECP_ID(x)				((x) & 0xff)
#define XHCI_LEGSUP_CAPID		0x01
#define XHCI_LEGSUP_OSOWNED		(1 << 24)	// OS Owned Semaphore
#define XHCI_LEGSUP_BIOSOWNED	(1 << 16)	// BIOS Owned Semaphore

#define XHCI_LEGCTLSTS					0x04
#define XHCI_LEGCTLSTS_RESERVED_BITS	(0xe1fee)
#define XHCI_LEGCTLSTS_READONLY_BITS	(0x110000)

// Host Controller Operational Registers
#define XHCI_CMD			0x00		// USB Command
// USB Command Register
#define CMD_RUN				(1 << 0)
#define CMD_HCRST			(1 << 1)	// Host Controller Reset
#define CMD_INTE			(1 << 2)	// IRQ Enable
#define CMD_HSEE			(1 << 3)	// Host System Error En
#define CMD_LHCRST			(1 << 7)	// Light Host Controller Reset
#define CMD_CSS				(1 << 8)	// Controller Save State
#define CMD_CRS				(1 << 9)	// Controller Restore State
#define CMD_EWE				(1 << 10)	// Enable Wrap Event

#define write_32(offs, val) *(volatile uint32_t *)(this->bar0.base+VMM_HIGHER_HALF+offs) = val
#define read_32(offs) *(volatile uint32_t *)(this->bar0.base+VMM_HIGHER_HALF+offs)

#define write_op_reg_32(offs, val) *(volatile uint32_t *)(this->bar0.base+VMM_HIGHER_HALF+offs+this->op_regs_offset) = val
#define read_op_reg_32(offs) *(volatile uint32_t *)(this->bar0.base+VMM_HIGHER_HALF+offs+this->op_regs_offset)

void XHCI::halt_ctrl() {
    write_op_reg_32(XHCI_CMD, read_op_reg_32(XHCI_CMD) & ~CMD_RUN);
}

XHCI::XHCI(pci_device *dev) {
    this->dev = dev;
    log.info("Probing device %04x:%04x...\n", dev->vendor_id, dev->device_id);
    this->bar0 = pci_get_bar(dev, 0);
    if (this->bar0.is_mmio == false) {
        log.error("Failed to init, BAR0 is not a MMIO\n");
        return;
    }
    log.info("BAR0 is at 0x%016lx\n", this->bar0.base);
    pci_map_bar(this->bar0);
    pci_set_privl(dev, PCI_PRIV_MMIO | PCI_PRIV_BUSMASTER);
    cap_regs *cap_r = (cap_regs *)(this->bar0.base+VMM_HIGHER_HALF);
    this->op_regs_offset = cap_r->caplength & 0xff;
    uint32_t runtime_regs_off = cap_r->rtsoff & ~0x1F;
    uint32_t doorbell_regs_off = cap_r->dboff & ~0x3;
    log.info("Interface version number: %u\n", cap_r->hciversion);
    log.info("caplength=%u\n",this->op_regs_offset);
    log.info("Runtime registers offset: %u\n", runtime_regs_off);
    log.info("Doorbell registers offset: %u\n", doorbell_regs_off);
    uint32_t cparams = cap_r->hccparams1;
    log.info("Capability parameters: 0x%04x\n", cparams);
    uint32_t eec = 0xffffffff;
	uint32_t eecp = HCS0_XECP(cparams) << 2;
    for (; eecp != 0 && XECP_NEXT(eec) != 0; eecp += XECP_NEXT(eec) << 2) {
		log.debug("eecp register: 0x%04x\n", eecp);
        eec = read_32(eecp);
        if (eec == 0xffffffff)
			break;
        if (XECP_ID(eec) != XHCI_LEGSUP_CAPID)
			continue;
        if (eec & XHCI_LEGSUP_BIOSOWNED) {
			log.debug("The host controller is bios owned, claiming"
				" ownership\n");
			write_32(eecp, eec | XHCI_LEGSUP_OSOWNED);

			for (int32_t i = 0; i < 20; i++) {
				eec = read_32(eecp);

				if ((eec & XHCI_LEGSUP_BIOSOWNED) == 0)
					break;

				log.debug("Controller is still bios owned, waiting\n");
				for (int _=0;_<500;_++) asm volatile ("pause");
			}

			if (eec & XHCI_LEGSUP_BIOSOWNED) {
				log.error("BIOS won't give up control over the host "
					"controller (ignoring)\n");
			} else if (eec & XHCI_LEGSUP_OSOWNED) {
				log.info("Successfully took ownership of the host "
					"controller\n");
			}

			// Force off the BIOS owned flag, and clear all SMIs. Some BIOSes
			// do indicate a successful handover but do not remove their SMIs
			// and then freeze the system when interrupts are generated.
			write_32(eecp, eec & ~XHCI_LEGSUP_BIOSOWNED);
		}
        uint32_t legctlsts = read_32(eecp + XHCI_LEGCTLSTS);
		legctlsts &= (XHCI_LEGCTLSTS_RESERVED_BITS | XHCI_LEGCTLSTS_READONLY_BITS);
		write_32(eecp + XHCI_LEGCTLSTS, legctlsts);
    }
    halt_ctrl();
}

static void xhci_init_pci(struct pci_device *device) {
    XHCI *xhci = new XHCI(device);
}

static struct pci_driver xhci_driver = {
    .name = "xhci",
    .match = PCI_MATCH_CLASS | PCI_MATCH_SUBCLASS | PCI_MATCH_PROG_IF,
    .init = xhci_init_pci,
    .pci_class = 0x0C, // Serial bus controller
    .subclass = 0x03, // USB Controller
    .prog_if = 0x30, // XHCI Controller
    .vendor = 0,
    .devcount = 0,
    .devices = { }
};

EXPORT_PCI_DRIVER(xhci_driver);