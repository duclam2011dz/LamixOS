/* SPDX-License-Identifier: GPL-3.0-only */
#include "graphics.h"

#define FONT_WIDTH 8
#define FONT_HEIGHT 16
#include "font8x16.h"

static int valid_mask(uint32_t mask) {
  if (!mask)
    return 0;
  while (!(mask & 1u))
    mask >>= 1;
  return (mask & (mask + 1u)) == 0;
}

static unsigned mask_shift(uint32_t mask) {
  unsigned shift = 0;
  while (!(mask & 1u)) { mask >>= 1; ++shift; }
  return shift;
}

static uint32_t channel(uint32_t value, uint32_t mask) {
  unsigned shift = mask_shift(mask), bits = 0;
  uint32_t field = mask >> shift;
  while (field) { ++bits; field >>= 1; }
  uint64_t maximum = (UINT64_C(1) << bits) - 1;
  return (uint32_t)(((uint64_t)value * maximum + 127) / 255 << shift) & mask;
}

static void mark(graphics_surface *s, uint32_t x, uint32_t y,
                 uint32_t width, uint32_t height) {
  if (!width || !height)
    return;
  if (!s->dirty) {
    s->damage = (graphics_rect){x, y, width, height};
    s->dirty = 1;
    return;
  }
  uint32_t x0 = x < s->damage.x ? x : s->damage.x;
  uint32_t y0 = y < s->damage.y ? y : s->damage.y;
  uint64_t right = (uint64_t)x + width;
  uint64_t old_right = (uint64_t)s->damage.x + s->damage.width;
  uint64_t bottom = (uint64_t)y + height;
  uint64_t old_bottom = (uint64_t)s->damage.y + s->damage.height;
  if (old_right > right) right = old_right;
  if (old_bottom > bottom) bottom = old_bottom;
  s->damage = (graphics_rect){x0, y0, (uint32_t)(right - x0),
                              (uint32_t)(bottom - y0)};
}

int graphics_init(graphics_surface *s, uint32_t *backbuffer,
                  size_t backbuffer_bytes, uint8_t *framebuffer,
                  uint64_t framebuffer_length, uint32_t width,
                  uint32_t height, uint32_t pitch, uint32_t red_mask,
                  uint32_t green_mask, uint32_t blue_mask) {
  if (!s || !backbuffer || !framebuffer || !width || !height ||
      width > INT32_MAX || height > INT32_MAX ||
      (uint64_t)width * height > SIZE_MAX / sizeof(uint32_t) ||
      backbuffer_bytes < (uint64_t)width * height * sizeof(uint32_t) ||
      pitch < (uint64_t)width * 4 || (pitch & 3) ||
      (uint64_t)pitch * height > framebuffer_length ||
      !valid_mask(red_mask) || !valid_mask(green_mask) ||
      !valid_mask(blue_mask) || (red_mask & green_mask) ||
      (red_mask & blue_mask) || (green_mask & blue_mask))
    return 0;
  *s = (graphics_surface){backbuffer, framebuffer, framebuffer_length,
                          width, height, pitch,
                          {red_mask, green_mask, blue_mask}, 0, {0, 0, 0, 0}};
  for (uint64_t i = 0, count = (uint64_t)width * height; i < count; ++i)
    backbuffer[i] = 0;
  mark(s, 0, 0, width, height);
  return 1;
}

void graphics_clear(graphics_surface *s, uint32_t color) {
  if (!s || !s->pixels)
    return;
  uint64_t count = (uint64_t)s->width * s->height;
  for (uint64_t i = 0; i < count; ++i)
    s->pixels[i] = color & UINT32_C(0x00ffffff);
  mark(s, 0, 0, s->width, s->height);
}

void graphics_pixel(graphics_surface *s, int32_t x, int32_t y,
                    uint32_t color) {
  if (!s || !s->pixels || x < 0 || y < 0 || (uint32_t)x >= s->width ||
      (uint32_t)y >= s->height)
    return;
  s->pixels[(uint64_t)(uint32_t)y * s->width + (uint32_t)x] =
      color & UINT32_C(0x00ffffff);
  mark(s, (uint32_t)x, (uint32_t)y, 1, 1);
}

void graphics_fill_rect(graphics_surface *s, int32_t x, int32_t y,
                        uint32_t width, uint32_t height, uint32_t color) {
  if (!s || !s->pixels || !width || !height)
    return;
  int64_t x0 = x, y0 = y, x1 = x0 + width, y1 = y0 + height;
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > s->width) x1 = s->width;
  if (y1 > s->height) y1 = s->height;
  if (x0 >= x1 || y0 >= y1)
    return;
  color &= UINT32_C(0x00ffffff);
  for (int64_t row = y0; row < y1; ++row)
    for (int64_t col = x0; col < x1; ++col)
      s->pixels[(uint64_t)row * s->width + (uint64_t)col] = color;
  mark(s, (uint32_t)x0, (uint32_t)y0, (uint32_t)(x1 - x0),
       (uint32_t)(y1 - y0));
}

void graphics_rect_outline(graphics_surface *s, int32_t x, int32_t y,
                           uint32_t width, uint32_t height, uint32_t color) {
  if (!width || !height)
    return;
  int64_t right = (int64_t)x + width - 1;
  int64_t bottom = (int64_t)y + height - 1;
  graphics_fill_rect(s, x, y, width, 1, color);
  if (height > 1 && bottom <= INT32_MAX)
    graphics_fill_rect(s, x, (int32_t)bottom, width, 1, color);
  if (height > 2 && y < INT32_MAX) {
    graphics_fill_rect(s, x, y + 1, 1, height - 2, color);
    if (width > 1 && right <= INT32_MAX)
      graphics_fill_rect(s, (int32_t)right, y + 1, 1, height - 2, color);
  }
}

