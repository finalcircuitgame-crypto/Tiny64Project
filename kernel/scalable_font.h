#ifndef SCALABLE_FONT_H
#define SCALABLE_FONT_H

#include <stdint.h>

// Simple scalable vector font system
// Each character is defined by line segments that can be scaled

typedef struct {
    int16_t x1, y1;  // Start point
    int16_t x2, y2;  // End point
} line_segment_t;

typedef struct {
    char character;
    int num_segments;
    const line_segment_t *segments;
} scalable_char_t;

// Font scaling and positioning
typedef struct {
    int scale;       // Scaling factor (1 = 8x8, 2 = 16x16, etc.)
    int char_width;  // Scaled character width
    int char_height; // Scaled character height
} font_metrics_t;

// Function declarations
void init_scalable_font(void);
void draw_scalable_char(uint32_t *fb, int fb_width, int fb_height,
                       char c, int x, int y, uint32_t color, int scale);
void draw_scalable_text(uint32_t *fb, int fb_width, int fb_height,
                       const char *text, int x, int y, uint32_t color, int scale);

#endif
