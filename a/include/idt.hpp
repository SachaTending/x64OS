#pragma once
#include <stdint.h>

struct idt_regs
{
	uint64_t rbp;
	uint64_t sfra; // stack frame return address
	
	// Registers pushed by KiTrapCommon. Pushed in reverse order from how they're laid out.
	uint16_t ds, es, fs, gs;
	uint64_t cr2;
	uint64_t rdi, rsi;
	uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
	uint64_t rdx;
	
	uint64_t rcx, rbx, rax;
	
	
	
	// Registers pushed by each trap handler
	uint64_t IntNumber; // the interrupt vector
	uint64_t ErrorCode; // only sometimes actually pushed by the CPU, if not, it's a zero
	
	// Registers pushed by the CPU when handling the interrupt.
	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;
};

typedef void (*idt_handl)(idt_regs *regs, void *priv);

void idt_set_int(uint64_t vec, idt_handl handl, void *priv=nullptr);

#define MAP_BASE 32