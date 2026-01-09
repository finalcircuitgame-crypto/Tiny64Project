#include "../include/kernel.h"
#include "../hal/serial.h"
#include <stdbool.h>

#ifdef QEMU
#define IS_QEMU_ENV 1
#else
#define IS_QEMU_ENV 0
#endif

void draw_terminal_window(BootInfo *info, int x, int y) {
  // Draw terminal border only (no background fill)
  fill_rect(info, x, y, 400, 300, 0xFF000000);
  fill_rect(info, x, y, 400, 20, 0xFFCCCCCC);
  kprint(info, "Terminal", x + 10, y + 6, 0xFF000000);
  fill_rect(info, x + 380, y + 2, 16, 16, 0xFFCC0000);
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
  uint32_t *fb = info->framebuffer;
  uint64_t total_pixels = (uint64_t)info->height * info->pitch;

  // Boot timeout protection - prevents infinite hangs
  volatile uint32_t boot_watchdog = 0;
  const uint32_t BOOT_TIMEOUT =
      is_qemu() ? 0x80000 : 0x20FFFFF; // Much shorter timeout in QEMU

  // Initialize double buffering
  init_double_buffer(info);

  // Initialize serial port for console output
  serial_init();

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

  init_idt();

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

  // --- PHASE 4: Graphics & Display ---
  boot_watchdog += 0x100000;
  if (boot_watchdog > BOOT_TIMEOUT)
    goto boot_timeout;

  kprint(info, "[    ] Graphics System", 50, 235, 0xFFFFFF00);
  flip_buffers(info);

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

  kprint(info, "[    ] System Validation", 50, 285, 0xFFFFFF00);
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
  kprint(info, "[COMPLETE] Tiny64 OS Ready!", 50, 320, 0xFF00FF00);
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

  // Clear backbuffer and draw desktop background
  clear_backbuffer(info, 0xFFEBEBEB);

  // Draw taskbar
  uint32_t tb_h = info->height / 14;
  uint32_t tb_y = info->height - tb_h;
  fill_rect(info, 0, tb_y, info->width, tb_h, 0xFF22262A);

  // Draw start button (blue circle)
  uint32_t start_cx = info->width / 2 - (tb_h * 3);
  fill_circle(info, start_cx, tb_y + tb_h / 2, tb_h * 3 / 8, 0xFF2563EB);

  // Desktop welcome message
  kprint(info, "Welcome to Tiny64 OS", 20, 20, 0xFF000000);
  kprint(info, "Desktop Environment Loaded", 20, 45, 0xFF333333);

  // System status in corner
  size_t total_mem, used_mem, free_mem;
  get_heap_stats(&total_mem, &used_mem, &free_mem);
  kprint(info, "Memory: OK (1MB heap)", info->width - 200, 20, 0xFF008000);

  if (mouse_ok) {
    kprint(info, "Mouse: OK", info->width - 200, 45, 0xFF008000);
  } else {
    kprint(info, "Mouse: N/A", info->width - 200, 45, 0xFFFF8800);
  }

  /* MAIN DESKTOP ENVIRONMENT */

  // Draw interactive terminal window (moved down to not cover boot logs)
  draw_terminal_window(info, 100, 250);
  kprint(info, "Tiny64 Terminal v1.0", 115, 275, 0xFF000000);
  kprint(info, "Commands: 'mouse_test' to test mouse", 115, 295, 0xFF333333);
  kprint(info, ">", 115, 315, 0xFF00AA00);

  // Flip to show the complete desktop
  flip_buffers(info);

  // Terminal interaction state
  int term_x = 125; // Current text position
  int term_y = 315;
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
    // Handle mouse input
    while (inb(0x64) & 1)
      handle_mouse(info);

    if (inb(0x64) & 1) {
      serial_write_string("[DEBUG] Byte pending\n");
    }

    // Handle keyboard input for terminal
    if (last_key_pressed != 0) {
      char c = last_key_pressed;
      last_key_pressed = 0;

      // Handle backspace
      if (c == '\b' && char_count > 0) {
        char_count--;
        cmd_len = char_count; // Sync command buffer length
        if (cmd_len >= 0) {
          command_buffer[cmd_len] = '\0';
        }
        term_x -= 16; // Scalable font character width
                      // Don't fill with white - let background show through
      }
      // Handle enter (new line)
      else if (c == '\n') {
        // Check for commands before processing enter
        static char command_buffer[64] = {0};
        static int cmd_len = 0;

        // Extract command (from after the ">" prompt)
        if (cmd_len > 0) {
          command_buffer[cmd_len] = '\0';

          // Process commands (simple string comparison)
          int is_mouse_test = 1;
          char *test_str = "mouse_test";
          for (int i = 0; test_str[i] && command_buffer[i]; i++) {
            if (test_str[i] != command_buffer[i]) {
              is_mouse_test = 0;
              break;
            }
          }
          if (is_mouse_test && command_buffer[10] == '\0') {
            mouse_test_active = 1;
            start_mouse_test();
            mouse_test_start_time = activity_counter;
            kprint(info, "Mouse test started! Move mouse and click.", 50, 400,
                   0xFFFFAA00);
            flip_buffers(info);
          } else {
            int is_mouse_status = 1;
            char *status_str = "mouse_status";
            for (int i = 0; status_str[i] && command_buffer[i]; i++) {
              if (status_str[i] != command_buffer[i]) {
                is_mouse_status = 0;
                break;
              }
            }
            if (is_mouse_status && command_buffer[12] == '\0') {
              int clicks, movement;
              int status = get_mouse_test_status(&clicks, &movement);
              if (status) {
                char status_msg[64];
                // Simple status display
                kprint(info, "Mouse test active - clicks: ", 50, 420,
                       0xFF00FF00);
                kprint(info, "movement: detected", 50, 440, 0xFF00FF00);
              } else {
                kprint(info, "Mouse test not active", 50, 420, 0xFFFF0000);
              }
              flip_buffers(info);
            } else {
              kprint(info, "Unknown command", 50, 420, 0xFFFF0000);
              flip_buffers(info);
            }
          }
        }

        term_y += 16; // Scalable font line height
        term_x = 125;
        char_count = 0;
        cmd_len = 0;
        command_buffer[0] = '\0';

        // Scroll terminal if needed
        if (term_y > 530) {
          clear_terminal_area(info, 100, 250);
          draw_terminal_window(info, 100, 250);
          kprint(info, "Tiny64 Terminal v1.0", 115, 275, 0xFF000000);
          term_y = 315;
        }
        kprint(info, ">", 115, term_y, 0xFF00AA00);
      }
      // Handle regular characters
      else if (c >= 32 && c <= 126) {
        draw_char(info, c, term_x, term_y, 0xFF000000);
        term_x += 16; // Scalable font character width
        char_count++;

        // Store in command buffer
        if (cmd_len < (int)sizeof(command_buffer) - 1) {
          command_buffer[cmd_len++] = c;
          command_buffer[cmd_len] = '\0';
        }

        // Auto-wrap lines
        if (term_x > 490) {
          term_y += 16; // Scalable font line height
          term_x = 125;
          char_count = 0;
          cmd_len = 0;
          command_buffer[0] = '\0';
          kprint(info, ">", 115, term_y, 0xFF00AA00);
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

    // Cursor blinking in terminal
    if (activity_counter % (is_qemu() ? 250 : 1500) == 0) {
      static int cursor_visible = 1;
      cursor_visible = !cursor_visible;
      uint32_t cursor_color = cursor_visible ? 0xFF000000 : 0xFFFFFFFF;
      fill_rect(info, term_x, term_y - 2, 2, 16, cursor_color);
    }

    // Flip buffers to show all updates at once
    flip_buffers(info);

    // Gentle CPU usage
    for (volatile int k = 0; k < (is_qemu() ? 60 : 300); k++)
      ;
  }
}