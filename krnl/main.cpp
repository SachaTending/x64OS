#include <libc.h>
#include <krnl.hpp>
#include <vmm.h>
#include <limine.h>
#include <logging.hpp>
#include <cpu.h>
#include <ioapic.hpp>
#include <sched.hpp>
#include <prg_loader.hpp>

static Logger log("Kernel");

extern limine_memmap_request m;
extern limine_kernel_address_request kaddr;
extern limine_hhdm_request hhdm2;

static bool krnl_called = false; // prevent another kernel::main call, why not?
#define TRACE_CODE2
#ifdef TRACE_CODE
#define trace(code) printf("%s:%d: %s\n", __FILE__, __LINE__, #code);code;
#else
#define trace(code) code;
#endif

const char *memmap_type_str(int t) {
    switch (t)
    {
        case LIMINE_MEMMAP_USABLE:
            return "usable";
        case LIMINE_MEMMAP_RESERVED:
            return "reserved";
        case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
            return "acpi reclaimable";
        case LIMINE_MEMMAP_ACPI_NVS:
            return "acpi nvs";
        case LIMINE_MEMMAP_BAD_MEMORY:
            return "bad memory";
        case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
            return "bootloader reclaimable";
        case LIMINE_MEMMAP_KERNEL_AND_MODULES:
            return "kernel and modules";
        case LIMINE_MEMMAP_FRAMEBUFFER:
            return "framebuffer";
        default:
            return "unknown";
    }
}
extern "C" uint64_t *get_next_level(uint64_t *top_level, size_t idx, bool allocate);

extern uint64_t kernel_start;
extern uint64_t kernel_end;

//#define VMM_HIGHER_HALF 0xffff800000000000
#define VMM_HIGHER_HALF hhdm2.response->offset

extern "C" uint64_t true_hhdm;

uint64_t max_addr = 0;

void print_memmap() {
    log.debug("memmap entries:\n");
    for (uint64_t i=0;i<m.response->entry_count;i++) {
        log.debug("base=0x%016lx  length=%08u  type=%s(%u)\n", 
            m.response->entries[i]->base, 
            m.response->entries[i]->length,
            memmap_type_str(m.response->entries[i]->type),
            m.response->entries[i]->type
        );
        if (m.response->entries[i]->base > max_addr) max_addr = m.response->entries[i]->base;
    }
}

pagemap *krnl_page;

void map_vmm(uint64_t addr, size_t count) {
    if (!krnl_page) return;
    uint64_t start = ALIGN_DOWN(addr, 4096);
    uint64_t end = ALIGN_UP(addr+count, 4096);
    for (uint64_t a=start; a<end; a+=4096) {
        vmm_map_page(krnl_page, a, a, PTE_WRITABLE | PTE_PRESENT);
        vmm_map_page(krnl_page, a, a+VMM_HIGHER_HALF, PTE_WRITABLE | PTE_PRESENT);
    }
}

void vmm_setup() {
#if 1
    trace(pagemap *pg = (pagemap *)malloc(sizeof(pagemap)));
    trace(memset(pg, 0, sizeof(pagemap)););
    trace(pg->top_level = (uint64_t *)pmm_alloc(1));
    pg->lock = SPINLOCK_INIT;
    if (!pg->top_level) {log.error("PML1 Not allcated, no memory avaible? Stopping.\n");asm volatile("cli;1:\nhlt;jmp 1");}
    //trace(pg->top_level += VMM_HIGHER_HALF);
    if ((uint64_t)pg->top_level < VMM_HIGHER_HALF) {
        uint64_t t = (uint64_t)pg->top_level;
        t += VMM_HIGHER_HALF;
        pg->top_level = (uint64_t *)t;
        // quick fix for int_13 exception.
    }
    print_memmap();
    uint64_t kstart = ALIGN_DOWN((uint64_t)&kernel_start, 4096),
        kend = ALIGN_UP((uint64_t)&kernel_end, 4096);
    log.info("Populating kernel's pml1...\n");
    for (int i=0;i<512;i++) {
        get_next_level(pg->top_level, i, true);
    }

    for (uintptr_t addr=kstart;addr<kend;addr+=4096) {
        uint64_t phys = addr - kaddr.response->virtual_base + kaddr.response->physical_base;
        log.info("Mapping 0x%016lx\r", addr);
        vmm_map_page(pg, addr, phys, PTE_WRITABLE | PTE_PRESENT);
    }
    printf("\n");
    int prog = 0;
    int oldpr = 0;
    //uint64_t addr_end = 0x100000000;
    uint64_t addr_end=0xfffffffb;
    log.info("Mapping memory...\n");
    for (uintptr_t addr = 0x0; addr < addr_end; addr += 4096) {
        oldpr = prog;
        prog = addr / (addr_end / 100);
        if (addr >= addr_end or addr >= addr_end-4096) prog = 100;
        if (oldpr != prog) log.info("Progress: %d%%\r", prog);
        //log.info("Progress: %d%% Address: 0x%016lx\r", prog, addr);
        vmm_map_page(pg, addr, addr, PTE_PRESENT | PTE_WRITABLE);
        vmm_map_page(pg, addr + VMM_HIGHER_HALF, addr, PTE_PRESENT | PTE_WRITABLE);
    }
    printf("\n");
    log.info("Switching pages...\n");
    vmm_switch_to(pg);
    krnl_page = pg;
#endif
}

