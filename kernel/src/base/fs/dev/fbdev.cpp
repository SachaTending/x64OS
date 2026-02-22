#include <stddef.h>
#include <stdint.h>
#include <limine.h>
#include <linux/fb.h>
#include <logging.hpp>
#include <libc.h>
#include <fs/resource.h>
#include <fs/devtmpfs.h>

struct framebuffer_device : public resource {
    struct limine_framebuffer *framebuffer;
    struct fb_var_screeninfo variable;
    struct fb_fix_screeninfo fixed;
};

static Logger log("fbdev");
extern volatile struct limine_framebuffer_request framebuffer_request;

static ssize_t fbdev_read(struct resource *this_, struct f_description *description, void *buf, off_t offset, size_t count) {
    (void)description;

    struct framebuffer_device *this2 = (struct framebuffer_device *)this_;

    if (count == 0) {
        return 0;
    }

    size_t actual_count = count;
    if (offset + count > this2->fixed.smem_len) {
        actual_count = count - ((offset + count) - this2->fixed.smem_len);
    }

    memcpy(buf, (void *)((uint64_t)this2->framebuffer->address + offset), actual_count);

    return actual_count;
}

static ssize_t fbdev_write(struct resource *this_, struct f_description *description, const void *buf, off_t offset, size_t count) {
    (void)description;

    struct framebuffer_device *this2 = (struct framebuffer_device *)this_;

    if (count == 0) {
        return 0;
    }

    size_t actual_count = count;
    if (offset + count > this2->fixed.smem_len) {
        actual_count = count - ((offset + count) - this2->fixed.smem_len);
    }

    memcpy((void *)((uint64_t)this2->framebuffer->address + offset), buf, actual_count);

    return actual_count;
}

static int fbdev_ioctl(struct resource *this_, struct f_description *description, uint64_t request, uint64_t arg) {
    struct framebuffer_device *this2 = (struct framebuffer_device *)this_;

    switch (request) {
        case FBIOGET_VSCREENINFO:
            *(struct fb_var_screeninfo *)arg = this2->variable;
            return 0;
        case FBIOGET_FSCREENINFO:
            *(struct fb_fix_screeninfo *)arg = this2->fixed;
            return 0;
        case FBIOPUT_VSCREENINFO:
            this2->variable = *(struct fb_var_screeninfo *)arg;
            return 0;
        case FBIOBLANK:
            return 0;
    }

    return resource_default_ioctl(this_, description, request, arg);
}

static void *fbdev_mmap(struct resource *this_, size_t file_page, int flags) {
    (void)flags;

    struct framebuffer_device *this2 = (struct framebuffer_device *)this_;

    size_t offset = file_page * PAGE_SIZE;

    if (offset >= this2->fixed.smem_len) {
        return NULL;
    }

    return ((void *)((uint64_t)this2->framebuffer->address + offset)) - VMM_HIGHER_HALF;
}

static bool fbdev_msync(struct resource *_this, size_t file_page, void *phys, int flags) {
    (void)_this;
    (void)file_page;
    (void)phys;
    (void)flags;
    return true;
}

void fbdev_init() {
    struct limine_framebuffer_response *framebuffer_response = framebuffer_request.response;
    if (framebuffer_response == NULL || framebuffer_response->framebuffer_count == 0) {
        log.error("No framebuffers available\n");
    }

    log.info("%d framebuffer(s) available\n", framebuffer_response->framebuffer_count);
    log.info("framebuffer response: 0x%lx\n", framebuffer_response);
    uint64_t i = 0;
    //for (uint64_t i = 0; i < (framebuffer_response->framebuffer_count); i++) { // For some reason this gives #GP misaligned access
    while (i < ((*framebuffer_response).framebuffer_count)) {
        struct limine_framebuffer *framebuffer = framebuffer_response->framebuffers[i];
        struct framebuffer_device *device = (framebuffer_device*)Resource::Create(sizeof(struct framebuffer_device));

        log.info("fbdev: Framebuffer #%d with mode %lux%lu (bpp=%lu, stride=%lu bytes)\n",
            i + 1, framebuffer->width, framebuffer->height, framebuffer->bpp, framebuffer->pitch);

        device->can_mmap = true;
        device->read = fbdev_read;
        device->write = fbdev_write;
        device->ioctl = fbdev_ioctl;
        device->mmap = fbdev_mmap;
        device->msync = fbdev_msync;
        device->framebuffer = framebuffer;

        device->stat.st_size = 0;
        device->stat.st_blocks = 0;
        device->stat.st_blksize = 4096;
        device->stat.st_rdev = resource_create_dev_id();
        device->stat.st_mode = 0666 | S_IFCHR;

        device->fixed.smem_len = framebuffer->pitch * framebuffer->height;
        device->fixed.mmio_len = framebuffer->pitch * framebuffer->height;
        device->fixed.line_length = framebuffer->pitch;
        device->fixed.type = FB_TYPE_PACKED_PIXELS;
        device->fixed.visual = FB_VISUAL_TRUECOLOR;

        device->variable.xres = framebuffer->width;
        device->variable.yres = framebuffer->height;
        device->variable.xres_virtual = framebuffer->width;
        device->variable.yres_virtual = framebuffer->height;
        device->variable.bits_per_pixel = framebuffer->bpp;
        device->variable.red = (struct fb_bitfield) {
            .offset = framebuffer->red_mask_shift,
            .length = framebuffer->red_mask_size
        };
        device->variable.green = (struct fb_bitfield) {
            .offset = framebuffer->green_mask_shift,
            .length = framebuffer->green_mask_size
        };
        device->variable.blue = (struct fb_bitfield) {
            .offset = framebuffer->blue_mask_shift,
            .length = framebuffer->blue_mask_size
        };
        device->variable.activate = FB_ACTIVATE_NOW;
        device->variable.vmode = FB_VMODE_NONINTERLACED;
        device->variable.width = -1;
        device->variable.height = -1;

        snprintf(device->fixed.id, sizeof(device->fixed.id), "limine-fb%lu", i);

        char device_name[32];
        snprintf(device_name, sizeof(device_name) - 1, "fb%lu", i);
        devtmpfs_add_device((struct resource *)device, device_name);
        i++;
    }
}