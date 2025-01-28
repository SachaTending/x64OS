#include <io.h>
#include <drivers/pci.hpp>
#include <libc.h>
#include <logging.hpp>
#include <vmm.h>

#define	SATA_SIG_ATA	0x00000101	// SATA drive
#define	SATA_SIG_ATAPI	0xEB140101	// SATAPI drive
#define	SATA_SIG_SEMB	0xC33C0101	// Enclosure management bridge
#define	SATA_SIG_PM	0x96690101	// Port multiplier

#define AHCI_DEV_NULL 0
#define AHCI_DEV_SATA 1
#define AHCI_DEV_SEMB 2
#define AHCI_DEV_PM 3
#define AHCI_DEV_SATAPI 4

#define HBA_PORT_IPM_ACTIVE 1
#define HBA_PORT_DET_PRESENT 3

typedef volatile struct tagHBA_PORT
{
	uint32_t clb;		// 0x00, command list base address, 1K-byte aligned
	uint32_t clbu;		// 0x04, command list base address upper 32 bits
	uint32_t fb;		// 0x08, FIS base address, 256-byte aligned
	uint32_t fbu;		// 0x0C, FIS base address upper 32 bits
	uint32_t is;		// 0x10, interrupt status
	uint32_t ie;		// 0x14, interrupt enable
	uint32_t cmd;		// 0x18, command and status
	uint32_t rsv0;		// 0x1C, Reserved
	uint32_t tfd;		// 0x20, task file data
	uint32_t sig;		// 0x24, signature
	uint32_t ssts;		// 0x28, SATA status (SCR0:SStatus)
	uint32_t sctl;		// 0x2C, SATA control (SCR2:SControl)
	uint32_t serr;		// 0x30, SATA error (SCR1:SError)
	uint32_t sact;		// 0x34, SATA active (SCR3:SActive)
	uint32_t ci;		// 0x38, command issue
	uint32_t sntf;		// 0x3C, SATA notification (SCR4:SNotification)
	uint32_t fbs;		// 0x40, FIS-based switch control
	uint32_t rsv1[11];	// 0x44 ~ 0x6F, Reserved
	uint32_t vendor[4];	// 0x70 ~ 0x7F, vendor specific
} HBA_PORT;

typedef volatile struct tagHBA_MEM
{
	// 0x00 - 0x2B, Generic Host Control
	uint32_t cap;		// 0x00, Host capability
	uint32_t ghc;		// 0x04, Global host control
	uint32_t is;		// 0x08, Interrupt status
	uint32_t pi;		// 0x0C, Port implemented
	uint32_t vs;		// 0x10, Version
	uint32_t ccc_ctl;	// 0x14, Command completion coalescing control
	uint32_t ccc_pts;	// 0x18, Command completion coalescing ports
	uint32_t em_loc;		// 0x1C, Enclosure management location
	uint32_t em_ctl;		// 0x20, Enclosure management control
	uint32_t cap2;		// 0x24, Host capabilities extended
	uint32_t bohc;		// 0x28, BIOS/OS handoff control and status

	// 0x2C - 0x9F, Reserved
	uint8_t  rsv[0xA0-0x2C];

	// 0xA0 - 0xFF, Vendor specific registers
	uint8_t  vendor[0x100-0xA0];

	// 0x100 - 0x10FF, Port control registers
	HBA_PORT	ports[1];	// 1 ~ 32
} HBA_MEM;

typedef struct ahci_state {
	pci_dev_t dev;
	HBA_MEM *hba_mem;
} ahci_state_t;

static Logger log("AHCI Generic driver");

static pci_id_t ahci_ids[] = {
    {0x8086, 0x2652},
    {0x8086, 0x2653},
	{0x8086, 0x2681},
	{0x8086, 0x2682},
	{0x8086, 0x2683},
	{0x8086, 0x27c1},
	{0x8086, 0x27c3},
	{0x8086, 0x27c5},
	{0x8086, 0x27c6},
	{0x8086, 0x2821},
	{0x8086, 0x2822},
	{0x8086, 0x2824},
	{0x8086, 0x2829},
	{0x8086, 0x282a},
	{0x8086, 0x2922},
	{0x8086, 0x2923},
	{0x8086, 0x2924},
	{0x8086, 0x2925},
	{0x8086, 0x2927},
	{0x8086, 0x2929},
	{0x8086, 0x292a},
	{0x8086, 0x292b},
	{0x8086, 0x292c},
	{0x8086, 0x292f},
	{0x8086, 0x294d},
	{0x8086, 0x294e},
	{0x8086, 0x3a05},
	{0x8086, 0x3a22},
	{0x8086, 0x3a25},
    {NULL, NULL}
};

