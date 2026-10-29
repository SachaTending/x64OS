#include <drivers/pci.hpp>
#include <drv_subsys.hpp>
#include <fs/resource.h>
#include <logging.hpp>
#include <net/net.hpp>
#include <drivers/net/e1000.hpp>
#include <sched/sched.hpp>
#include <arch/arch.hpp>
#include <krnl.hpp>

#include <fs/devtmpfs.h>

static Logger log("E1000");
// Original code was taken from lyre os

struct e1000_rxdesc {
    uint64_t addr;
    uint16_t len;
    uint16_t csum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
} __attribute__((packed));

struct e1000_txdesc {
    uint64_t addr;
    uint16_t len;
    uint8_t csumoff;
    uint8_t cmd;
    uint8_t rsvd : 4; // 4 bit reserved
    uint8_t status : 4; // 4 bit status
    uint8_t csstart;
    uint16_t special;
} __attribute__((packed));

struct e1000_device: net_adapter {

    // 8254x specific
    struct e1000_rxdesc *rxdescs;
    struct e1000_txdesc *txdescs;
    uint32_t rxtail;
    uint32_t txtail;
    uint64_t base;
    uint64_t iobase;
    uint8_t vector;
    bool io;
};

#define E8254X_CTRLFD (1 << 0)
#define E8254X_CTRLLRST (1 << 3)
#define E8254X_CTRLASDE (1 << 5)
#define E8254X_CTRLSLU (1 << 6)
#define E8254X_CTRLILOS (1 << 7)
#define E8254X_CTRLRST (1 << 26)

#define E8254X_STATUSFD (1 << 0)
#define E8254X_STATUSLU (1 << 1)
#define E8254X_STATUSFID (3 << 2)
#define E8254X_STATUSTXOFF (1 << 4)
#define E8254X_STATUSTBIMODE (1 << 5)
#define E8254X_STATUSSPEED (3 << 6)
#define E8254X_STATUSASDV (3 << 8)
#define E8254X_STATUSPCI66 (1 << 11)
#define E8254X_STATUSBUS64 (1 << 12) // usually not real
#define E8254X_STATUSPCIMODE (1 << 13)
#define E8254X_STATUSPCIXSPD (3 << 14)

#define E8254X_INTTXDW (1 << 0)
#define E8254X_INTTXQE (1 << 1)
#define E8254X_INTLSC (1 << 2)
#define E8254X_INTRXSEQ (1 << 3)
#define E8254X_INTRXDMT0 (1 << 4)
#define E8254X_INTRXO (1 << 6)
#define E8254X_INTRXT0 (1 << 7)
#define E8254X_INTMDAC (1 << 9)
#define E8254X_INTRXCFG (1 << 10)
#define E8254X_INTPHYINT (1 << 12)
// GC/EI specific
#define E8254X_GCEIINTGPI0 (1 << 11)
#define E8254X_GCEIINTGPI1 (1 << 12)
#define E8254X_GCEIINTGPI2 (1 << 13)
#define E8254X_GCEIINTGPI3 (1 << 14)
// standard
#define E8254X_INTGPI0 (1 << 13)
#define E8254X_INTGPI1 (1 << 14)
#define E8254X_INTTXDL (1 << 15)
#define E8254X_INTSRPD (1 << 16)

#define E8254X_RCTLEN (1 << 1)
#define E8254X_RCTLSBP (1 << 2)
#define E8254X_RCTLUPE (1 << 3)
#define E8254X_RCTLMPE (1 << 4)
#define E8254X_RCTLLPE (1 << 5)
#define E8254X_RCTLLBM (3 << 6)
#define E8254X_RCTLRDTMS (3 << 8)
#define E8254X_RCTLMO (3 << 12)
#define E8254X_RCTLBAM (1 << 15)
#define E8254X_RCTLBSIZE (3 << 16)
#define E8254X_RCTLGBSIZE(x) ((x & 3) << 16)
#define E8254X_RCTLVFE (1 << 18)
#define E8254X_RCTLCFIEN (1 << 19)
#define E8254X_RCTLCFI (1 << 20)
#define E8254X_RCTLDPF (1 << 22)
#define E8254X_RCTLPMCF (1 << 23)
#define E8254X_RCTLBSEX (1 << 25)
#define E8254X_RCTLSECRC (1 << 26)

#define E8254X_TCTLEN (1 << 1)
#define E8254X_TCTLPSP (1 << 3)
#define E8254X_TCTLCT(x) ((x & 0xff) << 4)
#define E8254X_TCTLCOLD(x) ((x & 0xff) << 12)
#define E8254X_TCTLSWXOFF (1 << 22)
#define E8254X_TCTLRTLC (1 << 24)
// GC/EI specific
#define E8254X_TCTLNRTU (1 << 25)

