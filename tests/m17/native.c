/* SPDX-License-Identifier: GPL-3.0-only */
#include "virtio_gpu.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

typedef struct __attribute__((packed)) header {
  uint32_t type, flags;
  uint64_t fence;
  uint32_t context;
  uint8_t ring, pad[3];
} header;
typedef struct __attribute__((packed)) mode {
  uint32_t x, y, width, height, enabled, flags;
} mode;
typedef struct __attribute__((packed)) display_response {
  header header;
  mode modes[16];
} display_response;
typedef struct mock {
  virtio_gpu_memory *memory;
  uint8_t regs[64];
  uint32_t commands[16];
  size_t command_count;
  int version_feature;
  int reset_stuck;
  int dma_fail;
  int response_override;
  int used_id_override;
  int used_length_override;
  uint32_t display_width, display_height;
} mock;

static uint32_t read_common(void *context, uint16_t offset, unsigned width) {
  mock *m = context;
  if (offset == 4 && width == 4) {
    uint32_t selector = 0;
    memcpy(&selector, m->regs, sizeof(selector));
    return selector ? (m->version_feature ? 1u : 0u) : 0;
  }
  if (offset == 24 && width == 2) return 256;
  if (width == 1) return m->regs[offset];
  if (width == 2) { uint16_t v; memcpy(&v, m->regs + offset, 2); return v; }
  uint32_t v; memcpy(&v, m->regs + offset, 4); return v;
}

static void write_common(void *context, uint16_t offset, unsigned width,
                         uint32_t value) {
  mock *m = context;
  if (offset == 20 && width == 1 && value == 0 && m->reset_stuck)
    return;
  if (width == 1) m->regs[offset] = (uint8_t)value;
  else if (width == 2) { uint16_t v = (uint16_t)value; memcpy(m->regs + offset, &v, 2); }
  else memcpy(m->regs + offset, &value, 4);
}

static int dma_address(void *context, const void *pointer, size_t length,
                       uint64_t *physical) {
  mock *m = context;
  if (!pointer || !length || !physical) return 0;
  if (m->dma_fail) return 0;
  *physical = (uintptr_t)pointer;
  return 1;
}

static void notify(void *context, uint16_t queue, uint16_t notify_offset) {
  (void)queue; (void)notify_offset;
  mock *m = context;
  virtio_gpu_queue *q = &m->memory->queue;
  assert(q->descriptors[0].flags == 1 && q->descriptors[0].next == 1);
  assert(q->descriptors[1].flags == 2);
  uint64_t request_address = q->descriptors[0].address;
  uint64_t response_address = q->descriptors[1].address;
  uint32_t type = *(uint32_t *)(uintptr_t)request_address;
  *(uint32_t *)(uintptr_t)response_address =
      m->response_override ? (uint32_t)m->response_override :
      type == 0x0100 ? 0x1101 : 0x1100;
  if (type == 0x0100) {
    display_response *display = (display_response *)(uintptr_t)response_address;
    display->modes[0] = (mode){0, 0,
        m->display_width ? m->display_width : 64,
        m->display_height ? m->display_height : 32, 1, 0};
  }
  assert(m->command_count < sizeof(m->commands) / sizeof(m->commands[0]));
  m->commands[m->command_count++] = type;
  uint16_t next = (uint16_t)(*q->used_index + 1);
  q->used_ring[*q->used_index % q->size] =
      (virtio_gpu_used_element){(uint32_t)m->used_id_override,
        m->used_length_override ? (uint32_t)m->used_length_override :
                                  q->descriptors[1].length};
  *q->used_index = next;
}

static uint8_t descriptors[4096] __attribute__((aligned(4096)));
static uint8_t available[4096] __attribute__((aligned(4096)));
static uint8_t used[4096] __attribute__((aligned(4096)));
static uint8_t commands[sizeof(uint32_t) * 8 + VIRTIO_GPU_MAX_BACKING_PAGES * 16]
    __attribute__((aligned(4096)));
static uint8_t backing_pages[VIRTIO_GPU_MAX_BACKING_PAGES * sizeof(uint64_t)];

static void test_queue_validation(void) {
  virtio_gpu_queue queue;
  mock m = {0};
  virtio_gpu_ops ops = {&m, read_common, write_common, notify, dma_address, 0};
  assert(virtio_gpu_queue_init(&queue, &ops, 3, descriptors, sizeof(descriptors),
      0x1000, available, sizeof(available), 0x2000, used, sizeof(used),
      0x3000) == VIRTIO_GPU_INVALID_ARGUMENT);
  assert(virtio_gpu_queue_init(&queue, &ops, 256, descriptors,
      sizeof(descriptors), 0x1000, available, sizeof(available), 0x2000,
      used, sizeof(used), 0x3000) == VIRTIO_GPU_OK);
}

