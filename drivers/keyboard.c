#include "../include/kernel.h"
#include "../include/string.h"
#include "../hal/serial.h"

// Keyboard state
#define KEYBOARD_BUFFER_SIZE 256
static char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static int buffer_head = 0;
static int buffer_tail = 0;
static int buffer_count = 0;

// Modifier key states
static uint8_t modifier_state = 0;
#define MOD_SHIFT_L    0x01
#define MOD_SHIFT_R    0x02
#define MOD_CTRL_L     0x04
#define MOD_CTRL_R     0x08
#define MOD_ALT_L      0x10
#define MOD_ALT_R      0x20
#define MOD_CAPS_LOCK  0x40
#define MOD_NUM_LOCK   0x80

// Key repeat state
static uint8_t last_scancode = 0;
static int repeat_count = 0;
static int repeat_delay = 500;  // Initial delay in ms
static int repeat_rate = 50;    // Repeat rate in ms

// Extended scancodes (0xE0 prefix)
static uint8_t extended_scancode = 0;

// Key state tracking for press/release
static uint8_t key_states[128] = {0};

// Initialization state tracking - must be volatile for shared access
static volatile uint8_t keyboard_initialized = 0;
static volatile int init_responses_expected = 0;

char last_key_pressed = 0;

// Enhanced scancode to ASCII mapping with shift states
const char scancode_map_normal[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

const char scancode_map_shifted[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// Buffer operations
static void keyboard_buffer_put(char c) {
    if (buffer_count < KEYBOARD_BUFFER_SIZE) {
        keyboard_buffer[buffer_head] = c;
        buffer_head = (buffer_head + 1) % KEYBOARD_BUFFER_SIZE;
        buffer_count++;
    }
}

void keyboard_enable_interrupt(void) {
    // Unmask keyboard interrupt (IRQ1)
    uint8_t mask = inb(0x21);
    mask &= ~(1 << 1); // Unmask IRQ1
    outb(0x21, mask);
    serial_write_string("[KEYBOARD] Keyboard interrupt enabled\n");
}

char keyboard_get_char(void) {
    if (buffer_count > 0) {
        char c = keyboard_buffer[buffer_tail];
        buffer_tail = (buffer_tail + 1) % KEYBOARD_BUFFER_SIZE;
        buffer_count--;

        serial_write_string("[KEYBOARD] Retrieved char from buffer: '");
        char charbuf[2] = {c, 0};
        serial_write_string(charbuf);
        serial_write_string("' (buffer count: ");
        char countbuf[4] = "000";
        int count = buffer_count;
        countbuf[0] = '0' + (count / 100) % 10;
        countbuf[1] = '0' + (count / 10) % 10;
        countbuf[2] = '0' + count % 10;
        serial_write_string(countbuf);
        serial_write_string(")\n");

        return c;
    }
    return 0;
}

int keyboard_has_data(void) {
    return buffer_count > 0;
}

// Enhanced keyboard initialization
void keyboard_init(void) {
    serial_write_string("[KEYBOARD_INIT] Starting enhanced PS/2 keyboard initialization...\n");

    // Reset initialization state
    keyboard_initialized = 0;
    init_responses_expected = 0;

    // Disable interrupts during critical initialization
    __asm__ volatile("cli");

    // Step 1: Flush any pending keyboard data
    serial_write_string("[KEYBOARD_INIT] Step 1: Flushing keyboard buffer...\n");
    int flush_count = 0;
    for (int i = 0; i < 100; i++) {
        if (inb(0x64) & 1) {
            inb(0x60);
            flush_count++;
        } else {
            break;
        }
        io_wait();
    }

    if (flush_count > 0) {
        serial_write_string("[KEYBOARD_INIT] Flushed ");
        char buf[4] = "000";
        buf[0] = '0' + (flush_count / 100) % 10;
        buf[1] = '0' + (flush_count / 10) % 10;
        buf[2] = '0' + flush_count % 10;
        serial_write_string(buf);
        serial_write_string(" bytes from keyboard buffer\n");
    } else {
        serial_write_string("[KEYBOARD_INIT] No bytes to flush\n");
    }

    // Step 2: Enable keyboard interface
    serial_write_string("[KEYBOARD_INIT] Step 2: Enabling keyboard interface...\n");
    outb(0x64, 0xAE);
    io_wait();
    serial_write_string("[KEYBOARD_INIT] Keyboard interface enabled\n");

    // Step 3: Reset keyboard to ensure clean state
    serial_write_string("[KEYBOARD_INIT] Step 3: Resetting keyboard...\n");

    // Disable mouse polling temporarily to avoid interference
    serial_write_string("[KEYBOARD_INIT] Temporarily disabling mouse polling\n");

    outb(0x60, 0xFF); // Reset command
    io_wait();
    serial_write_string("[KEYBOARD_INIT] Reset command sent\n");

    // Wait for reset responses with interrupts DISABLED (poll manually)
    serial_write_string("[KEYBOARD_INIT] Step 4: Waiting for reset responses...\n");

    // Poll for responses manually since interrupts might cause issues
    int responses_found = 0;
    for (int i = 0; i < 2000 && responses_found < 2; i++) {
        if (inb(0x64) & 1) {
            uint8_t response = inb(0x60);
            responses_found++;
            serial_write_string("[KEYBOARD_INIT] Polled response ");
            char numbuf[2] = "0";
            numbuf[0] = '0' + responses_found;
            serial_write_string(numbuf);
            serial_write_string(": 0x");

            char hexbuf[3] = "00";
            hexbuf[0] = "0123456789ABCDEF"[(response >> 4) & 0xF];
            hexbuf[1] = "0123456789ABCDEF"[response & 0xF];
            serial_write_string(hexbuf);
            serial_write_string("\n");

            if (response == 0xFA) {
                serial_write_string("[KEYBOARD_INIT] Keyboard reset ACK received\n");
                init_responses_expected++; // Manually increment counter
            } else if (response == 0xAA) {
                serial_write_string("[KEYBOARD_INIT] Keyboard self-test passed\n");
                init_responses_expected++; // Manually increment counter
            }
        }
        io_wait();
    }

    if (responses_found >= 1) {
        serial_write_string("[KEYBOARD_INIT] Got responses, proceeding with keyboard init\n");
    } else {
        serial_write_string("[KEYBOARD_INIT] No responses received, continuing anyway\n");
    }

    // Re-enable mouse polling
    serial_write_string("[KEYBOARD_INIT] Re-enabling mouse polling\n");

    // Disable interrupts again for final setup
    __asm__ volatile("cli");

    serial_write_string("[KEYBOARD_INIT] About to initialize keyboard state...\n");

    // Step 5: Initialize keyboard state
    serial_write_string("[KEYBOARD_INIT] Step 5: Initializing keyboard state...\n");
    modifier_state = 0;
    last_scancode = 0;
    repeat_count = 0;
    extended_scancode = 0;
    serial_write_string("[KEYBOARD_INIT] Clearing key states...\n");
    memset(key_states, 0, sizeof(key_states));
    serial_write_string("[KEYBOARD_INIT] Key states cleared\n");

    serial_write_string("[KEYBOARD_INIT] Clearing buffers...\n");

    // Clear buffers
    buffer_head = buffer_tail = buffer_count = 0;
    last_key_pressed = 0;

    serial_write_string("[KEYBOARD_INIT] Buffers cleared\n");

    // Mark keyboard as initialized
    keyboard_initialized = 1;

    serial_write_string("[KEYBOARD_INIT] Keyboard marked as initialized\n");

    // Mask keyboard interrupt BEFORE re-enabling interrupts globally
    uint8_t mask = inb(0x21);
    mask |= (1 << 1); // Mask IRQ1 (keyboard)
    outb(0x21, mask);

    serial_write_string("[KEYBOARD_INIT] Keyboard interrupt masked\n");

    // Now safely re-enable interrupts globally
    __asm__ volatile("sti");

    serial_write_string("[KEYBOARD_INIT] Interrupts globally re-enabled\n");

    serial_write_string("[KEYBOARD_INIT] === KEYBOARD INITIALIZATION SUCCESSFUL (IRQ masked) ===\n");
    serial_write_string("[KEYBOARD_INIT] Received ");
    char respbuf[3] = "00";
    respbuf[0] = '0' + (init_responses_expected / 10) % 10;
    respbuf[1] = '0' + init_responses_expected % 10;
    serial_write_string(respbuf);
    serial_write_string(" init responses\n");
    serial_write_string("[KEYBOARD_INIT] Ready for keyboard input!\n");
}

void keyboard_handler_main(uint8_t scancode) {
    // Only log during initialization or for actual key events
    // Suppress normal scancode logging to reduce interrupt overhead
    char hexbuf[3] = "00";
    hexbuf[0] = "0123456789ABCDEF"[(scancode >> 4) & 0xF];
    hexbuf[1] = "0123456789ABCDEF"[scancode & 0xF];

    // Handle keyboard responses during initialization
    // 0xFA = ACK, 0xAA = Self-test passed, 0xEE = Echo response
    if (!keyboard_initialized && (scancode == 0xFA || scancode == 0xAA || scancode == 0xEE)) {
        serial_write_string("[KEYBOARD] Init response: 0x");
        serial_write_string(hexbuf);
        if (scancode == 0xFA) {
            serial_write_string(" (ACK)");
            init_responses_expected++;
        } else if (scancode == 0xAA) {
            serial_write_string(" (Self-test passed)");
            init_responses_expected++;
        }
        serial_write_string("\n");
        return;
    }

    // Suppress logging of normal responses after initialization to reduce overhead
    if (scancode == 0xFA || scancode == 0xAA || scancode == 0xEE) {
        return; // Silently ignore
    }

    // Handle extended scancodes (0xE0 prefix)
    if (scancode == 0xE0) {
        extended_scancode = 1;
        serial_write_string("[KEYBOARD] Extended scancode prefix received\n");
        return;
    }

    uint8_t final_scancode = scancode;
    int is_release = (scancode & 0x80) != 0;
    int is_extended = extended_scancode;
    extended_scancode = 0;

    serial_write_string("[KEYBOARD] Processing ");
    serial_write_string(is_release ? "release" : "press");
    serial_write_string(" scancode: ");
    hexbuf[0] = "0123456789ABCDEF"[(final_scancode >> 4) & 0xF];
    hexbuf[1] = "0123456789ABCDEF"[final_scancode & 0xF];
    serial_write_string(hexbuf);
    if (is_extended) serial_write_string(" (extended)");
    serial_write_string("\n");

    if (is_release) {
        final_scancode &= 0x7F; // Clear release bit
    }

    // Handle modifier keys
    switch (final_scancode) {
        case 0x2A: // Left Shift
            if (is_release) modifier_state &= ~MOD_SHIFT_L;
            else modifier_state |= MOD_SHIFT_L;
            return;
        case 0x36: // Right Shift
            if (is_release) modifier_state &= ~MOD_SHIFT_R;
            else modifier_state |= MOD_SHIFT_R;
            return;
        case 0x1D: // Left Ctrl
            if (is_extended) {
                if (is_release) modifier_state &= ~MOD_CTRL_R;
                else modifier_state |= MOD_CTRL_R;
            } else {
                if (is_release) modifier_state &= ~MOD_CTRL_L;
                else modifier_state |= MOD_CTRL_L;
            }
            return;
        case 0x38: // Left Alt
            if (is_extended) {
                if (is_release) modifier_state &= ~MOD_ALT_R;
                else modifier_state |= MOD_ALT_R;
            } else {
                if (is_release) modifier_state &= ~MOD_ALT_L;
                else modifier_state |= MOD_ALT_L;
            }
            return;
        case 0x3A: // Caps Lock
            if (!is_release && !(key_states[final_scancode] & 1)) {
                modifier_state ^= MOD_CAPS_LOCK;
                // Send LED update command (basic implementation)
                outb(0x60, 0xED);
                io_wait();
                outb(0x60, (modifier_state & MOD_CAPS_LOCK) ? 0x04 : 0x00);
            }
            break;
    }

    // Update key state
    if (is_release) {
        key_states[final_scancode] &= ~1;
    } else {
        key_states[final_scancode] |= 1;
    }

    // Only process key presses (not releases)
    if (is_release) {
        // Reset repeat state on key release
        if (final_scancode == last_scancode) {
            last_scancode = 0;
            repeat_count = 0;
        }
        return;
    }

    // Handle key repeat
    if (final_scancode == last_scancode) {
        repeat_count++;
        // Only repeat after initial delay
        if (repeat_count < (repeat_delay / repeat_rate)) {
            return;
        }
    } else {
        last_scancode = final_scancode;
        repeat_count = 0;
    }

    // Convert scancode to ASCII
    char ascii_char = 0;
    int use_shift = (modifier_state & (MOD_SHIFT_L | MOD_SHIFT_R)) ||
                   ((modifier_state & MOD_CAPS_LOCK) &&
                    ((final_scancode >= 0x10 && final_scancode <= 0x19) ||  // q-p
                     (final_scancode >= 0x1E && final_scancode <= 0x26) ||  // a-l
                     (final_scancode >= 0x2C && final_scancode <= 0x32))); // z-m

    if (final_scancode < 128) {
        if (use_shift && scancode_map_shifted[final_scancode]) {
            ascii_char = scancode_map_shifted[final_scancode];
        } else if (scancode_map_normal[final_scancode]) {
            ascii_char = scancode_map_normal[final_scancode];
        }
    }

    // Handle special keys and buffer the character
    if (ascii_char >= 32 && ascii_char <= 126) { // Printable characters
        keyboard_buffer_put(ascii_char);
        last_key_pressed = ascii_char;
        serial_write_string("[KEYBOARD] Buffered printable char: '");
        char charbuf[2] = {ascii_char, 0};
        serial_write_string(charbuf);
        serial_write_string("' (buffer count: ");
        char countbuf[4] = "000";
        int count = buffer_count;
        countbuf[0] = '0' + (count / 100) % 10;
        countbuf[1] = '0' + (count / 10) % 10;
        countbuf[2] = '0' + count % 10;
        serial_write_string(countbuf);
        serial_write_string(")\n");
    } else if (ascii_char == '\n' || ascii_char == '\b' || ascii_char == '\t') {
        // Control characters
        keyboard_buffer_put(ascii_char);
        last_key_pressed = ascii_char;
        serial_write_string("[KEYBOARD] Buffered control char: ");
        if (ascii_char == '\n') serial_write_string("newline");
        else if (ascii_char == '\b') serial_write_string("backspace");
        else if (ascii_char == '\t') serial_write_string("tab");
        serial_write_string("\n");
    } else {
        serial_write_string("[KEYBOARD] Ignoring non-printable character\n");
    }
}