static void e8254x_writereg(struct e1000_device *device, uint32_t reg, uint32_t data) {
    //if (device->io) {
    //    outd(device->iobase + reg, data);
    //} else {
    //    *((uint32_t *)(device->base + VMM_HIGHER_HALF + reg)) = data;
    //}


    *((uint32_t *)(device->base + VMM_HIGHER_HALF + reg)) = data; // MMIO is much faster lol
}

static uint32_t e8254x_readreg(struct e1000_device *device, uint32_t reg) {
    /*
    if (device->io) {
        return ind(device->iobase + reg);
    } else {
        return *((uint32_t *)(device->base + VMM_HIGHER_HALF + reg));
    }*/
    return *((uint32_t *)(device->base + VMM_HIGHER_HALF + reg));
}

static uint16_t e8254x_readnvm(struct e1000_device *device, uint8_t addr) {
    uint32_t ret = 0;
    e8254x_writereg(device, E1000_EERD, 1 | (((uint32_t)addr) << 8));
    while (!((ret = e8254x_readreg(device, E1000_EERD)) & (1 << 4)));
    return (uint16_t)((ret >> 16) & 0xffff);
}

static void e1000_initrx(struct e1000_device *device) {
    device->rxdescs = (struct e1000_rxdesc *)pmm_alloc(1);

    memset(device->rxdescs, 0, PAGE_SIZE);

    uint32_t rxlo = (uint64_t)(device->rxdescs) & 0xffffffff;
    uint32_t rxhi = (uint64_t)(device->rxdescs) >> 32;
    uint32_t rxlen = PAGE_SIZE;
    uint32_t rxhead = 0;
    uint32_t rxtail = 255;

    e8254x_writereg(device, E1000_RDBAL(0), rxlo);
    e8254x_writereg(device, E1000_RDBAH(0), rxhi);
    e8254x_writereg(device, E1000_RDLEN(0), rxlen);
    e8254x_writereg(device, E1000_RDH(0), rxhead);
    e8254x_writereg(device, E1000_RDT(0), rxtail);

    for (size_t i = 0; i < 256; i++) {
        struct e1000_rxdesc *desc = &device->rxdescs[i]; // since it's allocated as physical memory we want it as higher half
        uint64_t phys = (uint64_t)pmm_alloc(1);
        memset((void *)phys, 0, PAGE_SIZE);
        desc->addr = phys;
        desc->status = 0;
    }

    e8254x_writereg(device, E1000_RCTL, E8254X_RCTLEN | E8254X_RCTLSBP | E8254X_RCTLUPE | E8254X_RCTLMPE | E8254X_RCTLLPE | E8254X_RCTLBAM | E8254X_RCTLSECRC | (E8254X_RCTLGBSIZE(3) | E8254X_RCTLBSEX)); // enable a few flags along with setting block size to our native page size (4096)
}

static void e1000_inittx(struct e1000_device *device) {
    device->txdescs = (struct e1000_txdesc *)pmm_alloc(1);
    uint32_t txlo = (uint64_t)(device->txdescs) & 0xffffffff;
    uint32_t txhi = (uint64_t)(device->txdescs) >> 32;
    uint32_t txlen = PAGE_SIZE;
    uint32_t txhead = 0;
    uint32_t txtail = 255;

    e8254x_writereg(device, E1000_TDBAL(0), txlo);
    e8254x_writereg(device, E1000_TDBAH(0), txhi);
    e8254x_writereg(device, E1000_TDLEN(0), txlen);
    e8254x_writereg(device, E1000_TDH(0), txhead);
    e8254x_writereg(device, E1000_TDT(0), txtail);

    for (size_t i = 0; i < 256; i++) {
        struct e1000_txdesc *desc = &device->txdescs[i];
        uint64_t phys = (uint64_t)pmm_alloc(1);
        memset((void *)phys, 0, PAGE_SIZE);
        desc->addr = phys;
        desc->status = 0;
    }

    e8254x_writereg(device, E1000_TCTL, E8254X_TCTLEN | E8254X_TCTLPSP); // enable and pad packets for us
}

