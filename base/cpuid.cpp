#include <libc.h>
#include <cpuid.h>

#define CPUID_p uint32_t *

void print_cpu() {
    char name[48];
    __get_cpuid(0x80000002, (CPUID_p)(name), (CPUID_p)(name+4), (CPUID_p)(name+8), (CPUID_p)(name+12));
    __get_cpuid(0x80000003, (CPUID_p)(name+16), (CPUID_p)(name+20), (CPUID_p)(name+24), (CPUID_p)(name+28));
    __get_cpuid(0x80000004, (CPUID_p)(name+32), (CPUID_p)(name+36), (CPUID_p)(name+40), (CPUID_p)(name+44));
    printf("CPU: %s\nCPUID leafs: 0x%x\n", name, __get_cpuid_max(0x80000000, 0));
}