#include <io.h>
#include <libc.h>
#include <krnl.hpp>
#define BIT(n) (1 << n)

char kbdus[128] = {
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
int serial_received() {
   return inb(0x3f8 + 5) & 1;
}

static char get_input() { // Gets input from PS/2 or serial
    while (true) {
        unsigned char status_reg = inb(0x64);
        if (status_reg & BIT(0)) {
            unsigned char scancode = inb(0x60);
            if (!(scancode & 0x80)) return kbdus[scancode];
        }
        else if (serial_received()) {
            return inb(0x3f8);
        }
    }
}
static char buf[512];

static uint16_t buf_loc = 0;

struct cmd {
    const char *cmd;
    const char *desc;
    void (*cmd_main)(const char *cmd_txt);
};

static void cmd_test(const char *txt) {
    printf("CMD TEST, TXT: %s\n", txt);
}
uint32_t hex2int(const char *hex) {
    uint32_t val = 0;
    while (*hex) {
        // get current character then increment
        uint8_t byte = *hex++; 
        // transform hex character to the 4bit equivalent number, using the ascii table indexes
        if (byte >= '0' && byte <= '9') byte = byte - '0';
        else if (byte >= 'a' && byte <='f') byte = byte - 'a' + 10;
        else if (byte >= 'A' && byte <='F') byte = byte - 'A' + 10;    
        // shift 4 to make space for new digit, and add the 4 bits of the new digit 
        val = (val << 4) | (byte & 0xF);
    }
    return val;
}

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t edx = 0, eax = 0;
    asm volatile (
        "rdmsr\n\t"
        : "=a" (eax), "=d" (edx)
        : "c" (msr)
        : "memory"
    );
    return ((uint64_t)edx << 32) | eax;
}

static void cmd_rdmem64(const char *txt) {
    txt += 6+2;
    if (*(txt-1) == '\0') {
        printf("Usage: rdmem64 <addr>\n");
        return;
    }
    txt += 2;
    uint64_t addr = hex2int(txt);
    printf("0x%lx = 0x%lx\n", addr, *(uint64_t *)addr);
}

static void cmd_rdmsr(const char *txt) {
    txt += 6;
    if (*(txt-1) == '\0') {
        printf("Usage: rdmsr <msr>\n");
        return;
    }
    txt += 2;
    uint32_t msr = hex2int(txt);
    uint64_t v = rdmsr(msr);
    printf("MSR 0x%lx = 0x%lx %lu\n", msr, v, v);
}

const struct cmd cmds[] = {
    {"test", "Just a test command", cmd_test},
    {"rdmsr", "Reads MSR", cmd_rdmsr},
    {"rdmem64", "Read memory address(64 bit)", cmd_rdmem64},
    {NULL, NULL, NULL}
};

cmd *get_cmd(size_t i) {
    return (cmd *)&(cmds[i]);
}

void start_debug_console() {
    asm volatile ("cli"); // Disable ints, we don't need that
    if (!check_val("nodebug")) return;
    printf("Starting debug console...\n");
    outb(0x64, 0xA7); // Disable second ps/2 port
    printf("x64OS Debug console v0.1 activated, type help for commands.\n");
    printf("> ");
    while (true) {
        unsigned char c = get_input();
        if (c == '\r') c = '\n';
        printf("%c", c);
        if (c == '\n') {
            printf("\r");
            //printf("buf: %s\n", buf);
            bool cmd_found = false;
            if (!strcmp(buf, "help")) {
                printf("Avaible commands:\n");
                for (size_t i=0;cmds[i].cmd != NULL;i++) {
                    printf("%s %s\n", cmds[i].cmd, cmds[i].desc);
                }
            }
            else {
                for (size_t i=0;cmds[i].cmd != NULL;i++) {
                    //printf("cmd: %s ptr: 0x%lx\n", cmds[i].cmd, cmds[i].cmd_main);
                    if (!memcmp(buf, cmds[i].cmd, strlen(cmds[i].cmd))) {
                        cmds[i].cmd_main((const char *)buf);
                        cmd_found = true;
                    }
                }
                if (!cmd_found) {
                    printf("Unknown command, type help to see avaible commands\n");
                }
            }
            printf("> ");
            memset(buf, 0, 512);
            buf_loc = 0;
        }
        else {
            buf[buf_loc] = c;
            buf_loc += 1;
            if (buf_loc >= 511) {
                printf("\nCommand buffer overrun, resetting buffer...\n");
                buf_loc = 0;
                memset(buf, 0, 512);
            }
        }
    }
}