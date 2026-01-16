// BIOS-specific stubs for missing functions
// These functions are referenced in kernel.c but may not be implemented

#include <stdint.h>

// Mouse variables and functions
int mouse_x = 320;
int mouse_y = 240;

void draw_cursor(void) {
    // Stub - no cursor drawing in BIOS mode
}

void handle_mouse(void) {
    // Stub - no mouse handling in BIOS mode
}

void mouse_handle_byte(uint8_t byte) {
    // Stub - no mouse byte handling in BIOS mode
    (void)byte;
}

void mouse_request_sample(void) {
    // Stub - no mouse sampling in BIOS mode
}

// Keyboard variables
int last_key_pressed = 0;

// Keyboard functions
void keyboard_enable_interrupt(void) {
    // Stub - no interrupts in BIOS mode
}

void keyboard_init(void) {
    // Stub - no keyboard initialization in BIOS mode
}

// GDT functions
void init_gdt(void) {
    // Stub - GDT already set up in stage2.S
}

void init_idt(void) {
    // Stub - no IDT in BIOS mode
}
