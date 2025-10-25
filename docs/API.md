# PMM
```c
void *pmm_alloc(size_t pages);
```

Allocate some physical memory, 1 page = PAGE_SIZE

# Logging

```c++
Logger::Logger(const char *name);
Logger::~Logger();
```

APIs for initizalizing Logger class, example of usage: `Logger *log = new Logger("idk");`

```c++
void Logger::info(const char *msg, ...);
void Logger::error(const char *msg, ...);
void Logger::debug(const char *msg, ...);
void Logger::warn(const char *msg, ...);
```

Literarly logs a text. These functions use printf internally. Example usage:
```c++
log->info("Hello, world!\n");
log->error("Something went wrong\n");
log->debug("This text (on x86_64) is sent only to COM1 serial port\n");
log->warn("Just a warning.\n");
```

# Kernel

```c++
void Kernel::DispatchInterrupt(cpu_ctx *regs, uint64_t vector);
```

(SHOULD BE ONLY CALLED BY ARCH-SPECIFIC's INTERRUPT HANDLER) Handles interrupt.


# Arch-specific code

```c++
void Arch::Init();
void Arch::InitStage2();
void Arch::InitACPI(); // SHOULD BE CALLED AFTER UACPI INIT
void Arch::InitTImer();
```
(SHOULD BE CALLED ONLY BY KERNEL, OTHERWISE YOU CAN BREAK ENTIRE SYSTEM) Different stages of init
1. Init(); - Called BEFORE any kernel init, this function's task is to setup basic enviroment(for example: set early output device for outputting logs)
2. InitStage2(); - Called AFTER pmm init, this function should init vmm and arch-specific code
3. InitACPI(); - Called AFTER initialization of uACPI
4. InitTimer(); - Should initialize timer for context switching

```c++
bool vmm_map_page(struct pagemap *pagemap, uintptr_t virt, uintptr_t phys, uint64_t flags);
```
Maps physical address to virtual. List of flags:
1. PTE_PRESENT - Indicates that page is present
2. PTE_WRITABLE - Indicates that virtual address is writable
3. PTE_USER - Indicates that virtual address can be accessed by user
4. PTE_NOCACHE - Indicates that virtual addrees SHOULD NOT be cached

```c++
void vmm_map_range(pagemap *pgm, uint64_t start, size_t count, uint64_t flags);
```
idk how to describe this, sorry

```c++
uintptr_t vmm_virt2phys(struct pagemap *pagemap, uintptr_t virt);
```
Converts virtual address to physical

```c++
void vmm_switch_to(struct pagemap *pagemap);
```
Switches to other pagemap. Use it very carefully

# More info about VMM
Kernel's pagemap is called krnl_page, use it to map something to kernel's address space