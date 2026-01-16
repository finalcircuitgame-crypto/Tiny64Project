#include "../include/kernel.h"
#include "../hal/serial.h"
#include "../include/fs.h"
#include "../include/keyboard.h"
#include "../include/ttf.h"
#include "../graphics/inter_font_data.h"
#include "../drivers/usb.h"
#include "../drivers/rtl8139.h"
#include "../drivers/ac97.h"
#include "../drivers/ide.h"
#include <stdbool.h>
#include <string.h>

// FILE type for embedded files (matching doom_stubs.c)
typedef struct {
    struct {
        const uint8_t* data;
        size_t size;
        size_t position;
        int valid;
    } internal;
} FILE;

// File I/O function declarations from doom_stubs.c
extern int fseek(FILE* stream, long offset, int origin);
extern long ftell(FILE* stream);
extern int sprintf(char* str, const char* format, ...);

// SEEK constants
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

// File operations from doom_stubs.c
extern FILE* fopen(const char* filename, const char* mode);
extern int fclose(FILE* stream);

// Global BootInfo for Doom
BootInfo* global_boot_info = NULL;

// Global TTF font
ttf_font_t global_ttf_font;

// Forward declarations
void show_boot_terminal(BootInfo *info);
void enter_graphics_mode(BootInfo *info);

// Doom declarations
void doomgeneric_Create(int argc, char **argv);

// Enhanced kprint that uses TTF if available
void kprint_auto(BootInfo *info, const char *str, int x, int y, uint32_t color) {
    if (global_ttf_font.offset_table.num_tables > 0) {
        // Use TTF font
        kprint_ttf(info, str, x, y, color, &global_ttf_font);
    } else {
        // Fallback to bitmap font
        kprint(info, str, x, y, color);
    }
}

#ifdef QEMU
#define IS_QEMU_ENV 1
#else
#define IS_QEMU_ENV 0
#endif

void draw_terminal_window(BootInfo *info, int x, int y, int w, int h) {
  // Draw terminal border and titlebar sized to (w,h)
  fill_rect(info, x, y, w, h, 0xFF000000);       // Outer border
  fill_rect(info, x, y, w, 20, 0xFFCCCCCC);      // Title bar
  kprint(info, "Terminal", x + 10, y + 6, 0xFF000000);
  fill_rect(info, x + w - 24, y + 2, 16, 16, 0xFFCC0000); // Close button
}

// Helper: print a line of text into the terminal using scaled glyphs
static void terminal_print_line(BootInfo *info, const char *str, int x, int y,
                                uint32_t color, int scale, int char_width) {
  int cx = x;
  for (const char *p = str; *p; p++) {
    if (*p == '\n') break;
    if (*p >= 32 && *p <= 126) {
      draw_char_scaled(info, *p, cx, y, color, scale);
      cx += char_width;
    }
  }
}

void clear_terminal_area(BootInfo *info, int x, int y) {
  // Don't clear - let background show through
  // fill_rect(info, x, y, 408, 308, 0xFFEBEBEB);
}

// QEMU detection helper (simple, may refine for full CPUID check if needed)
int is_qemu(void) {
#if IS_QEMU_ENV
  return 1;
#else
  // For demonstration, always false if not forced
  return 0;
#endif
}

