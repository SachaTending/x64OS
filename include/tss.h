#pragma once
#include <stdint.h>

#define tss_field_lh(fname) fname##_low; \
    uint32_t fname##_high

typedef struct tss_entry {
    uint32_t reserved1;
    uint64_t rsp[3];
    uint32_t reserved2[2];
    uint64_t ist[7];
    uint32_t reserved3[2];
    uint16_t reserver4;
    uint16_t iopb;
} __attribute__((packed)) tss_entry_t;

void tss_set_stack(uint64_t stack, tss_entry_t *tss);
