#pragma once
#include <stdint.h>

typedef union pci_dev {
    uint32_t bits;
    struct {
        uint32_t always_zero    : 2;
        uint32_t field_num      : 6;
        uint32_t function_num   : 3;
        uint32_t device_num     : 5;
        uint32_t bus_num        : 8;
        uint32_t reserved       : 7;
        uint32_t enable         : 1;
    };
} pci_dev_t;

#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC

#define PCI_VENDOR_ID            0x00
#define PCI_DEVICE_ID            0x02
#define PCI_COMMAND              0x04
#define PCI_STATUS               0x06
#define PCI_REVISION_ID          0x08
#define PCI_PROG_IF              0x09
#define PCI_SUBCLASS             0x0a
#define PCI_CLASS                0x0b
#define PCI_CACHE_LINE_SIZE      0x0c
#define PCI_LATENCY_TIMER        0x0d
#define PCI_HEADER_TYPE          0x0e
#define PCI_BIST                 0x0f
#define PCI_BAR0                 0x10
#define PCI_BAR1                 0x14
#define PCI_BAR2                 0x18
#define PCI_BAR3                 0x1C
#define PCI_BAR4                 0x20
#define PCI_BAR5                 0x24
#define PCI_INTERRUPT_LINE       0x3C
#define PCI_SECONDARY_BUS        0x09

#define DEVICE_PER_BUS           32
#define FUNCTION_PER_DEVICE      8

typedef struct pci_id {
    uint16_t vendor_id, dev_id;
} pci_id_t;

typedef bool (*pci_prode_f)(struct pci_driver *, pci_dev_t *dev);
typedef void * (*pci_init_f)(struct pci_driver *, pci_dev_t *dev);

typedef struct pci_driver {
    pci_id_t *supported_ids;
    const char *name;
    pci_prode_f probe;
    pci_init_f init;
    void *priv;
} pci_driver_t;

void pci_register_driver(pci_driver_t *driver);
uint32_t pci_read(pci_dev_t dev, uint32_t field);
uint64_t pci_get_bar(int bar, pci_dev_t *dev);
void pci_write(pci_dev_t dev, uint32_t field, uint32_t value);