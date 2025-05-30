#define SSFN_IMPLEMENTATION
#define SSFN_CONSOLEBITMAP_TRUECOLOR        /* use the special renderer for 32 bit truecolor packed pixels */
#include <libc.h>
#define SSFN_memcmp  memcmp
#define SSFN_memset  memset
#define SSFN_realloc realloc
#define SSFN_free    free
#include <ssfn.h>
#include <limine.h>
#include <flanterm.h>
#include <backends/fb.h>
ssfn_t *ctx;
ssfn_buf_t buf;
extern char _binary_freesans_sfn_start;
void ssfn_print(const wchar_t *txt) {
    int i=0;
    while (txt[i]) {
        ssfn_putc2(txt[i]);
        i++;
    }
}

void ssfn_set_fg(uint32_t fg) {
    ssfn_dst.fg = fg;
}

void ssfn_cprint(const char *txt) {
    while (*txt) {
        ssfn_putc2(*txt);
        txt++;
    }
}

void *fb2 = 0;
#define ARGB(a, r, g, b) (a << 24) | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF)
#define RGB(r, g, b) ARGB(255, r, g, b)
void movefb(void *dst, const void *src, size_t n) {
    uint8_t *p1 = (uint8_t *)dst;
    uint8_t *p2 = (uint8_t *)src;
    for (size_t i=0;i<n;i++) {
        uint8_t v1 = p1[i];
        uint8_t v2 = p2[i];
        if (v1 || v2 != v1) p1[i] = p2[i];
    }
}

void ssfn_ptc(uint32_t c) {
    ssfn_putc(c);
}

#define SSFN_RENDER(c) ssfn_ptc(c)

void fast_memcpy(void *dst, const void *src, size_t n) {
    uint64_t *p1 = (uint64_t *)dst;
    uint64_t *p2 = (uint64_t *)src;
    for (size_t i=0;i<n/8;i++) {
        if (p2[i] != 0) p1[i] = p2[i];
    }
}
size_t get_screen_size();
void flanterm_putchar(struct flanterm_context *ctx, uint8_t c);
struct flanterm_context *ft_ctx;
void ssfn_putc2(uint32_t c) {
    if (ft_ctx == NULL) return;
    char c2 = (char)c;
    flanterm_write(ft_ctx, &c2, 1);
    return;
    uint32_t oldx, oldy;
    uint8_t *oldfb;
    if (c == '\n') {
        ssfn_dst.y+=ssfn_src->width;
        ssfn_dst.x=0;
        goto end;
    }
    if (c == '\r') {
        memset(ssfn_dst.ptr+10+(ssfn_dst.p*ssfn_dst.y), 0, ssfn_dst.p);
        ssfn_dst.x = 0;
        goto end;
    }
    if (c == '\t') {
        for (int i=0;i<4;i++) {
            ssfn_putc2(' ');
        }
        goto end;
    }
    oldx = ssfn_dst.x;
    oldy = ssfn_dst.y;
    oldfb = ssfn_dst.ptr;
    SSFN_RENDER(c);
    if (fb2) {
        ssfn_dst.x = oldx;
        ssfn_dst.y = oldy;
        ssfn_dst.ptr = (uint8_t *)fb2;
        SSFN_RENDER(c);
        ssfn_dst.ptr = oldfb;
    }
end:
    if ((ssfn_dst.x + ssfn_src->height) > ssfn_dst.w) {
        ssfn_dst.x = 0;
        ssfn_dst.y += ssfn_src->width;
    }
    if ((ssfn_dst.y + ssfn_src->width) > ssfn_dst.h) {
        ssfn_dst.y-=ssfn_src->height;
        ssfn_dst.x = 0;
        if (fb2) {
            //__builtin_memcpy((void *)ssfn_dst.ptr, (void *)(uint64_t)(fb2+(ssfn_dst.p*ssfn_src->width)), ssfn_dst.p*(ssfn_dst.w-ssfn_src->width));
            __builtin_memcpy((void *)fb2, (void *)(uint64_t)(fb2+(ssfn_dst.p*ssfn_src->width)), get_screen_size());
            //__builtin_memmove((void *)fb2, (void *)(uint64_t)(fb2+(ssfn_dst.p*ssfn_src->width)), ssfn_dst.p*(ssfn_dst.w-ssfn_src->width));
            __builtin_memcpy(ssfn_dst.ptr, fb2, get_screen_size());
            //memset(ssfn_dst.ptr, 0, ssfn_dst.p*(ssfn_dst.w-ssfn_src->width));
            //fast_memcpy(ssfn_dst.ptr, (void *)fb2, ssfn_dst.p*(ssfn_dst.w-ssfn_src->width));
        }
        else {
            fast_memcpy((void *)ssfn_dst.ptr, (void *)(uint64_t)(ssfn_dst.ptr+(ssfn_dst.p*ssfn_src->width)), ssfn_dst.p*(ssfn_dst.w-ssfn_src->width));
        }
    }
}
struct limine_framebuffer *frb2;

size_t get_x() {
    return frb2->width/ssfn_src->width;
}

size_t get_y() {
    return frb2->height/ssfn_src->height;
}

size_t get_screen_size() {
    return ssfn_dst.p*(ssfn_dst.w-ssfn_src->width);
}

extern int print_debug;
void ssfn_setup(struct limine_framebuffer *frb) {
    ft_ctx = flanterm_fb_init(NULL,
        NULL,
        (uint32_t *)frb->address, frb->width, frb->height, frb->pitch,
        frb->red_mask_size, frb->red_mask_shift,
        frb->green_mask_size, frb->green_mask_shift,
        frb->blue_mask_size, frb->blue_mask_shift,
        NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, 0, 0, 1,
        1, 1,
        0
    );
    frb2 = frb;
    printf("\e[97m");
    //printf("ssfn_term: initialized, fb: %dx%dx%d fb_ptr: 0x%lx fb2_ptr: 0x%lx\n", frb->width, frb->height, frb->bpp, frb->address, fb2);
    //printf("ssfn_term: font size %dx%d\n", ssfn_src->width, ssfn_src->height);
    printf("Terminal initialized, screen: %dx%dx%d\n", frb->width, frb->height, frb->bpp);
}