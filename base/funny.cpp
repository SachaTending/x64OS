#include <limine.h>
#include <libc.h>

extern limine_framebuffer_request framebuffer_request;

extern uint8_t kernel_start, kernel_end;

void funny() {
    size_t size = ((size_t)&kernel_end - (size_t)&kernel_start);
    limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    #define MAX_SIZE fb->width*fb->pitch
    if (size > MAX_SIZE) {
        size = (size_t) MAX_SIZE;
    }
    memcpy((void *)fb->address, (const void *)&kernel_start, size);
}

const char *funny2 = "Its fun to see kernel's data on framebuffer";