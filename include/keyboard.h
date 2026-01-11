#pragma once

// Keyboard initialization and control
void keyboard_init(void);
void keyboard_enable_interrupt(void);

// Keyboard buffer functions
char keyboard_get_char(void);
int keyboard_has_data(void);

// Legacy support (for compatibility)
extern char last_key_pressed;