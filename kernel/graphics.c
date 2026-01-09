#include "../include/kernel.h"
#include "../hal/serial.h"
#include "../include/font.h"

// External font declaration
extern const uint16_t font16x16[96][16];

uint16_t icon_search[] = { 0x0000, 0x07E0, 0x0810, 0x1008, 0x1008, 0x1008, 0x0810, 0x07E0, 0x0020, 0x0040, 0x0080, 0x0100, 0x0200, 0x0000, 0x0000, 0x0000 };
uint16_t icon_folder[] = { 0x0000, 0x0000, 0x0380, 0x0440, 0x0440, 0x3FF8, 0x2004, 0x2004, 0x2004, 0x2004, 0x2004, 0x2004, 0x3FF8, 0x0000, 0x0000, 0x0000 };
uint16_t icon_term[]   = { 0x0000, 0x7FFE, 0x4002, 0x4002, 0x4802, 0x5402, 0x5202, 0x4102, 0x4002, 0x4002, 0x4032, 0x4032, 0x7FFE, 0x0000, 0x0000, 0x0000 };

/* Double Buffering Implementation */
void init_double_buffer(BootInfo *info) {
    // For now, disable double buffering to avoid memory allocation issues
    // during early boot phases and recovery kernel
    info->backbuffer = info->framebuffer; // Direct rendering fallback

    // TODO: Enable double buffering after memory manager is stable
    /*
    // Allocate backbuffer (same size as framebuffer)
    size_t buffer_size = info->height * info->pitch * sizeof(uint32_t);
    info->backbuffer = kmalloc(buffer_size);

    if (!info->backbuffer) {
        // Fallback to direct rendering if allocation fails
        info->backbuffer = info->framebuffer;
        return;
    }

    // Clear backbuffer
    for (size_t i = 0; i < info->height * info->pitch; i++) {
        info->backbuffer[i] = 0xFF000000; // Black background
    }
    */
}

void flip_buffers(BootInfo *info) {
    // Currently disabled - direct rendering only
    // When double buffering is enabled, this will copy backbuffer to framebuffer
    return;
}

void clear_backbuffer(BootInfo *info, uint32_t color) {
    if (!info->backbuffer) return;

    for (size_t i = 0; i < info->height * info->pitch; i++) {
        info->backbuffer[i] = color;
    }
}
void fill_rect(BootInfo *info, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    uint32_t *fb = info->backbuffer ? info->backbuffer : info->framebuffer;
    for (uint32_t dy = 0; dy < h; dy++) {
        if (y + dy >= info->height) break;
        uint32_t *row = fb + (y + dy) * info->pitch;
        for (uint32_t dx = 0; dx < w; dx++) {
            if (x + dx >= info->width) break;
            row[x + dx] = color;
        }
    }
}

void draw_rect(BootInfo *info, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    // Alias for fill_rect - draws a filled rectangle
    fill_rect(info, x, y, w, h, color);
}
void fill_circle(BootInfo *info, int cx, int cy, int radius, uint32_t color) {
    uint32_t *fb = info->backbuffer ? info->backbuffer : info->framebuffer;
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x*x + y*y <= radius*radius) {
                int px = cx + x;
                int py = cy + y;
                if (px >= 0 && px < (int)info->width && py >= 0 && py < (int)info->height) {
                    fb[py * info->pitch + px] = color;
                }
            }
        }
    }
}
void draw_bitmap(BootInfo *info, uint16_t *bitmap, int x, int y, int scale, uint32_t color) {
    for (int row = 0; row < 16; row++) {
        uint16_t bitmask = bitmap[row];
        for (int col = 0; col < 16; col++) {
            if ((bitmask >> (15 - col)) & 1) {
                fill_rect(info, x + col*scale, y + row*scale, scale, scale, color);
            }
        }
    }
}
void draw_char(BootInfo *info, char c, int x, int y, uint32_t color) {
    if (c < 32 || c > 126) return;
    uint32_t *fb = info->backbuffer ? info->backbuffer : info->framebuffer;
    const uint16_t *glyph = font16x16[c - 32];

    // 16x16 font, no scaling needed - direct pixel mapping
    for (int row = 0; row < 16; row++) {
        uint16_t row_data = glyph[row];
        for (int col = 0; col < 16; col++) {
            if ((row_data >> (15 - col)) & 1) {
                int px = x + col;
                int py = y + row;
                if (px >= 0 && px < (int)info->width && py >= 0 && py < (int)info->height) {
                    uint32_t index = py * info->pitch + px;
                    if (index < info->width * info->height) fb[index] = color;
                }
            }
        }
    }
}
void kprint(BootInfo *info, const char *str, int x, int y, uint32_t color) {
    // Also output to serial console
    serial_write_string(str);

    // Draw the text using high-quality bitmap font (16x16 characters)
    int current_x = x;
    for (const char *c = str; *c; c++) {
        if (*c == '\n') {
            // Handle newline
            current_x = x;
            y += 18; // 16 pixels height + 2 pixels spacing
        } else {
            draw_char(info, *c, current_x, y, color);
            current_x += 18; // 16 pixels width + 2 pixels spacing
        }
    }
}
