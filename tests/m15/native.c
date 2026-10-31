/* SPDX-License-Identifier: GPL-3.0-only */
#include "input.h"
#include <assert.h>
#include <stdint.h>

static input_event take(input_state *s) { input_event e; assert(input_poll(s,&e)); return e; }
static void test_keyboard(void) {
  input_state s; input_state_init(&s);
  input_keyboard_scancode(&s,0x1e); input_event e=take(&s);
  assert(e.type==INPUT_EVENT_KEY && e.data.key.keycode==INPUT_KEY_A && e.data.key.pressed && e.data.key.ascii=='a');
  input_keyboard_scancode(&s,0x9e); e=take(&s); assert(!e.data.key.pressed && !e.data.key.ascii);
  input_keyboard_scancode(&s,0x2a); e=take(&s); assert(e.data.key.keycode==INPUT_KEY_LEFT_SHIFT && e.data.key.modifiers&INPUT_MOD_SHIFT);
  input_keyboard_scancode(&s,0x1e); e=take(&s); assert(e.data.key.ascii=='A');
  input_keyboard_scancode(&s,0x9e); (void)take(&s);
  input_keyboard_scancode(&s,0xaa); (void)take(&s);
  input_keyboard_scancode(&s,0x3a); e=take(&s); assert(e.data.key.modifiers&INPUT_MOD_CAPS);
  input_keyboard_scancode(&s,0xba); (void)take(&s);
  input_keyboard_scancode(&s,0x1e); e=take(&s); assert(e.data.key.ascii=='A');
  input_keyboard_scancode(&s,0x9e); (void)take(&s);
  input_keyboard_scancode(&s,0xe0); input_keyboard_scancode(&s,0x48);
  e=take(&s); assert(e.data.key.keycode==INPUT_KEY_UP && !e.data.key.ascii);
  input_keyboard_scancode(&s,0xe0); input_keyboard_scancode(&s,0xc8);
  e=take(&s); assert(e.data.key.keycode==INPUT_KEY_UP && !e.data.key.pressed);
  input_keyboard_scancode(&s,0xe1); for(int i=0;i<5;i++)input_keyboard_scancode(&s,0);
  input_stats st; input_get_stats(&s,&st); assert(st.malformed_keyboard_bytes==0);
}
static void test_mouse(void) {
  input_state s; input_state_init(&s);
  input_mouse_byte(&s,0x19); input_mouse_byte(&s,0xfe); input_mouse_byte(&s,0x03);
  input_event e=take(&s); assert(e.type==INPUT_EVENT_MOUSE && e.data.mouse.dx==-2 && e.data.mouse.dy==-3 && e.data.mouse.buttons==1);
  input_mouse_byte(&s,0x08); input_mouse_byte(&s,0x00); input_mouse_byte(&s,0x00);
  e=take(&s); assert(e.data.mouse.dx==0 && e.data.mouse.dy==0 && e.data.mouse.buttons==0);
  input_mouse_byte(&s,0x00);
  input_stats st; input_get_stats(&s,&st); assert(st.malformed_mouse_packets==1);
  input_mouse_byte(&s,0x48); input_mouse_byte(&s,0); input_mouse_byte(&s,0);
  input_get_stats(&s,&st); assert(st.malformed_mouse_packets==2 && st.queued_events==0);

  input_state wide; input_state_init(&wide);
  input_mouse_byte(&wide,0x08); input_mouse_byte(&wide,0xc8); input_mouse_byte(&wide,0x00);
  e=take(&wide); assert(e.data.mouse.dx==200 && e.data.mouse.dy==0);
  input_mouse_byte(&wide,0x18); input_mouse_byte(&wide,0x38); input_mouse_byte(&wide,0x00);
  e=take(&wide); assert(e.data.mouse.dx==-200 && e.data.mouse.dy==0);
  input_mouse_byte(&wide,0x08); input_mouse_byte(&wide,0x00); input_mouse_byte(&wide,0xc8);
  e=take(&wide); assert(e.data.mouse.dx==0 && e.data.mouse.dy==-200);
  input_mouse_byte(&wide,0x28); input_mouse_byte(&wide,0x00); input_mouse_byte(&wide,0x38);
  e=take(&wide); assert(e.data.mouse.dx==0 && e.data.mouse.dy==200);
}
static void test_queue(void) {
  input_state s; input_state_init(&s);
  for(unsigned i=0;i<INPUT_EVENT_CAPACITY;i++) input_keyboard_scancode(&s,0x1e), input_keyboard_scancode(&s,0x9e);
  input_stats st; input_get_stats(&s,&st); assert(st.queued_events==INPUT_EVENT_CAPACITY && st.dropped_events==INPUT_EVENT_CAPACITY);
  uint64_t prior=0; input_event e;
  while(input_poll(&s,&e)){ assert(e.sequence>prior); prior=e.sequence; }
  assert(prior==INPUT_EVENT_CAPACITY);
  input_keyboard_scancode(&s,0x1e); e=take(&s); assert(e.sequence>prior);
  input_keyboard_scancode(&s,0x9e); e=take(&s); assert(e.sequence>prior);
}
int main(void) { test_keyboard(); test_mouse(); test_queue(); return 0; }
