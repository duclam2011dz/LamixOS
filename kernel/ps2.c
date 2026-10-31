/* SPDX-License-Identifier: GPL-3.0-only */
#include "ps2.h"

#define PS2_DATA 0x60u
#define PS2_STATUS 0x64u
#define PS2_COMMAND 0x64u
#define PS2_INPUT_FULL 0x02u
#define PS2_OUTPUT_FULL 0x01u
#define PS2_AUX 0x20u
#define PS2_ACK 0xfau
#define PS2_RESEND 0xfeu
#define PS2_WAIT_LIMIT 100000u

static int wait_input(ps2_controller *c) {
  for (uint32_t n = 0; n < PS2_WAIT_LIMIT; ++n)
    if (!(c->io.read_port(c->io.context, PS2_STATUS) & PS2_INPUT_FULL)) return 1;
  ++c->timeout_count; return 0;
}
static int wait_output(ps2_controller *c, int aux, uint8_t *value) {
  for (uint32_t n = 0; n < PS2_WAIT_LIMIT; ++n) {
    uint8_t status = c->io.read_port(c->io.context, PS2_STATUS);
    if (status & PS2_OUTPUT_FULL) {
      uint8_t data = c->io.read_port(c->io.context, PS2_DATA);
      if (!!(status & PS2_AUX) == !!aux) { *value = data; return 1; }
    }
  }
  ++c->timeout_count; return 0;
}
static int command(ps2_controller *c, uint8_t value) {
  if (!wait_input(c)) return 0;
  c->io.write_port(c->io.context, PS2_COMMAND, value); return 1;
}
static int data(ps2_controller *c, uint8_t value) {
  if (!wait_input(c)) return 0;
  c->io.write_port(c->io.context, PS2_DATA, value); return 1;
}
static int read_data(ps2_controller *c, int aux, uint8_t *value) {
  return wait_output(c, aux, value);
}
static int device_write(ps2_controller *c, int aux, uint8_t value) {
  for (unsigned retry = 0; retry < 3; ++retry) {
    uint8_t response;
    if ((aux && !command(c, 0xd4)) || !data(c, value) ||
        !read_data(c, aux, &response)) return 0;
    if (response == PS2_ACK) return 1;
    if (response != PS2_RESEND) { ++c->protocol_error_count; return 0; }
  }
  ++c->protocol_error_count; return 0;
}
static int device_reset(ps2_controller *c, int aux) {
  uint8_t value;
  if (!device_write(c, aux, 0xff) || !read_data(c, aux, &value) || value != 0xaa) {
    ++c->protocol_error_count; return 0;
  }
  if (aux && (!read_data(c, 1, &value) || (value != 0x00 && value != 0x03 && value != 0x04))) {
    ++c->protocol_error_count; return 0;
  }
  return 1;
}

ps2_status ps2_initialize(ps2_controller *c, ps2_io io, int mouse_route_available) {
  if (!c || !io.read_port || !io.write_port) return PS2_INVALID_ARGUMENT;
  *c = (ps2_controller){0}; c->io = io;
  uint8_t value;
  if (!command(c, 0xad) || !command(c, 0xa7)) return PS2_TIMEOUT;
  for (unsigned i = 0; i < 32; ++i) {
    if (!(io.read_port(io.context, PS2_STATUS) & PS2_OUTPUT_FULL)) break;
    (void)io.read_port(io.context, PS2_DATA);
  }
  if (!command(c, 0x20) || !read_data(c, 0, &value)) return PS2_TIMEOUT;
  value &= (uint8_t)~0x43u;
  value |= 0x40u; /* translate Set 2 from keyboard to Set 1 */
  if (!command(c, 0x60) || !data(c, value) || !command(c, 0xaa) ||
      !read_data(c, 0, &value)) return PS2_TIMEOUT;
  if (value != 0x55) { ++c->protocol_error_count; return PS2_PROTOCOL_ERROR; }
  if (!command(c, 0xae) || !device_reset(c, 0) ||
      !device_write(c, 0, 0xf5) || !device_write(c, 0, 0xf0) ||
      !device_write(c, 0, 0x02) || !device_write(c, 0, 0xf4))
    return c->timeout_count ? PS2_TIMEOUT : PS2_PROTOCOL_ERROR;
  c->initialized = 1;

  if (mouse_route_available && command(c, 0xa8) && device_reset(c, 1) &&
      device_write(c, 1, 0xf6) && device_write(c, 1, 0xf4)) {
    c->mouse_available = 1;
  }
  if (!c->mouse_available) (void)command(c, 0xa7);
  if (!command(c, 0x20) || !read_data(c, 0, &value) || !command(c, 0x60))
    return PS2_TIMEOUT;
  value |= 0x01u; /* keyboard IRQ remains enabled if optional mouse fails */
  if (c->mouse_available) value = (uint8_t)((value | 0x02u) & (uint8_t)~0x20u);
  else value &= (uint8_t)~0x22u;
  if (!data(c, value)) return PS2_TIMEOUT;
  return PS2_OK;
}

int ps2_read_irq_byte(ps2_controller *c, uint8_t *byte, int *auxiliary) {
  if (!c || !byte || !auxiliary || !c->initialized) return 0;
  uint8_t status = c->io.read_port(c->io.context, PS2_STATUS);
  if (!(status & PS2_OUTPUT_FULL)) return 0;
  *byte = c->io.read_port(c->io.context, PS2_DATA);
  *auxiliary = !!(status & PS2_AUX);
  return 1;
}
