#pragma once

#include "types.h"

union Descriptor {
    struct {
        WORD limit_lo;
        WORD base_lo;
        BYTE base_hi;
        BYTE type : 4;
        BYTE descriptor_type : 1;
        BYTE dpl : 2;
        BYTE segment_present : 1;
        BYTE limit_hi : 4;
        BYTE : 1;
        BYTE zero : 1;
        BYTE operation_size : 1;
        BYTE granularity : 1;
        BYTE base_hi2;
    };
    struct {
        DWORD low;
        DWORD high;
    };

    enum Type {
        Invalid = 0,
        AvailableTSS_16bit = 0x1,
        LDT = 0x2,
        BusyTSS_16bit = 0x3,
        CallGate_16bit = 0x4,
        TaskGate = 0x5,
        InterruptGate_16bit = 0x6,
        TrapGate_16bit = 0x7,
        AvailableTSS_32bit = 0x9,
        BusyTSS_32bit = 0xb,
        CallGate_32bit = 0xc,
        InterruptGate_32bit = 0xe,
        TrapGate_32bit = 0xf,
    };

    void setBase(void* b)
    {
        base_lo = (DWORD)(b) & 0xffff;
        base_hi = ((DWORD)(b) >> 16) & 0xff;
        base_hi2 = ((DWORD)(b) >> 24) & 0xff;
    }

    void setLimit(DWORD l)
    {
        limit_lo = (DWORD)l & 0xffff;
        limit_hi = ((DWORD)l >> 16) & 0xff;
    }
} PACKED;

void gdt_init();
void idt_init();
void registerInterruptHandler(BYTE number, void (*f)());
void registerUserCallableInterruptHandler(BYTE number, void (*f)());
void flushIDT();
void flushGDT();
void loadTaskRegister(WORD selector);
WORD allocateGDTEntry();
Descriptor& getGDTEntry(WORD selector);
void writeGDTEntry(WORD selector, Descriptor&);

#define HANG asm volatile( "cli; hlt" );
#define LSW(x) ((DWORD)(x) & 0xFFFF)
#define MSW(x) (((DWORD)(x) >> 16) & 0xFFFF)
#define LSB(x) ((x) & 0xFF)
#define MSB(x) (((x)>>8) & 0xFF)

#define disableInterrupts() asm volatile("cli");
#define enableInterrupts() asm volatile("sti");

/* Map IRQ0-15 @ ISR 0x50-0x5F */
#define IRQ_VECTOR_BASE 0x50