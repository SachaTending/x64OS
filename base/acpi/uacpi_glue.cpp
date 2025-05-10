#include <uacpi/kernel_api.h>
#include <libc.h>
#include <logging.hpp>
#include <vmm.h>
#include <krnl.hpp>
#include <io.h>
#include <drivers/pci.hpp>
#include <spinlock.h>
#include <uacpi/uacpi.h>
#include <uacpi/event.h>
#include <idt.hpp>

typedef struct {
    uacpi_interrupt_handler handl;
    uacpi_handle ctx;
    int irq;
} irq_context_t;

void uacpi_generic_irq_handler(idt_regs *regs, void *ctx) {
    irq_context_t *c = (irq_context_t *)ctx;
    c->handl(c->handl);
}

extern limine_rsdp_request limine_rsdp;

static Logger log("UACPI");

extern "C" {
    void *uacpi_kernel_alloc(uacpi_size size) {
        //log.debug("alloc %u\n", size);
        return malloc(size);
    }
    void uacpi_kernel_free(void *mem) {
        //log.debug("free 0x%lx\n", mem);
        free(mem);
    }
    uacpi_status uacpi_kernel_io_map(
        uacpi_io_addr base, uacpi_size len, uacpi_handle *out_handle
    ) {
        *out_handle = (void *)base;
        log.debug("map io 0x%lx\n", base);
        return UACPI_STATUS_OK;
    }
    void uacpi_kernel_io_unmap(uacpi_handle handle) {
        //log.debug("unmap io 0x%lx\n", handle);
    }
    void *uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len) {
        //log.debug("map mem 0x%lx size 0x%lx\n", addr, len);
        return (void*)addr+VMM_HIGHER_HALF;
    }
    void uacpi_kernel_unmap(void *addr, uacpi_size len) {
        (void)len;
        //log.debug("unmap mem 0x%lx\n", addr);
    }
    uacpi_status uacpi_kernel_io_read8(
        uacpi_handle handl, uacpi_size offset, uacpi_u8 *out_value
    ) {
        *out_value = inb((uint16_t)handl+offset);
        return UACPI_STATUS_OK;
    }
    uacpi_status uacpi_kernel_io_read16(
        uacpi_handle handl, uacpi_size offset, uacpi_u16 *out_value
    ) {
        *out_value = inw((uint16_t)handl+offset);
        return UACPI_STATUS_OK;
    }
    uacpi_status uacpi_kernel_io_read32(
        uacpi_handle handl, uacpi_size offset, uacpi_u32 *out_value
    ) {
        *out_value = inl((uint16_t)handl+offset);
        return UACPI_STATUS_OK;
    }

    uacpi_status uacpi_kernel_io_write8(
        uacpi_handle handl, uacpi_size offset, uacpi_u8 in_value
    ) {
        outb((uint16_t)handl+offset, in_value);
        return UACPI_STATUS_OK;
    }

    uacpi_status uacpi_kernel_io_write16(
        uacpi_handle handl, uacpi_size offset, uacpi_u16 in_value
    ) {
        outw((uint16_t)handl+offset, in_value);
        return UACPI_STATUS_OK;
    }

    uacpi_status uacpi_kernel_io_write32(
        uacpi_handle handl, uacpi_size offset, uacpi_u32 in_value
    ) {
        outl((uint16_t)handl+offset, in_value);
        return UACPI_STATUS_OK;
    }
    uacpi_status uacpi_kernel_pci_device_open(
        uacpi_pci_address address, uacpi_handle *out_handle
    ) {
        pci_dev_t *dev = new pci_dev_t;
        if (dev == NULL) return UACPI_STATUS_OUT_OF_MEMORY;
        dev->device_num = address.device;
        dev->function_num = address.function;
        dev->bus_num = address.bus;
        *out_handle = (void *)dev;
        return UACPI_STATUS_OK;
    }
    void uacpi_kernel_pci_device_close(uacpi_handle h) {
        pci_dev_t *d = (pci_dev_t *)h;
        delete d;
    }
    uacpi_status uacpi_kernel_pci_read8(
        uacpi_handle device, uacpi_size offset, uacpi_u8 *value
    ) {
        pci_dev_t *d = (pci_dev_t *)device;
        *value = pci_read_size(*d, offset, 1);
        return UACPI_STATUS_OK;
    }
    uacpi_status uacpi_kernel_pci_read16(
        uacpi_handle device, uacpi_size offset, uacpi_u16 *value
    ) {
        pci_dev_t *d = (pci_dev_t *)device;
        *value = pci_read_size(*d, offset, 2);
        return UACPI_STATUS_OK;
    }
    uacpi_status uacpi_kernel_pci_read32(
        uacpi_handle device, uacpi_size offset, uacpi_u32 *value
    ) {
        pci_dev_t *d = (pci_dev_t *)device;
        *value = pci_read_size(*d, offset, 4);
        return UACPI_STATUS_OK;
    }
    uacpi_status uacpi_kernel_pci_write8(
        uacpi_handle device, uacpi_size offset, uacpi_u8 value
    ) {
        pci_dev_t *d = (pci_dev_t *)device;
        pci_write(*d, offset, value);
        return UACPI_STATUS_OK;
    }
    uacpi_status uacpi_kernel_pci_write16(
        uacpi_handle device, uacpi_size offset, uacpi_u16 value
    ) {
        pci_dev_t *d = (pci_dev_t *)device;
        pci_write(*d, offset, value);
        return UACPI_STATUS_OK;
    }
    uacpi_status uacpi_kernel_pci_write32(
        uacpi_handle device, uacpi_size offset, uacpi_u32 value
    ) {
        pci_dev_t *d = (pci_dev_t *)device;
        pci_write(*d, offset, value);
        return UACPI_STATUS_OK;
    }
    uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out_rsdp_address) {
        log.info("uacpi get rsdp, addr 0x%lx\n", limine_rsdp.response->address);
        *out_rsdp_address = ((uacpi_phys_addr)limine_rsdp.response->address)-VMM_HIGHER_HALF;
        return UACPI_STATUS_OK;
    }
    uacpi_thread_id uacpi_kernel_get_thread_id(void) {
        return (void *)getpid();
    }
    void uacpi_kernel_log(uacpi_log_level level, const uacpi_char*txt) {
        switch (level) {
            case UACPI_LOG_DEBUG:
                log.debug("uacpi debug log: %s", txt);
                break;
            case UACPI_LOG_ERROR:
                log.error("uacpi error log: %s", txt);
                break;
            case UACPI_LOG_INFO:
                log.info("uacpi info log: %s", txt);
                break;
            case UACPI_LOG_WARN:
                log.warn("uacpi warn log: %s", txt);
                break;
            case UACPI_LOG_TRACE:
                log.debug("uacpi trace log: %s", txt);
                break;
            default:
                log.info("uacpi unknown log level(%d) log: %s", level, txt);
                break;
        }
    }
    uacpi_handle uacpi_kernel_create_mutex() {
        return malloc(sizeof(spinlock_t));
    }
    void uacpi_kernel_free_mutex(uacpi_handle h) {
        free(h);
    }
    uacpi_status uacpi_kernel_acquire_mutex(uacpi_handle h, uacpi_u16 t) {
        spinlock_t *s = (spinlock_t *)h;
        if (t == 0) {
            bool r = spinlock_test_and_acq(s);
            if (r == 1) {
                return UACPI_STATUS_OK;
            } else {
                return UACPI_STATUS_TIMEOUT;
            }
        } else if (t == 0xFFFF) {
            spinlock_acquire_no_dead_check(s);
            return UACPI_STATUS_OK;
        } else {
            for (int i=0;i<(t*100);i++) {
                bool r = spinlock_test_and_acq(s);
                if (r == 1) {
                    return UACPI_STATUS_OK;
                }
            }
            return UACPI_STATUS_TIMEOUT;
        }
    }
    void uacpi_kernel_release_mutex(uacpi_handle h) {
        spinlock_t *s = (spinlock_t *)h;
        spinlock_release(s);
    }
    uacpi_handle uacpi_kernel_create_spinlock(void) {
        return malloc(sizeof(spinlock_t));
    }
    void uacpi_kernel_free_spinlock(uacpi_handle h) {
        free(h);
    }
    uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle h) {
        spinlock_acquire((spinlock_t *)h);
        return 0;
    }
    void uacpi_kernel_unlock_spinlock(uacpi_handle h, uacpi_cpu_flags f) {
        (void)f;
        spinlock_release((spinlock_t *)h);
    }
    void uacpi_kernel_stall(uacpi_u8 usec) {
        for (int i=0;i<(usec*100);i++) {
            asm volatile ("pause");
        }
    }
    void uacpi_kernel_sleep(uacpi_u64 msec) {
        for (int i=0;i<(msec*10);i++) {
            asm volatile ("pause");
        }
    }
    uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void) {
        return 0; // TODO
    }
    uacpi_handle uacpi_kernel_create_event(void) {
        log.info("uacpi create event called\n");
        return (void*)UACPI_STATUS_UNIMPLEMENTED; // TODO
    }
    void uacpi_kernel_free_event(uacpi_handle h) {
        (void)h;
    }
    uacpi_status uacpi_kernel_schedule_work(
        uacpi_work_type t, uacpi_work_handler h, uacpi_handle ctx
    ) {
        log.debug("schedule work called: %d 0x%lx 0x%lx\n", t, h, ctx);
        return UACPI_STATUS_UNIMPLEMENTED;
    }
    uacpi_status uacpi_kernel_install_interrupt_handler(
        uacpi_u32 irq, uacpi_interrupt_handler h, uacpi_handle ctx,
        uacpi_handle *out_irq_handle
    ) {
        (void)h;
        (void)ctx;
        (void)out_irq_handle;
        log.debug("install interrupt handler for irq %lu called.\n", irq);
        irq_context_t *c = new irq_context_t;
        if (c == NULL) return UACPI_STATUS_OUT_OF_MEMORY;
        c->handl = h;
        c->ctx = ctx;
        c->irq = irq;
        *out_irq_handle = (uacpi_handle)c;
        idt_set_int(irq, uacpi_generic_irq_handler, (void *)c);
        return UACPI_STATUS_OK;
    }
    uacpi_status uacpi_kernel_wait_for_work_completion(void) {
        log.debug("wait work called.\n");
        return UACPI_STATUS_UNIMPLEMENTED;
    }
    uacpi_status uacpi_kernel_uninstall_interrupt_handler(
        uacpi_interrupt_handler h, uacpi_handle irq_handle
    ) {
        (void)h;
        (void)irq_handle;
        return UACPI_STATUS_UNIMPLEMENTED;
    }
    void uacpi_kernel_signal_event(uacpi_handle) {
        // TODO
    }
    void uacpi_kernel_reset_event(uacpi_handle) {
        // TODO
    }
    uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle, uacpi_u16) {
        return UACPI_FALSE; // TODO
    }
    uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request* rq) {
        log.debug("handle firmware request called, rq: 0x%lx.\n", rq);
        return UACPI_STATUS_UNIMPLEMENTED;
    }
}

void uacpi_init() {
    uacpi_status ret = uacpi_initialize(0);
    if (uacpi_unlikely_error(ret)) {
        log.error("uacpi_initialize error: %s", uacpi_status_to_string(ret));
        return;
    }
    ret = uacpi_namespace_load();
    if (uacpi_unlikely_error(ret)) {
        log.error("uacpi_namespace_load error: %s", uacpi_status_to_string(ret));
        return;
    }
    ret = uacpi_namespace_initialize();
    if (uacpi_unlikely_error(ret)) {
        log.error("uacpi_namespace_initialize error: %s", uacpi_status_to_string(ret));
        return;
    }
    ret = uacpi_finalize_gpe_initialization();
    if (uacpi_unlikely_error(ret)) {
        log.error("uACPI GPE initialization error: %s", uacpi_status_to_string(ret));
        return;
    }
}