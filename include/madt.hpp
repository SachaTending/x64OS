#pragma once
#include <stdint.h>
#include <frg/vector.hpp>
#include <frg/std_compat.hpp>

struct madt_header {
    uint8_t id;
    uint8_t length;
} __attribute__((packed));

struct madt_lapic {
    struct madt_header;
    uint8_t processor_id;
    uint8_t apic_id;
    uint32_t flags;
} __attribute__((packed));

struct madt_io_apic {
    struct madt_header;
    uint8_t apic_id;
    uint8_t reserved;
    uint32_t address;
    uint32_t gsib;
} __attribute__((packed));

struct madt_iso {
    struct madt_header;
    uint8_t bus_source;
    uint8_t irq_source;
    uint32_t gsi;
    uint16_t flags;
} __attribute__((packed));

struct madt_nmi {
    struct madt_header;
    uint8_t processor;
    uint16_t flags;
    uint8_t lint;
} __attribute__((packed));

#define VECTOR(type) frg::vector<type, frg::stl_allocator>

typedef VECTOR(struct madt_lapic *) madt_lapic_vec;
typedef VECTOR(struct madt_io_apic *) madt_io_apic_vec;
typedef VECTOR(struct madt_iso *) madt_iso_vec;
typedef VECTOR(struct madt_nmi *) madt_nmi_vec;

#ifndef MADT
extern madt_lapic_vec madt_lapics;
extern madt_io_apic_vec madt_io_apics;
extern madt_iso_vec madt_isos;
extern madt_nmi_vec madt_nmis;
#endif