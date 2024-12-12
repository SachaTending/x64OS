#include <descriptor/gdt.hpp>

gdt_entry_t  __attribute__((aligned(0x10))) gdt_entries[11] = {
	{0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00},
	{0xFFFF, 0x0000, 0x00, 0x9A, (1 << 5) | (1 << 7) | 0x0F, 0x00},
	{0xFFFF, 0x0000, 0x00, 0x92, (1 << 5) | (1 << 7) | 0x0F, 0x00},
	{0xFFFF, 0x0000, 0x00, 0xFA, (1 << 5) | (1 << 7) | 0x0F, 0x00},
	{0xFFFF, 0x0000, 0x00, 0xF2, (1 << 5) | (1 << 7) | 0x0F, 0x00},
	{0xFFFF, 0x0000, 0x00, 0xFA, (1 << 5) | (1 << 7) | 0x0F, 0x00},
	{0x0067, 0x0000, 0x00, 0xE9, 0x00, 0x00},
};

void encodeGdtEntry(gdt_entry_t *target, uint32_t limit, uint64_t base, uint8_t access_byte, uint8_t flags)
{
    // Check the limit to make sure that it can be encoded
    if (limit > 0xFFFFF) {return;}
 
    // Encode the limit
    target->limit = limit;
 
    // Encode the base
    target->base_low = base & 0xFF;
    target->base_mid = (base >> 8) & 0xFF;
    target->base_hi = (base >> 16) & 0xFF;
 
    // Encode the access byte
    target->access = access_byte;
 
    // Encode the flags
    target->limit |= (flags << 4);
}

void setDesc(int index, uint32_t limit, uint64_t base, uint8_t access_byte, uint8_t flags) {
    encodeGdtEntry((gdt_entry_t *)&(gdt_entries[index]), limit, base, access_byte, flags);
}
extern "C" void load_gdt(gdt_ptr_t *gdt);
gdt_ptr_t gdtr;
void gdt_reload();
void GDT::Init() {
    gdtr.base = (uint64_t)&gdt_entries;
    gdtr.limit = sizeof(gdt_entries)-1;
    load_gdt(&gdtr);
}