#include <fs/devtmpfs.hpp>
#include <libc.h>
#include <printf/printf.h>
#include <io.h>
#include <drivers/serial.hpp>
#include <logging.hpp>

static Logger log("/dev/console");

#define BIT(n) (1 << n)

static char kbdus[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', /* 9 */
    '9', '0', '-', '=', '\b',   /* Backspace */
    '\t',           /* Tab */
    'q', 'w', 'e', 'r', /* 19 */
    't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',       /* Enter key */
    0,          /* 29   - Control */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',   /* 39 */
    '\'', '`',   0,     /* Left shift */
    '\\', 'z', 'x', 'c', 'v', 'b', 'n',         /* 49 */
    'm', ',', '.', '/',   0,                    /* Right shift */
    '*',
    0,  /* Alt */
    ' ',    /* Space bar */
    0,  /* Caps lock */
    0,  /* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,  /* < ... F10 */
    0,  /* 69 - Num lock*/
    0,  /* Scroll Lock */
    0,  /* Home key */
    0,  /* Up Arrow */
    0,  /* Page Up */
    '-',
    0,  /* Left Arrow */
    0,
    0,  /* Right Arrow */
    '+',
    0,  /* 79 - End key*/
    0,  /* Down Arrow */
    0,  /* Page Down */
    0,  /* Insert Key */
    0,  /* Delete Key */
    0,   0,   0,
    0,  /* F11 Key */
    0,  /* F12 Key */
    0,  /* All other keys are undefined */
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
            if (!(scancode & 0x80) and !(scancode == 0)) return kbdus[scancode];
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
        while ((inb(0x3f8 + 5) & 1) == 0);
        *buf1 = inb(0x3f8);
        if (*buf1 == '\r') *buf1 = '\n';
        log.debug("serial recv: %c\n", *buf1);
        return 1;
    }
    uint8_t c = 0;
    for (size_t i=0;i<len;i++) {
        //c = get_input();
        buf1[i] = read_com1();
        log.debug("serial recv: %c, hex: 0x%02x, i: %lu\n", buf1[i], buf1[i], i);
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