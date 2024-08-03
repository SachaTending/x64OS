#include <incbin.h>
#include <stddef.h>
#include <limine.h>
#include <libc.h>
#include <descriptor/gdt.hpp>
#include <krnl.hpp>
#include <vmm.h>
#include <logging.hpp>

#ifndef LIMINE_BASE_REVISION
#define LIMINE_BASE_REVISION(N) \
    uint64_t limine_base_revision[3] = { 0xf9562b2d5c95a6c8, 0x6a7b384944536bdc, (N) };
#endif

LIMINE_BASE_REVISION(1)
 
struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0,
    .response = 0
};
 
// Halt and catch fire function.
static void hcf(void) {
    asm ("cli");
    for (;;) {
        asm ("hlt");
    }
}

limine_memmap_request m = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0,
    .response = 0
};

limine_hhdm_request hhdm2 = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0,
    .response = 0
};

limine_bootloader_info_request btldr_info = {
    .id = LIMINE_BOOTLOADER_INFO_REQUEST,
    .revision = 0,
    .response = 0
};

limine_efi_system_table_request efiTable = {
    .id = LIMINE_EFI_SYSTEM_TABLE_REQUEST,
    .revision = 0,
    .response = 0
};

limine_kernel_file_request limine_krnl_file = {
    .id = LIMINE_KERNEL_FILE_REQUEST,
    .revision = 0,
    .response = 0
};

limine_module_request modules = {
    .id = LIMINE_MODULE_REQUEST,
    .revision = 0,
    .response = 0
};

void pmm_init(void);
extern "C" void ssfn_setup(struct limine_framebuffer *frb);
extern "C" void ssfn_set_fg(uint32_t fg);
void idt_init();

#define ok() printf("\e[32mOK.\e[97m\n");

#define fail() printf("\e[31mFAIL.\e[97m\n");

typedef void (*constructor)();
extern constructor start_ctors;
extern constructor end_ctors;

void callConstructors(void)
{
    for(constructor* i = &start_ctors;i != &end_ctors; i++)
        (*i)();
}

limine_kernel_address_request kaddr = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST,
    .revision = 0,
    .response = 0
};


extern "C" uint64_t *get_next_level(uint64_t *top_level, size_t idx, bool allocate);
#define DIV_ROUNDUP(VALUE, DIV) ({ \
    typeof(VALUE) DIV_ROUNDUP_value = VALUE; \
    typeof(DIV) DIV_ROUNDUP_div = DIV; \
    (DIV_ROUNDUP_value + (DIV_ROUNDUP_div - 1)) / DIV_ROUNDUP_div; \
})

#define ALIGN_UP(VALUE, ALIGN) ({ \
    typeof(VALUE) ALIGN_UP_value = VALUE; \
    typeof(ALIGN) ALIGN_UP_align = ALIGN; \
    DIV_ROUNDUP(ALIGN_UP_value, ALIGN_UP_align) * ALIGN_UP_align; \
})

#define ALIGN_DOWN(VALUE, ALIGN) ({ \
    typeof(VALUE) ALIGN_DOWN_value = VALUE; \
    typeof(VALUE) ALIGN_DOWN_align = ALIGN; \
    (ALIGN_DOWN_value / ALIGN_DOWN_align) * ALIGN_DOWN_align; \
})

extern uint64_t kernel_start;
extern uint64_t kernel_end;
#define PTE_PRESENT (1ull << 0ull)
#define PTE_WRITABLE (1ull << 1ull)
extern "C" void vmm_switch_to(struct pagemap *pagemap);
extern "C" void *pmm_alloc(size_t pages);

//#define VMM_HIGHER_HALF 0xffff800000000000
#define VMM_HIGHER_HALF hhdm2.response->offset


extern "C" uint64_t true_hhdm = 0;
extern "C" void *fb2;
#define TRACE_CODE
#ifdef TRACE_CODE
#define trace(code) printf("%s:%d: %s\n", __FILE__, __LINE__, #code);code;
#else
#define trace(code) code;
#endif

void copyfb(void *to, void *from, size_t size) {
    #define FB_PTR_TYPE uint8_t
    FB_PTR_TYPE *fb1 = (FB_PTR_TYPE *)from;
    FB_PTR_TYPE *fb2 = (FB_PTR_TYPE *)to;
    for (size_t i=0;i<(size/sizeof(FB_PTR_TYPE));i+=sizeof(FB_PTR_TYPE)) {
        FB_PTR_TYPE data = fb1[i];
        if (data != 0) {
            fb2[i] = data;
        }
    }
}
void stackwalk_init();

extern "C" {
    extern const char *compdate;
    extern const char *compmachine;
}
void print_cpu();
extern "C" void _start() {
 
    // Ensure we got a framebuffer.
    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }
    //memset(framebuffer_request.response->framebuffers[0]->address, 0, framebuffer_request.response->framebuffers[0]->width*framebuffer_request.response->framebuffers[0]->pitch);
    pmm_init();
    fb2 = malloc(framebuffer_request.response->framebuffers[0]->width*framebuffer_request.response->framebuffers[0]->pitch);
    ssfn_setup(framebuffer_request.response->framebuffers[0]);
    printf("Allocated fb2 at 0x%lx, size: %u\n", fb2, framebuffer_request.response->framebuffers[0]->width*framebuffer_request.response->framebuffers[0]->pitch);
    printf("Framebuffer count: %lu\n", framebuffer_request.response->framebuffer_count);
    printf("Loading GDT...");
    GDT::Init();
    ok();
    printf("Loading IDT...");
    idt_init();
    ok();
    stackwalk_init();
    printf("x64OS kernel, compiled on %s, machine: %s\n", compdate, compmachine);
    printf("Booted by %s version %s\n", btldr_info.response->name, btldr_info.response->version);
    printf("Running on %s system\n", efiTable.response ? "UEFI":"BIOS");
    print_cpu();
    //printf("Copying framebuffer, this may take a while(depending on your screen size)...\n");
    //copyfb(fb2, framebuffer_request.response->framebuffers[0]->address, framebuffer_request.response->framebuffers[0]->width*framebuffer_request.response->framebuffers[0]->pitch);
    callConstructors();
    printf("All done, can call kernel's main.\n");
    Kernel::Main();
    hcf();
}

limine_entry_point_request entr = {
    .id = LIMINE_ENTRY_POINT_REQUEST,
    .revision = 0,
    .response = 0,
    .entry = _start
};