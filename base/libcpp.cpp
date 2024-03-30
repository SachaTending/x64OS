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

extern "C" {

    void *__dso_handle = 0; //Attention! Optimally, you should remove the '= 0' part and define this in your asm script.
    
    int __cxa_atexit(void (*f)(void *), void *objptr, void *dso)
    {
        debug("__cxa_atexit called, args: deconstructor: 0x%lx, obj: 0x%lx, dso: 0x%lx\n", f, objptr, dso);
        return 0;
    }

}

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