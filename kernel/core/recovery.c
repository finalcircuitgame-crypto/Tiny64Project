#include "../include/kernel.h"
#include "../hal/serial.h"
#include "../include/font.h"  // use bitmap font

// Print a message using the 16x16 bitmap font
static void bitmap_print(BootInfo *info, const char *msg, int x, int y, uint32_t color) {
    int orig_x = x;
    for (const char *p = msg; *p; ++p) {
        if (*p == '\n') {
            y += 18;
            x = orig_x;
            continue;
        }
        if (*p >= 32 && *p <= 126) {
            draw_char(info, *p, x, y, color);
        }
        x += 12;
    }
}

void kernel_main(BootInfo *info) {
    uint32_t *fb = info->framebuffer;
    uint64_t total_pixels = (uint64_t)info->height * info->pitch;

    // Initialize serial port for console output
    serial_init();

    /* 1. Paint Red/Orange screen */
    for (uint64_t i = 0; i < total_pixels; i++) fb[i] = 0xFFFF4500; 

    bitmap_print(info, "TINY64 SELF-REPAIR SYSTEM", 100, 100, 0xFFFFFFFF);
    bitmap_print(info, "Status: Triple Fault Prevented.", 100, 130, 0xFFFFFFFF);
    bitmap_print(info, "Action: Resetting hardware CMOS state...", 100, 160, 0xFFFFFFFF);

    /* 2. CLEAR THE CMOS FLAG so we can boot normally next time */
    write_cmos(0x34, 0x00);

    /* 3. Delay for user visibility */
    for(volatile uint64_t i=0; i<0x2FFFFFFF; i++);

    bitmap_print(info, "Success. Restarting system...", 100, 200, 0xFFFFFFFF);
    for(volatile uint64_t i=0; i<0x1FFFFFFF; i++);

    /* 4. Reboot */
    outb(0x64, 0xFE); 
    for(;;);
}