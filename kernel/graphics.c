#include "../include/kernel.h"
#include "../include/ttf.h"
#include "../hal/serial.h"
#include "../include/font.h"

// External font declaration
extern const uint16_t* font16x16[96];

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
    draw_char_scaled(info, c, x, y, color, 2); // Default 2x scale for balanced readability
}

void draw_char_scaled(BootInfo *info, char c, int x, int y, uint32_t color, int scale) {
    if (c < 32 || c > 126) return;
    uint32_t *fb = info->backbuffer ? info->backbuffer : info->framebuffer;
    const uint16_t *glyph = font16x16[c - 32];

    // 16x16 font with scaling for better readability
    for (int row = 0; row < 16; row++) {
        uint16_t row_data = glyph[row];
        for (int col = 0; col < 16; col++) {
            if ((row_data >> (15 - col)) & 1) {
                int px = x + col * scale;
                int py = y + row * scale;
                // Fill a scale x scale block for each pixel
                for (int sy = 0; sy < scale; sy++) {
                    for (int sx = 0; sx < scale; sx++) {
                        int px_scaled = px + sx;
                        int py_scaled = py + sy;
                        if (px_scaled >= 0 && px_scaled < (int)info->width &&
                            py_scaled >= 0 && py_scaled < (int)info->height) {
                            uint32_t index = py_scaled * info->pitch + px_scaled;
                            if (index < info->width * info->height) fb[index] = color;
                        }
                    }
                }
            }
        }
    }

    // Character rendering complete
}
void kprint(BootInfo *info, const char *str, int x, int y, uint32_t color) {
    // Also output to serial console
    serial_write_string(str);

    // Draw the text using optimized bitmap font (16x16 characters, 2x scaled)
    int current_x = x;
    const int char_width = 32;  // 16 * 2 (font width * scale)
    const int line_advance = 40; // 32 pixels glyph height + 8 pixels baseline spacing

    for (const char *c = str; *c; c++) {
        if (*c == '\n') {
            // Handle newline with proper line advancement
            current_x = x;
            y += line_advance;
        } else if (*c == '\t') {
            // Handle tab (4 spaces)
            current_x += char_width * 4;
        } else if (*c >= 32 && *c <= 126) {
            // Only render printable ASCII characters
            draw_char(info, *c, current_x, y, color);
            current_x += char_width;
        }
        // Skip other control characters
    }
}

#ifndef RECOVERY_KERNEL
void kprint_ttf(BootInfo *info, const char *str, int x, int y, uint32_t color, void *ttf_font_ptr) {
    ttf_font_t *font = (ttf_font_t*)ttf_font_ptr;
    if (!font) {
        // Fall back to regular kprint
        kprint(info, str, x, y, color);
        return;
    }

    // Also output to serial console
    serial_write_string(str);

    // Draw the text using TTF font
    int current_x = x;
    const int char_width = 8;   // 8x8 pixel glyphs
    const int char_height = 8;
    const int line_advance = 10; // 8 pixels glyph height + 2 pixels spacing

    for (const char *c = str; *c; c++) {
        if (*c == '\n') {
            // Handle newline
            current_x = x;
            y += line_advance;
        } else if (*c == '\t') {
            // Handle tab (4 spaces)
            current_x += char_width * 4;
        } else if (*c >= 32 && *c <= 126) {
            // Render printable ASCII characters
            int glyph_index = ttf_get_glyph_index(font, (uint32_t)*c);
            uint8_t glyph_bitmap[64]; // 8x8 bitmap

            if (ttf_render_glyph(font, glyph_index, glyph_bitmap, 8, 8, 0, 0, 1) == 0) {
                // Render the glyph bitmap to screen
                for (int gy = 0; gy < 8; gy++) {
                    for (int gx = 0; gx < 8; gx++) {
                        if (glyph_bitmap[gy * 8 + gx] > 128) {  // Threshold for visibility
                            int screen_x = current_x + gx;
                            int screen_y = y + gy;
                            if (screen_x >= 0 && screen_x < info->width &&
                                screen_y >= 0 && screen_y < info->height) {
                                uint32_t *fb = info->backbuffer ? info->backbuffer : info->framebuffer;
                                fb[screen_y * info->pitch + screen_x] = color;
                            }
                        }
                    }
                }
            }
            current_x += char_width;
        }
        // Skip other control characters
    }
}
#endif // !RECOVERY_KERNEL