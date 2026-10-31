/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_KERNEL_PS2_H
#define LAMIX_KERNEL_PS2_H

#include <stdint.h>

typedef uint8_t (*ps2_read_port_fn)(void *context, uint16_t port);
typedef void (*ps2_write_port_fn)(void *context, uint16_t port, uint8_t value);

typedef struct ps2_io {
  void *context;
  ps2_read_port_fn read_port;
  ps2_write_port_fn write_port;
} ps2_io;

typedef struct ps2_controller {
  ps2_io io;
  uint32_t timeout_count;
  uint32_t protocol_error_count;
  uint8_t initialized;
  uint8_t mouse_available;
} ps2_controller;

typedef enum ps2_status {
  PS2_OK,
  PS2_INVALID_ARGUMENT,
  PS2_TIMEOUT,
  PS2_PROTOCOL_ERROR
} ps2_status;

ps2_status ps2_initialize(ps2_controller *controller, ps2_io io,
                           int mouse_route_available);
int ps2_read_irq_byte(ps2_controller *controller, uint8_t *byte,
                      int *auxiliary);

#endif