void acpi_init();
void smp_init();
void funny();
void lapic_init(void);
void init_pit();
void madt_init();
void parse_opts();

extern uint64_t bsp_lapic_id;

void fpu_init() {
    size_t t;

    asm("clts");
    asm("mov %%cr0, %0" : "=r"(t));
    t &= ~(1 << 2);
    t |= (1 << 1);
    asm("mov %0, %%cr0" :: "r"(t));
    asm("mov %%cr4, %0" : "=r"(t));
    t |= 3 << 9;
    asm("mov %0, %%cr4" :: "r"(t));
    asm("fninit");
}
extern "C" {
    void sse_enable(void);
    void syscall_entry(void);
    void syscall_c_entry(void) {
        log.info("sysenter called lol\n");
    }
}
size_t global_ticks;


void lapic_timer_oneshot(uint64_t us, uint8_t vector);
void init_pic();

#include <vfs.hpp>

int exec(const char *path);

vfs_fs_t *tmpfs_create_fs();
void unpack_initrd();
int krnl_task() {
    log.info("multitaskin'\n");
    task_t *t = root_task;
    do {
        log.info("Task: %s PID: %u\n", t->name, t->pid);
        t = t->next;
    } while (t != root_task);
    log.info("Mounting tmpfs...\n");
    vfs_fs_t *fs = tmpfs_create_fs();
    int ret = vfs_mount("none", "/", fs);
    if (ret != 0) {
        PANIC("Failed to mount tmpfs, ret=%d\n", ret);
    }
    log.info("Unpacking ramdisk...\n");
    unpack_initrd();
    vfs_node_t *n = vfs_get_node("/hello_world");
    if (n == NULL) {
        log.info("cannot get file, bruh.\n");
    } else {
        char *buf = new char[1500];
        n->read(n, buf, 150);
        log.info("File content: %s\n", buf);
        delete[] buf;
        n->close(n);
    }
    /*
    log.info("Experiment number 2: ELF Loading.\n");
    pagemap *pgm = new pagemap;
    pgm->top_level = (uint64_t *)pmm_alloc(1);
    pgm->lock = SPINLOCK_INIT;
    log.info("pgm->top_level = 0x%lx\n", pgm->top_level);
    if (pgm->top_level < VMM_HIGHER_HALF) {
        uint64_t t = pgm->top_level;
        t += VMM_HIGHER_HALF;
        pgm->top_level = t;
    }
    for (int i=0;i<512;i++) {
        //log.info("get_next_level(0x%lx, %i, true);\n", pgm->top_level, i);
        get_next_level(pgm->top_level, i, true);
    }
    LOADER_ERROR ret2 = load_program("/init", pgm, NULL, NULL);
    log.info("Return: ");
    switch (ret2)
    {
        case LOADER_OK:
            printf("LOADER_OK\n");
            break;
        
        default:
            printf("Unknown, %i\n", ret2);
            break;
    }
    */
    log.info("Starting /init...\n");
    exec("/init");
    for(;;) asm volatile ("hlt");
}

int krnl2_task() {
    //for(;;);
    return 255;
}

extern "C" int test_user_function() {
    log.info("User mode, lol\n");
    asm volatile ("iretq");
    for(;;);
}

extern "C" int jump_to_usermode();

void init_tss();
void init_elf();
void Kernel::Main() {
    if (krnl_called) return;
    krnl_called = true;
    log.info("Trying to setup fpu...\n");
    fpu_init();
    log.info("Trying to setup SSE...\n");
    sse_enable();
    vmm_setup();
    acpi_init();
    parse_opts();
    madt_init();
    smp_init();
    //log.info("SMP Not working.\n");
    init_pit();
    init_pic();
    init_elf();
    //asm volatile ("int $32");
    log.info("Initializing TSS...\n");
    init_tss();
    log.info("%lx\n", *((uint32_t *)test_user_function));
    //for(;;);
    log.info("Starting scheduler...\n");
    sched_init();
    create_task(krnl_task, "task2");
    create_task(krnl2_task, "task3(should exit)");
    //create_task(test_user_function, "usermode", true);
    start_sched();
    for(;;);
}