static void e1000_transmitpacket(struct net_adapter *adapter, const void *data, size_t length) {
    log.info("e1000_transmitpacket(0x%016lx, 0x%016lx, %lu);\n", adapter, data, length);
    struct e1000_device *device = (struct e1000_device *)adapter;
    bool old = interrupt_toggle(false);

    struct pagemap *pagemap = NULL;
    if (Scheduler::GetCurrentThread() != NULL && Scheduler::GetCurrentThread()->pgm != krnl_page) {
        // XXX: Do NOT give up and simply swap pagemaps, as that's simply not a good idea (there might be issues later down the line)
        pagemap = Scheduler::GetCurrentThread()->pgm;  // store current pagemap
        vmm_switch_to(krnl_page); // swap to the kernel's pagemap
    }

    struct e1000_txdesc *desc = &device->txdescs[device->txtail];
    memcpy((void *)(desc->addr), data, length); // copy packet into data
    desc->len = length;
    // descriptor should be:
    // - EOP (end of packet)
    // - IFCS (insert frame check sequence)
    // - RS (report status)
    // legacy (no extension) is implict
    desc->cmd = (1 << 0) | (1 << 1) | (1 << 3);

    device->txtail = (device->txtail + 1) % 256;

    e8254x_writereg(device, E1000_TDT(0), device->txtail);

    if (pagemap) {
        vmm_switch_to(pagemap); // swap back to our old pagemap to prevent issues (kernel has access to everything in maps, but this pagemap doesn't)
    }

    interrupt_toggle(old);
}

static void e1000_linkupdate(struct e1000_device *device) {
    bool link = e8254x_readreg(device, E1000_STATUS) & E8254X_STATUSLU;

    // IFF_RUNNING only defines the operative state of the hardware *not* the administrative state (administrative is completely software)
    if (link) {
        device->flags |= IFF_RUNNING;
        int speed = e8254x_readreg(device, E1000_STATUS) & E8254X_STATUSSPEED;
        char *speed_str = "10MB/s";
        if (speed == 0x00000040) {
            speed_str = "100MB/s";
        } if (speed == 0x00000080) {
            speed_str = "1GB/s";
        }
        log.info("link up! (%s)\n", speed_str);
    } else {
        device->flags &= ~IFF_RUNNING;
        log.info("link down\n");
    }
}

static void e1000_updateflags(struct net_adapter *device, uint16_t old) {
    if (!(device->flags & IFF_UP) && device->flags & IFF_DYNAMIC) { // interface is down and dynamic (loses interfaces on interface down)
        memset(&device->ip, 0, sizeof(struct net_inetaddr));
        memset(&device->gateway, 0, sizeof(struct net_inetaddr));
        memset(&device->subnetmask, 0, sizeof(struct net_inetaddr));
    }

    if (old & IFF_RUNNING && !(device->flags & IFF_RUNNING)) {
        device->flags |= IFF_RUNNING; // disallow change
    } else if (!(old & IFF_RUNNING) && (device->flags & IFF_RUNNING)) {
        device->flags &= ~IFF_RUNNING; // disallow changes
    }
}

static void e1000_int_handler(struct cpu_ctx *_, void *priv) {
    log.info("got int\n");
}

