#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>

/* --- System Structures --- */

typedef struct {
    uint32_t *framebuffer;
    uint32_t *backbuffer;  // Double buffering support
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
} BootInfo;

/* --- Hardware Port I/O (Inline Assembly) --- */

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

// Note: outw, inw, outl, inl are defined in kernel.c to avoid conflicts

static inline void io_wait(void) {
    /* Port 0x80 is used for checkpoints during POST, safe to write to */
    outb(0x80, 0);
}

/* --- CMOS NVRAM Helpers (Survives Reboot) --- */

static inline void write_cmos(uint8_t addr, uint8_t val) {
    outb(0x70, addr);
    outb(0x71, val);
}

static inline uint8_t read_cmos(uint8_t addr) {
    outb(0x70, addr);
    return inb(0x71);
}

/* --- Graphics Prototypes (graphics.c / font.c) --- */

void init_double_buffer(BootInfo *info);
void flip_buffers(BootInfo *info);
void clear_backbuffer(BootInfo *info, uint32_t color);

void fill_rect(BootInfo *info, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fill_circle(BootInfo *info, int cx, int cy, int radius, uint32_t color);
void draw_rect(BootInfo *info, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void draw_bitmap(BootInfo *info, uint16_t *bitmap, int x, int y, int scale, uint32_t color);
void draw_char(BootInfo *info, char c, int x, int y, uint32_t color);
void draw_char_terminal(BootInfo *info, char c, int x, int y, uint32_t color);
void draw_char_scaled(BootInfo *info, char c, int x, int y, uint32_t color, int scale);
void mouse_request_sample(void);
void kprint(BootInfo *info, const char *str, int x, int y, uint32_t color);

// TTF font rendering
void kprint_ttf(BootInfo *info, const char *str, int x, int y, uint32_t color, void *ttf_font);

/* --- Cursor Logic (mouse.c) --- */

void draw_cursor(BootInfo *info, int x, int y);
void restore_cursor_bg(BootInfo *info, int x, int y);

/* --- Memory Management Prototypes --- */

void init_heap(void);
void* kmalloc(size_t size);
void kfree(void *ptr);
void get_heap_stats(size_t *total_size, size_t *used_size, size_t *free_size);
void debug_heap(BootInfo *info, int start_y);

/* --- HAL & Driver Prototypes --- */

void init_gdt(void);
void init_idt(void);
void set_idt_gate_ist(int n, uint64_t handler, uint8_t ist);
int mouse_init(void);
void handle_mouse(BootInfo *info);
void mouse_handle_byte(BootInfo *info, uint8_t data);
void start_mouse_test(void);
int get_mouse_test_status(int *clicks, int *movement);
void keyboard_handler_main(uint8_t scancode);

/* --- Doom Integration --- */

void doomgeneric_Create(int argc, char **argv);
void doomgeneric_Tick();
void doomgeneric_SetBootInfo(BootInfo* info);

/* --- Windows XP UI --- */

void draw_winxp_desktop(BootInfo *info);
void draw_winxp_window(BootInfo *info, int x, int y, int w, int h, const char *title, int active);
void draw_winxp_taskbar(BootInfo *info);
void draw_winxp_button(BootInfo *info, int x, int y, int w, int h, const char *text, uint32_t bg_color);
void draw_winxp_icon(BootInfo *info, int x, int y, const char *label);
void draw_winxp_terminal(BootInfo *info, int x, int y, int w, int h);
void init_winxp_desktop(BootInfo *info);

/* --- Global Variables (extern) --- */

/* Input State */
extern int mouse_x;
extern int mouse_y;
extern uint8_t mouse_left_pressed;
extern char last_key_pressed;

/* Visual Data */
extern uint16_t icon_search[];
extern uint16_t icon_folder[];
extern uint16_t icon_term[];
extern const uint8_t font8x8_basic[96][8];

#endif