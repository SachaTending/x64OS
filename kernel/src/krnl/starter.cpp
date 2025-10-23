#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#define LIMINE_API_REVISION 3
#include <limine.h>
#include <arch/arch.hpp>
#include <io/text.hpp>
#include <libc.h>
#include <logging.hpp>

static Logger log("Kernel starter");

// Set the base revision to 3, this is recommended as this is the latest
// base revision described by the Limine boot protocol specification.
// See specification for further info.
extern "C" size_t global_ticks = 0;
__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3);

// The Limine requests can be placed anywhere, but it is important that
// the compiler does not optimise them away, so, usually, they should
// be made volatile or equivalent, _and_ they should be accessed at least
// once or marked as used with the "used" attribute as done here.

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
volatile struct limine_executable_address_request kernel_addr_request = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
volatile struct limine_mp_request smp_request = {
    .id = LIMINE_MP_REQUEST,
    .revision = 0
};

// Finally, define the start and end markers for the Limine requests.
// These can also be moved anywhere, to any .c file, as seen fit.

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

// GCC and Clang reserve the right to generate calls to the following
// 4 functions even if they are not directly called.
// Implement them as the C specification mandates.
// DO NOT remove or rename these functions, or stuff will eventually break!
// They CAN be moved to a different .c file.
#define restrict

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;

    if (src > dest) {
        for (size_t i = 0; i < n; i++) {
            pdest[i] = psrc[i];
        }
    } else if (src < dest) {
        for (size_t i = n; i > 0; i--) {
            pdest[i-1] = psrc[i-1];
        }
    }

    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }

    return 0;
}

// Halt and catch fire function.
static void hcf(void) {
    for (;;) {
#if defined (__x86_64__)
        asm ("hlt");
#elif defined (__aarch64__) || defined (__riscv)
        asm ("wfi");
#elif defined (__loongarch64)
        asm ("idle 0");
#endif
    }
}

// The following will be our kernel's entry point.
// If renaming kmain() to something else, make sure to change the
// linker script accordingly.

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile ( "outb %b0, %w1" : : "a"(val), "Nd"(port) : "memory");
    /* There's an outb %al, $imm8 encoding, for compile-time constant port numbers that fit in 8b. (N constraint).
     * Wider immediate constants would be truncated at assemble-time (e.g. "i" constraint).
     * The  outb  %al, %dx  encoding is the only option for all other cases.
     * %1 expands to %dx because  port  is a uint16_t.  %w1 could be used if we had the port number a wider C type */
}
void putchar(char c) {
    outb(0x3f8, c);
    return;
    //*uart = c;
}

void print(const char *s) {
    while(*s != '\0') {
        putc(*s);
        s++;
    }
}
typedef void (*constructor)();
extern constructor start_ctors;
extern constructor end_ctors;

void callConstructors(void)
{
    for(constructor* i = &start_ctors;i != &end_ctors; i++)
        (*i)();
}
void pmm_init(void);
void pre_sched_uacpi_init();
void init_uacpi();
bool try_to_init_hpet();
extern size_t regsitered_loggers;
void kmain(void) {
    Arch::Init();
    pmm_init();
    //print("arch stuff has been initialized, btw this is a early kernel print\n");
    //print("x64OS v2 IS REAL!\n");
    printf("printf testing, %d\n", 123);
    // Ensure the bootloader actually understands our base revision (see spec).
    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        print("limine doesn't support this base revision, bruh\n");
        hcf();
    }

    // Ensure we got a framebuffer.
    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        print("no framebuffers, bruh.\n");
        hcf();
    }

    // Fetch the first framebuffer.
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];
    setup_flanterm(framebuffer);
    callConstructors();
    Arch::InitStage2();
    pre_sched_uacpi_init();
    Arch::InitACPI();
    // Initialize timers
    bool r = try_to_init_hpet(); // This is actually stupid, only x86 has HPET
    if (r == false) {
        // Well, no HPET, fallback to fastest platform timer
        /* Why we need fastest timer?
         * Well, for scheduling
         * We need to perform context switch so fast, that it will look like all threads are executing simualtenisly(sorry for bad english)
        */
       log.info("Falling back to arch-specific timers\n");
       Arch::InitTImer();
       // TODO
    }
    // TODO: Scheduler
    //init_uacpi();

    // We're done, just hang...
    //print("done\n");
    log.info("\e[97mHello, world!\n");
    log.info("Привет мир!\n");
    log.info("Это очень минимальный прототип ядра x64OS v2, здесь нет:\n");
    log.info("  - PS/2 драйвера, да и вообще подсистемы драйверов\n");
    log.info("  - Мультизадачность\n");
    log.info("  - VFS\n");
    log.info("  - Обработка прерыванией(пока что только получение прерываний)\n");
    log.info("fun fact: обычный flanterm не может отображать UTF-8 текст, я модифицировал его и добавил туда ssfn, для отображения такого текста\n");
    log.info("Текущие параметры экрана: %dx%dx%d\n", framebuffer->width, framebuffer->height, framebuffer->bpp);
    log.info("fun fact 2: В ядре иниицализировано %lu логгеров\n", regsitered_loggers);
    hcf();
}


extern "C" void starter_main() {
    //print("lol\n");
    kmain();
}