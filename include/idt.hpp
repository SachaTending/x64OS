#pragma once
#include <stdint.h>

struct idt_regs
{
	uint64_t rbp;
	uint64_t sfra; // stack frame return address
	
	// Registers pushed by int_common. Pushed in reverse order from how they're laid out.
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


struct cpu_ctx {
    uint64_t ds;
    uint64_t es;
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
	
	// Registers pushed by each trap handler
	uint64_t IntNumber; // the interrupt vector
	uint64_t ErrorCode; // only sometimes actually pushed by the CPU, if not, it's a zero
	
	// Registers pushed by the CPU when handling the interrupt.
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed));

typedef void (*idt_handl)(cpu_ctx *regs);
typedef bool (*idt_exc_handl)(cpu_ctx *regs);

void idt_set_int(uint64_t vec, idt_handl handl);
void idt_register_exception_handler(int vector, idt_exc_handl handl);

#define MAP_BASE 32
