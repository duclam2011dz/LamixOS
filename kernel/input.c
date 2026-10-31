/* SPDX-License-Identifier: GPL-3.0-only */
#include "input.h"

#define INPUT_QUEUE_MASK (INPUT_EVENT_CAPACITY - 1u)

_Static_assert((INPUT_EVENT_CAPACITY & (INPUT_EVENT_CAPACITY - 1u)) == 0,
               "input queue capacity must be a power of two");

typedef struct key_description {
  uint16_t code;
  uint8_t plain;
  uint8_t shifted;
  uint8_t letter;
} key_description;

static const key_description set1_keys[0x59] = {
    [0x01] = {INPUT_KEY_ESCAPE, 0x1b, 0x1b, 0},
    [0x02] = {INPUT_KEY_1, '1', '!', 0}, [0x03] = {INPUT_KEY_2, '2', '@', 0},
    [0x04] = {INPUT_KEY_3, '3', '#', 0}, [0x05] = {INPUT_KEY_4, '4', '$', 0},
    [0x06] = {INPUT_KEY_5, '5', '%', 0}, [0x07] = {INPUT_KEY_6, '6', '^', 0},
    [0x08] = {INPUT_KEY_7, '7', '&', 0}, [0x09] = {INPUT_KEY_8, '8', '*', 0},
    [0x0a] = {INPUT_KEY_9, '9', '(', 0}, [0x0b] = {INPUT_KEY_0, '0', ')', 0},
    [0x0c] = {INPUT_KEY_MINUS, '-', '_', 0},
    [0x0d] = {INPUT_KEY_EQUAL, '=', '+', 0},
    [0x0e] = {INPUT_KEY_BACKSPACE, '\b', '\b', 0},
    [0x0f] = {INPUT_KEY_TAB, '\t', '\t', 0},
    [0x10] = {INPUT_KEY_Q, 'q', 'Q', 1}, [0x11] = {INPUT_KEY_W, 'w', 'W', 1},
    [0x12] = {INPUT_KEY_E, 'e', 'E', 1}, [0x13] = {INPUT_KEY_R, 'r', 'R', 1},
    [0x14] = {INPUT_KEY_T, 't', 'T', 1}, [0x15] = {INPUT_KEY_Y, 'y', 'Y', 1},
    [0x16] = {INPUT_KEY_U, 'u', 'U', 1}, [0x17] = {INPUT_KEY_I, 'i', 'I', 1},
    [0x18] = {INPUT_KEY_O, 'o', 'O', 1}, [0x19] = {INPUT_KEY_P, 'p', 'P', 1},
    [0x1a] = {INPUT_KEY_LEFT_BRACKET, '[', '{', 0},
    [0x1b] = {INPUT_KEY_RIGHT_BRACKET, ']', '}', 0},
    [0x1c] = {INPUT_KEY_ENTER, '\n', '\n', 0},
    [0x1d] = {INPUT_KEY_LEFT_CTRL, 0, 0, 0},
    [0x1e] = {INPUT_KEY_A, 'a', 'A', 1}, [0x1f] = {INPUT_KEY_S, 's', 'S', 1},
    [0x20] = {INPUT_KEY_D, 'd', 'D', 1}, [0x21] = {INPUT_KEY_F, 'f', 'F', 1},
    [0x22] = {INPUT_KEY_G, 'g', 'G', 1}, [0x23] = {INPUT_KEY_H, 'h', 'H', 1},
    [0x24] = {INPUT_KEY_J, 'j', 'J', 1}, [0x25] = {INPUT_KEY_K, 'k', 'K', 1},
    [0x26] = {INPUT_KEY_L, 'l', 'L', 1},
    [0x27] = {INPUT_KEY_SEMICOLON, ';', ':', 0},
    [0x28] = {INPUT_KEY_APOSTROPHE, '\'', '"', 0},
    [0x29] = {INPUT_KEY_GRAVE, '`', '~', 0},
    [0x2a] = {INPUT_KEY_LEFT_SHIFT, 0, 0, 0},
    [0x2b] = {INPUT_KEY_BACKSLASH, '\\', '|', 0},
    [0x2c] = {INPUT_KEY_Z, 'z', 'Z', 1}, [0x2d] = {INPUT_KEY_X, 'x', 'X', 1},
    [0x2e] = {INPUT_KEY_C, 'c', 'C', 1}, [0x2f] = {INPUT_KEY_V, 'v', 'V', 1},
    [0x30] = {INPUT_KEY_B, 'b', 'B', 1}, [0x31] = {INPUT_KEY_N, 'n', 'N', 1},
    [0x32] = {INPUT_KEY_M, 'm', 'M', 1},
    [0x33] = {INPUT_KEY_COMMA, ',', '<', 0}, [0x34] = {INPUT_KEY_DOT, '.', '>', 0},
    [0x35] = {INPUT_KEY_SLASH, '/', '?', 0},
    [0x36] = {INPUT_KEY_RIGHT_SHIFT, 0, 0, 0},
    [0x37] = {INPUT_KEY_KP_MULTIPLY, '*', '*', 0},
    [0x38] = {INPUT_KEY_LEFT_ALT, 0, 0, 0},
    [0x39] = {INPUT_KEY_SPACE, ' ', ' ', 0},
    [0x3a] = {INPUT_KEY_CAPS_LOCK, 0, 0, 0},
    [0x3b] = {INPUT_KEY_F1, 0, 0, 0}, [0x3c] = {INPUT_KEY_F2, 0, 0, 0},
    [0x3d] = {INPUT_KEY_F3, 0, 0, 0}, [0x3e] = {INPUT_KEY_F4, 0, 0, 0},
    [0x3f] = {INPUT_KEY_F5, 0, 0, 0}, [0x40] = {INPUT_KEY_F6, 0, 0, 0},
    [0x41] = {INPUT_KEY_F7, 0, 0, 0}, [0x42] = {INPUT_KEY_F8, 0, 0, 0},
    [0x43] = {INPUT_KEY_F9, 0, 0, 0}, [0x44] = {INPUT_KEY_F10, 0, 0, 0},
    [0x45] = {INPUT_KEY_NUM_LOCK, 0, 0, 0},
    [0x46] = {INPUT_KEY_SCROLL_LOCK, 0, 0, 0},
    [0x47] = {INPUT_KEY_KP_7, '7', '7', 0}, [0x48] = {INPUT_KEY_KP_8, '8', '8', 0},
    [0x49] = {INPUT_KEY_KP_9, '9', '9', 0}, [0x4a] = {INPUT_KEY_KP_MINUS, '-', '-', 0},
    [0x4b] = {INPUT_KEY_KP_4, '4', '4', 0}, [0x4c] = {INPUT_KEY_KP_5, '5', '5', 0},
    [0x4d] = {INPUT_KEY_KP_6, '6', '6', 0}, [0x4e] = {INPUT_KEY_KP_PLUS, '+', '+', 0},
    [0x4f] = {INPUT_KEY_KP_1, '1', '1', 0}, [0x50] = {INPUT_KEY_KP_2, '2', '2', 0},
    [0x51] = {INPUT_KEY_KP_3, '3', '3', 0}, [0x52] = {INPUT_KEY_KP_0, '0', '0', 0},
    [0x53] = {INPUT_KEY_KP_DOT, '.', '.', 0},
    [0x57] = {INPUT_KEY_F11, 0, 0, 0}, [0x58] = {INPUT_KEY_F12, 0, 0, 0}};

