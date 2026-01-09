#include "scalable_font.h"

// Simple absolute value function
static int abs_val(int x) {
    return x < 0 ? -x : x;
}

// Simple scalable vector font
// Characters are defined using line segments in a 8x8 unit grid
// Coordinates range from 0-8, will be scaled up when rendered

// Define some basic characters as line segments
static const line_segment_t font_A[] = {
    {1, 7, 4, 0}, {7, 7, 4, 0},  // Sides to top
    {2, 4, 6, 4}                  // Crossbar
};

static const line_segment_t font_B[] = {
    {0, 0, 0, 8}, {0, 0, 5, 1}, {5, 1, 6, 2}, {6, 2, 5, 3}, {5, 3, 0, 4},
    {0, 4, 5, 5}, {5, 5, 6, 6}, {6, 6, 5, 7}, {5, 7, 0, 8}
};

static const line_segment_t font_C[] = {
    {6, 1, 2, 0}, {2, 0, 0, 2}, {0, 2, 0, 6}, {0, 6, 2, 8}, {2, 8, 6, 7}
};

static const line_segment_t font_D[] = {
    {0, 0, 0, 8}, {0, 0, 4, 1}, {4, 1, 6, 3}, {6, 3, 6, 5}, {6, 5, 4, 7}, {4, 7, 0, 8}
};

static const line_segment_t font_E[] = {
    {6, 0, 0, 0}, {0, 0, 0, 8}, {0, 8, 6, 8}, {0, 4, 4, 4}
};

static const line_segment_t font_F[] = {
    {6, 0, 0, 0}, {0, 0, 0, 8}, {0, 4, 4, 4}
};

static const line_segment_t font_G[] = {
    {6, 1, 2, 0}, {2, 0, 0, 2}, {0, 2, 0, 6}, {0, 6, 2, 8}, {2, 8, 6, 7}, {6, 7, 6, 5}, {6, 5, 4, 5}
};

static const line_segment_t font_H[] = {
    {0, 0, 0, 8}, {8, 0, 8, 8}, {0, 4, 8, 4}
};

static const line_segment_t font_I[] = {
    {2, 0, 6, 0}, {4, 0, 4, 8}, {2, 8, 6, 8}
};

static const line_segment_t font_J[] = {
    {6, 0, 8, 2}, {8, 2, 8, 6}, {8, 6, 6, 8}, {6, 8, 2, 8}, {2, 8, 0, 6}
};

static const line_segment_t font_K[] = {
    {0, 0, 0, 8}, {0, 4, 8, 0}, {0, 4, 8, 8}
};

static const line_segment_t font_L[] = {
    {0, 0, 0, 8}, {0, 8, 8, 8}
};

static const line_segment_t font_M[] = {
    {0, 8, 0, 0}, {0, 0, 4, 4}, {4, 4, 8, 0}, {8, 0, 8, 8}
};

static const line_segment_t font_N[] = {
    {0, 8, 0, 0}, {0, 0, 8, 8}, {8, 8, 8, 0}
};

static const line_segment_t font_O[] = {
    {2, 0, 0, 2}, {0, 2, 0, 6}, {0, 6, 2, 8}, {2, 8, 6, 8}, {6, 8, 8, 6}, {8, 6, 8, 2}, {8, 2, 6, 0}, {6, 0, 2, 0}
};

static const line_segment_t font_P[] = {
    {0, 8, 0, 0}, {0, 0, 6, 0}, {6, 0, 8, 1}, {8, 1, 6, 3}, {6, 3, 0, 4}
};

static const line_segment_t font_Q[] = {
    {2, 0, 0, 2}, {0, 2, 0, 6}, {0, 6, 2, 8}, {2, 8, 6, 8}, {6, 8, 8, 6}, {8, 6, 8, 2}, {8, 2, 6, 0}, {6, 0, 2, 0}, {4, 6, 8, 8}
};

static const line_segment_t font_R[] = {
    {0, 8, 0, 0}, {0, 0, 6, 0}, {6, 0, 8, 1}, {8, 1, 6, 3}, {6, 3, 0, 4}, {4, 4, 8, 8}
};

