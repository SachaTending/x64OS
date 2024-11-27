#include <libc.h>
#include <stdarg.h>
#include <printf/printf.h>
#include <spinlock.h>

extern "C" int print_debug;
#include <io.h>

int is_format_letter(char c) {
    return c == 'c' ||  c == 'd' || c == 'i' ||c == 'e' ||c == 'E' ||c == 'f' ||c == 'g' ||c == 'G' ||c == 'o' ||c == 's' || c == 'u' || c == 'x' || c == 'X' || c == 'p' || c == 'n';
}

void vsprintf_helper(char * str, void (*putchar)(char), const char * format, uint64_t * pos, va_list arg) {
    char c;
    int sign, ival, sys;
    char buf[512];
    char width_str[10];
    uint64_t uval = 0;
    uint64_t size = 8;
    uint64_t i = 0;
    uint64_t len = 0;
    int size_override = 0;
    memset(buf, 0, 512);
    memset(width_str, 0, 10);
    // Ensure that all vals are zero
    c = sign = ival = sys = uval = size = i = len = size_override = 0;

    while((c = *format++) != 0) {
        sign = 0;

        if(c == '%') {
            c = *format++;
            switch(c) {
                // Handle calls like printf("%08x", 0xaa);
                case '0':
                    size_override = 1;
                    // Get the number between 0 and (x/d/p...)
                    i = 0;
                    c = *format;
                    while(!is_format_letter(c)) {
                        width_str[i++] = c;
                        format++;
                        c = *format;
                    }
                    width_str[i] = 0;
                    format++;
                    // Convert to a number
                    size = atoi(width_str);
                    [[fallthrough]];
                case 'l':
                    format++;
                    [[fallthrough]];
                case 'd':
                case 'u':
                case 'x':
                case 'p':
                    if(c == 'd' || c == 'u')
                        sys = 10;
                    else
                        sys = 16;

                    uval = ival = va_arg(arg, uintptr_t);
                    if(c == 'd' && ival < 0) {
                        sign= 1;
                        uval = -ival;
                    }
                    itoa(buf, uval, sys);
                    len = strlen(buf);
                    // If use did not specify width, then just use len = width
                    if(!size_override) size = len;
                    if((c == 'x' || c == 'p' || c == 'd') &&len < size) {
                        for(i = 0; i < len; i++) {
                            buf[size - 1 - i] = buf[len - 1 - i];
                        }
                        for(i = 0; i < size - len; i++) {
                            buf[i] = '0';
                        }
                    }
                    if(c == 'd' && sign) {
                        if(str) {
                            *(str + *pos) = '-';
                            *pos = *pos + 1;
                        }
                        else
                            (*putchar)('-');
                    }
                    if(str) {
                        strcpy(str + *pos, buf);
                        *pos = *pos + strlen(buf);
                    }
                    else {
                        char * t = buf;
                        while(*t) {
                            putchar(*t);
                            t++;
                        }
                    }
                    break;
                case 'c':
                    if(str) {
                        *(str + *pos) = (char)va_arg(arg, int);
                        *pos = *pos + 1;
                    }
                    else {
                        (*putchar)((char)va_arg(arg, int));
                    }
                    break;
                case 's':
                    if(str) {
                        char * t = (char *) (uintptr_t)va_arg(arg, uintptr_t);
                        strcpy(str + (*pos), t);
                        *pos = *pos + strlen(t);
                    }
                    else {
                        char * t = (char *) va_arg(arg, uintptr_t);
                        while(*t) {
                            putchar(*t);
                            t++;
                        }
                    }
                    break;
                default:
                    break;
            }
            continue;
        }
        if(str) {
            *(str + *pos) = c;
            *pos = *pos + 1;
        }
        else {
            (*putchar)(c);
        }

    }
}

void vsprintf(char *buf, void(*putc)(char), const char *fmt, va_list lst) {
    uint64_t pos = 0;
    pos = 0; // ensure that it's zero
    vsprintf_helper(buf, putc, fmt, &pos, lst);
}

enum escType {
    NO_ESC,
    TERM_COLOR
};

bool escStart = false;
escType etype = NO_ESC;

bool etc_got_fg = false;

char etc_tmp[10];
int etc_tmp_pos = 0;

int etc_fg, etc_bg;
#define ARGB(a, r, g, b) (a << 24) | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF)
#define RGB(r, g, b) ARGB(255, r, g, b)
#define white RGB(242, 242, 242)
#define fill_37_90_gap white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white,white
int etc_lookup_fg[] = {
    RGB(12, 12, 12),
    RGB(197, 15, 31),
    RGB(19, 161, 14),
    RGB(193, 156, 0),
    RGB(0, 55, 218),
    RGB(136, 23, 152),
    RGB(58, 150, 221),
    RGB(204, 204, 204),
    fill_37_90_gap,
    RGB(118, 118, 118),
    RGB(231, 72, 86),
    RGB(22, 198, 12),
    RGB(249, 241, 165),
    RGB(59, 120, 255),
    RGB(180, 0, 158),
    RGB(97, 214, 214),
    white
};

#undef white
#undef fill_37_90_gap

void putchar(char c) {
    #ifdef E9_HACK
    outb(0xe9, c);
    #endif
    #ifndef E9_HACK
    while ((inb(0x3f8 + 5) & 0x20) == 0);
    outb(0x3f8, c);
    #endif
    if (c == '\e') {
        escStart = true;
    } else if (escStart && etype == NO_ESC) {
        switch (c)
        {
            case '[':
                etype = TERM_COLOR;
                break;
            
            default:
                break;
        }
    } else if (escStart) {
        switch (etype)
        {
            case TERM_COLOR:
                if (c == 'm') {
                    escStart = false;
                    etype = NO_ESC;
                    if (etc_got_fg) { // etc_fg already set
                        etc_bg = atoi((char *)&etc_tmp);
                    } else {
                        etc_fg = atoi((char *)&etc_tmp);
                    }
                    if (etc_bg) {
                        etc_bg = 0;
                    }
                    if (etc_fg) {
                        if (etc_fg <= 97 && etc_fg >= 30) {
                            ssfn_set_fg(etc_lookup_fg[etc_fg-30]);
                        }
                        etc_fg = 0;
                    }
                    etc_got_fg = false;

                    etc_tmp_pos = 0;
                    memset((void *)&etc_tmp, 0, sizeof(etc_tmp));
                } else if (c == ';') {
                    etc_got_fg = true;
                    etc_tmp_pos = 0;
                    memset((void *)&etc_tmp, 0, sizeof(etc_tmp));
                } else {
                    etc_tmp[etc_tmp_pos] = c;
                    etc_tmp_pos++;
                    if (etc_tmp_pos >= 10) {
                        etc_tmp_pos = 0;
                        memset((void *)&etc_tmp, 0, sizeof(etc_tmp));
                    }
                }
                break;
            
            default:
                break;
        }
    }
    else if (!print_debug) ssfn_putc2((uint32_t)c);
}
extern "C" void putchar_(char c) {
    putchar(c);
}

spinlock_t print_spinlock = SPINLOCK_INIT;

void spinlock_printf() {
    spinlock_acquire(&print_spinlock);
}

void release_printf() {
    spinlock_release(&print_spinlock);
}

void printf(const char *fmt, ...) {
    //va_list lst;
    //va_start(lst, fmt);
    //vsprintf(NULL, putchar, fmt, lst);
    //va_end(lst);
    //spinlock_acquire(&print_spinlock);
    printf_(fmt);
    //spinlock_release(&print_spinlock);
}