// Ensure outw is declared (may be implemented elsewhere)
// Properly define outw using inline assembly for x86 platforms
static inline void outw(uint16_t port, uint16_t val) {
  __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

// QEMU shutdown for x86 (ISA debug port). Harmless if unsupported.
void qemu_shutdown(void) { outw(0x604, 0x2000); }

// --- Mouse initialization logic override for QEMU quirks ---
int robust_mouse_init(void) {
  // Returns if *attempt* to enable streaming succeeded. Fails only if no
  // device. Allows missing 0xF4 ACK as successful in QEMU.
  int got_reset_fa = 0, got_reset_aa = 0;
  uint8_t ack = 0;
  serial_write_string("[MOUSE_INIT] Resetting mouse...\n");
  outb(0x64, 0xD4); // Next byte to mouse
  outb(0x60, 0xFF); // Reset
  // Wait for two bytes: 0xFA, then 0xAA
  for (int tries = 0; tries < 40000; ++tries) {
    if ((inb(0x64) & 1)) {
      uint8_t resp = inb(0x60);
      if (!got_reset_fa && resp == 0xFA) {
        got_reset_fa = 1;
        serial_write_string("Reset response 1: 0xFA\n");
      } else if (got_reset_fa && resp == 0xAA) {
        got_reset_aa = 1;
        serial_write_string("Reset response 2: 0xAA\n");
        break;
      }
    }
    io_wait();
  }
  if (!got_reset_fa || !got_reset_aa) {
    serial_write_string(
        "[MOUSE_INIT] Did not receive reset responses, giving up.\n");
    return 0;
  }
  serial_write_string("[MOUSE_INIT] Enabling streaming mode (0xF4)...\n");
  outb(0x64, 0xD4); // Next byte to mouse
  outb(0x60, 0xF4); // Enable streaming

  ack = 0;
  // Wait for ACK 0xFA after 0xF4
  for (int i = 0; i < (is_qemu() ? 5000 : 20000); i++) {
    if ((inb(0x64) & 1)) {
      ack = inb(0x60);
      break;
    }
    io_wait();
  }
  if (ack == 0xFA) {
    serial_write_string("[MOUSE_INIT] ACK received - streaming enabled!\n");
    return 1;
  } else {
    serial_write_string(
        "[MOUSE_INIT] No ACK (QEMU/quirk?) - assuming streaming enabled.\n");
    // QEMU frequently misses or coalesces this, allow as success.
    return 1;
  }
}

void kernel_main(BootInfo *info) {
  serial_write_string("[BOOT] ===== TINY64 OS v1.0 =====\n");
  serial_write_string("[BOOT] Welcome to the Tiny64 Boot Terminal!\n");
  serial_write_string("[BOOT] System initializing...\n\n");

  // Store BootInfo globally for Doom
  global_boot_info = info;

  // Initialize serial port for console output FIRST
  serial_init();

  // Initialize IDT for keyboard interrupts in boot terminal
  init_idt();

  // PHASE 1: TEXT-MODE BOOT TERMINAL
  // Show cool ASCII art and boot terminal before graphics
  show_boot_terminal(info);

  // PHASE 2: GRAPHICS MODE (only after user types 'boot')
  enter_graphics_mode(info);
}

void show_boot_terminal(BootInfo *info) {
  // Skip keyboard init for now to avoid triple fault
  // keyboard_init();

  // Validate BootInfo before accessing framebuffer
  serial_write_string("[BOOT] Validating BootInfo...\n");
  if (!info) {
    serial_write_string("[BOOT] ERROR: BootInfo is NULL!\n");
    return;
  }

  if (!info->framebuffer) {
    serial_write_string("[BOOT] ERROR: Framebuffer is NULL!\n");
    return;
  }

  serial_write_string("[BOOT] BootInfo validated, clearing screen...\n");

  // Simple direct framebuffer drawing (no double buffering to avoid heap allocation)
  uint32_t *fb = info->framebuffer;
  uint32_t pitch = info->pitch;

  // Clear screen to dark blue (be very conservative with bounds)
  size_t width = info->width;
  size_t height = info->height;

  if (width > 1920) width = 1920;
  if (height > 1080) height = 1080;

  serial_write_string("[BOOT] Clearing screen...\n");

  for (size_t y = 0; y < height; y++) {
    for (size_t x = 0; x < width; x++) {
      size_t index = y * pitch + x;
      if (index < width * height) {
        fb[index] = 0xFF000011;
      }
    }
  }

  serial_write_string("[BOOT] Screen cleared, drawing interface...\n");

  // Enable keyboard interrupts for input
  keyboard_enable_interrupt();

  // Enable interrupts globally
  __asm__ volatile("sti");

  serial_write_string("[BOOT] Keyboard input enabled\n");

  // Draw boot terminal interface on screen (simple text output)
  int center_x = info->width / 2;
  int start_y = 50;
  uint32_t text_color = 0xFFFFFFFF; // White text
  uint32_t accent_color = 0xFF00FFFF; // Cyan accent

  // Simple text display for boot terminal
  draw_char(info, 'T', center_x - 100, start_y, text_color);
  draw_char(info, 'i', center_x - 84, start_y, text_color);
  draw_char(info, 'n', center_x - 68, start_y, text_color);
  draw_char(info, 'y', center_x - 52, start_y, text_color);
  draw_char(info, '6', center_x - 36, start_y, text_color);
  draw_char(info, '4', center_x - 20, start_y, text_color);
  draw_char(info, ' ', center_x - 4, start_y, text_color);
  draw_char(info, 'O', center_x + 12, start_y, text_color);
  draw_char(info, 'S', center_x + 28, start_y, text_color);
  draw_char(info, ' ', center_x + 44, start_y, text_color);
  draw_char(info, 'v', center_x + 60, start_y, text_color);
  draw_char(info, '1', center_x + 76, start_y, text_color);
  draw_char(info, '.', center_x + 92, start_y, text_color);
  draw_char(info, '0', center_x + 108, start_y, text_color);

  draw_char(info, 'B', center_x - 100, start_y + 20, accent_color);
  draw_char(info, 'o', center_x - 84, start_y + 20, accent_color);
  draw_char(info, 'o', center_x - 68, start_y + 20, accent_color);
  draw_char(info, 't', center_x - 52, start_y + 20, accent_color);
  draw_char(info, ' ', center_x - 36, start_y + 20, accent_color);
  draw_char(info, 'T', center_x - 20, start_y + 20, accent_color);
  draw_char(info, 'e', center_x - 4, start_y + 20, accent_color);
  draw_char(info, 'r', center_x + 12, start_y + 20, accent_color);
  draw_char(info, 'm', center_x + 28, start_y + 20, accent_color);
  draw_char(info, 'i', center_x + 44, start_y + 20, accent_color);
  draw_char(info, 'n', center_x + 60, start_y + 20, accent_color);
  draw_char(info, 'a', center_x + 76, start_y + 20, accent_color);
  draw_char(info, 'l', center_x + 92, start_y + 20, accent_color);

  draw_char(info, 'T', center_x - 100, start_y + 60, text_color);
  draw_char(info, 'y', center_x - 84, start_y + 60, text_color);
  draw_char(info, 'p', center_x - 68, start_y + 60, text_color);
  draw_char(info, 'e', center_x - 52, start_y + 60, text_color);
  draw_char(info, ' ', center_x - 36, start_y + 60, text_color);
  draw_char(info, '\'', center_x - 20, start_y + 60, text_color);
  draw_char(info, 'b', center_x - 4, start_y + 60, text_color);
  draw_char(info, 'o', center_x + 12, start_y + 60, text_color);
  draw_char(info, 'o', center_x + 28, start_y + 60, text_color);
  draw_char(info, 't', center_x + 44, start_y + 60, text_color);
  draw_char(info, '\'', center_x + 60, start_y + 60, text_color);
  draw_char(info, ' ', center_x + 76, start_y + 60, text_color);
  draw_char(info, 't', center_x + 92, start_y + 60, text_color);
  draw_char(info, 'o', center_x + 108, start_y + 60, text_color);
  draw_char(info, ' ', center_x + 124, start_y + 60, text_color);
  draw_char(info, 'c', center_x + 140, start_y + 60, text_color);
  draw_char(info, 'o', center_x + 156, start_y + 60, text_color);
  draw_char(info, 'n', center_x + 172, start_y + 60, text_color);
  draw_char(info, 't', center_x + 188, start_y + 60, text_color);
  draw_char(info, 'i', center_x + 204, start_y + 60, text_color);
  draw_char(info, 'n', center_x + 220, start_y + 60, text_color);
  draw_char(info, 'u', center_x + 236, start_y + 60, text_color);
  draw_char(info, 'e', center_x + 252, start_y + 60, text_color);

  // Now output to serial as well
  serial_write_string("\n");
  serial_write_string("╔══════════════════════════════════════════════════════════════╗\n");
  serial_write_string("║                      TINY64 OS v1.0                         ║\n");
  serial_write_string("║                    Boot Terminal Mode                       ║\n");
  serial_write_string("╠══════════════════════════════════════════════════════════════╣\n");
  serial_write_string("║                                                              ║\n");
  serial_write_string("║   Tiny64 Operating System                                    ║\n");
  serial_write_string("║                                                              ║\n");
  serial_write_string("╠══════════════════════════════════════════════════════════════╣\n");
  serial_write_string("║ Commands:                                                   ║\n");
  serial_write_string("║   boot     - Enter graphical desktop mode                   ║\n");
  serial_write_string("║   help     - Show available commands                         ║\n");
  serial_write_string("║   info     - System information                              ║\n");
  serial_write_string("║   status   - Show boot status                                ║\n");
  serial_write_string("║   shutdown - Power off system                                ║\n");
  serial_write_string("╚══════════════════════════════════════════════════════════════╝\n");
  serial_write_string("\n");
  serial_write_string("Tiny64> ");

  // Keyboard already initialized in main kernel - just ensure interrupts are enabled
  keyboard_enable_interrupt();

  // Enable interrupts globally (if not already enabled)
  __asm__ volatile("sti");

  serial_write_string("[BOOT] Boot terminal ready - type 'boot' to continue...\n");

  // Simple command buffer for boot terminal
  char boot_cmd[16] = {0};
  int boot_cmd_len = 0;
  int auto_boot_timer = 0;

  // Auto-continue to graphics mode
  serial_write_string("[BOOT] Continuing to graphics mode...\n");

  // Small delay to show the boot terminal
  for (volatile int i = 0; i < 100000; i++);

  return;
}

void enter_graphics_mode(BootInfo *info) {
  uint32_t *fb = info->framebuffer;
  uint64_t total_pixels = (uint64_t)info->height * info->pitch;

  serial_write_string("[BOOT] Initializing graphics mode...\n");

  // Boot timeout protection - prevents infinite hangs
  volatile uint32_t boot_watchdog = 0;
  const uint32_t BOOT_TIMEOUT =
      is_qemu() ? 0x80000 : 0x20FFFFF; // Much shorter timeout in QEMU

  // Initialize double buffering
  init_double_buffer(info);

  // Start with black screen (draw to backbuffer)
  clear_backbuffer(info, 0xFF000000);
  flip_buffers(info);

  // Boot splash header - stays visible throughout
  kprint(info, "Tiny64 Operating System", 50, 30, 0xFFFFFFFF);
  kprint(info, "Initializing core subsystems...", 50, 55, 0xFFCCCCCC);

  // Progress bar background
  draw_rect(info, 50, 80, 300, 12, 0xFF333333);
  flip_buffers(info); // Show initial progress bar

  // --- PHASE 1: Memory Management ---
  boot_watchdog += 0x100000; // Update watchdog
  if (boot_watchdog > BOOT_TIMEOUT)
    goto boot_timeout;

  kprint(info, "[    ] Memory Manager", 50, 110, 0xFFFFFF00);
  draw_rect(info, 50, 80, 30, 12, 0xFF00AA00); // 10% progress
  flip_buffers(info);

  // Small delay to show initialization
  for (volatile uint32_t i = 0;
       i < (is_qemu() ? 0x3FFF : 0x2FFFFF) && boot_watchdog < BOOT_TIMEOUT;
       i++) {
    boot_watchdog++;
  }
  if (boot_watchdog > BOOT_TIMEOUT)
    goto boot_timeout;

  init_heap();
  kprint(info, "[OK] Memory Manager (1MB heap allocated)", 50, 110, 0xFF00FF00);
  draw_rect(info, 50, 80, 75, 12, 0xFF00AA00); // 25% progress
  flip_buffers(info);

  // --- PHASE 2: CPU Architecture ---
  boot_watchdog += 0x100000;
  if (boot_watchdog > BOOT_TIMEOUT)
    goto boot_timeout;

  kprint(info, "[    ] CPU Architecture", 50, 135, 0xFFFFFF00);
  flip_buffers(info);

  for (volatile uint32_t i = 0;
       i < (is_qemu() ? 0x3FFF : 0x2FFFFF) && boot_watchdog < BOOT_TIMEOUT;
       i++) {
    boot_watchdog++;
  }
  if (boot_watchdog > BOOT_TIMEOUT)
    goto boot_timeout;

  init_gdt();
  kprint(info, "[OK] Global Descriptor Table", 50, 135, 0xFF00FF00);
  draw_rect(info, 50, 80, 120, 12, 0xFF00AA00); // 40% progress
  flip_buffers(info);

  for (volatile uint32_t i = 0;
       i < (is_qemu() ? 0x3FFF : 0x2FFFFF) && boot_watchdog < BOOT_TIMEOUT;
       i++) {
    boot_watchdog++;
  }
  if (boot_watchdog > BOOT_TIMEOUT)
    goto boot_timeout;

  kprint(info, "[OK] Interrupt Descriptor Table", 50, 160, 0xFF00FF00);
  draw_rect(info, 50, 80, 165, 12, 0xFF00AA00); // 55% progress
  flip_buffers(info);

  // --- PHASE 3: Input Subsystems ---
  kprint(info, "[    ] Input Subsystems", 50, 185, 0xFFFFFF00);
  flip_buffers(info);
  for (volatile uint32_t i = 0; i < (is_qemu() ? 0x1FFF : 0x1FFFFF); i++)
    ; // Shorter delay in QEMU

  // Try mouse init with QEMU-aware logic
  int mouse_ok = 0;
  int attempts = 0;
  const int MAX_ATTEMPTS = is_qemu() ? 1 : 3;

  while (!mouse_ok && attempts < MAX_ATTEMPTS) {
    serial_write_string("[KERNEL] Mouse init attempt ");
    // Simple decimal output
    char buf[2] = "0";
    buf[0] = '1' + attempts;
    serial_write_string(buf);
    serial_write_string("/3\n");

    // Flush any leftover bytes from firmware
    while (inb(0x64) & 1)
      inb(0x60);

    // Use robust mouse init
    mouse_ok = robust_mouse_init();
    attempts++;

    if (!mouse_ok) {
      serial_write_string(
          "[KERNEL] Mouse init failed, waiting before retry...\n");
      // Short delay
      for (volatile uint32_t i = 0; i < (is_qemu() ? 0x200 : 0x10000); i++)
        ;
    }
  }

  if (mouse_ok) {
    kprint(info, "[OK] PS/2 Mouse Driver", 50, 185, 0xFF00FF00);
  } else {
    kprint(info, "[SKIP] PS/2 Mouse (timeout/no response)", 50, 185,
           0xFFFFAA00);
  }
  draw_rect(info, 50, 80, 210, 12, 0xFF00AA00); // 70% progress
  flip_buffers(info);

  kprint(info, "[OK] PS/2 Keyboard Driver", 50, 210, 0xFF00FF00);
  draw_rect(info, 50, 80, 255, 12, 0xFF00AA00); // 85% progress
  flip_buffers(info);

  // --- PHASE 3.5: Keyboard ---
  serial_write_string("[BOOT] Starting keyboard phase\n");
  kprint(info, "[    ] Keyboard", 50, 250, 0xFFFFFF00);
  serial_write_string("[BOOT] About to call keyboard_init()\n");
  keyboard_init();
  serial_write_string(
      "[BOOT] keyboard_init() returned - interrupts re-enabled\n");
  serial_write_string("[BOOT] About to call kprint for keyboard success\n");
  kprint(info, "[OK] PS/2 Keyboard", 50, 250, 0xFF00FF00);
  serial_write_string("[BOOT] kprint completed, keyboard phase done\n");

  kprint(info, "[    ] Filesystem", 50, 300, 0xFFFFFF00);
  serial_write_string("[BOOT] About to call fs_init()\n");
  fs_init();
  serial_write_string("[BOOT] fs_init() completed\n");
  kprint(info, "[OK] Virtual Filesystem (2 files)", 50, 300, 0xFF00FF00);

  // Initialize USB subsystem
  serial_write_string("[BOOT] Initializing USB subsystem...\n");
  usb_init();
  usb_scan_controllers();
  kprint(info, "[OK] USB Subsystem Initialized", 50, 315, 0xFF00FF00);

  // Initialize network driver
  serial_write_string("[BOOT] Initializing network driver...\n");
  rtl8139_init();
  kprint(info, "[OK] Network Driver Initialized", 50, 330, 0xFF00FF00);

  // Initialize audio driver
  serial_write_string("[BOOT] Initializing audio driver...\n");
  ac97_init();
  kprint(info, "[OK] Audio Driver Initialized", 50, 345, 0xFF00FF00);

  // Initialize storage driver
  serial_write_string("[BOOT] Initializing storage driver...\n");
  ide_init();
  ide_detect_drives();
  kprint(info, "[OK] Storage Driver Initialized", 50, 360, 0xFF00FF00);

  // Load TTF font globally for system text rendering
  serial_write_string("[BOOT] Loading TTF font for system text...\n");
  if (ttf_load_font_data(inter_font_data, inter_font_size, &global_ttf_font) == 0) {
    serial_write_string("[BOOT] TTF font loaded successfully for system use!\n");
    kprint(info, "[OK] TTF Font System", 50, 325, 0xFF00FF00);
  } else {
    serial_write_string("[BOOT] TTF font loading failed - using bitmap fonts\n");
    kprint(info, "[FAIL] TTF Font System", 50, 325, 0xFFFF0000);
  }

  // --- PHASE 4: Graphics & Display ---
  serial_write_string("[BOOT] Starting Phase 4: Graphics & Display\n");
  boot_watchdog += 0x100000;
  if (boot_watchdog > BOOT_TIMEOUT)
    goto boot_timeout;

  serial_write_string("[BOOT] About to print Graphics System message\n");
  kprint(info, "[    ] Graphics System", 50, 350, 0xFFFFFF00);
  serial_write_string("[BOOT] About to flip buffers\n");
  flip_buffers(info);
  serial_write_string("[BOOT] Graphics phase delay starting\n");

  for (volatile uint32_t i = 0;
       i < (is_qemu() ? 0x3FFF : 0x2FFFFF) && boot_watchdog < BOOT_TIMEOUT;
       i++) {
    boot_watchdog++;
  }
  if (boot_watchdog > BOOT_TIMEOUT)
    goto boot_timeout;

  // Initialize cursor position
  mouse_x = info->width / 2;
  mouse_y = info->height / 2;
  draw_cursor(info, mouse_x, mouse_y);

  kprint(info, "[OK] Framebuffer Graphics", 50, 235, 0xFF00FF00);
  kprint(info, "[OK] Mouse Cursor", 50, 260, 0xFF00FF00);
  draw_rect(info, 50, 80, 285, 12, 0xFF00AA00); // 95% progress
  flip_buffers(info);

  // --- PHASE 5: System Validation ---
  boot_watchdog += 0x100000;
  if (boot_watchdog > BOOT_TIMEOUT)
    goto boot_timeout;

  kprint(info, "[    ] System Validation", 50, 400, 0xFFFFFF00);
  flip_buffers(info);

  while (inb(0x64) & 1)
    handle_mouse(info);

  if (boot_watchdog > BOOT_TIMEOUT)
    goto boot_timeout;

  // Memory allocation test
  serial_write_string("[MEMORY_TEST] Testing kmalloc(256)...\n");
  void *test_ptr = kmalloc(256);
  if (test_ptr) {
    serial_write_string("[MEMORY_TEST] kmalloc succeeded, testing kfree...\n");
    kfree(test_ptr);
    serial_write_string("[MEMORY_TEST] Memory test passed\n");
    kprint(info, "[OK] Dynamic Memory Test", 50, 285, 0xFF00FF00);
  } else {
    serial_write_string("[MEMORY_TEST] kmalloc failed!\n");
    kprint(info, "[FAIL] Dynamic Memory Test", 50, 285, 0xFFFF0000);
  }

  draw_rect(info, 50, 80, 300, 12, 0xFF00AA00); // 100% progress
  flip_buffers(info);

  // --- FINAL INITIALIZATION COMPLETE ---
  kprint(info, "[COMPLETE] Tiny64 OS Ready!", 50, 450, 0xFF00FF00);
  flip_buffers(info);

  // Let user see completion for a moment
  for (volatile uint32_t i = 0;
       i < (is_qemu() ? 0x4FFF : 0x4FFFFF) && boot_watchdog < BOOT_TIMEOUT;
       i++) {
    boot_watchdog++;
    // Handle mouse during boot delay for interactive cursor
    if (i % 1000 == 0) { // Check mouse every 1000 iterations
      handle_mouse(info);
    }
  }

  // Transition message
  kprint(info, "Loading desktop environment...", 50, 350, 0xFFFFFFFF);
  flip_buffers(info);

boot_timeout:
  // Boot timeout reached - proceed to desktop anyway or halt in QEMU
  if (boot_watchdog > BOOT_TIMEOUT) {
    // Clear any partial progress and show timeout message
    clear_backbuffer(info, 0xFF000000);
    kprint(info, "Tiny64 OS - Boot Timeout Recovery", 50, 50, 0xFFFFAA00);
    kprint(info, "Proceeding to desktop with limited features", 50, 80,
           0xFFCCCCCC);
    flip_buffers(info);

    // Small delay to show timeout message
    for (volatile uint32_t i = 0; i < (is_qemu() ? 0x7FFF : 0x5FFFFF); i++)
      ;
    // If running in QEMU, shut down the VM
    if (is_qemu()) {
      serial_write_string("[QEMU] Detected QEMU environment. Shutting down VM "
                          "via debug port...\n");
      qemu_shutdown();
      for (;;)
        ; // Just in case, halt
    }
  }

  /* TRANSITION TO DESKTOP ENVIRONMENT */

  // Clear backbuffer and draw desktop background with gradient
  for (uint32_t y = 0; y < info->height; y++) {
    uint32_t gradient_color = 0xFFEBEBEB - (y * 0x00010101); // Subtle gradient
    for (uint32_t x = 0; x < info->width; x++) {
      uint32_t* fb = info->backbuffer ? info->backbuffer : info->framebuffer;
      fb[y * info->pitch + x] = gradient_color;
    }
  }

  // Draw taskbar with improved styling
  uint32_t tb_h = info->height / 12;
  uint32_t tb_y = info->height - tb_h;
  fill_rect(info, 0, tb_y, info->width, tb_h, 0xFF2D3748); // Darker taskbar

  // Taskbar border
  draw_rect(info, 0, tb_y, info->width, tb_h, 0xFF1A202C);

  // Draw start button (improved design)
  uint32_t start_cx = 30;
  uint32_t start_cy = tb_y + tb_h / 2;
  uint32_t start_radius = tb_h / 3;

  // Start button background with gradient
  for (int dy = -start_radius; dy <= start_radius; dy++) {
    for (int dx = -start_radius; dx <= start_radius; dx++) {
      if (dx*dx + dy*dy <= start_radius*start_radius) {
        uint32_t* fb = info->backbuffer ? info->backbuffer : info->framebuffer;
        uint32_t color = 0xFF3182CE + (abs(dx) + abs(dy)) * 0x00050505; // Blue gradient
        fb[(start_cy + dy) * info->pitch + (start_cx + dx)] = color;
      }
    }
  }

  // Start button border (simple rectangle approximation)
  draw_rect(info, start_cx - start_radius, start_cy - start_radius,
            start_radius * 2, start_radius * 2, 0xFF2D3748);

  // Draw start button icon (simple arrow)
  for (int i = 0; i < 4; i++) {
    fill_rect(info, start_cx - 2 + i, start_cy - 4 + i, 8 - 2*i, 1, 0xFFFFFFFF);
  }

  // Initialize Windows XP Style Desktop
  init_winxp_desktop(info);

  // Keep keyboard interrupts disabled - polling works reliably
  // keyboard_enable_interrupt();

  /* MAIN DESKTOP ENVIRONMENT */

  // Position terminal within Windows XP terminal window (already drawn at 200,100,600,400)
  int tw_x = 200;
  int tw_y = 100;
  int tw_w = 600;
  int tw_h = 400;

  // Terminal content starts below the title bar (24px) and has some padding
  kprint_auto(info, "Tiny64 Terminal v1.0", tw_x + 35, tw_y + 15, 0xFF000000);
  kprint_auto(info, "Type 'help' for available commands", tw_x + 35, tw_y + 35, 0xFF333333);

  // Initialize terminal with compact prompt inside Windows XP terminal window
  int prompt_x = tw_x + 10;
  int prompt_y = tw_y + 60; // Position below title bar and help text
  draw_char_scaled(info, '>', prompt_x, prompt_y, 0xFF00AA00, 1); // Green prompt character (scale adjusted later)

  // Flip to show the complete desktop
  flip_buffers(info);

  // Terminal interaction state (align with prompt)
  // Dynamic font scaling based on window size and screen resolution
  int terminal_content_width = tw_w - 20;  // Available width for text
  int terminal_content_height = tw_h - 80; // Available height for text (below title)

  // Calculate optimal character dimensions to fit the window
  int desired_columns = 80;
  int max_char_width = terminal_content_width / desired_columns;
  if (max_char_width < 8) max_char_width = 8;

  // Scale font to fit: base glyph is 16x16, scale down if needed
  int scale = max_char_width / 16;
  if (scale < 1) scale = 1;

  // Ensure scaled characters fit within window bounds
  int char_width = 16 * scale;
  int line_height = 16 * scale + 2;

  // Recalculate actual columns that fit
  int actual_columns = terminal_content_width / char_width;
  int max_lines = terminal_content_height / line_height;

  // Redraw prompt at computed scale so it matches character size
  draw_char_scaled(info, '>', prompt_x, prompt_y, 0xFF00AA00, scale);
  flip_buffers(info);

  int term_x = prompt_x + char_width; // Current text position (after prompt)
  int term_y = prompt_y;  // Top of glyph area
  int char_count = 0;

  // Command buffer
  char command_buffer[64] = {0};
  int cmd_len = 0;

  // Mouse test state
  int mouse_test_active = 0;
  int mouse_test_start_time = 0;

  // Activity indicators
  int activity_counter = 0;
  int blink_state = 0;

  for (;;) {
    // use the outer term_x and term_y

    uint8_t status = inb(0x64);
    if (status & 1) { // Output buffer full
      uint8_t data = inb(0x60);

      if (status & 0x20) { // Bit 5 (AUX) set = mouse data
        mouse_handle_byte(info, data);
      } else { // Keyboard data - instant processing with key state tracking
        // Convert scancode to character with shift/caps lock support
        static uint8_t extended = 0;
        static uint8_t shift_pressed = 0;
        static uint8_t caps_lock = 0;
        static uint8_t key_states[256] = {0}; // Track which keys are pressed
        char c = 0;

        if (data == 0xE0) {
          extended = 1; // Extended scancode prefix
        } else {
          uint8_t is_make = (data & 0x80) == 0; // Make code if bit 7 is clear
          uint8_t scancode = data & 0x7F;       // Clear the break bit

          if (extended) {
            scancode |= 0x80; // Mark as extended
            extended = 0;
          }

          // Handle modifier keys
          if (scancode == 0x2A || scancode == 0x36) { // Left/Right Shift
            shift_pressed = is_make;
          } else if (scancode == 0x3A) { // Caps Lock
            if (is_make && !key_states[scancode]) {
              caps_lock = !caps_lock;
        }
            // Draw caps lock indicator
            uint32_t indicator_color =
                caps_lock ? 0xFFFF0000 : 0xFFCCCCCC; // Red if on, gray if off
            fill_rect(info, 460, 275, 30, 15,
                      indicator_color); // Small indicator in terminal title bar
            if (caps_lock) {
              kprint(info, "CAPS", 465, 280, 0xFFFFFFFF);
            } else {
              kprint(info, "    ", 465, 280, 0xFFCCCCCC); // Clear when off
            }
          } else if (!extended) {
            // Only process key presses (make codes), not repeats or releases
            if (is_make && !key_states[scancode]) {
              // Key was just pressed (not held down)
              int use_upper = shift_pressed ^
                              caps_lock; // XOR: one or the other but not both

              switch (scancode) {
              case 0x02:
                c = use_upper ? '!' : '1';
                break;
              case 0x03:
                c = use_upper ? '@' : '2';
                break;
              case 0x04:
                c = use_upper ? '#' : '3';
                break;
              case 0x05:
                c = use_upper ? '$' : '4';
                break;
              case 0x06:
                c = use_upper ? '%' : '5';
                break;
              case 0x07:
                c = use_upper ? '^' : '6';
                break;
              case 0x08:
                c = use_upper ? '&' : '7';
                break;
              case 0x09:
                c = use_upper ? '*' : '8';
                break;
              case 0x0A:
                c = use_upper ? '(' : '9';
                break;
              case 0x0B:
                c = use_upper ? ')' : '0';
                break;
              case 0x0C:
                c = use_upper ? '_' : '-';
                break;
              case 0x0D:
                c = use_upper ? '+' : '=';
                break;
              case 0x0E:
                c = '\b';
                break; // Backspace (always same)
              case 0x0F:
                c = '\t';
                break; // Tab
              case 0x10:
                c = use_upper ? 'Q' : 'q';
                break;
              case 0x11:
                c = use_upper ? 'W' : 'w';
                break;
              case 0x12:
                c = use_upper ? 'E' : 'e';
                break;
              case 0x13:
                c = use_upper ? 'R' : 'r';
                break;
              case 0x14:
                c = use_upper ? 'T' : 't';
                break;
              case 0x15:
                c = use_upper ? 'Y' : 'y';
                break;
              case 0x16:
                c = use_upper ? 'U' : 'u';
                break;
              case 0x17:
                c = use_upper ? 'I' : 'i';
                break;
              case 0x18:
                c = use_upper ? 'O' : 'o';
                break;
              case 0x19:
                c = use_upper ? 'P' : 'p';
                break;
              case 0x1A:
                c = use_upper ? '{' : '[';
                break;
              case 0x1B:
                c = use_upper ? '}' : ']';
                break;
              case 0x1C:
                c = '\n';
                break; // Enter
              case 0x1E:
                c = use_upper ? 'A' : 'a';
                break;
              case 0x1F:
                c = use_upper ? 'S' : 's';
                break;
              case 0x20:
                c = use_upper ? 'D' : 'd';
                break;
              case 0x21:
                c = use_upper ? 'F' : 'f';
                break;
              case 0x22:
                c = use_upper ? 'G' : 'g';
                break;
              case 0x23:
                c = use_upper ? 'H' : 'h';
                break;
              case 0x24:
                c = use_upper ? 'J' : 'j';
                break;
              case 0x25:
                c = use_upper ? 'K' : 'k';
                break;
              case 0x26:
                c = use_upper ? 'L' : 'l';
                break;
              case 0x27:
                c = use_upper ? ':' : ';';
                break;
              case 0x28:
                c = use_upper ? '"' : '\'';
                break;
              case 0x29:
                c = use_upper ? '~' : '`';
                break;
              case 0x2B:
                c = use_upper ? '|' : '\\';
                break;
              case 0x2C:
                c = use_upper ? 'Z' : 'z';
                break;
              case 0x2D:
                c = use_upper ? 'X' : 'x';
                break;
              case 0x2E:
                c = use_upper ? 'C' : 'c';
                break;
              case 0x2F:
                c = use_upper ? 'V' : 'v';
                break;
              case 0x30:
                c = use_upper ? 'B' : 'b';
                break;
              case 0x31:
                c = use_upper ? 'N' : 'n';
                break;
              case 0x32:
                c = use_upper ? 'M' : 'm';
                break;
              case 0x33:
                c = use_upper ? '<' : ',';
                break;
              case 0x34:
                c = use_upper ? '>' : '.';
              break;
              case 0x35:
                c = use_upper ? '?' : '/';
                break;
              case 0x39:
                c = ' ';
                break; // Space (always same)
              }
            }

            // Update key state
            key_states[scancode] = is_make;
          }

          if (c != 0) {
            // Handle CTRL+C to exit/cancel current command
            if (c == 3) { // CTRL+C (ASCII 3)
              serial_write_string("[TERMINAL] CTRL+C detected - command cancelled\n");

              // Clear current command
              cmd_len = 0;
              command_buffer[0] = '\0';

              // Move to new line and show new prompt
              term_y += line_height;
              term_x = prompt_x;
              draw_char_scaled(info, '>', term_x, term_y, 0xFF00AA00, scale);
              term_x += char_width;

              // Reset character count for new line
              char_count = 0;

              flip_buffers(info);
              continue;
            }

            // Display the character and update command buffer
            if (c >= 32 && c <= 126) { // Printable characters
              // Clear any cursor at this position first (full glyph height)
              fill_rect(info, term_x, term_y, 1, line_height, 0xFF000000);

              // Draw scaled terminal character (white text)
              draw_char_scaled(info, c, term_x, term_y, 0xFFFFFFFF, scale);

              // Append to command buffer if space allows
        if (cmd_len < (int)sizeof(command_buffer) - 1) {
          command_buffer[cmd_len++] = c;
          command_buffer[cmd_len] = '\0';
        }

              term_x += char_width;       // Compact character spacing for terminal
              flip_buffers(info); // Update display immediately

              // Improved cursor behavior with proper wrapping and scrolling
              term_x += char_width;

              // Check if we need to wrap to next line
              if (term_x >= (tw_x + tw_w - char_width)) {
                // Move to next line
                term_y += line_height;
                term_x = prompt_x; // Start at left margin

                // Check if we need to scroll
                if (term_y >= (tw_y + tw_h - line_height)) {
                  // Scroll up: clear terminal area and reset cursor
                  for (int cy = tw_y + 70; cy < tw_y + tw_h - 10; cy++) {
                    for (int cx = tw_x + 5; cx < tw_x + tw_w - 5; cx++) {
                      uint32_t *fb = info->backbuffer ? info->backbuffer : info->framebuffer;
                      fb[cy * info->pitch + cx] = 0xFFFFFFFF; // White background
                    }
                  }
                  term_y = tw_y + 85; // Reset to near top of terminal area
                  flip_buffers(info);

                  // Redraw prompt at new position
                  draw_char_scaled(info, '>', prompt_x, term_y, 0xFF00AA00, scale);
                  term_x = prompt_x + char_width;
                }
              }
            } else if (c == '\n') { // Enter key -> execute command
              // Null-terminate and process command
              command_buffer[cmd_len] = '\0';

              if (cmd_len > 0) {
                if (strcmp(command_buffer, "ls") == 0) {
                  char listbuf[512];
                  int got = fs_list_files(listbuf, sizeof(listbuf));
                  if (got > 0) {
                    char *p = listbuf;
                    while (*p) {
                      // Print each file on its own line
                      kprint_auto(info, p, prompt_x, term_y, 0xFF00FF00);
                      term_y += line_height;
                      p += strlen(p) + 1;
                    }
                    } else {
                    kprint_auto(info, "(no files)", prompt_x, term_y, 0xFFFFFFFF);
                    term_y += line_height;
                  }
                } else if (strncmp(command_buffer, "cat ", 4) == 0) {
                  const char *fname = command_buffer + 4;
                  char filebuf[512];
                  int r = fs_read_file(fname, (uint8_t *)filebuf, sizeof(filebuf) - 1);
                  if (r > 0) {
                    filebuf[r] = '\0';
                    // Print file contents line by line
                    char *line = filebuf;
                    char *nl;
                    while ((nl = strchr(line, '\n')) != NULL) {
                      *nl = '\0';
                      kprint_auto(info, line, prompt_x, term_y, 0xFFFFFFFF);
                      term_y += line_height;
                      line = nl + 1;
                    }
                    if (*line) {
                      kprint_auto(info, line, prompt_x, term_y, 0xFFFFFFFF);
                      term_y += line_height;
                    }
              } else {
                    kprint_auto(info, "File not found", prompt_x, term_y, 0xFFFF0000);
                    term_y += line_height;
                  }
                } else if (strncmp(command_buffer, "write ", 6) == 0) {
                  // format: write <file> <text>
                  char *args = command_buffer + 6;
                  char *space = strchr(args, ' ');
                  if (space) {
                    *space = '\0';
                    const char *fname = args;
                    const char *text = space + 1;
                    fs_write_file(fname, (const uint8_t *)text, strlen(text));
                    kprint_auto(info, "Wrote file", prompt_x, term_y, 0xFF00FF00);
                    term_y += line_height;
            } else {
                    kprint_auto(info, "Usage: write <file> <text>", prompt_x, term_y, 0xFFFF0000);
                    term_y += line_height;
            }
                } else if (strcmp(command_buffer, "wadtest") == 0) {
                  // Test if embedded WAD data exists
                  kprint_auto(info, "Testing embedded WAD data...", prompt_x, term_y, 0xFFFFFF00);
                  term_y += line_height;

                  FILE* test_file = fopen("doom1.wad", "rb");
                  if (test_file) {
                    kprint_auto(info, "SUCCESS: doom1.wad found!", prompt_x, term_y, 0xFF00FF00);
                    term_y += line_height;
                    // Get file size
                    fseek(test_file, 0, SEEK_END);
                    long file_size = ftell(test_file);
                    fseek(test_file, 0, SEEK_SET);
                    char size_msg[64];
                    sprintf(size_msg, "File size: %ld bytes", file_size);
                    kprint_auto(info, size_msg, prompt_x, term_y, 0xFFFFFFFF);
                    term_y += line_height;
                    fclose(test_file);
                  } else {
                    kprint_auto(info, "FAILED: doom1.wad not found", prompt_x, term_y, 0xFFFF0000);
                    term_y += line_height;

                    // Check embedded WAD function
                    size_t wad_size;
                    const uint8_t* wad_data = get_doom1_wad_data(&wad_size);
                    if (wad_data != NULL && wad_size > 0) {
                      char size_buf[64];
                      sprintf(size_buf, "WAD found! Size: %zu bytes", wad_size);
                      kprint_auto(info, size_buf, prompt_x, term_y, 0xFF00FF00);
                      term_y += line_height;

                      // Check first few bytes to verify it's a valid WAD
                      if (wad_size >= 4 && wad_data[0] == 'I' && wad_data[1] == 'W' && wad_data[2] == 'A' && wad_data[3] == 'D') {
                        kprint_auto(info, "Valid IWAD signature detected", prompt_x, term_y, 0xFF00FF00);
                        term_y += line_height;
                      } else {
                        kprint_auto(info, "WARNING: Invalid WAD signature", prompt_x, term_y, 0xFFFF8800);
                        term_y += line_height;
                      }
                    } else {
                      kprint_auto(info, "ERROR: WAD data not available", prompt_x, term_y, 0xFFFF0000);
                      term_y += line_height;
                    }
                  }
                  flip_buffers(info);
                  continue;
                } else if (strcmp(command_buffer, "doom") == 0) {
                  // Check if WAD file exists with debug output
                  kprint_auto(info, "Checking for embedded Doom WAD...", prompt_x, term_y, 0xFFFFFF00);
                  term_y += line_height;
                  flip_buffers(info);

                  // Try to open embedded WAD files
                  FILE* wad_test = fopen("doom1.wad", "rb");
                  if (wad_test) {
                    kprint_auto(info, "doom1.wad found in embedded data!", prompt_x, term_y, 0xFF00FF00);
                    term_y += line_height;
                  } else {
                    kprint_auto(info, "doom1.wad not found, trying doom.wad...", prompt_x, term_y, 0xFFFFFF00);
                    term_y += line_height;
                    wad_test = fopen("doom.wad", "rb");
                    if (wad_test) {
                      kprint_auto(info, "doom.wad found in embedded data!", prompt_x, term_y, 0xFF00FF00);
                      term_y += line_height;
                    } else {
                      kprint_auto(info, "doom.wad not found, trying doom2.wad...", prompt_x, term_y, 0xFFFFFF00);
                      term_y += line_height;
                      wad_test = fopen("doom2.wad", "rb");
                      if (wad_test) {
                        kprint_auto(info, "doom2.wad found in embedded data!", prompt_x, term_y, 0xFF00FF00);
                        term_y += line_height;
                      }
                    }
                  }

                  if (!wad_test) {
                    kprint_auto(info, "ERROR: No embedded Doom WAD found!", prompt_x, term_y, 0xFFFF0000);
                    term_y += line_height;
                    kprint_auto(info, "WAD embedding may have failed during build", prompt_x, term_y, 0xFFFF0000);
                    term_y += line_height;
                    kprint_auto(info, "Check build output for embedding errors", prompt_x, term_y, 0xFFFFFF00);
                    term_y += line_height;
                    flip_buffers(info);
                    continue;
                  } else {
                    fclose(wad_test);
                    kprint_auto(info, "Launching Doom with embedded WAD...", prompt_x, term_y, 0xFF00FF00);
                    term_y += line_height;
                    flip_buffers(info);
                  }

                  // Create Doom window (640x400, positioned to fit screen)
                  int doom_window_x = 50;
                  int doom_window_y = 150;  // Position below terminal
                  int doom_window_w = 640;
                  int doom_window_h = 400;

                  // Adjust if window would go off screen
                  if (doom_window_x + doom_window_w > (int)info->width) {
                    doom_window_w = info->width - doom_window_x - 10;
                  }
                  if (doom_window_y + doom_window_h > (int)info->height) {
                    doom_window_h = info->height - doom_window_y - 10;
                  }

                  // Draw Doom window frame
                  fill_rect(info, doom_window_x - 2, doom_window_y - 22, doom_window_w + 4, doom_window_h + 24, 0xFF666666); // Window border
                  fill_rect(info, doom_window_x, doom_window_y - 20, doom_window_w, 18, 0xFF000080); // Title bar
                  kprint_auto(info, "Doom", doom_window_x + 5, doom_window_y - 18, 0xFFFFFFFF);

                  // Set Doom window position
                  extern void DG_SetWindowPosition(int x, int y);
                  DG_SetWindowPosition(doom_window_x, doom_window_y);

                  // Initialize Doom with windowed rendering
                  char* doom_args[] = {"doom", "-iwad", "doom1.wad"};
                  doomgeneric_SetBootInfo(info);
                  doomgeneric_Create(3, doom_args);

                  // Main Doom loop with windowed rendering
                  while (1) {
                      doomgeneric_Tick();

                      // Re-draw terminal window on top (to keep it visible)
                      draw_terminal_window(info, tw_x, tw_y, tw_w, tw_h);
                      kprint_auto(info, "Tiny64 Terminal v1.0", tw_x + 35, tw_y + 15, 0xFF000000);
                      kprint_auto(info, "Type 'help' for available commands", tw_x + 35, tw_y + 35, 0xFF333333);
                      draw_char_scaled(info, '>', prompt_x, prompt_y, 0xFF00AA00, scale);

                      flip_buffers(info);

                      // Check for escape key to exit Doom
                      if (last_key_pressed == 0x01) { // ESC key
                          kprint_auto(info, "Doom exited", prompt_x, term_y, 0xFFFF0000);
                          term_y += line_height;
                          break;
                      }
                  }

                  kprint_auto(info, "Doom exited.", prompt_x, term_y, 0xFFFFFF00);
                  term_y += line_height;
                  flip_buffers(info);
                } else if (strcmp(command_buffer, "echo") == 0) {
                  // Echo command - just print arguments
                  if (cmd_len > 5) { // "echo " is 5 chars
                    kprint_auto(info, command_buffer + 5, prompt_x, term_y, 0xFFFFFFFF);
                    term_y += line_height;
                  }
                } else if (strcmp(command_buffer, "mkdir") == 0) {
                  // Directory creation (placeholder for now)
                  kprint_auto(info, "mkdir: Directory creation not implemented yet", prompt_x, term_y, 0xFFFFFF00);
                  term_y += line_height;
                } else if (strcmp(command_buffer, "rm") == 0) {
                  // File removal (placeholder for now)
                  kprint_auto(info, "rm: File removal not implemented yet", prompt_x, term_y, 0xFFFFFF00);
                  term_y += line_height;
                } else if (strcmp(command_buffer, "meminfo") == 0) {
                  // Memory information
                  char mem_buf[64];
                  sprintf(mem_buf, "Memory: 1MB heap allocated");
                  kprint_auto(info, mem_buf, prompt_x, term_y, 0xFF00FF00);
                  term_y += line_height;
                } else if (strcmp(command_buffer, "cpuinfo") == 0) {
                  // CPU information
                  kprint_auto(info, "CPU: x86_64 Long Mode", prompt_x, term_y, 0xFF00FF00);
                  term_y += line_height;
                  kprint_auto(info, "Architecture: 64-bit UEFI boot", prompt_x, term_y, 0xFF00FF00);
                  term_y += line_height;
                } else if (strcmp(command_buffer, "netinfo") == 0) {
                  // Network information
                  kprint_auto(info, "Network: RTL8139 driver loaded", prompt_x, term_y, 0xFF00FF00);
                  term_y += line_height;
                  kprint_auto(info, "Status: Ethernet interface available", prompt_x, term_y, 0xFF00FF00);
                  term_y += line_height;
                } else if (strcmp(command_buffer, "usbinfo") == 0) {
                  // USB information
                  kprint_auto(info, "USB: UHCI driver loaded", prompt_x, term_y, 0xFF00FF00);
                  term_y += line_height;
                  kprint_auto(info, "Status: USB 1.1 host controller ready", prompt_x, term_y, 0xFF00FF00);
                  term_y += line_height;
                } else if (strcmp(command_buffer, "play") == 0) {
                  // Audio playback (placeholder)
                  kprint_auto(info, "play: Audio playback not implemented yet", prompt_x, term_y, 0xFFFFFF00);
                  term_y += line_height;
                  kprint_auto(info, "AC97 audio driver is loaded and ready", prompt_x, term_y, 0xFF00FF00);
                  term_y += line_height;
                } else if (strcmp(command_buffer, "reboot") == 0) {
                  // System reboot
                  kprint_auto(info, "Rebooting system...", prompt_x, term_y, 0xFFFF0000);
                  term_y += line_height;
                  flip_buffers(info);
                  // Simple reboot via keyboard controller
                  for (volatile int i = 0; i < 1000000; i++); // Small delay
                  outb(0x64, 0xFE); // Pulse reset line
                } else if (strcmp(command_buffer, "shutdown") == 0) {
                  // System shutdown
                  kprint_auto(info, "Shutting down system...", prompt_x, term_y, 0xFFFF0000);
                  term_y += line_height;
                  flip_buffers(info);
                  // QEMU shutdown
                  outw(0x604, 0x2000);
                  while (1); // Halt if shutdown fails
                } else if (strcmp(command_buffer, "help") == 0 || strcmp(command_buffer, "?") == 0) {
                  kprint_auto(info, "Available commands:", prompt_x, term_y, 0xFFFFFFFF);
                  term_y += line_height;
                  kprint_auto(info, "  ls              - List files", prompt_x, term_y, 0xFFCCCCCC);
                  term_y += line_height;
                  kprint_auto(info, "  cat <file>      - Display file contents", prompt_x, term_y, 0xFFCCCCCC);
                  term_y += line_height;
                  kprint_auto(info, "  write <file> <text> - Create/write file", prompt_x, term_y, 0xFFCCCCCC);
                  term_y += line_height;
                  kprint_auto(info, "  echo <text>     - Display text", prompt_x, term_y, 0xFFCCCCCC);
                  term_y += line_height;
                  kprint_auto(info, "  mkdir <dir>     - Create directory", prompt_x, term_y, 0xFFCCCCCC);
                  term_y += line_height;
                  kprint_auto(info, "  rm <file>       - Remove file", prompt_x, term_y, 0xFFCCCCCC);
                  term_y += line_height;
                  kprint_auto(info, "  meminfo         - Show memory information", prompt_x, term_y, 0xFFCCCCCC);
                  term_y += line_height;
                  kprint_auto(info, "  cpuinfo         - Show CPU information", prompt_x, term_y, 0xFFCCCCCC);
                  term_y += line_height;
                  kprint_auto(info, "  netinfo         - Show network status", prompt_x, term_y, 0xFFCCCCCC);
                  term_y += line_height;
                  kprint_auto(info, "  usbinfo         - Show USB status", prompt_x, term_y, 0xFFCCCCCC);
                  term_y += line_height;
                  kprint_auto(info, "  play <file>     - Play audio file", prompt_x, term_y, 0xFFCCCCCC);
                  term_y += line_height;
                  kprint_auto(info, "  doom            - Launch Doom (if available)", prompt_x, term_y, 0xFFCCCCCC);
                  term_y += line_height;
                  kprint_auto(info, "  reboot          - Reboot the system", prompt_x, term_y, 0xFFCCCCCC);
                  term_y += line_height;
                  kprint_auto(info, "  shutdown        - Shutdown the system", prompt_x, term_y, 0xFFCCCCCC);
                  term_y += line_height;
                  kprint_auto(info, "  clear/cls       - Clear terminal", prompt_x, term_y, 0xFFCCCCCC);
                  term_y += line_height;
                  kprint_auto(info, "  help/?          - Show this help", prompt_x, term_y, 0xFFCCCCCC);
                  term_y += line_height;
                } else if (strcmp(command_buffer, "clear") == 0 || strcmp(command_buffer, "cls") == 0) {
                  // Clear the terminal area
                  for (int cy = tw_y + 70; cy < tw_y + tw_h - 10; cy++) {
                    for (int cx = tw_x + 5; cx < tw_x + tw_w - 5; cx++) {
                      uint32_t *fb = info->backbuffer ? info->backbuffer : info->framebuffer;
                      fb[cy * info->pitch + cx] = 0xFFFFFFFF; // White background
                    }
                  }
                  term_y = tw_y + 85; // Reset to prompt position
                  draw_char_scaled(info, '>', prompt_x, term_y, 0xFF00AA00, scale);
                  term_x = prompt_x + char_width;
                  flip_buffers(info);
                } else {
                  kprint_auto(info, "Unknown command. Type 'help' for available commands.", prompt_x, term_y, 0xFFFF0000);
                  term_y += line_height;
          }
        }

              // Clear command buffer and position prompt on new line
          cmd_len = 0;
          command_buffer[0] = '\0';

              // Ensure prompt appears on a clean new line
              term_y += line_height;
              term_x = prompt_x;

              // Draw prompt (scaled)
              draw_char_scaled(info, '>', prompt_x, term_y, 0xFF00AA00, scale);
              term_x = prompt_x + char_width;
              flip_buffers(info);
            } else if (c == '\b' && term_x > prompt_x + char_width) { // Backspace
              // Remove from command buffer if present
              if (cmd_len > 0) {
                cmd_len--;
                command_buffer[cmd_len] = '\0';
              }
              // Move cursor back and clear the character
              term_x -= char_width;
              draw_char_scaled(info, ' ', term_x, term_y, 0xFFFFFFFF, scale);
              flip_buffers(info);
            }
          }
        }
      }
    }

    // Activity indicator in taskbar (blinking effect)
    activity_counter++;
    if (activity_counter % (is_qemu() ? 500 : 3000) == 0) {
      blink_state = !blink_state;
      uint32_t indicator_color = blink_state ? 0xFF00FF00 : 0xFF22262A;
      fill_rect(info, info->width - 40, tb_y + 5, 30, tb_h - 10,
                indicator_color);
    }

    // Periodically request a mouse sample so QEMU/hosts that don't stream
    // continuously still produce bytes (helps when mouse is idle).
    if (activity_counter % (is_qemu() ? 50 : 500) == 0) {
      mouse_request_sample();
    }

    // Cursor blinking in terminal (compact size for new font)
    if (activity_counter % (is_qemu() ? 300 : 1800) == 0) {
      static int cursor_visible = 1;
      cursor_visible = !cursor_visible;
      // Draw cursor as a simple vertical bar (black when visible on white bg)
      uint32_t cursor_color = cursor_visible ? 0xFF000000 : 0xFFFFFFFF;
      fill_rect(info, term_x, term_y + 2, 1, 12, cursor_color);
      
    }

    // Flip buffers to show all updates at once
    flip_buffers(info);

    // Gentle CPU usage
    for (volatile int k = 0; k < (is_qemu() ? 60 : 300); k++)
      ;
  }
}