static int clip_code(int64_t x, int64_t y, uint32_t width, uint32_t height) {
  return (x < 0 ? 1 : x >= width ? 2 : 0) |
         (y < 0 ? 4 : y >= height ? 8 : 0);
}

void graphics_line(graphics_surface *s, int32_t x0, int32_t y0,
                   int32_t x1, int32_t y1, uint32_t color) {
  if (!s || !s->pixels)
    return;
  int64_t ax=x0, ay=y0, bx=x1, by=y1;
  for (unsigned tries = 0; tries < 8; ++tries) {
    int ca=clip_code(ax,ay,s->width,s->height), cb=clip_code(bx,by,s->width,s->height);
    if (!(ca|cb)) break;
    if (ca&cb) return;
    int c=ca?ca:cb; int64_t x, y;
    if (c&8) { if (by==ay) return; y=s->height-1; x=ax+(bx-ax)*(y-ay)/(by-ay); }
    else if (c&4) { if (by==ay) return; y=0; x=ax+(bx-ax)*(y-ay)/(by-ay); }
    else if (c&2) { if (bx==ax) return; x=s->width-1; y=ay+(by-ay)*(x-ax)/(bx-ax); }
    else { if (bx==ax) return; x=0; y=ay+(by-ay)*(x-ax)/(bx-ax); }
    if (c==ca) { ax=x; ay=y; } else { bx=x; by=y; }
  }
  if (clip_code(ax,ay,s->width,s->height) ||
      clip_code(bx,by,s->width,s->height)) return;
  int64_t dx=bx>=ax?bx-ax:ax-bx, sx=ax<bx?1:-1;
  int64_t dy=by>=ay?-(by-ay):-(ay-by), sy=ay<by?1:-1, err=dx+dy;
  for (;;) {
    graphics_pixel(s,(int32_t)ax,(int32_t)ay,color);
    if (ax==bx && ay==by) break;
    int64_t twice=2*err;
    if (twice>=dy) { err+=dy; ax+=sx; }
    if (twice<=dx) { err+=dx; ay+=sy; }
  }
}

static uint8_t glyph_row(unsigned char c, unsigned row) {
  if (c < 32 || c > 126) c='?';
  return graphics_font[c-32][row];
}

void graphics_text(graphics_surface *s, int32_t x, int32_t y,
                   const char *text, uint32_t color) {
  if (!s || !text) return;
  int64_t origin=x;
  for (; *text; ++text, origin += FONT_WIDTH) {
    if (origin > INT32_MAX - FONT_WIDTH) break;
    unsigned char c=(unsigned char)*text;
    if (c < 32 || c > 126) c='?';
    for (unsigned row=0; row<FONT_HEIGHT; ++row) {
      uint8_t bits=glyph_row(c,row);
      for (unsigned col=0; col<FONT_WIDTH; ++col)
        if (bits & (0x80u >> col))
          graphics_pixel(s,(int32_t)(origin+col),(int32_t)((int64_t)y+row),color);
    }
  }
}

int graphics_present(graphics_surface *s) {
  if (!s || !s->pixels || !s->framebuffer || !s->dirty)
    return s && s->pixels && s->framebuffer;
  graphics_rect r=s->damage;
  for (uint32_t y=r.y; y<r.y+r.height; ++y) {
    uint8_t *dst=s->framebuffer+(uint64_t)y*s->pitch+(uint64_t)r.x*4;
    const uint32_t *src=s->pixels+(uint64_t)y*s->width+r.x;
    for (uint32_t x=0; x<r.width; ++x) {
      uint32_t c=src[x];
      uint32_t packed=channel((c>>16)&255,s->masks[0]) |
                      channel((c>>8)&255,s->masks[1]) |
                      channel(c&255,s->masks[2]);
      dst[x*4+0]=(uint8_t)packed; dst[x*4+1]=(uint8_t)(packed>>8);
      dst[x*4+2]=(uint8_t)(packed>>16); dst[x*4+3]=(uint8_t)(packed>>24);
    }
  }
  s->dirty=0; s->damage=(graphics_rect){0,0,0,0};
  return 1;
}

int graphics_verify_present(const graphics_surface *s) {
  if (!s || !s->pixels || !s->framebuffer) return 0;
  for (uint32_t y=0; y<s->height; ++y) {
    const uint8_t *src=s->framebuffer+(uint64_t)y*s->pitch;
    for (uint32_t x=0; x<s->width; ++x) {
      uint32_t c=s->pixels[(uint64_t)y*s->width+x];
      uint32_t expected=channel((c>>16)&255,s->masks[0]) |
                        channel((c>>8)&255,s->masks[1]) |
                        channel(c&255,s->masks[2]);
      uint32_t actual=(uint32_t)src[x*4] | ((uint32_t)src[x*4+1]<<8) |
                      ((uint32_t)src[x*4+2]<<16) | ((uint32_t)src[x*4+3]<<24);
      if (actual!=expected) return 0;
    }
  }
  return 1;
}

uint64_t graphics_checksum(const graphics_surface *s) {
  if (!s || !s->pixels) return 0;
  uint64_t hash=UINT64_C(14695981039346656037);
  uint64_t count=(uint64_t)s->width*s->height;
  for (uint64_t i=0;i<count;++i) { hash^=s->pixels[i]; hash*=UINT64_C(1099511628211); }
  return hash;
}
