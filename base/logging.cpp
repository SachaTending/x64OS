#include <libc.h>
#include <logging.hpp>
#include <printf/printf.h>
#include <sched.hpp>

void spinlock_printf();
void release_printf();

void putchar(char c);
extern "C" int print_debug = 0;
static inline uint64_t rdtsc()
{
    uint64_t ret;
    asm volatile ( "rdtsc" : "=A"(ret) );
    return ret;
}

Logger::Logger(const char *name) {
    this->name = strdup(name);
}
extern size_t global_ticks;
#define TICK global_ticks
void resume_sched();
extern uint64_t used_pages;
static size_t get_used_ram() {
    return used_ram;
}
void pre_log2() {
    stop_sched();
}

void post_log2() {
    resume_sched();
}
static inline unsigned long save_irqdisable(void)
{
    unsigned long flags;
    asm volatile ("pushf\n\tcli\n\tpop %0" : "=r"(flags) : : "memory");
    return flags;
}

static inline void irqrestore(unsigned long flags)
{
    asm ("push %0\n\tpopf" : : "rm"(flags) : "memory","cc");
}

#define pre_log()  pre_log2()

#define post_log()  post_log2()
void Logger::info(const char *msg, ...) {
    pre_log();
    printf("\e[97m[INFO][%lu][%u][%s]: ", get_used_ram(), TICK, this->name);
    va_list lst;
    va_start(lst, msg);
    vprintf_(msg, lst);
    va_end(lst);
    post_log();
}

void Logger::error(const char *msg, ...) {
    pre_log();
    printf("\e[31m[ERROR][%lu][%u][%s]: ", get_used_ram(), TICK, this->name);
    va_list lst;
    va_start(lst, msg);
    vprintf_(msg, lst);
    va_end(lst);
    printf("\e[97m"); // switch fg to white
    post_log();
}

void Logger::debug(const char *msg, ...) {
    pre_log();
    bool old_print_debug = print_debug;
    print_debug = 1;
    printf("\e[34m[DEBUG][%lu][%u][%s]: ", get_used_ram(), TICK, this->name);
    va_list lst;
    va_start(lst, msg);
    vprintf_(msg, lst);
    va_end(lst);
    printf("\e[97m"); // switch fg to white
    print_debug = old_print_debug;
    post_log();
}