static void e1000_init(struct pci_device *device) {
    log.info("Initializing ethernet controller %04x:%04x...\n", device->vendor_id, device->device_id);

    pci_set_privl(device, PCI_PRIV_BUSMASTER | PCI_PRIV_MMIO | PCI_PRIV_PIO);

    struct e1000_device *dev = CREATE_RESOURCE(struct e1000_device);

    dev->rxtail = 0;
    dev->txtail = 0;

    struct pci_bar mmio_bar = pci_get_bar(device, 0);
    pci_map_bar(mmio_bar);

    dev->base = mmio_bar.base;
    dev->iobase = !pci_get_bar(device, 1).is_mmio ? pci_get_bar(device, 1).base : pci_get_bar(device, 2).base;

    log.debug("base=0x%016lx, iobase=0x%016lx\n", dev->base, dev->iobase);

    e8254x_writereg(dev, E1000_IMC, 0xffffffff);
    e8254x_writereg(dev, E1000_ICR, 0xffffffff);
    e8254x_readreg(dev, E1000_STATUS);

    e8254x_writereg(dev, E1000_RCTL, 0);
    e8254x_writereg(dev, E1000_TCTL, E8254X_TCTLPSP);
    e8254x_readreg(dev, E1000_STATUS);

    log.warn("TODO: Add sleep\n");

    for (int i=0;i<5000;i++) asm volatile ("pause");

    e8254x_writereg(dev, E1000_CTRL, e8254x_readreg(dev, E1000_CTRL) | E8254X_CTRLRST); // reset controller
    while (e8254x_readreg(dev, E1000_CTRL) & E8254X_CTRLRST) {
        for (int i=0;i<5000;i++) asm volatile ("pause"); // wait for bit clear
    }

    e8254x_writereg(dev, E1000_IMC, 0xffffffff);
    e8254x_writereg(dev, E1000_ICR, 0xffffffff);
    e8254x_readreg(dev, E1000_STATUS);

    bool haseeprom = e8254x_readreg(dev, E1000_EEC) & (1 << 8);
    if (!haseeprom) {
        log.error("No EEPROM Detected!\n");
        free(dev);
        return;
    }

    int pciirq = PCI_READB(device, 0x3c);

    log.debug("irq=%d\n", pciirq);

    uint16_t maclo = e8254x_readnvm(dev, 0);
    uint16_t macmid = e8254x_readnvm(dev, 1);
    uint16_t machi = e8254x_readnvm(dev, 2);

    dev->mac.mac[0] = maclo & 0xff;
    dev->mac.mac[1] = (maclo >> 8) & 0xff;
    dev->mac.mac[2] = macmid & 0xff;
    dev->mac.mac[3] = (macmid >> 8) & 0xff;
    dev->mac.mac[4] = machi & 0xff;
    dev->mac.mac[5] = (machi >> 8) & 0xff;

    dev->permmac = dev->mac; // permanent mac starts initially as this (TODO: allow changing of MAC?)

    dev->hwmtu = 1522;

    log.info("MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n", NET_PRINTMAC(dev->mac));

    e8254x_writereg(dev, E1000_CTRL, e8254x_readreg(dev, E1000_CTRL) | E8254X_CTRLSLU | E8254X_CTRLASDE); // link enable, auto speed negotiation

    for (size_t i = 0; i < 128; i++) {
        e8254x_writereg(dev, E1000_MTA + i * 4, 0); // multicast table array
    }

    for (size_t i = 0; i < 64; i++) {
        e8254x_readreg(dev, E1000_CRCERRS + i * 4); // clear statistic registers
    }

    e1000_initrx(dev);
    e1000_inittx(dev);

    e8254x_writereg(dev, E1000_RDTR, 0); // zero delay on interrupt timer
    e8254x_writereg(dev, E1000_ITR, 651); // interrupt throttling (initial suggested as of SDM)
    e8254x_readreg(dev, E1000_STATUS);

    e8254x_writereg(dev, E1000_IMS, E8254X_INTTXDW | E8254X_INTTXQE | E8254X_INTLSC | E8254X_INTRXSEQ | E8254X_INTRXT0);
    e1000_linkupdate(dev);

    dev->can_mmap = false;
    dev->stat.st_mode = 0666 | S_IFCHR;
    dev->stat.st_rdev = resource_create_dev_id();
    dev->ioctl = net_ifioctl;

    dev->txpacket = e1000_transmitpacket;
    dev->updateflags = e1000_updateflags;

    dev->type = NET_ADAPTERETH; // ethernet network driver
    Net::Register((struct net_adapter *)dev);
    Kernel::RegisterInterruptHandler(dev->vector, e1000_int_handler, (void *)dev);

    const char *tst =     // Ethernet Header (14 bytes)
    "\xff\xff\xff\xff\xff\xff"      // Destination MAC (broadcast)
    "\x52\x54\x00\x12\x34\x56"      // Source MAC (your device MAC)
    "\x08\x06"                       // EtherType (0x0806 = ARP)
    
    // ARP Header (28 bytes)
    "\x00\x01"                       // Hardware type (1 = Ethernet)
    "\x08\x00"                       // Protocol type (0x0800 = IPv4)
    "\x06"                           // Hardware address length (6 bytes)
    "\x04"                           // Protocol address length (4 bytes)
    "\x00\x01"                       // Operation (1 = ARP Request)
    "\x52\x54\x00\x12\x34\x56"      // Sender MAC (your device MAC)
    "\x0a\x00\x02\x0f"               // Sender IP (192.168.1.100)
    "\xff\xff\xff\xff\xff\xff"      // Target MAC (unknown, broadcast)
    "\x0a\x00\x02\x02";              // Target IP (192.168.1.1)

    dev->txpacket(dev, (void *)tst, sizeof(tst));

    dev->cachelock = (spinlock_t)SPINLOCK_INIT;

    devtmpfs_add_device((struct resource *)dev, dev->ifname);

    //log.warn("TOOD: Start RX routine\n");
}

static struct pci_driver e8254x_driver = {
    .name = "e1000",
    .match = PCI_MATCH_DEVICE,
    .init = e1000_init,
    .pci_class = 0,
    .subclass = 0,
    .prog_if = 0,
    .vendor = 0x8086,
    .devcount = 25,
    .devices = {
        0x1004, 0x100e, 0x100f, 0x1010, 0x1011, 0x1012,
        0x1013, 0x1015, 0x1016, 0x1017, 0x1018, 0x1019,
        0x101a, 0x101d, 0x1026, 0x1027, 0x1028, 0x1076,
        0x1078, 0x1079, 0x107a, 0x107b, 0x1107, 0x1112,
        0x100c
    }
};

EXPORT_PCI_DRIVER(e8254x_driver);