static void enqueue(input_state *state, input_event *event) {
  event->sequence = state->next_sequence++;
  uint32_t head = state->head;
  if (head - state->tail >= INPUT_EVENT_CAPACITY) {
    ++state->dropped_events;
    return;
  }
  state->events[head & INPUT_QUEUE_MASK] = *event;
  __asm__ volatile("" ::: "memory");
  state->head = head + 1;
}

void input_state_init(input_state *state) {
  if (!state) return;
  for (size_t i = 0; i < sizeof(*state); ++i)
    ((uint8_t *)state)[i] = 0;
  state->next_sequence = 1;
}

static uint16_t extended_key(uint8_t scan) {
  switch (scan) {
  case 0x1c: return INPUT_KEY_KP_ENTER;
  case 0x1d: return INPUT_KEY_RIGHT_CTRL;
  case 0x35: return INPUT_KEY_KP_DIVIDE;
  case 0x38: return INPUT_KEY_RIGHT_ALT;
  case 0x47: return INPUT_KEY_HOME;
  case 0x48: return INPUT_KEY_UP;
  case 0x49: return INPUT_KEY_PAGE_UP;
  case 0x4b: return INPUT_KEY_LEFT;
  case 0x4d: return INPUT_KEY_RIGHT;
  case 0x4f: return INPUT_KEY_END;
  case 0x50: return INPUT_KEY_DOWN;
  case 0x51: return INPUT_KEY_PAGE_DOWN;
  case 0x52: return INPUT_KEY_INSERT;
  case 0x53: return INPUT_KEY_DELETE;
  case 0x5b: return INPUT_KEY_LEFT_META;
  case 0x5c: return INPUT_KEY_RIGHT_META;
  default: return INPUT_KEY_NONE;
  }
}

static uint8_t key_ascii(const key_description *key, uint8_t modifiers) {
  if (!key->code) return 0;
  if (key->letter)
    return ((!!(modifiers & INPUT_MOD_SHIFT)) ^
            (!!(modifiers & INPUT_MOD_CAPS))) ? key->shifted : key->plain;
  return modifiers & INPUT_MOD_SHIFT ? key->shifted : key->plain;
}

