// DoomGeneric implementation for Tiny64 OS
// Based on the DoomGeneric SDL port

#include "doomkeys.h"
#include "m_argv.h"
#include "doomgeneric.h"
#include "../include/kernel.h"
#include "../hal/serial.h"
#include <stdbool.h>
#include <ctype.h>

// Access to global boot info (declared in kernel.c)
extern BootInfo* global_boot_info;

// Timer function from system_stubs.c
extern uint64_t timer_ms(void);

#define KEYQUEUE_SIZE 16

static unsigned short s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex = 0;
static unsigned int s_KeyQueueReadIndex = 0;

static unsigned char convertToDoomKey(unsigned int key) {
    switch (key) {
        case 0x1C: // Enter
            key = KEY_ENTER;
            break;
        case 0x01: // Escape
            key = KEY_ESCAPE;
            break;
        case 0x4B: // Left arrow
            key = KEY_LEFTARROW;
            break;
        case 0x4D: // Right arrow
            key = KEY_RIGHTARROW;
            break;
        case 0x48: // Up arrow
            key = KEY_UPARROW;
            break;
        case 0x50: // Down arrow
            key = KEY_DOWNARROW;
            break;
        case 0x1D: // Left Ctrl
            key = KEY_FIRE;
            break;
        case 0x39: // Space
            key = KEY_USE;
            break;
        case 0x1E: // A
            key = 'a';
            break;
        case 0x30: // B
            key = 'b';
            break;
        case 0x2E: // C
            key = 'c';
            break;
        case 0x20: // D
            key = 'd';
            break;
        case 0x12: // E
            key = 'e';
            break;
        case 0x21: // F
            key = 'f';
            break;
        case 0x22: // G
            key = 'g';
            break;
        case 0x23: // H
            key = 'h';
            break;
        case 0x17: // I
            key = 'i';
            break;
        case 0x24: // J
            key = 'j';
            break;
        case 0x25: // K
            key = 'k';
            break;
        case 0x26: // L
            key = 'l';
            break;
        case 0x32: // M
            key = 'm';
            break;
        case 0x31: // N
            key = 'n';
            break;
        case 0x18: // O
            key = 'o';
            break;
        case 0x19: // P
            key = 'p';
            break;
        case 0x10: // Q
            key = 'q';
            break;
        case 0x13: // R
            key = 'r';
            break;
        case 0x1F: // S
            key = 's';
            break;
        case 0x14: // T
            key = 't';
            break;
        case 0x16: // U
            key = 'u';
            break;
        case 0x2F: // V
            key = 'v';
            break;
        case 0x11: // W
            key = 'w';
            break;
        case 0x2D: // X
            key = 'x';
            break;
        case 0x15: // Y
            key = 'y';
            break;
        case 0x2C: // Z
            key = 'z';
            break;
        case 0x02: // 1
            key = '1';
            break;
        case 0x03: // 2
            key = '2';
            break;
        case 0x04: // 3
            key = '3';
            break;
        case 0x05: // 4
            key = '4';
            break;
        case 0x06: // 5
            key = '5';
            break;
        case 0x07: // 6
            key = '6';
            break;
        case 0x08: // 7
            key = '7';
            break;
        case 0x09: // 8
            key = '8';
            break;
        case 0x0A: // 9
            key = '9';
            break;
        case 0x0B: // 0
            key = '0';
            break;
        case 0x27: // ;
            key = ';';
            break;
        case 0x28: // '
            key = '\'';
            break;
        case 0x33: // ,
            key = ',';
            break;
        case 0x34: // .
            key = '.';
            break;
        case 0x35: // /
            key = '/';
            break;
        case 0x0C: // -
            key = '-';
            break;
        case 0x0D: // =
            key = '=';
            break;
        case 0x1A: // [
            key = '[';
            break;
        case 0x1B: // ]
            key = ']';
            break;
        case 0x2B: // \
            key = '\\';
            break;
        case 0x29: // `
            key = '`';
            break;
        default:
            key = tolower(key);
            break;
    }

    return key;
}

