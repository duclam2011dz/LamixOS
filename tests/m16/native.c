/* SPDX-License-Identifier: GPL-3.0-only */
#include "graphics.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint32_t back[8 * 6];
static uint8_t front[8 * 6 * 4 + 64];
static uint32_t text_back[760 * 16];
static uint8_t text_front[760 * 16 * 4];

static void test_surface(void) {
  memset(back, 0xa5, sizeof back); memset(front, 0xcc, sizeof front);
  graphics_surface s;
  assert(graphics_init(&s,back,sizeof back,front,sizeof front,8,6,40,
                       0x00ff0000,0x0000ff00,0x000000ff));
  assert(s.dirty && s.damage.width==8 && s.damage.height==6);
  assert(graphics_present(&s) && !s.dirty);
  assert(graphics_verify_present(&s));
  graphics_clear(&s,0x00112233);
  assert(graphics_checksum(&s)==UINT64_C(0xe22c1a3cfe21c6b5));
  assert(graphics_present(&s) && graphics_verify_present(&s));
  graphics_pixel(&s,0,0,0x00112233);
  assert(graphics_present(&s));
  assert(front[0]==0x33 && front[1]==0x22 && front[2]==0x11);
  graphics_clear(&s,0x00112233);
  graphics_fill_rect(&s,-2,-1,5,4,0x00ff0000);
  graphics_pixel(&s,7,5,0x000000ff);
  graphics_pixel(&s,8,5,0x00ffffff);
  graphics_rect_outline(&s,1,1,4,3,0x0000ff00);
  graphics_line(&s,-100,3,100,3,0x00ffffff);
  graphics_line(&s,INT32_MIN,INT32_MIN,INT32_MAX,INT32_MAX,0x0000ff00);
  graphics_rect_outline(&s,INT32_MAX,INT32_MAX,UINT32_MAX,UINT32_MAX,
                        0x000000ff);
  graphics_text(&s,0,0,"A9?",0x00ffffff);
  uint64_t hash=graphics_checksum(&s);
  assert(hash!=0 && s.dirty && s.damage.x==0 && s.damage.y==0);
  assert(graphics_present(&s) && !s.dirty);
  assert(front[39]==0xcc && front[5*40+7*4]==0xff);
  assert(graphics_present(&s));
}

static void test_rejections(void) {
  graphics_surface s;
  assert(!graphics_init(&s,back,sizeof back,front,sizeof front,8,6,4,
                        0x00ff0000,0x0000ff00,0x000000ff));
  assert(!graphics_init(&s,back,1,front,sizeof front,8,6,32,
                        0x00ff0000,0x0000ff00,0x000000ff));
  assert(!graphics_init(&s,back,sizeof back,front,sizeof front,8,6,32,
                        0x00ff0000,0x00ff0000,0x000000ff));
  assert(!graphics_init(&s,back,sizeof back,front,10,8,6,40,
                        0x00ff0000,0x0000ff00,0x000000ff));
}

static void test_ascii(void) {
  char text[96];
  for (unsigned i=0;i<95;i++) text[i]=(char)(32+i);
  text[95]=0;
  graphics_surface s;
  assert(graphics_init(&s,text_back,sizeof text_back,text_front,
                       sizeof text_front,760,16,760*4,
                       0x00ff0000,0x0000ff00,0x000000ff));
  graphics_text(&s,0,0,text,0x00ffffff);
  uint64_t checksum=graphics_checksum(&s);
  assert(checksum!=0 && graphics_present(&s));
  assert(graphics_verify_present(&s));
}

int main(void) { test_surface(); test_rejections(); test_ascii(); return 0; }
