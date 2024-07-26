#pragma once
#include <stdint.h>

#define tss_field_lh(fname) fname#_low; \
    uint32_t fname#_high

typedef struct tss_entry {
    uint32_t reserved1;
    uint32_t tss_field_lh(rsp0);
    uint32_t tss_field_lh(rsp1);
    uint32_t tss_field_lh(rsp2);
    uint32_t reserved2[2];
    uint32_t tss_field_lh(ist1);
    uint32_t tss_field_lh(ist2);
    uint32_t tss_field_lh(ist3);
    uint32_t tss_field_lh(ist4);
    uint32_t tss_field_lh(ist5);
    uint32_t tss_field_lh(ist6);
    uint32_t tss_field_lh(ist7);
    uint32_t reserved3[2];
    uint16_t reserver4;
    uint16_t iopb;
} __attribute__((packed)) tss_entry_t;