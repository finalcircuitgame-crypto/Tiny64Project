#include "../hal/serial.h"
#include "../include/kernel.h"

int mouse_x = 0;
int mouse_y = 0;
uint8_t mouse_left_pressed = 0;

static uint8_t mouse_cycle = 0;
static int8_t mouse_byte[3];
static uint32_t cursor_backbuffer[256];

#define CURSOR_SIZE 8

// Mouse testing state
static int mouse_test_mode = 0;
static int mouse_test_clicks = 0;
static int mouse_test_movement = 0;
static int last_mouse_x = 0;
static int last_mouse_y = 0;

/* Simple I/O wait */

/* Helper: Wait with timeout - type 0 = read, 1 = write */
static int mouse_wait(int type) {
  uint32_t timeout = 100000; // generous timeout
  if (type == 0) {           // Read
    while (timeout--) {
      if (inb(0x64) & 1)
        return 1;
      io_wait();
    }
  } else { // Write
    while (timeout--) {
      if ((inb(0x64) & 2) == 0)
        return 1;
      io_wait();
    }
  }
  return 0; // Timed out
}

static void mouse_write_device(uint8_t data) {
  // Tell controller next byte is for mouse
  mouse_wait(1);
  outb(0x64, 0xD4);

  // Now send the actual byte
  mouse_wait(1);
  outb(0x60, data);
}

static uint8_t mouse_read_device(void) {
  mouse_wait(0);
  return inb(0x60);
}

int mouse_init(void) {
  serial_write_string("[MOUSE_INIT] Starting robust PS/2 mouse initialization...\n");

  __asm__ volatile("cli");

  // Step 1: Disable both keyboard and mouse ports
  serial_write_string("[MOUSE_INIT] Disabling keyboard and mouse ports...\n");

  // Disable keyboard
  mouse_wait(1);
  outb(0x64, 0xAD);
  io_wait();

  // Disable mouse
  mouse_wait(1);
  outb(0x64, 0xA7);
  io_wait();

  // Step 2: Flush output buffer thoroughly
  serial_write_string("[MOUSE_INIT] Flushing output buffer...\n");
  int flush_count = 0;
  for (int i = 0; i < 1000; i++) {
    if (inb(0x64) & 1) {
      inb(0x60);
      flush_count++;
      io_wait();
    } else {
      break;
    }
  }
  char numbuf[4] = "000";
  numbuf[0] = '0' + (flush_count / 100) % 10;
  numbuf[1] = '0' + (flush_count / 10) % 10;
  numbuf[2] = '0' + flush_count % 10;
  serial_write_string("[MOUSE_INIT] Flushed ");
  serial_write_string(numbuf);
  serial_write_string(" bytes\n");

  // Step 3: Configure controller for mouse
  serial_write_string("[MOUSE_INIT] Configuring PS/2 controller...\n");

  mouse_wait(1);
  outb(0x64, 0x20); // Get config byte

  uint8_t config = mouse_read_device();
  config |= (1 << 1); // Enable mouse clock
  config |= (1 << 5); // Enable mouse interrupts

  mouse_wait(1);
  outb(0x64, 0x60); // Set config byte
  mouse_wait(1);
  outb(0x60, config);

  // Step 4: Reset mouse with robust handling
  serial_write_string("[MOUSE_INIT] Resetting mouse...\n");

  int got_reset_fa = 0, got_reset_aa = 0;
  outb(0x64, 0xD4); // Next byte to mouse
  outb(0x60, 0xFF); // Reset command

  // Wait for reset responses: 0xFA, then 0xAA
  for (int tries = 0; tries < 40000; ++tries) {
    if ((inb(0x64) & 1)) {
      uint8_t resp = inb(0x60);
      if (!got_reset_fa && resp == 0xFA) {
        got_reset_fa = 1;
        serial_write_string("[MOUSE_INIT] Got reset ACK (0xFA)\n");
      } else if (got_reset_fa && resp == 0xAA) {
        got_reset_aa = 1;
        serial_write_string("[MOUSE_INIT] Got self-test passed (0xAA)\n");
        break;
      }
    }
    io_wait();
  }

  if (!got_reset_fa || !got_reset_aa) {
    serial_write_string("[MOUSE_INIT] Mouse reset incomplete, continuing anyway...\n");
    // Don't fail here - QEMU and some real hardware don't respond properly
  }

  // Step 5: Enable streaming mode with robust ACK handling
  serial_write_string("[MOUSE_INIT] Enabling streaming mode...\n");

  outb(0x64, 0xD4); // Next byte to mouse
  outb(0x60, 0xF4); // Enable streaming

  uint8_t ack = 0;
  // Wait longer if reset responses were incomplete (likely QEMU)
  int ack_timeout = (!got_reset_fa || !got_reset_aa) ? 5000 : 20000;

  for (int i = 0; i < ack_timeout; i++) {
    if ((inb(0x64) & 1)) {
      ack = inb(0x60);
      break;
    }
    io_wait();
  }

  if (ack == 0xFA) {
    serial_write_string("[MOUSE_INIT] ACK received - streaming enabled!\n");
  } else {
    serial_write_string("[MOUSE_INIT] No ACK received (normal for QEMU), assuming enabled\n");
  }

  // Step 6: Re-enable ports
  serial_write_string("[MOUSE_INIT] Re-enabling keyboard and mouse ports...\n");

  mouse_wait(1);
  outb(0x64, 0xAE); // Re-enable keyboard
  io_wait();

  mouse_wait(1);
  outb(0x64, 0xA8); // Re-enable mouse
  io_wait();

  // Unmask IRQ12 on slave PIC for mouse interrupts
  uint8_t mask = inb(0xA1);
  mask &= ~(1 << 4); // IRQ12 = bit 4
  outb(0xA1, mask);

  // Final flush to ensure clean state
  for (int i = 0; i < 100; i++) {
    if (inb(0x64) & 1) {
      inb(0x60);
    } else {
      break;
    }
    io_wait();
  }

  __asm__ volatile("sti");

  serial_write_string("[MOUSE_INIT] Mouse initialization complete!\n");
  return 1;
}

