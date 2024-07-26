#include <libc.h>
#include <logging.hpp>
#include <printf/printf.h>

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
    this->name = name; // TODO: strdup(name) instead of this
}
extern size_t global_ticks;
#define TICK global_ticks

void Logger::info(const char *msg, ...) {
    printf("\e[97m[INFO][%lu][%u][%s]: ", used_ram, TICK, this->name);
    va_list lst;
    va_start(lst, msg);
    vprintf_(msg, lst);
    va_end(lst);
}

void Logger::error(const char *msg, ...) {
    printf("\e[31m[ERROR][%lu][%u][%s]: ", used_ram, TICK, this->name);
    va_list lst;
    va_start(lst, msg);
    vprintf_(msg, lst);
    va_end(lst);
    printf("\e[97m"); // switch fg to white
}

void Logger::debug(const char *msg, ...) {
    print_debug = 1;
    printf("\e[34m[DEBUG][%lu][%u][%s]: ", used_ram, TICK, this->name);
    va_list lst;
    va_start(lst, msg);
    vprintf_(msg, lst);
    va_end(lst);
    printf("\e[97m"); // switch fg to white
    print_debug = 0;
}