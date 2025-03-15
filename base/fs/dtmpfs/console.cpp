#include <fs/devtmpfs.hpp>
#include <libc.h>
#include <printf/printf.h>
#include <io.h>
#include <drivers/serial.hpp>
#include <logging.hpp>

static Logger log("/dev/console");

#define BIT(n) (1 << n)

static const char convtab_nomod[] = {
    '\0', '\e', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', '\0', 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', '\0', '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', '\0', '\0', '\0', ' '
};
bool check_val(const char *opt);
static int serial_received() {
   return inb(0x3f8 + 5) & 1;
}
uint8_t ps2_recv_dev();
static char get_input() { // Gets input from PS/2 or serial
    while (true) {
        if (true) {
            unsigned char scancode = ps2_recv_dev();
            if (scancode < 0x57) return convtab_nomod[scancode];
        }
    }
}

static int console_write(devtmpfs_node_t *node, void *buf, size_t len) {
    (void)node;
    for (size_t i=0;i<len;i++) {
        putchar_((((char *)buf)[i]));
    }
    return (int)len;
}
uint8_t read_com1();
static int console_read(devtmpfs_node_t *node, void *buf, size_t len) {
    (void)node;
    uint8_t *buf1 = (uint8_t *)buf;
    if (len == 1) {
        //while ((inb(0x3f8 + 5) & 1) == 0);
        //*buf1 = inb(0x3f8);
        *buf1 = get_input();
        if (*buf1 == '\r') *buf1 = '\n';
        log.debug("serial recv: %c\n", *buf1);
        printf("%c", *buf1);
        return 1;
    }
    uint8_t c = 0;
    for (size_t i=0;i<len;i++) {
        //c = get_input();
        //buf1[i] = read_com1();
        buf1[i] = get_input();
        log.debug("serial recv: %c, hex: 0x%02x, i: %lu\n", buf1[i], buf1[i], i);
        printf("%c", buf1[i]);
        if (buf1[i] == '\n' or buf1[i] == '\r') {
            return i;
        }
    }
}

static size_t console_seek(devtmpfs_node_t *node, size_t seek_pos, int seek_type) {
    (void)node;
    (void)seek_pos;
    (void)seek_type;
    return 0;
}

static devtmpfs_node_t console_node = {
    .read = console_read,
    .write = console_write,
    .seek = console_seek
};

void console_init() {
    devtmpfs_register_file(&console_node, "/console");
}