void pci_write(pci_dev_t dev, uint32_t field, uint32_t value);
#define BIT(n) (1 << n)
bool ahci_probe(pci_driver_t *drv, pci_dev_t *pdev) {
    uint16_t dev_id = pci_read(*pdev, PCI_DEVICE_ID);
    uint16_t ven_id = pci_read(*pdev, PCI_VENDOR_ID);
	uint16_t cls = pci_read(*pdev, PCI_CLASS);
	uint16_t subclass = pci_read(*pdev, PCI_SUBCLASS);
	if (cls == 1 and subclass == 6) {
		uint64_t b5 = pci_read(*pdev, PCI_BAR5);
		pci_write(*pdev, PCI_BAR5, ~0);
		uint32_t b5_size = pci_read(*pdev, PCI_BAR5);
		pci_write(*pdev, PCI_BAR5, b5);
		if (b5 & 1) {
			return false; // PIO Not supported.
		}
		else {
			int type = (b5 >> 1) & 3;
			uint32_t base_high = pci_read(*pdev, PCI_BAR5+4);
			if (type == 2) {
				b5 &= 0xfffffff0;
				b5 |= ((uint64_t)base_high) << 32;
			}
			b5_size = ~(b5_size & ~0b1111) + 1;
			log.debug("bar & 1 == 0\n");
		}
		log.info("BAR5: 0x%lx, SIZE: %lu\n", b5, b5_size);
		pagemap *pgm = GET_CURRENT_PGM();
		if (get_current_task() == NULL) pgm = krnl_page;
		vmm_map_range(pgm, b5, b5_size, PTE_PRESENT | PTE_WRITABLE);
		HBA_MEM *ctrl_hba = (HBA_MEM *)b5;
		if (ctrl_hba->ghc & BIT(31)) {
			log.info("Found controller %04x, %04x, %u, %u\n", ven_id, dev_id, cls, subclass);
			return true;
		}
	}
    return false;
}

// Check device type
static int check_type(HBA_PORT *port)
{
	uint32_t ssts = port->ssts;

	uint8_t ipm = (ssts >> 8) & 0x0F;
	uint8_t det = ssts & 0x0F;

	if (det != HBA_PORT_DET_PRESENT)	// Check drive status
		return AHCI_DEV_NULL;
	if (ipm != HBA_PORT_IPM_ACTIVE)
		return AHCI_DEV_NULL;

	switch (port->sig)
	{
	case SATA_SIG_ATAPI:
		return AHCI_DEV_SATAPI;
	case SATA_SIG_SEMB:
		return AHCI_DEV_SEMB;
	case SATA_SIG_PM:
		return AHCI_DEV_PM;
	default:
		return AHCI_DEV_SATA;
	}
}

static void probe_port(ahci_state_t *state) {
	HBA_MEM *abar = state->hba_mem;
	uint32_t pi = abar->pi;
	int i = 0;
	while (i<32)
	{
		if (pi & 1)
		{
			int dt = check_type(&abar->ports[i]);
			if (dt == AHCI_DEV_SATA)
			{
				log.info("SATA drive found at port %d\n", i);
			}
			else if (dt == AHCI_DEV_SATAPI)
			{
				log.info("SATAPI drive found at port %d\n", i);
			}
			else if (dt == AHCI_DEV_SEMB)
			{
				log.info("SEMB drive found at port %d\n", i);
			}
			else if (dt == AHCI_DEV_PM)
			{
				log.info("PM drive found at port %d\n", i);
			}
			else
			{
				log.info("No drive found at port %d\n", i);
			}
		}

		pi >>= 1;
		i ++;
	}
}

void *ahci_init_driver(pci_driver_t *drv, pci_dev_t *pdev) {
	ahci_state_t *state = new ahci_state_t;
	log.info("allocated 0x%lx\n", state);
	state->dev = *pdev;
	uint64_t b5 = pci_read(*pdev, PCI_BAR5);
	pci_write(*pdev, PCI_BAR5, ~0);
	size_t b5_size = pci_read(*pdev, PCI_BAR5);
	pci_write(*pdev, PCI_BAR5, b5);
	// Ensuring that BAR5 is not PIO
	int type = (b5 >> 1) & 3;
	uint32_t base_high = pci_read(*pdev, PCI_BAR5+4);
	if (type == 2) {
		b5 &= 0xfffffff0;
		b5 |= ((uint64_t)base_high) << 32;
	}
	b5_size = ~(b5_size & ~0b1111) + 1;
	pagemap *pgm = GET_CURRENT_PGM();
	if (get_current_task() == NULL) pgm = krnl_page;
	//vmm_map_range(pgm, b5, b5_size, PTE_PRESENT | PTE_WRITABLE); // Map BAR5 in case if pgm changed.
	state->hba_mem = (HBA_MEM *)b5;
	probe_port(state);
	return (void *)state;
}

static pci_driver_t ahci_driver = {
    .supported_ids = ahci_ids,
    .name = "AHCI Generic driver",
    .probe = ahci_probe,
	.init = ahci_init_driver,
    .priv = NULL
};

__attribute__((constructor)) static void on_init() {
    pci_register_driver(&ahci_driver);
}