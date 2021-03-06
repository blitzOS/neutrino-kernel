#pragma once
#include <stdint.h>
#include <neutrino/macros.h>

#define INTERRUPT_GATE  0x8e
#define TRAP_GATE       0x8f

#define KERNEL_CODE     0x18
#define IDT_SIZE        256

#define APIC_TIMER_IRQ  32

struct IDT_entry {
    uint16_t offset_lowerbits;
    uint16_t selector;
    uint8_t ist : 3;
    uint8_t zero : 5;
    uint8_t type_attr;
    uint16_t offset_middlebits;
    uint32_t offset_higherbits;
    uint32_t zero2;
};

struct IDT_pointer {
    uint16_t size;   
    uint64_t offset; 
} packed;

struct __interrupt_stack {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;

    uint64_t irq;
    uint64_t error_code;

    // Interrupt stack frame
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} packed;

typedef struct __interrupt_stack InterruptStack;
typedef struct __interrupt_stack Registers;

void init_idt();
InterruptStack* exception_handler(InterruptStack* stack);
InterruptStack* interrupt_handler(InterruptStack* stack);
void pagefault_handler(InterruptStack* stack);

static inline void disable_interrupts() {
    asm volatile ("cli");
}

static inline void enable_interrupts() {
    asm volatile ("sti");
}

extern int load_idt(uintptr_t);
extern void* _interrupt_vector[128];