static const line_segment_t font_S[] = {
    {6, 0, 2, 0}, {2, 0, 0, 1}, {0, 1, 0, 3}, {0, 3, 6, 5}, {6, 5, 8, 6}, {8, 6, 8, 7}, {8, 7, 6, 8}, {6, 8, 0, 8}
};

static const line_segment_t font_T[] = {
    {0, 0, 8, 0}, {4, 0, 4, 8}
};

static const line_segment_t font_U[] = {
    {0, 0, 0, 6}, {0, 6, 2, 8}, {2, 8, 6, 8}, {6, 8, 8, 6}, {8, 6, 8, 0}
};

static const line_segment_t font_V[] = {
    {0, 0, 4, 8}, {4, 8, 8, 0}
};

static const line_segment_t font_W[] = {
    {0, 0, 2, 8}, {2, 8, 4, 4}, {4, 4, 6, 8}, {6, 8, 8, 0}
};

static const line_segment_t font_X[] = {
    {0, 0, 8, 8}, {8, 0, 0, 8}
};

static const line_segment_t font_Y[] = {
    {4, 0, 4, 4}, {4, 4, 0, 8}, {4, 4, 8, 8}
};

static const line_segment_t font_Z[] = {
    {0, 0, 8, 0}, {8, 0, 0, 8}, {0, 8, 8, 8}
};

// Lowercase letters (simplified)
static const line_segment_t font_a[] = {
    {4, 4, 8, 4}, {8, 4, 8, 8}, {8, 8, 4, 8}, {4, 8, 4, 6}, {4, 6, 0, 6}
};

static const line_segment_t font_b[] = {
    {0, 0, 0, 8}, {0, 4, 4, 4}, {4, 4, 8, 5}, {8, 5, 4, 6}, {4, 6, 0, 7}
};

static const line_segment_t font_c[] = {
    {8, 4, 4, 4}, {4, 4, 0, 6}, {0, 6, 4, 8}, {4, 8, 8, 8}
};

static const line_segment_t font_d[] = {
    {8, 0, 8, 8}, {8, 4, 4, 4}, {4, 4, 0, 5}, {0, 5, 4, 6}, {4, 6, 8, 7}
};

static const line_segment_t font_e[] = {
    {0, 6, 8, 6}, {8, 6, 8, 8}, {8, 8, 0, 8}, {0, 8, 0, 6}, {0, 7, 8, 7}
};

static const line_segment_t font_f[] = {
    {4, 0, 4, 8}, {0, 4, 8, 4}
};

static const line_segment_t font_g[] = {
    {4, 4, 8, 4}, {8, 4, 8, 8}, {8, 8, 4, 8}, {4, 8, 4, 10}, {4, 10, 0, 10}
};

static const line_segment_t font_h[] = {
    {0, 0, 0, 8}, {0, 4, 8, 4}, {8, 4, 8, 8}
};

static const line_segment_t font_i[] = {
    {4, 2, 4, 8}, {2, 2, 6, 2}
};

static const line_segment_t font_j[] = {
    {6, 2, 6, 10}, {6, 10, 2, 10}, {4, 2, 8, 2}
};

static const line_segment_t font_k[] = {
    {0, 0, 0, 8}, {0, 6, 6, 4}, {0, 6, 8, 8}
};

static const line_segment_t font_l[] = {
    {4, 0, 4, 8}
};

static const line_segment_t font_m[] = {
    {0, 8, 0, 4}, {0, 4, 4, 6}, {4, 6, 8, 4}, {8, 4, 8, 8}
};

static const line_segment_t font_n[] = {
    {0, 8, 0, 4}, {0, 4, 8, 6}, {8, 6, 8, 8}
};

static const line_segment_t font_o[] = {
    {4, 4, 0, 6}, {0, 6, 4, 8}, {4, 8, 8, 6}, {8, 6, 4, 4}
};

static const line_segment_t font_p[] = {
    {0, 10, 0, 4}, {0, 4, 4, 4}, {4, 4, 8, 5}, {8, 5, 4, 6}, {4, 6, 0, 7}
};

static const line_segment_t font_q[] = {
    {8, 10, 8, 4}, {8, 4, 4, 4}, {4, 4, 0, 5}, {0, 5, 4, 6}, {4, 6, 8, 7}
};

static const line_segment_t font_r[] = {
    {0, 8, 0, 4}, {0, 4, 4, 4}, {4, 4, 8, 8}
};

