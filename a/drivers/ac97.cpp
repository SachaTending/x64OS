#include <drivers/pci.hpp>
#include <io.h>
#include <logging.hpp>
#include <libc.h>
#include <vmm.h>
#include <krnl.hpp>
#include <fs/devtmpfs.hpp>
#include <idt.hpp>
#define VMM_HIGHER_HALF hhdm2.response->offset

struct ac97_buf_info {
    uint32_t buf_addr;
    uint16_t samples;
    uint16_t bits;
} __attribute__((packed));

static Logger log("AC97");

static pci_id_t ac97_ids[] = {
    {0x8086, 0x2415}
};

struct ac97_priv {
    uint16_t bar0, bar1;
    ac97_buf_info *bufs;
    char buf_count;
    pci_dev_t *dev;
    bool normal_sample_rate;
    bool int_trig;
};

#define BIT(n) (1 << n)
static bool ac97_probe(struct pci_driver *d, pci_dev_t *dev) {
    uint8_t clazz = pci_read(*dev, PCI_CLASS);
    uint8_t subclass = pci_read(*dev, PCI_SUBCLASS);
    if (clazz == 0x04 and subclass == 0x01) {
        log.info("Detected AC97 Card %04x:%04x\n", pci_read(*dev, PCI_VENDOR_ID), pci_read(*dev, PCI_DEVICE_ID));
        return true;
    }
    return false;
}

static void ac97_write_samp_rate(ac97_priv *priv, uint16_t samp_rate) {
    outw(priv->bar0+0x2C, samp_rate);
    outw(priv->bar0+0x2E, samp_rate);
    outw(priv->bar0+0x30, samp_rate);
    outw(priv->bar0+0x32, samp_rate);
}
static void ac97_int_handler(idt_regs *r, void *priv) {
    (void)r;
    ac97_priv *priv2 = (ac97_priv *)priv;
    if (inw(priv2->bar1+0x16) & BIT(2)) {
        priv2->int_trig = true;
        outw(priv2->bar1+0x16, BIT(2));
    } else if (inw(priv2->bar1+0x16) & BIT(3)) {
        priv2->int_trig = true;
        outw(priv2->bar1+0x16, BIT(3));
    }
}
 static int ac97_dsp_write(devtmpfs_node_t *node, void *buf, size_t len) {
   // log.debug("ac97_dsp_write(0x%lx, 0x%lx, %lu);\n", node, buf, len);
    ac97_priv *priv = (ac97_priv *)node->priv;
    // Wait for card to complete playing audio
    while (priv->int_trig == false);
    priv->int_trig = false;
    //log.debug("bar0: 0x%04x bar1: 0x%04x, bdl: 0x%lx\n", priv->bar0, priv->bar1, priv->bufs);
    // Calculate needed buffers for this data
    uint32_t samples = len/2; // divide by 2 because only 16 bit samples are supported.
    if (samples >= 0xfffe) {
        return -1; // TODO: Handle this properly.
        // Looks like we need more buffers.
        size_t buf_cnt = samples / 0xfffe;
        if (buf_cnt > 31) {
            // Oh no, someone tried to write more samples than buffers can hold.
            // TODO: Do something about it.
            return -1;
        }
    } else size_t buf_cnt = 1;
    // Now set last valid entry to next buffer.
    uint16_t curr_buf = inb(priv->bar1+0x14); // Get current buffer
    //outb(priv->bar1+0x15, (curr_buf+1) & 0b11111);
    // Now write samples to current buffer
    pagemap *pgm = get_current_task()->pgm;
    vmm_switch_to(krnl_page);
    ac97_buf_info *buf2 = &priv->bufs[curr_buf];
    buf2->buf_addr = vmm_virt2phys(pgm, (uint64_t)buf);
    buf2->samples = (uint16_t)samples;
    // Switch back to task pgm.
    vmm_switch_to(pgm);
    //log.debug("curr_buf: 0x%lx, samples: %lu\n", curr_buf, samples);
    // Start DMA.
    outb(priv->bar1+0x1B, 1 | BIT(3) | BIT(2));
    uint16_t s = inw(priv->bar1+0x16);
    return len;
}

static void * ac97_init(struct pci_driver *d, pci_dev_t *dev) {
    ac97_priv *priv = new ac97_priv;
    uint16_t bar0 = pci_get_bar(0, dev);
    uint16_t bar1 = pci_get_bar(1, dev);
    priv->bar0 = bar0;
    priv->bar1 = bar1;
    priv->dev = dev;
    uint64_t pci_cmd = pci_read(*dev, PCI_COMMAND);
    pci_cmd |= BIT(2) | BIT(0);
    pci_write(*dev, PCI_COMMAND, pci_cmd);
    outl(bar1+0x2c, 0x3);
    outw(bar0, 0xFF);
    outb(bar1+0xb, 2);
    outb(bar1+0x1b, 2 | BIT(3));
    outb(bar1+0x2b, 2);
    priv->normal_sample_rate = false;
    //outw(bar0+0x18, 0);
    //outw(bar0+0x02, 0);
    uint16_t caps1 = inw(bar0);
    uint16_t caps2 = inw(bar0+0x28);
    log.info("Capabilites: 0x%x, BAR0: 0x%x, Extended capabilites 1: 0x%x, BAR1: 0x%04x\n", caps1, bar0, caps2, bar1);
    if (caps2 & BIT(0)) {
        log.info("Card supports more sample rates.\n");
        outw(bar0+0x2A, BIT(0));
        priv->normal_sample_rate = true;
        ac97_write_samp_rate(priv, 8000);
    }
    outw(bar0+0x02, 0);
    outw(bar0+0x18, 0b01000);
    ac97_buf_info *buf = (ac97_buf_info *)pmm_alloc(1);
    vmm_map_range(krnl_page, (uint64_t)buf, 4096, PTE_PRESENT | PTE_WRITABLE);
    memset(buf, 0, sizeof(ac97_buf_info)*32);
    //buf += VMM_HIGHER_HALF;
    priv->bufs = buf;
    priv->buf_count = 32;
    for (int i=0;i<32;i++) {
        buf[i].bits = BIT(15);
    }
    outb(bar1+0x1B, 0x2 | BIT(3));
    uint8_t stat = inb(bar1+0x1B);
    while (stat & 0x2) {
        log.info("Waiting for card to clear... bar1: 0x%04x\n", bar1);
        log.info("stat: 0x%x\n", stat);
        stat = inb(bar1+0x1B);
    }
    //outb(bar1+0x1B, 2);
    outw(bar1+0x16, 0x1C);
    log.debug("phys addr of BDL: 0x%lx, 0x%lx\n", (uint32_t)vmm_virt2phys(krnl_page, (uint64_t)buf), vmm_virt2phys(krnl_page, (uint64_t)buf));
    outl(bar1+0x10, (uint32_t)vmm_virt2phys(krnl_page, (uint64_t)buf));
    outb(priv->bar1+0x15, 32);
    idt_set_int(pci_read(*dev, PCI_INTERRUPT_LINE), ac97_int_handler, (void *)priv);
    //outb(bar1+0x15, 1);
    devtmpfs_node_t *n = new devtmpfs_node_t;
    n->priv = (void *)priv;
    n->write = ac97_dsp_write;
    devtmpfs_register_file(n, "/dsp");
    priv->int_trig = true;
    return priv;
}

static pci_driver_t drv = {
    .supported_ids = ac97_ids,
    .name = "AC97 Generic driver",
    .probe = ac97_probe,
    .init = ac97_init
};

__attribute__((constructor)) static void reg() {
    pci_register_driver(&drv);
}