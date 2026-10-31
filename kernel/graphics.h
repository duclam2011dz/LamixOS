/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_KERNEL_GRAPHICS_H
#define LAMIX_KERNEL_GRAPHICS_H

#include <stddef.h>
#include <stdint.h>

typedef struct graphics_rect {
  uint32_t x, y, width, height;
} graphics_rect;

typedef struct graphics_surface {
  uint32_t *pixels;
  uint8_t *framebuffer;
  uint64_t framebuffer_length;
  uint32_t width, height, pitch;
  uint32_t masks[3];
  uint32_t dirty;
  graphics_rect damage;
} graphics_surface;

int graphics_init(graphics_surface *surface, uint32_t *backbuffer,
                  size_t backbuffer_bytes, uint8_t *framebuffer,
                  uint64_t framebuffer_length, uint32_t width,
                  uint32_t height, uint32_t pitch, uint32_t red_mask,
                  uint32_t green_mask, uint32_t blue_mask);
void graphics_clear(graphics_surface *surface, uint32_t color);
void graphics_pixel(graphics_surface *surface, int32_t x, int32_t y,
                    uint32_t color);
void graphics_fill_rect(graphics_surface *surface, int32_t x, int32_t y,
                        uint32_t width, uint32_t height, uint32_t color);
void graphics_rect_outline(graphics_surface *surface, int32_t x, int32_t y,
                           uint32_t width, uint32_t height, uint32_t color);
void graphics_line(graphics_surface *surface, int32_t x0, int32_t y0,
                   int32_t x1, int32_t y1, uint32_t color);
void graphics_text(graphics_surface *surface, int32_t x, int32_t y,
                   const char *text, uint32_t color);
int graphics_present(graphics_surface *surface);
int graphics_verify_present(const graphics_surface *surface);
uint64_t graphics_checksum(const graphics_surface *surface);

#endif