static void test_gpu_commands(void) {
  mock m = {.version_feature = 1};
  virtio_gpu_memory memory = {
      .descriptor_storage = descriptors,
      .descriptor_storage_size = sizeof(descriptors),
      .available_storage = available,
      .available_storage_size = sizeof(available),
      .used_storage = used,
      .used_storage_size = sizeof(used),
      .command_storage = commands,
      .command_storage_size = sizeof(commands),
      .backing_pages = (uint64_t *)backing_pages,
      .backing_page_capacity = VIRTIO_GPU_MAX_BACKING_PAGES};
  m.memory = &memory;
  uint32_t pixels[64 * 32] = {0};
  virtio_gpu_ops ops = {&m, read_common, write_common, notify, dma_address, 0};
  virtio_gpu gpu;
  assert(virtio_gpu_initialize(&gpu, &ops, &memory, 64, 32, pixels,
      sizeof(pixels)) == VIRTIO_GPU_OK);
  assert(gpu.initialized && gpu.width == 64 && gpu.height == 32);
  assert(m.command_count == 6);
  assert(m.commands[0] == 0x0100 && m.commands[1] == 0x0101 &&
         m.commands[2] == 0x0106 && m.commands[3] == 0x0103 &&
         m.commands[4] == 0x0105 && m.commands[5] == 0x0104);
  *memory.queue.available_index = UINT16_MAX - 1;
  *memory.queue.used_index = UINT16_MAX - 1;
  memory.queue.last_used = UINT16_MAX - 1;
  assert(virtio_gpu_present(&gpu, pixels, 64 * 32) == VIRTIO_GPU_OK);
  assert(*memory.queue.available_index == 0 && *memory.queue.used_index == 0);
  assert(m.command_count == 8);
  assert(virtio_gpu_present(&gpu, pixels, 64 * 31) ==
         VIRTIO_GPU_INVALID_ARGUMENT);
  assert(virtio_gpu_shutdown(&gpu));
  assert(!gpu.initialized && !memory.queue.ready);
}

static void test_queue_and_device_errors(void) {
  mock m = {.version_feature = 1};
  virtio_gpu_memory memory = {
      .descriptor_storage = descriptors, .descriptor_storage_size = sizeof(descriptors),
      .available_storage = available, .available_storage_size = sizeof(available),
      .used_storage = used, .used_storage_size = sizeof(used),
      .command_storage = commands, .command_storage_size = sizeof(commands),
      .backing_page_capacity = VIRTIO_GPU_MAX_BACKING_PAGES};
  m.memory = &memory;
  uint32_t pixels[64 * 32] = {0};
  virtio_gpu_ops ops = {&m, read_common, write_common, notify, dma_address, 0};
  virtio_gpu gpu;
  assert(virtio_gpu_initialize(&gpu, &ops, &memory, 64, 32, pixels,
      sizeof(pixels)) == VIRTIO_GPU_OK);
  m.used_id_override = 1;
  assert(virtio_gpu_present(&gpu, pixels, 64 * 32) == VIRTIO_GPU_BAD_QUEUE);
  m.used_id_override = 0;
  m.used_length_override = 8;
  assert(virtio_gpu_present(&gpu, pixels, 64 * 32) == VIRTIO_GPU_BAD_QUEUE);
  m.used_length_override = 0;
  m.response_override = 0x1205;
  assert(virtio_gpu_present(&gpu, pixels, 64 * 32) == VIRTIO_GPU_DEVICE_ERROR);
  m.response_override = 0;
  m.dma_fail = 1;
  assert(virtio_gpu_present(&gpu, pixels, 64 * 32) == VIRTIO_GPU_DMA_ERROR);
  assert(virtio_gpu_shutdown(&gpu));
}

static void test_missing_feature(void) {
  mock m = {0};
  virtio_gpu_memory memory = {
      .descriptor_storage = descriptors, .descriptor_storage_size = sizeof(descriptors),
      .available_storage = available, .available_storage_size = sizeof(available),
      .used_storage = used, .used_storage_size = sizeof(used),
      .command_storage = commands, .command_storage_size = sizeof(commands),
      .backing_page_capacity = VIRTIO_GPU_MAX_BACKING_PAGES};
  m.memory = &memory;
  virtio_gpu_ops ops = {&m, read_common, write_common, notify, dma_address, 0};
  uint32_t pixels[64 * 32] = {0};
  virtio_gpu gpu;
  assert(virtio_gpu_initialize(&gpu, &ops, &memory, 64, 32, pixels,
      sizeof(pixels)) == VIRTIO_GPU_UNSUPPORTED);
  assert(!gpu.initialized);
}

static void test_display_bounds(void) {
  mock m = {.version_feature = 1, .display_width = 32, .display_height = 16};
  virtio_gpu_memory memory = {
      .descriptor_storage = descriptors, .descriptor_storage_size = sizeof(descriptors),
      .available_storage = available, .available_storage_size = sizeof(available),
      .used_storage = used, .used_storage_size = sizeof(used),
      .command_storage = commands, .command_storage_size = sizeof(commands),
      .backing_page_capacity = VIRTIO_GPU_MAX_BACKING_PAGES};
  m.memory = &memory;
  uint32_t pixels[64 * 32] = {0};
  virtio_gpu_ops ops = {&m, read_common, write_common, notify, dma_address, 0};
  virtio_gpu gpu;
  assert(virtio_gpu_initialize(&gpu, &ops, &memory, 64, 32, pixels,
      sizeof(pixels)) == VIRTIO_GPU_OK);
  assert(gpu.width == 32 && gpu.height == 16);
  assert(virtio_gpu_present(&gpu, pixels, 32 * 16) == VIRTIO_GPU_OK);
  m.reset_stuck = 1;
  assert(!virtio_gpu_shutdown(&gpu));
  assert(!gpu.initialized && memory.queue.ready);
}

int main(void) {
  test_queue_validation();
  test_gpu_commands();
  test_queue_and_device_errors();
  test_missing_feature();
  test_display_bounds();
  return 0;
}
