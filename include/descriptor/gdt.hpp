#pragma once
#include <stdint.h>

typedef struct gdt_entry
{
    uint16_t limit;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_hi;
} __attribute__((packed)) gdt_entry_t;

typedef struct gdt_ptr
{
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdt_ptr_t;

namespace GDT
{
    void Init();    
} // namespace GDT
