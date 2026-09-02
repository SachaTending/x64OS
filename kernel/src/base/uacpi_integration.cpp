#include <limine.h>
#include <uacpi/kernel_api.h>
#include <uacpi/uacpi.h>
#include <logging.hpp>
#include <libc.h>
#include <arch/vmm.h>
#include <spinlock.h>
#include <uacpi/event.h>

static Logger log("uACPI");

extern volatile limine_rsdp_request rsdp_request;

extern "C" {
    uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *rsdp) {
        *rsdp = (uint64_t)(rsdp_request.response->address);
        return UACPI_STATUS_OK;
    }
    void *uacpi_kernel_alloc(uacpi_size size) {
        //log.debug("alloc(%lu)\n", size);
        //log.debug("allocate %lu pages\n", ALIGN_UP(size, 4096)/PAGE_SIZE);
        void *a = pmm_alloc(ALIGN_UP(size, 4096)/PAGE_SIZE);
        //log.debug("alloc(%lu): 0x%lx\n", size, a);
        return a+VMM_HIGHER_HALF;
    }
    void uacpi_kernel_free(void *mem) {
        if (mem == 0) return;
        if ((uint64_t)mem < VMM_HIGHER_HALF) return;
        //log.debug("free(0x%lx)\n", mem);
        //free(mem);
    }
    void *uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len) {
        log.debug("map 0x%lx, size: 0x%lx\n", addr, len);
        //len += 0x100000;
        if (addr > VMM_HIGHER_HALF) return (void *)addr;
        uint64_t start2 = ALIGN_DOWN(addr, 4096);
        uint64_t end = ALIGN_UP(addr+len, 4096);
        size_t pages = (end/4096)-(start2/4096);
        pages += 1;
        for (int i=0;i<pages;i++) {
            vmm_map_page(krnl_page, start2+(i*PAGE_SIZE), start2+(i*PAGE_SIZE), PTE_WRITABLE | PTE_PRESENT);
        }
        
        log.debug("map result: 0x%lx\n", addr);
        return (void *)(addr);
    }
    uacpi_handle uacpi_kernel_create_spinlock() {
        return malloc(sizeof(spinlock_t));
    }
    uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle s) {
        spinlock_acquire((spinlock_t *)s);
        return 0;
    }
    void uacpi_kernel_unlock_spinlock(uacpi_handle s, uacpi_cpu_flags _) {
        (void)_;
        spinlock_release((spinlock_t *)s);
    }
    void uacpi_kernel_free_spinlock(uacpi_handle s) {
        free(s);
    }
    void uacpi_kernel_unmap(void *addr, uacpi_size len) {
        (void)addr;
        (void)len;
        //log.info("attempted to unmap addr 0x%x len %lu\n", addr, len);
    }
    void uacpi_kernel_log(uacpi_log_level l, const uacpi_char*t) {
        switch (l)
        {
            case UACPI_LOG_DEBUG:
                log.debug("uACPI: %x", t);
                break;
            case UACPI_LOG_ERROR:
                log.error("uACPI: %x", t);
                break;
            case UACPI_LOG_INFO:
                log.info("uACPI: %s", t);
                break;
            case UACPI_LOG_TRACE:
                log.debug("(trace)uACPI: %x\n", t);
                break;
            case UACPI_LOG_WARN:
                log.warn("uACPI: %s", t);
                break;
            default:
                log.info("(unknown log level %d)uACPI: %s", l, t);
                break;
        }
    }
    uacpi_thread_id uacpi_kernel_get_thread_id(void) {
        return 0; // TODO
    }
    uacpi_status uacpi_kernel_wait_for_work_completion(void) {
        return UACPI_STATUS_UNIMPLEMENTED; // TODO
    }
    void uacpi_kernel_release_mutex(uacpi_handle h) {
        spinlock_t *s = (spinlock_t *)h;
        spinlock_release(s);
    }
    uacpi_handle uacpi_kernel_create_mutex(void) {
        return malloc(sizeof(spinlock_t)); // TODO
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
    uacpi_status uacpi_kernel_pci_write8(
        uacpi_handle device, uacpi_size offset, uacpi_u8 value
    ) {
        return UACPI_STATUS_UNIMPLEMENTED;
    }
    uacpi_status uacpi_kernel_pci_write16(
        uacpi_handle device, uacpi_size offset, uacpi_u16 value
    ) {
        return UACPI_STATUS_UNIMPLEMENTED;
    }
    uacpi_status uacpi_kernel_pci_write32(
        uacpi_handle device, uacpi_size offset, uacpi_u32 value
    ) {
        return UACPI_STATUS_UNIMPLEMENTED;
    }
    uacpi_status uacpi_kernel_pci_read8(
        uacpi_handle device, uacpi_size offset, uacpi_u8 *value
    ) {
        return UACPI_STATUS_UNIMPLEMENTED;
    }
    uacpi_status uacpi_kernel_pci_read16(
        uacpi_handle device, uacpi_size offset, uacpi_u16 *value
    ) {
        return UACPI_STATUS_UNIMPLEMENTED;
    }
    uacpi_status uacpi_kernel_pci_read32(
        uacpi_handle device, uacpi_size offset, uacpi_u32 *value
    ) {
        return UACPI_STATUS_UNIMPLEMENTED;
    }
    uacpi_status uacpi_kernel_pci_device_open(
        uacpi_pci_address address, uacpi_handle *out_handle
    ) {
        return UACPI_STATUS_UNIMPLEMENTED;
    }
    void uacpi_kernel_pci_device_close(uacpi_handle) {
        // TODO
    }
    uacpi_status uacpi_kernel_schedule_work(
        uacpi_work_type, uacpi_work_handler, uacpi_handle ctx
    )  {
        return UACPI_STATUS_UNIMPLEMENTED;
    }

    uacpi_status uacpi_kernel_uninstall_interrupt_handler(
        uacpi_interrupt_handler, uacpi_handle irq_handle
    )  {
        log.error("attempted to uninstall int handler, but it's unimplemented\n");
        return UACPI_STATUS_UNIMPLEMENTED;
    }
    uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request*) {
        return UACPI_STATUS_UNIMPLEMENTED;
    }
    void uacpi_kernel_signal_event(uacpi_handle) {
        // TODO 
    }
    void uacpi_kernel_reset_event(uacpi_handle) {
        // TODO
    }
    uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle, uacpi_u16) {
        return UACPI_FALSE;
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
        return 0;
    }
    void uacpi_kernel_free_mutex(uacpi_handle) {
        // TODO
    }
    uacpi_handle uacpi_kernel_create_event(void) {
        return 0; // TODO
    }
    void uacpi_kernel_free_event(uacpi_handle) {
        // TODO
    }
    uacpi_status uacpi_kernel_install_interrupt_handler(
        uacpi_u32 irq, uacpi_interrupt_handler, uacpi_handle ctx,
        uacpi_handle *out_irq_handle
    ) {
        log.error("attempted to install int handler on int %lu but it's unimplemented.\n", irq);
        return UACPI_STATUS_UNIMPLEMENTED;
    }
}
void init_uacpi();
void pre_sched_uacpi_init() {
    void *buf = pmm_alloc(4); // 4 pages(4096*4=16384 bytes=16 kilobytes) should be enough
    uacpi_setup_early_table_access(buf, 4*PAGE_SIZE);
}

void init_uacpi() {
    uacpi_status ret = uacpi_initialize(0);
    if (uacpi_unlikely_error(ret)) {
        log.error("Failed to initialize uACPI: %s\n", uacpi_status_to_string(ret));
        return;
    }

    ret = uacpi_namespace_load();
    if (uacpi_unlikely_error(ret)) {
        log.error("uacpi_namespace_load error: %s", uacpi_status_to_string(ret));
    }
    log.debug("uacpi namespace loaded.\n");

    ret = uacpi_namespace_initialize();
    if (uacpi_unlikely_error(ret)) {
        log.error("uacpi_namespace_initialize error: %s", uacpi_status_to_string(ret));
    }

    ret = uacpi_finalize_gpe_initialization();
    if (uacpi_unlikely_error(ret)) {
        log.error("uACPI GPE initialization error: %s", uacpi_status_to_string(ret));
        return;
    }
}