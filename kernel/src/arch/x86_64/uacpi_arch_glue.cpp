#include <uacpi/kernel_api.h>
#include <arch/io.h>
#include <libc.h>

struct io_handle {
    uint16_t io_addr;
    size_t size;
};

extern "C" {
    uacpi_status uacpi_kernel_io_map(
        uacpi_io_addr base, uacpi_size len, uacpi_handle *out_handle
    ) {
        *out_handle = new io_handle;
        io_handle *h = (io_handle *)*out_handle;
        h->io_addr = base;
        h->size = len;
        return UACPI_STATUS_OK;
    }
    void uacpi_kernel_io_unmap(uacpi_handle h) {
        free(h);
    }
    uacpi_status uacpi_kernel_io_read8(
        uacpi_handle handl2, uacpi_size offset, uacpi_u8 *out_value
    ) {
        uint16_t handl = ((io_handle *)handl2)->io_addr;
        *out_value = inb((uint16_t)handl+offset);
        return UACPI_STATUS_OK;
    }
    uacpi_status uacpi_kernel_io_read16(
        uacpi_handle handl2, uacpi_size offset, uacpi_u16 *out_value
    ) {
        uint16_t handl = ((io_handle *)handl2)->io_addr;
        *out_value = inw((uint16_t)handl+offset);
        return UACPI_STATUS_OK;
    }
    uacpi_status uacpi_kernel_io_read32(
        uacpi_handle handl2, uacpi_size offset, uacpi_u32 *out_value
    ) {
        uint16_t handl = ((io_handle *)handl2)->io_addr;
        *out_value = inl((uint16_t)handl+offset);
        return UACPI_STATUS_OK;
    }

    uacpi_status uacpi_kernel_io_write8(
        uacpi_handle handl2, uacpi_size offset, uacpi_u8 in_value
    ) {
        uint16_t handl = ((io_handle *)handl2)->io_addr;
        outb((uint16_t)handl+offset, in_value);
        return UACPI_STATUS_OK;
    }

    uacpi_status uacpi_kernel_io_write16(
        uacpi_handle handl2, uacpi_size offset, uacpi_u16 in_value
    ) {
        uint16_t handl = ((io_handle *)handl2)->io_addr;
        outw((uint16_t)handl+offset, in_value);
        return UACPI_STATUS_OK;
    }

    uacpi_status uacpi_kernel_io_write32(
        uacpi_handle handl2, uacpi_size offset, uacpi_u32 in_value
    ) {
        uint16_t handl = ((io_handle *)handl2)->io_addr;
        outl((uint16_t)handl+offset, in_value);
        return UACPI_STATUS_OK;
    }
}