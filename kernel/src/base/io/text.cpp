#include <io/text.hpp>
#include <base/flanterm/flanterm.h>
#include <base/flanterm/fb/fb.h>
#include <limine.h>
#include <libc.h>

static early_putc_t p;
static struct flanterm_context *fl;
extern "C" int print_debug;
extern "C" void putc(char c) {
    if (p != nullptr) p(c);
    if (fl != nullptr && print_debug == 0) {
        flanterm_write(fl, &c, 1);
    }
}

extern "C" void putchar_(char c) {
    putc(c);
}

void set_early_putc(early_putc_t idk) {
    p = idk;
}

// Flanterm part
// I'm using my own version of flanterm, with ability to print UTF-8 chars, by using ssfn
void free2(void *p, size_t _unused) {
    (void)_unused;
    free(p);
}
void setup_flanterm(struct limine_framebuffer *fb) {
    fl = flanterm_fb_init(malloc,
        free2,
        (uint32_t *)fb->address, fb->width, fb->height, fb->pitch,
        fb->red_mask_size, fb->red_mask_shift,
        fb->green_mask_size, fb->green_mask_shift,
        fb->blue_mask_size, fb->blue_mask_shift,
        NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, 0, 0, 1,
        1, 1,
        0
    );
    fl->set_text_bg(fl, 0);
}