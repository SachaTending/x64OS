#pragma once
#include <drivers/pci.hpp>

class XHCI {
    private:
    pci_device *dev;
    pci_bar bar0;
    uint32_t op_regs_offset;
    public:
    XHCI(pci_device *dev);

    void halt_ctrl();
};