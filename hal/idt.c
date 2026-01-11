/* hal/idt.c */
#include "../include/kernel.h"

typedef struct {
    uint16_t low; uint16_t sel; uint8_t ist; uint8_t attr;
    uint16_t mid; uint32_t high; uint32_t zero;
} __attribute__((packed)) idt_entry_t;

typedef struct { uint16_t limit; uint64_t base; } __attribute__((packed)) idt_ptr_t;

static idt_entry_t idt[256];
static idt_ptr_t idt_ptr;

extern void load_idt(void* ptr);
extern void isr_stub_keyboard(void);
extern void isr_stub_mouse(void);

extern void isr_stub_double_fault(void);

void handle_double_fault(void) {
    /* Set recovery flag in CMOS for bootloader detection */
    write_cmos(0x34, 0xEE);

    /* Don't attempt hardware reset during exception - just halt */
    /* Bootloader will detect CMOS flag and load recovery kernel */
    for(;;) {
        __asm__ volatile("hlt");
    }
}

/* KEYBOARD: Handle via Interrupt (Good!) */
void handle_keyboard_interrupt(void) {
    uint8_t scancode = inb(0x60);
    keyboard_handler_main(scancode);
    outb(0x20, 0x20); // Master EOI
}

/* MOUSE: We will DISABLE this interrupt so Polling works */
void handle_mouse_interrupt(void) {
    outb(0xA0, 0x20); // Slave EOI
    outb(0x20, 0x20); // Master EOI
}

void set_idt_gate(int n, uint64_t handler) {
    set_idt_gate_ist(n, handler, 0);
}

void set_idt_gate_ist(int n, uint64_t handler, uint8_t ist) {
    idt[n].low = handler & 0xFFFF;

    // MUST BE 0x08 to match the assembly GDT above
    idt[n].sel = 0x08;

    idt[n].ist = ist;  // IST index (0 = no IST, 1-7 = IST stack)
    idt[n].attr = 0x8E;
    idt[n].mid = (handler >> 16) & 0xFFFF;
    idt[n].high = (handler >> 32) & 0xFFFFFFFF;
    idt[n].zero = 0;
}

void init_idt(void) {
    /* 1. Remap PIC */
    outb(0x20, 0x11); io_wait(); outb(0xA0, 0x11); io_wait();
    outb(0x21, 0x20); io_wait(); outb(0xA1, 0x28); io_wait();
    outb(0x21, 0x04); io_wait(); outb(0xA1, 0x02); io_wait();
    outb(0x21, 0x01); io_wait(); outb(0xA1, 0x01); io_wait();

    /* 2. Mask All Initially */
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);

    /* 3. Set Gates - Validate addresses before setting */
    if (isr_stub_double_fault && isr_stub_keyboard && isr_stub_mouse) {
        set_idt_gate_ist(8, (uint64_t)isr_stub_double_fault, 1);  // Double fault uses IST1
        set_idt_gate(0x21, (uint64_t)isr_stub_keyboard);
        set_idt_gate(0x2C, (uint64_t)isr_stub_mouse);
    }

    /* 4. Load IDT */
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (uint64_t)&idt;
    load_idt(&idt_ptr);

    /*
     * 5. SET INTERRUPT MASKS (The Fix)
     * Master PIC (0x21):
     *   - Bit 1 (IRQ 1 Keyboard) = 0 (Unmasked/Enabled)
     *   - Bit 2 (IRQ 2 Cascade)  = 0 (Unmasked/Enabled)
     *   - Result: 1111 1001 = 0xF9
     *
     * Slave PIC (0xA1):
     *   - Bit 4 (IRQ 12 Mouse)   = 1 (MASKED/DISABLED)
     *   - Result: 1111 1111 = 0xFF
     *
     * Why? We want the mouse data to stay in port 0x60 so our
     * 'handle_mouse' function in the kernel loop can pick it up.
     */
    outb(0x21, 0xF9);
    outb(0xA1, 0xFF); // Disable Mouse IRQ (Let Polling handle it)

    __asm__ volatile ("sti");
}