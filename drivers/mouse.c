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
  serial_write_string("[MOUSE_INIT] Starting PS/2 mouse initialization...\n");

  __asm__ volatile("cli");

  // Step 1: Quick check if PS/2 controller exists
  serial_write_string("[MOUSE_INIT] Step 1: Checking PS/2 controller...\n");
  uint8_t test_status = inb(0x64);
  serial_write_string("[MOUSE_INIT] Status byte: 0x");
  char hexbuf[3] = "00";
  hexbuf[0] = "0123456789ABCDEF"[(test_status >> 4) & 0xF];
  hexbuf[1] = "0123456789ABCDEF"[test_status & 0xF];
  serial_write_string(hexbuf);
  serial_write_string("\n");

  if ((test_status & 0xFF) == 0xFF) {
    serial_write_string("[MOUSE_INIT] No PS/2 controller found, returning 0\n");
    __asm__ volatile("sti");
    return 0;
  }
  serial_write_string("[MOUSE_INIT] PS/2 controller found, continuing...\n");

  // Step 2: Disable both keyboard and mouse ports
  serial_write_string("[MOUSE_INIT] Step 2: Disabling ports...\n");

  mouse_wait(1);
  outb(0x64, 0xAD); // Disable keyboard
  io_wait();

  mouse_wait(1);
  outb(0x64, 0xA7); // Disable mouse
  io_wait();

  // Flush any pending data from output buffer
  serial_write_string("[MOUSE_INIT] Flushing output buffer...\n");
  while (inb(0x64) & 1) {
    inb(0x60);
    io_wait();
  }
  serial_write_string("[MOUSE_INIT] Output buffer flushed\n");

  // Step 3: Configure controller for mouse
  serial_write_string("[MOUSE_INIT] Step 3: Configuring controller...\n");

  mouse_wait(1);
  outb(0x64, 0x20); // Get config byte
  io_wait();

  serial_write_string("[MOUSE_INIT] Waiting for config byte...\n");
  uint8_t config = mouse_read_device();
  serial_write_string("[MOUSE_INIT] Got config byte\n");
  serial_write_string("[MOUSE_INIT] Current config: 0x");
  hexbuf[0] = "0123456789ABCDEF"[(config >> 4) & 0xF];
  hexbuf[1] = "0123456789ABCDEF"[config & 0xF];
  serial_write_string(hexbuf);
  serial_write_string("\n");

  config |= (1 << 1); // Enable mouse clock
  config |= (1 << 5); // Enable mouse IRQ

  serial_write_string("[MOUSE_INIT] New config: 0x");
  hexbuf[0] = "0123456789ABCDEF"[(config >> 4) & 0xF];
  hexbuf[1] = "0123456789ABCDEF"[config & 0xF];
  serial_write_string(hexbuf);
  serial_write_string("\n");

  mouse_wait(1);
  outb(0x64, 0x60); // Set config byte
  io_wait();

  mouse_wait(1);
  outb(0x60, config);
  io_wait();
  serial_write_string("[MOUSE_INIT] Config set\n");

  // Step 4: Reset mouse
  serial_write_string("[MOUSE_INIT] Step 4: Resetting mouse...\n");

  mouse_write_device(0xFF); // Reset command
  serial_write_string("[MOUSE_INIT] Reset command sent\n");

  // Read responses: 0xFA (ACK), 0xAA (BAT OK)
  serial_write_string("[MOUSE_INIT] Reading reset responses...\n");
  int responses = 0;
  for (int attempts = 0; attempts < 1000 && responses < 2; attempts++) {
    if (inb(0x64) & 1) {
      uint8_t data = inb(0x60);
      responses++;
      serial_write_string("[MOUSE_INIT] Reset response ");
      char numbuf[2] = "0";
      numbuf[0] = '0' + responses;
      serial_write_string(numbuf);
      serial_write_string(": 0x");
      hexbuf[0] = "0123456789ABCDEF"[(data >> 4) & 0xF];
      hexbuf[1] = "0123456789ABCDEF"[data & 0xF];
      serial_write_string(hexbuf);
      serial_write_string("\n");
    } else {
      io_wait();
    }
  }

  // Step 5: Enable streaming mode
  serial_write_string("[MOUSE_INIT] Step 5: Enabling streaming mode...\n");

  mouse_write_device(0xF4); // Enable streaming

  serial_write_string("[MOUSE_INIT] Waiting for ACK...\n");
  uint8_t ack = 0;
  for (int i = 0; i < 20000; i++) { // ~20ms window
    if (inb(0x64) & 1) {
      ack = inb(0x60);
      break;
    }
    io_wait();
  }

  serial_write_string("[MOUSE_INIT] Got response: 0x");
  hexbuf[0] = "0123456789ABCDEF"[(ack >> 4) & 0xF];
  hexbuf[1] = "0123456789ABCDEF"[ack & 0xF];
  serial_write_string(hexbuf);
  serial_write_string("\n");

  if (ack == 0xFA) {
    serial_write_string("[MOUSE_INIT] ACK received - streaming enabled!\n");
  } else {
    serial_write_string(
        "[MOUSE_INIT] No valid ACK received, continuing anyway...\n");
  }

  // Step 6: Re-enable the ports
  serial_write_string("[MOUSE_INIT] Step 6: Re-enabling ports...\n");

  mouse_wait(1);
  outb(0x64, 0xAE); // Enable keyboard
  io_wait();

  mouse_wait(1);
  outb(0x64, 0xA8); // Enable mouse
  io_wait();

  // Unmask IRQ12 on slave PIC
  uint8_t mask = inb(0xA1);
  mask &= ~(1 << 4); // IRQ12 = bit 4
  outb(0xA1, mask);

  __asm__ volatile("sti");

  serial_write_string(
      "[MOUSE_INIT] Initialization complete, returning success\n");
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

      if (mouse_cycle > 0 && !(mouse_byte[0] & 0x08)) {
        // Lost sync mid-packet — reset
        mouse_cycle = 0;
        continue;
      }

      // Sync: first byte must have bit 3 set
      // First byte must have bit 3 set
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
        int16_t dy = (int8_t)mouse_byte[2];

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
      // Keyboard or other data: drain to keep mouse in sync
      inb(0x60);
    }
  }
}