void restore_cursor_bg(BootInfo *info, int x, int y) {
  uint32_t *fb = info->backbuffer ? info->backbuffer : info->framebuffer;
  for (int cy = 0; cy < CURSOR_SIZE; cy++) {
    if (y + cy >= (int)info->height)
      break;
    for (int cx = 0; cx < CURSOR_SIZE; cx++) {
      if (x + cx >= (int)info->width)
        break;
      fb[(y + cy) * info->pitch + (x + cx)] =
          cursor_backbuffer[cy * CURSOR_SIZE + cx];
    }
  }
}

void draw_cursor(BootInfo *info, int x, int y) {
  mouse_x = x;
  mouse_y = y;
  uint32_t *fb = info->backbuffer ? info->backbuffer : info->framebuffer;
  for (int cy = 0; cy < CURSOR_SIZE; cy++) {
    if (y + cy >= (int)info->height)
      break;
    for (int cx = 0; cx < CURSOR_SIZE; cx++) {
      if (x + cx >= (int)info->width)
        break;
      uint32_t index = (y + cy) * info->pitch + (x + cx);
      cursor_backbuffer[cy * CURSOR_SIZE + cx] = fb[index];
      int draw = 0;
      if (cx == 0 || cy == 0 || cx == cy || (cx < 5 && cy == 5))
        draw = 1;
      if (draw) {
        if (mouse_left_pressed)
          fb[index] = 0xFF00FF00;
        else
          fb[index] = 0xFF000000;
      } else if (cx < cy && cy < 5)
        fb[index] = 0xFFFFFFFF;
    }
  }
}

void start_mouse_test(void) {
  mouse_test_mode = 1;
  mouse_test_clicks = 0;
  mouse_test_movement = 0;
  last_mouse_x = mouse_x;
  last_mouse_y = mouse_y;
}

int get_mouse_test_status(int *clicks, int *movement) {
  *clicks = mouse_test_clicks;
  *movement = mouse_test_movement;
  return mouse_test_mode;
}

// Improved mouse handler: Fix movement and drain stray bytes from keyboard
void handle_mouse(BootInfo *info) {
  uint8_t status;

  while (1) {
    status = inb(0x64);

    // No data pending
    if (!(status & 0x01))
      break;

    // Mouse data (bit 5 set)
    if (status & 0x20) {
      uint8_t data = inb(0x60);

      // Sync: first byte must have bit 3 set
      if (mouse_cycle == 0) {
        if (!(data & 0x08)) {
          // Not a valid first byte — discard and stay in sync search mode
          continue;
        }
      }

      mouse_byte[mouse_cycle++] = data;

      if (mouse_cycle == 3) {
        mouse_cycle = 0;

        uint8_t old_left_pressed = mouse_left_pressed;
        mouse_left_pressed = (mouse_byte[0] & 0x01);

        if (mouse_test_mode && !old_left_pressed && mouse_left_pressed) {
          mouse_test_clicks++;
        }

        restore_cursor_bg(info, mouse_x, mouse_y);

        int16_t dx = (int8_t)mouse_byte[1];
        int16_t dy = -(int8_t)mouse_byte[2];  // Invert Y: PS/2 positive = up, screen positive = down

        // Apply sensitivity scaling and offset correction
        dx = dx * 1 / 2;  // Reduce horizontal movement
        dy = dy * 1 / 2;  // Reduce vertical movement

        int new_x = mouse_x + dx;
        int new_y = mouse_y + dy;


        if (mouse_test_mode) {
          if (new_x != last_mouse_x || new_y != last_mouse_y) {
            mouse_test_movement++;
            last_mouse_x = new_x;
            last_mouse_y = new_y;
          }
        }

        if (new_x < 0)
          new_x = 0;
        if (new_y < 0)
          new_y = 0;
        if (new_x > (int)info->width - CURSOR_SIZE)
          new_x = info->width - CURSOR_SIZE;
        if (new_y > (int)info->height - CURSOR_SIZE)
          new_y = info->height - CURSOR_SIZE;

        draw_cursor(info, new_x, new_y);

        // NOTE: no PIC EOI here — this is not an IRQ handler
      }
    } else {
      // Keyboard data: let the keyboard IRQ handler deal with it
      // DO NOT drain here - it steals keyboard initialization responses!
      break;  // Exit the polling loop to avoid interfering
    }
  }
}
