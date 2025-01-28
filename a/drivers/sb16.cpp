#include <io.h>
#include <logging.hpp>
#include <module.hpp>
#include <fs/devtmpfs.hpp>
#include <frg/vector.hpp>
#include <frg/std_compat.hpp>
#include <new>
#include <idt.hpp>

static Logger log("SB16 Driver");
#define DSP_WRITE 0x22C

struct sb16_priv {
    uint16_t io_base;
    uint8_t major_ver;
    uint8_t minor_ver;
    void *buf1;
    void *buf2;
    uint16_t buf_size;
    uint8_t current_buf;

};

struct sb16_drv_int {
    int intr;
    sb16_priv *struc;
};

typedef frg::vector<sb16_drv_int *, frg::stl_allocator> int_vec_t;

int_vec_t int_vec = int_vec_t();

static void sb16_intr(idt_regs *regs) {
    for (sb16_drv_int *intr : int_vec) {
        if (intr->intr == (int)regs->IntNumber) {
            sb16_priv *str = intr->struc;
            // TODO: Handle interrupt.
            if (str->current_buf = 0) {
                str->current_buf = 1;
                outb(0x0A, 5);
                outb(0x0C, 1);
                outb(0x08, 0x49);
                outb(0x83, ((uint64_t)str->buf2 >> 16) & 0xff);
                outb(0x02, ((uint64_t)str->buf2) & 0xff);
                outb(0x02, ((uint64_t)str->buf2 >> 8) & 0xff);
                outb(0x03, str->buf_size & 0xff);
                outb(0x03, (str->buf_size >> 8) & 0xff);
                outb(0x0A, 1);
            } else {
                str->current_buf = 0;
                outb(0x0A, 5);
                outb(0x0C, 1);
                outb(0x08, 0x49);
                outb(0x83, ((uint64_t)str->buf1 >> 16) & 0xff);
                outb(0x02, ((uint64_t)str->buf1) & 0xff);
                outb(0x02, ((uint64_t)str->buf1 >> 8) & 0xff);
                outb(0x03, str->buf_size & 0xff);
                outb(0x03, (str->buf_size >> 8) & 0xff);
                outb(0x0A, 1);
            }
            if (str->major_ver >= 4) {
                inb(str->io_base + 0xF); // 16 bit interrupt acknowledge
            }
        }
    }
    log.debug("Unhandled SB16 Interrupt %u\n", regs->IntNumber);
}

static void *init_sb16(module_t *mod) {
    log.info("Probing for Sound Blaster 16...\n");
    outb(0x226, 1);
    for (int i=0;i<10000;i++); // Wait 1 microsecond(or i hope so)
    outb(0x226, 0);
    uint8_t ret = inb(0x22A); // Should return 0xAA
    if (ret == 0xAA) {
        log.info("Detected card on io base 0x220\n");
        while (inb(DSP_WRITE));
        outb(DSP_WRITE, 0xE1);
        uint8_t major = inb(0x22A);
        uint8_t minor = inb(0x22A);
        log.info("Got version: %u.%u\n", major, minor);
        sb16_priv *prv = new sb16_priv;
        prv->io_base = 0x220;
        prv->major_ver = major;
        prv->minor_ver = minor;
        return (void *)prv;
    }
    return 0;
}

static module_t mod = {
    .name = "sb16",
    .priv = 0,
    .start = init_sb16,
    .stop = 0
};

__attribute__((constructor)) static void reg_mod() {
    register_module(&mod);
}