static void addKeyToQueue(int pressed, unsigned char keyCode) {
    unsigned short keyData = (pressed << 8) | keyCode;

    s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
    s_KeyQueueWriteIndex++;
    s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
}

void DG_Init() {
    // DoomGeneric screen buffer is already allocated in doomgeneric.c
    // Just ensure it's valid
    if (!DG_ScreenBuffer) {
        // Handle allocation failure
        return;
    }
}

// Window position for Doom (set by caller)
static int doom_window_x = 50;
static int doom_window_y = 100;

void DG_SetWindowPosition(int x, int y) {
    doom_window_x = x;
    doom_window_y = y;
}

void DG_DrawFrame() {
    // Debug: indicate frame is being drawn
    static int frame_count = 0;
    if (frame_count % 60 == 0) { // Print every 60 frames (~1 second at 60fps)
        serial_write_string("Doom: Drawing frame ");
        // Simple frame counter print
        char buf[16];
        int n = frame_count;
        int i = 0;
        do {
            buf[i++] = '0' + (n % 10);
            n /= 10;
        } while (n > 0);
        while (i > 0) {
            serial_write_char(buf[--i]);
        }
        serial_write_string("\n");
    }
    frame_count++;

    // Copy the Doom frame buffer to the window
    if (global_boot_info && DG_ScreenBuffer) {
        uint32_t* fb = global_boot_info->framebuffer;
        uint32_t pitch = global_boot_info->pitch;

        // Render Doom to fit within window bounds
        for (int y = 0; y < DOOMGENERIC_RESY; y++) {
            for (int x = 0; x < DOOMGENERIC_RESX; x++) {
                int src_idx = y * DOOMGENERIC_RESX + x;
                int dst_x = doom_window_x + x;
                int dst_y = doom_window_y + y;

                // Only render if within screen bounds
                if (dst_x >= 0 && dst_x < (int)global_boot_info->width &&
                    dst_y >= 0 && dst_y < (int)global_boot_info->height) {
                    int dst_idx = dst_y * pitch + dst_x;
                    if (dst_idx < (int)global_boot_info->width * (int)global_boot_info->height) {
                        fb[dst_idx] = DG_ScreenBuffer[src_idx];
                    }
                }
            }
        }
    }
}

void DG_SleepMs(uint32_t ms) {
    // Simple busy wait - in a real OS we'd use proper timers
    volatile uint32_t count = ms * 1000; // Rough approximation
    while (count--) {
        // Busy wait
    }
}

uint32_t DG_GetTicksMs() {
    // Return milliseconds since start
    static uint32_t start_ticks = 0;
    if (start_ticks == 0) {
        start_ticks = timer_ms();
    }
    return timer_ms() - start_ticks;
}

int DG_GetKey(int* pressed, unsigned char* key) {
    if (s_KeyQueueReadIndex == s_KeyQueueWriteIndex) {
        return 0; // No key
    } else {
        unsigned short keyData = s_KeyQueue[s_KeyQueueReadIndex];
        s_KeyQueueReadIndex++;
        s_KeyQueueReadIndex %= KEYQUEUE_SIZE;

        *pressed = keyData >> 8;
        *key = keyData & 0xFF;

        // Debug: show key being processed
        if (*pressed) {
            serial_write_string("Doom: Key processed: 0x");
            char hex[3];
            unsigned char val = *key;
            hex[0] = "0123456789ABCDEF"[val >> 4];
            hex[1] = "0123456789ABCDEF"[val & 0xF];
            hex[2] = 0;
            serial_write_string(hex);
            serial_write_string("\n");
        }

        return 1;
    }
}

void DG_SetWindowTitle(const char* title) {
    // For Tiny64, we don't have windows, so just log to serial
    if (title) {
        serial_write_string("Doom Window Title: ");
        serial_write_string(title);
        serial_write_string("\n");
    }
}

// Keyboard input handling for Doom
void doom_handle_key_press(unsigned char scancode, int pressed) {
    unsigned char doomKey = convertToDoomKey(scancode);
    if (doomKey != 0) {
        addKeyToQueue(pressed, doomKey);
    }
}
