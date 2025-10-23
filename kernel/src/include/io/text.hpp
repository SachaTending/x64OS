#pragma once
#include <limine.h>

typedef void (*early_putc_t)(char);

void set_early_putc(early_putc_t p);
extern "C" void putc(char c);
void setup_flanterm(struct limine_framebuffer *fb);