#include "serial.h"
#include "../include/kernel.h"

// COM1 serial port base address
#define SERIAL_PORT 0x3F8

void serial_init(void) {
    // Disable interrupts
    outb(SERIAL_PORT + 1, 0x00);

    // Enable DLAB (Divisor Latch Access Bit)
    outb(SERIAL_PORT + 3, 0x80);

    // Set divisor to 3 (38400 baud with 1.8432MHz clock)
    outb(SERIAL_PORT + 0, 0x03); // Low byte
    outb(SERIAL_PORT + 1, 0x00); // High byte

    // 8 bits, no parity, one stop bit
    outb(SERIAL_PORT + 3, 0x03);

    // Enable FIFO, clear them, 14-byte threshold
    outb(SERIAL_PORT + 2, 0xC7);

    // IRQs enabled, RTS/DSR set
    outb(SERIAL_PORT + 4, 0x0B);

    // Set loopback mode for testing
    outb(SERIAL_PORT + 4, 0x1E);

    // Test serial chip (send/receive byte)
    outb(SERIAL_PORT + 0, 0xAE);

    // Check if serial is faulty
    if (inb(SERIAL_PORT + 0) != 0xAE) {
        // Serial is faulty, disable it
        return;
    }

    // Set normal operation mode
    outb(SERIAL_PORT + 4, 0x0F);
}

static int serial_is_transmit_empty() {
    return inb(SERIAL_PORT + 5) & 0x20;
}

void serial_write_char(char c) {
    while (!serial_is_transmit_empty());
    outb(SERIAL_PORT, c);
}

void serial_write_string(const char *str) {
    while (*str) {
        serial_write_char(*str);
        str++;
    }
}
