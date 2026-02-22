#include <io/text.hpp>
#define LIMINE_API_REVISION 3
#include <limine.h>
#include <stdint.h>
#include <stddef.h>
#include <arch/vmm.h>
#include <logging.hpp>
#include <pmm.h>
#include <libc.h>
#include <arch/arch.hpp>
#include <arch/io.h>
static Logger log("Arch stuff(x86-64)");

extern "C" pagemap *krnl_page = 0;
void fpu_init() {
    size_t t;

    asm("clts");
    asm("mov %%cr0, %0" : "=r"(t));
    t &= ~(1 << 2);
    t |= (1 << 1);
    asm("mov %0, %%cr0" :: "r"(t));
    asm("mov %%cr4, %0" : "=r"(t));
    t |= 3 << 9;
    //t |= 1 << 16;
    asm("mov %0, %%cr4" :: "r"(t));
    asm("fninit");
}
static void early_serial_putc(char c) {
    //while ((inb(0x3f8 + 5) & 0x20) == 0);
    outb(0xe9, c);
}

void Arch::Init() {
    fpu_init();
    set_early_putc(early_serial_putc);
}
extern limine_executable_address_request kernel_addr_request;
extern uint64_t kernel_start;
extern uint64_t kernel_end;

uint64_t smp_bsp_lapic = 0;

extern limine_mp_request smp_request;

void pmm_on_vmm_enabled();
void arch_gdt_init();
void arch_idt_init();
void arch_tss_setup();
void arch_setup_syscall();
void arch_setup_percore_struct();
void Arch::InitStage2() {
    // This stage is only called when PMM is initialized
    // TendingStream73: I feel like this should be done by main kernel, not by arch-depended code
    smp_bsp_lapic = smp_request.response->bsp_lapic_id;
    krnl_page = new pagemap;
    if (!krnl_page) {
        log.error("Failed to allocate pagemap for kernel.\n");
        while (1);
    }
    memset(krnl_page, 0, sizeof(pagemap));
    krnl_page->lock = SPINLOCK_INIT;
    krnl_page->top_level = (uint64_t *)pmm_alloc(1);
    log.debug("top level: 0x%lx\n", krnl_page->top_level);
    if (!krnl_page->top_level) {
        log.error("Failed to allocate top level pagetable.\n");
        while (1);
    }
    //uint64_t _a = (uint64_t)krnl_page->top_level;
    //_a += VMM_HIGHER_HALF;
    krnl_page->top_level = (uint64_t *)(((uint64_t)krnl_page->top_level)+VMM_HIGHER_HALF);
    uint64_t kstart = ALIGN_DOWN((uint64_t)&kernel_start, 4096),
        kend = ALIGN_UP((uint64_t)&kernel_end, 4096);
    log.info("Populating kernel's pml1...\n");
    for (int i=0;i<512;i++) {
        get_next_level(krnl_page->top_level, i, true);
    }
    for (uintptr_t addr=kstart;addr<kend;addr+=4096) {
        uint64_t phys = addr - kernel_addr_request.response->virtual_base + kernel_addr_request.response->physical_base;
        log.info("Mapping 0x%016lx\r", addr);
        vmm_map_page(krnl_page, addr, phys, PTE_WRITABLE | PTE_PRESENT);
    }
    putc('\n');
    int prog = 0;
    int oldpr = 0;
    //uint64_t addr_end = 0x100000000;
    uint64_t addr_end=0xffffff;
    /*
    log.info("Mapping memory...\n");
    for (uintptr_t addr = 0x0; addr < addr_end; addr += 4096) {
        oldpr = prog;
        prog = addr / (addr_end / 100);
        if (addr >= addr_end or addr >= addr_end-4096) prog = 100;
        //if (oldpr != prog) log.info("Progress: %d%%\r", prog);
        if (oldpr != prog) {
            log.info("Progress: ");
            printf("[");
            #define PROGRESSBAR_SIZE 50
            int prog_min = prog / (100 / PROGRESSBAR_SIZE);
            for (int i=0;i<PROGRESSBAR_SIZE-(PROGRESSBAR_SIZE-prog_min);i++) putc('=');
            for (int i=0;i<PROGRESSBAR_SIZE-prog_min;i++) putc(' ');
            printf("] %02d%%\r", prog);
        }
        //log.info("Progress: %d%% Address: 0x%016lx\r", prog, addr);
        vmm_map_page(krnl_page, addr, addr, PTE_PRESENT | PTE_WRITABLE);
        vmm_map_page(krnl_page, addr + VMM_HIGHER_HALF, addr, PTE_PRESENT | PTE_WRITABLE);
    }
    */ // PMM already maps all avaible addresses
    putc('\n');
    pmm_on_vmm_enabled();
    log.info("Switching pagetable...\n");
    vmm_switch_to(krnl_page);
    log.info("Loading GDT...\n");
    arch_gdt_init();
    arch_tss_setup();
    log.info("Loading IDT...\n");
    arch_idt_init();
    arch_setup_syscall();
    arch_setup_percore_struct();
#ifdef CONFIG_ARCH_TEST_INT_SUBSYS
    log.info("Triggerint interrupt 0x30...\n");
    asm volatile ("int $0x32");
#endif
}

static inline uint64_t wrmsr(uint32_t msr, uint64_t val) {
    uint32_t eax = (uint32_t)val;
    uint32_t edx = (uint32_t)(val >> 32);
    asm volatile (
        "wrmsr\n\t"
        :
        : "a" (eax), "d" (edx), "c" (msr)
        : "memory"
    );
    return ((uint64_t)edx << 32) | eax;
}


struct per_core_struct {
    void *syscall_stack;
    void *filler;
    void *user_stack;
} __attribute__((packed));
void arch_setup_percore_struct() {
    per_core_struct *st = new per_core_struct;
    st->syscall_stack = (void *)new char[128*1024];
    st->syscall_stack = (void *)(((uint64_t)st->syscall_stack)+128*1024);
    wrmsr(0xC0000102, (uint64_t)st);
    wrmsr(0xC0000084, 0x200);
}

void Arch::InitACPI() {
    Arch::x86::ACPI::MadtSetup();
    Arch::x86::ACPI::LAPICSetup();
}

void Arch::InitTImer() {
    Arch::x86::InitPIC();
}