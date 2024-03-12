#include <libc.h>
#include <logging.hpp>
#include <useful.hpp>

static Logger log("libcpp");

#define DEBUG_LIBCPP

#ifdef DEBUG_LIBCPP
#define debug(...) log.debug(__VA_ARGS__)
#else
#define debug(...)
#endif


void * operator new(size_t size) {
    debug("someone is trying to allocate data of size %u\n", size);
    void *p = malloc(size);
    memset(p, 0, size);
    return p;
}

void * operator new[](size_t size) {
    debug("someone is trying to allocate data of size %u\n", size);
    void *p = malloc(size);
    memset(p, 0, size);
    return p;
}


void operator delete(void *ptr) {
    debug("someone is trying to deallocate data at 0x%lx\n", ptr);
    free(ptr);
}

void operator delete(void *ptr, size_t s) {
    debug("someone is trying to deallocate data at 0x%lx size %u\n", ptr, s);
    free(ptr);
}

void operator delete[](void *ptr, size_t s) {
    debug("someone is trying to deallocate data at 0x%lx size %u\n", ptr, s);
    free(ptr);
}

void operator delete[](void *ptr) {
    debug("someone is trying to deallocate data at 0x%lx\n", ptr);
    free(ptr);
}