static const line_segment_t font_s[] = {
    {8, 4, 0, 4}, {0, 4, 0, 6}, {0, 6, 8, 6}, {8, 6, 8, 8}, {8, 8, 0, 8}
};

static const line_segment_t font_t[] = {
    {4, 2, 4, 8}, {0, 4, 8, 4}
};

static const line_segment_t font_u[] = {
    {0, 4, 0, 8}, {0, 8, 4, 8}, {4, 8, 8, 6}, {8, 6, 8, 4}
};

static const line_segment_t font_v[] = {
    {0, 4, 4, 8}, {4, 8, 8, 4}
};

static const line_segment_t font_w[] = {
    {0, 4, 2, 8}, {2, 8, 4, 6}, {4, 6, 6, 8}, {6, 8, 8, 4}
};

static const line_segment_t font_x[] = {
    {0, 4, 8, 8}, {8, 4, 0, 8}
};

static const line_segment_t font_y[] = {
    {0, 4, 4, 8}, {4, 8, 8, 4}, {4, 8, 4, 10}
};

static const line_segment_t font_z[] = {
    {0, 4, 8, 4}, {8, 4, 0, 8}, {0, 8, 8, 8}
};

// Numbers
static const line_segment_t font_0[] = {
    {2, 0, 0, 2}, {0, 2, 0, 6}, {0, 6, 2, 8}, {2, 8, 6, 8}, {6, 8, 8, 6}, {8, 6, 8, 2}, {8, 2, 6, 0}, {6, 0, 2, 0}, {2, 4, 6, 4}
};

static const line_segment_t font_1[] = {
    {6, 0, 4, 0}, {4, 0, 4, 8}, {2, 8, 6, 8}
};

static const line_segment_t font_2[] = {
    {0, 2, 2, 0}, {2, 0, 6, 0}, {6, 0, 8, 2}, {8, 2, 8, 3}, {8, 3, 0, 8}, {0, 8, 8, 8}
};

static const line_segment_t font_3[] = {
    {0, 0, 8, 0}, {8, 0, 6, 2}, {6, 2, 4, 4}, {4, 4, 6, 6}, {6, 6, 8, 8}, {8, 8, 0, 8}, {4, 4, 8, 4}
};

static const line_segment_t font_4[] = {
    {6, 0, 6, 8}, {6, 0, 0, 4}, {0, 4, 8, 4}
};

static const line_segment_t font_5[] = {
    {8, 0, 0, 0}, {0, 0, 0, 4}, {0, 4, 6, 4}, {6, 4, 8, 6}, {8, 6, 8, 8}, {8, 8, 0, 8}
};

static const line_segment_t font_6[] = {
    {6, 0, 2, 0}, {2, 0, 0, 2}, {0, 2, 0, 6}, {0, 6, 2, 8}, {2, 8, 6, 8}, {6, 8, 8, 6}, {8, 6, 6, 4}, {6, 4, 0, 4}
};

static const line_segment_t font_7[] = {
    {0, 0, 8, 0}, {8, 0, 2, 8}
};

static const line_segment_t font_8[] = {
    {2, 0, 0, 2}, {0, 2, 0, 3}, {0, 3, 2, 4}, {2, 4, 6, 4}, {6, 4, 8, 3}, {8, 3, 8, 2}, {8, 2, 6, 0}, {6, 0, 2, 0}, {2, 4, 6, 4}, {2, 5, 0, 6}, {0, 6, 0, 8}, {0, 8, 2, 8}, {2, 8, 6, 8}, {6, 8, 8, 6}, {8, 6, 8, 5}, {8, 5, 6, 4}
};

static const line_segment_t font_9[] = {
    {6, 4, 2, 4}, {2, 4, 0, 6}, {0, 6, 2, 8}, {2, 8, 6, 8}, {6, 8, 8, 6}, {8, 6, 8, 2}, {8, 2, 6, 0}, {6, 0, 2, 0}, {2, 0, 0, 2}, {0, 2, 6, 4}
};

// Space and punctuation
static const line_segment_t font_space[] = {}; // Empty
static const line_segment_t font_exclam[] = {{4, 0, 4, 6}, {4, 8, 4, 8}};
static const line_segment_t font_period[] = {{3, 8, 5, 8}};
static const line_segment_t font_comma[] = {{4, 8, 3, 10}};