static void update_modifiers(input_state *state, uint16_t key, int pressed,
                             int was_down) {
  uint8_t modifier = 0;
  if (key == INPUT_KEY_LEFT_SHIFT || key == INPUT_KEY_RIGHT_SHIFT)
    modifier = INPUT_MOD_SHIFT;
  else if (key == INPUT_KEY_LEFT_CTRL || key == INPUT_KEY_RIGHT_CTRL)
    modifier = INPUT_MOD_CTRL;
  else if (key == INPUT_KEY_LEFT_ALT || key == INPUT_KEY_RIGHT_ALT)
    modifier = INPUT_MOD_ALT;
  if (modifier) {
    if (pressed) state->modifiers |= modifier;
    else {
      uint16_t other = key == INPUT_KEY_LEFT_SHIFT ? INPUT_KEY_RIGHT_SHIFT :
                       key == INPUT_KEY_RIGHT_SHIFT ? INPUT_KEY_LEFT_SHIFT :
                       key == INPUT_KEY_LEFT_CTRL ? INPUT_KEY_RIGHT_CTRL :
                       key == INPUT_KEY_RIGHT_CTRL ? INPUT_KEY_LEFT_CTRL :
                       key == INPUT_KEY_LEFT_ALT ? INPUT_KEY_RIGHT_ALT :
                       INPUT_KEY_LEFT_ALT;
      if (!state->key_down[other]) state->modifiers &= (uint8_t)~modifier;
    }
  } else if (key == INPUT_KEY_CAPS_LOCK && pressed && !was_down) {
    state->modifiers ^= INPUT_MOD_CAPS;
  }
}

void input_keyboard_scancode(input_state *state, uint8_t byte) {
  if (!state) return;
  if (state->pause_bytes_left) {
    --state->pause_bytes_left;
    return;
  }
  if (byte == 0xe1) {
    state->extended = 0;
    state->pause_bytes_left = 5;
    return;
  }
  if (byte == 0xe0) {
    state->extended = 1;
    return;
  }
  int extended = state->extended;
  state->extended = 0;
  uint8_t scan = byte & 0x7f;
  int pressed = !(byte & 0x80);
  if (extended && (scan == 0x2a || scan == 0x36 || scan == 0x37)) return;
  if (!scan) {
    ++state->malformed_keyboard_bytes;
    return;
  }
  uint16_t code = extended ? extended_key(scan) :
                  scan < sizeof(set1_keys) / sizeof(set1_keys[0]) ?
                      set1_keys[scan].code : INPUT_KEY_NONE;
  if (!code) return;
  int was_down = state->key_down[code] != 0;
  if (pressed) state->key_down[code] = 1;
  else state->key_down[code] = 0;
  update_modifiers(state, code, pressed, was_down);

  input_event event = {0};
  event.type = INPUT_EVENT_KEY;
  event.data.key.keycode = code;
  event.data.key.pressed = (uint8_t)pressed;
  event.data.key.modifiers = state->modifiers;
  if (pressed && !extended)
    event.data.key.ascii = key_ascii(&set1_keys[scan], state->modifiers);
  else if (pressed && code == INPUT_KEY_KP_ENTER)
    event.data.key.ascii = '\n';
  else if (pressed && code == INPUT_KEY_KP_DIVIDE)
    event.data.key.ascii = '/';
  enqueue(state, &event);
}

static int8_t signed_delta(uint8_t value) {
  return (int8_t)value;
}

void input_mouse_byte(input_state *state, uint8_t byte) {
  if (!state) return;
  if (!state->mouse_packet_length) {
    if (!(byte & 0x08)) {
      ++state->malformed_mouse_packets;
      return;
    }
    state->mouse_packet[0] = byte;
    state->mouse_packet_length = 1;
    return;
  }
  state->mouse_packet[state->mouse_packet_length++] = byte;
  if (state->mouse_packet_length < 3) return;
  state->mouse_packet_length = 0;

  uint8_t first = state->mouse_packet[0];
  if (first & 0xc0) { ++state->malformed_mouse_packets; return; }
  int16_t dx = signed_delta(state->mouse_packet[1]);
  int16_t raw_y = signed_delta(state->mouse_packet[2]);
  int16_t dy = (int16_t)-raw_y;
  uint8_t buttons = first & 0x07;
  if (!dx && !dy && buttons == state->mouse_buttons) return;

  state->mouse_buttons = buttons;
  input_event event = {0};
  event.type = INPUT_EVENT_MOUSE;
  event.data.mouse.dx = dx;
  event.data.mouse.dy = dy;
  event.data.mouse.buttons = buttons;
  enqueue(state, &event);
}

int input_poll(input_state *state, input_event *event) {
  if (!state || !event) return 0;
  uint32_t tail = state->tail;
  if (tail == state->head) return 0;
  __asm__ volatile("" ::: "memory");
  *event = state->events[tail & INPUT_QUEUE_MASK];
  state->tail = tail + 1;
  return 1;
}

void input_get_stats(const input_state *state, input_stats *stats) {
  if (!state || !stats) return;
  stats->next_sequence = state->next_sequence;
  stats->dropped_events = state->dropped_events;
  stats->malformed_keyboard_bytes = state->malformed_keyboard_bytes;
  stats->malformed_mouse_packets = state->malformed_mouse_packets;
  stats->queued_events = state->head - state->tail;
}
