/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_KERNEL_INPUT_H
#define LAMIX_KERNEL_INPUT_H

#include <stddef.h>
#include <stdint.h>

#define INPUT_EVENT_CAPACITY 128u

typedef enum input_event_type {
  INPUT_EVENT_KEY = 1,
  INPUT_EVENT_MOUSE = 2
} input_event_type;

typedef enum input_keycode {
  INPUT_KEY_NONE = 0,
  INPUT_KEY_A = 0x04, INPUT_KEY_B, INPUT_KEY_C, INPUT_KEY_D, INPUT_KEY_E,
  INPUT_KEY_F, INPUT_KEY_G, INPUT_KEY_H, INPUT_KEY_I, INPUT_KEY_J,
  INPUT_KEY_K, INPUT_KEY_L, INPUT_KEY_M, INPUT_KEY_N, INPUT_KEY_O,
  INPUT_KEY_P, INPUT_KEY_Q, INPUT_KEY_R, INPUT_KEY_S, INPUT_KEY_T,
  INPUT_KEY_U, INPUT_KEY_V, INPUT_KEY_W, INPUT_KEY_X, INPUT_KEY_Y,
  INPUT_KEY_Z,
  INPUT_KEY_1 = 0x1e, INPUT_KEY_2, INPUT_KEY_3, INPUT_KEY_4, INPUT_KEY_5,
  INPUT_KEY_6, INPUT_KEY_7, INPUT_KEY_8, INPUT_KEY_9, INPUT_KEY_0,
  INPUT_KEY_ENTER = 0x28, INPUT_KEY_ESCAPE, INPUT_KEY_BACKSPACE,
  INPUT_KEY_TAB, INPUT_KEY_SPACE, INPUT_KEY_MINUS, INPUT_KEY_EQUAL,
  INPUT_KEY_LEFT_BRACKET, INPUT_KEY_RIGHT_BRACKET, INPUT_KEY_BACKSLASH,
  INPUT_KEY_SEMICOLON = 0x33, INPUT_KEY_APOSTROPHE, INPUT_KEY_GRAVE,
  INPUT_KEY_COMMA, INPUT_KEY_DOT, INPUT_KEY_SLASH,
  INPUT_KEY_CAPS_LOCK = 0x39,
  INPUT_KEY_F1 = 0x3a, INPUT_KEY_F2, INPUT_KEY_F3, INPUT_KEY_F4,
  INPUT_KEY_F5, INPUT_KEY_F6, INPUT_KEY_F7, INPUT_KEY_F8, INPUT_KEY_F9,
  INPUT_KEY_F10, INPUT_KEY_F11, INPUT_KEY_F12,
  INPUT_KEY_PRINT_SCREEN = 0x46, INPUT_KEY_SCROLL_LOCK, INPUT_KEY_PAUSE,
  INPUT_KEY_INSERT, INPUT_KEY_HOME, INPUT_KEY_PAGE_UP, INPUT_KEY_DELETE,
  INPUT_KEY_END, INPUT_KEY_PAGE_DOWN, INPUT_KEY_RIGHT, INPUT_KEY_LEFT,
  INPUT_KEY_DOWN, INPUT_KEY_UP,
  INPUT_KEY_NUM_LOCK = 0x53, INPUT_KEY_KP_DIVIDE, INPUT_KEY_KP_MULTIPLY,
  INPUT_KEY_KP_MINUS, INPUT_KEY_KP_PLUS, INPUT_KEY_KP_ENTER,
  INPUT_KEY_KP_1, INPUT_KEY_KP_2, INPUT_KEY_KP_3, INPUT_KEY_KP_4,
  INPUT_KEY_KP_5, INPUT_KEY_KP_6, INPUT_KEY_KP_7, INPUT_KEY_KP_8,
  INPUT_KEY_KP_9, INPUT_KEY_KP_0, INPUT_KEY_KP_DOT,
  INPUT_KEY_LEFT_CTRL = 0xe0, INPUT_KEY_LEFT_SHIFT, INPUT_KEY_LEFT_ALT,
  INPUT_KEY_LEFT_META, INPUT_KEY_RIGHT_CTRL, INPUT_KEY_RIGHT_SHIFT,
  INPUT_KEY_RIGHT_ALT, INPUT_KEY_RIGHT_META
} input_keycode;

enum {
  INPUT_MOD_SHIFT = 1u << 0,
  INPUT_MOD_CTRL = 1u << 1,
  INPUT_MOD_ALT = 1u << 2,
  INPUT_MOD_CAPS = 1u << 3
};

typedef struct input_event {
  uint64_t sequence;
  input_event_type type;
  union {
    struct {
      uint16_t keycode;
      uint8_t pressed;
      uint8_t ascii;
      uint8_t modifiers;
    } key;
    struct {
      int16_t dx;
      int16_t dy;
      uint8_t buttons;
    } mouse;
  } data;
} input_event;

typedef struct input_state {
  input_event events[INPUT_EVENT_CAPACITY];
  volatile uint32_t head;
  volatile uint32_t tail;
  uint64_t next_sequence;
  uint64_t dropped_events;
  uint32_t malformed_keyboard_bytes;
  uint32_t malformed_mouse_packets;
  uint8_t modifiers;
  uint8_t extended;
  uint8_t pause_bytes_left;
  uint8_t mouse_packet[3];
  uint8_t mouse_packet_length;
  uint8_t mouse_buttons;
  uint8_t key_down[256];
} input_state;

typedef struct input_stats {
  uint64_t next_sequence;
  uint64_t dropped_events;
  uint32_t malformed_keyboard_bytes;
  uint32_t malformed_mouse_packets;
  uint32_t queued_events;
} input_stats;

void input_state_init(input_state *state);
void input_keyboard_scancode(input_state *state, uint8_t byte);
void input_mouse_byte(input_state *state, uint8_t byte);
int input_poll(input_state *state, input_event *event);
void input_get_stats(const input_state *state, input_stats *stats);

#endif