// Character definitions array
static const scalable_char_t font_chars[128] = {
    {' ', 0, font_space}, // Space
    {'!', 2, font_exclam},
    {'.', 1, font_period},
    {',', 1, font_comma},
    {'0', 10, font_0},
    {'1', 1, font_1},
    {'2', 6, font_2},
    {'3', 8, font_3},
    {'4', 4, font_4},
    {'5', 7, font_5},
    {'6', 7, font_6},
    {'7', 3, font_7},
    {'8', 10, font_8},
    {'9', 10, font_9},
    {'A', 3, font_A},
    {'B', 9, font_B},
    {'C', 5, font_C},
    {'D', 6, font_D},
    {'E', 4, font_E},
    {'F', 3, font_F},
    {'G', 7, font_G},
    {'H', 3, font_H},
    {'I', 3, font_I},
    {'J', 4, font_J},
    {'K', 3, font_K},
    {'L', 2, font_L},
    {'M', 4, font_M},
    {'N', 3, font_N},
    {'O', 4, font_O},
    {'P', 4, font_P},
    {'Q', 5, font_Q},
    {'R', 5, font_R},
    {'S', 6, font_S},
    {'T', 2, font_T},
    {'U', 2, font_U},
    {'V', 2, font_V},
    {'W', 4, font_W},
    {'X', 2, font_X},
    {'Y', 3, font_Y},
    {'Z', 3, font_Z},
    {'a', 2, font_a},
    {'b', 3, font_b},
    {'c', 3, font_c},
    {'d', 3, font_d},
    {'e', 4, font_e},
    {'f', 3, font_f},
    {'g', 4, font_g},
    {'h', 2, font_h},
    {'i', 2, font_i},
    {'j', 3, font_j},
    {'k', 3, font_k},
    {'l', 1, font_l},
    {'m', 3, font_m},
    {'n', 2, font_n},
    {'o', 3, font_o},
    {'p', 3, font_p},
    {'q', 3, font_q},
    {'r', 2, font_r},
    {'s', 4, font_s},
    {'t', 3, font_t},
    {'u', 2, font_u},
    {'v', 2, font_v},
    {'w', 3, font_w},
    {'x', 2, font_x},
    {'y', 3, font_y},
    {'z', 3, font_z},
};

void init_scalable_font(void) {
    // Font is ready to use - all data is static
}

static void draw_line(uint32_t *fb, int fb_width, int fb_height,
                     int x1, int y1, int x2, int y2, uint32_t color) {
    // Bresenham's line algorithm for drawing scalable lines
    int dx = abs_val(x2 - x1);
    int dy = abs_val(y2 - y1);
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;

    while (1) {
        if (x1 >= 0 && x1 < fb_width && y1 >= 0 && y1 < fb_height) {
            fb[y1 * fb_width + x1] = color;
        }

        if (x1 == x2 && y1 == y2) break;

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

void draw_scalable_char(uint32_t *fb, int fb_width, int fb_height,
                       char c, int x, int y, uint32_t color, int scale) {
    if ((unsigned char)c >= 128) return;

    const scalable_char_t *chr = &font_chars[(unsigned char)c];
    if (chr->num_segments == 0) return;

    // Draw each line segment, scaled
    for (int i = 0; i < chr->num_segments; i++) {
        const line_segment_t *seg = &chr->segments[i];

        int x1 = x + seg->x1 * scale;
        int y1 = y + seg->y1 * scale;
        int x2 = x + seg->x2 * scale;
        int y2 = y + seg->y2 * scale;

        draw_line(fb, fb_width, fb_height, x1, y1, x2, y2, color);
    }
}

void draw_scalable_text(uint32_t *fb, int fb_width, int fb_height,
                       const char *text, int x, int y, uint32_t color, int scale) {
    int current_x = x;

    while (*text) {
        if (*text == '\n') {
            current_x = x;
            y += 8 * scale; // Line height
        } else {
            draw_scalable_char(fb, fb_width, fb_height, *text, current_x, y, color, scale);
            current_x += 8 * scale; // Character width (fixed at 8 units)
        }
        text++;
    }
}
