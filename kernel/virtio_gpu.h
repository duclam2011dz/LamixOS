/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_KERNEL_VIRTIO_GPU_H
#define LAMIX_KERNEL_VIRTIO_GPU_H

#include <stddef.h>
#include <stdint.h>

#define VIRTIO_GPU_QUEUE_SIZE 256u
#define VIRTIO_GPU_MAX_BACKING_PAGES 8192u
#define VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM 2u

typedef struct __attribute__((packed)) virtio_gpu_desc {
  uint64_t address;
  uint32_t length;
  uint16_t flags;
  uint16_t next;
} virtio_gpu_desc;

typedef struct __attribute__((packed)) virtio_gpu_used_element {
  uint32_t id;
  uint32_t length;
} virtio_gpu_used_element;

typedef struct virtio_gpu_queue {
  virtio_gpu_desc *descriptors;
  volatile uint16_t *available_flags;
  volatile uint16_t *available_index;
  volatile uint16_t *available_ring;
  volatile uint16_t *used_flags;
  volatile uint16_t *used_index;
  volatile virtio_gpu_used_element *used_ring;
  uint64_t descriptors_physical;
  uint64_t available_physical;
  uint64_t used_physical;
  uint16_t size;
  uint16_t last_used;
  uint16_t notify_offset;
  uint8_t ready;
} virtio_gpu_queue;

typedef struct virtio_gpu_ops {
  void *context;
  uint32_t (*read_common)(void *context, uint16_t offset, unsigned width);
  void (*write_common)(void *context, uint16_t offset, unsigned width,
                       uint32_t value);
  void (*notify)(void *context, uint16_t queue, uint16_t offset);
  int (*dma_address)(void *context, const void *pointer, size_t length,
                     uint64_t *physical);
  void (*wait_hint)(void *context);
} virtio_gpu_ops;

typedef struct virtio_gpu_memory {
  virtio_gpu_queue queue;
  uint8_t *descriptor_storage;
  size_t descriptor_storage_size;
  uint8_t *available_storage;
  size_t available_storage_size;
  uint8_t *used_storage;
  size_t used_storage_size;
  uint8_t *command_storage;
  size_t command_storage_size;
  uint64_t *backing_pages;
  size_t backing_page_capacity;
} virtio_gpu_memory;

typedef struct virtio_gpu {
  virtio_gpu_ops ops;
  virtio_gpu_memory *memory;
  uint32_t resource_id;
  uint32_t width;
  uint32_t height;
  uint32_t initialized;
} virtio_gpu;

typedef enum virtio_gpu_status {
  VIRTIO_GPU_OK,
  VIRTIO_GPU_INVALID_ARGUMENT,
  VIRTIO_GPU_BAD_DEVICE,
  VIRTIO_GPU_UNSUPPORTED,
  VIRTIO_GPU_BAD_QUEUE,
  VIRTIO_GPU_DMA_ERROR,
  VIRTIO_GPU_TIMEOUT,
  VIRTIO_GPU_DEVICE_ERROR,
  VIRTIO_GPU_OUT_OF_RANGE,
  VIRTIO_GPU_RESET_TIMEOUT
} virtio_gpu_status;

virtio_gpu_status virtio_gpu_queue_init(virtio_gpu_queue *queue,
                                       const virtio_gpu_ops *ops,
                                       uint16_t size,
                                       uint8_t *descriptor_storage,
                                       size_t descriptor_storage_size,
                                       uint64_t descriptor_physical,
                                       uint8_t *available_storage,
                                       size_t available_storage_size,
                                       uint64_t available_physical,
                                       uint8_t *used_storage,
                                       size_t used_storage_size,
                                       uint64_t used_physical);
virtio_gpu_status virtio_gpu_submit(virtio_gpu *gpu, const void *request,
                                   size_t request_size, void *response,
                                   size_t response_size);
virtio_gpu_status virtio_gpu_initialize(virtio_gpu *gpu,
                                        const virtio_gpu_ops *ops,
                                        virtio_gpu_memory *memory,
                                        uint32_t width, uint32_t height,
                                        uint32_t *backbuffer,
                                        size_t backbuffer_size);
virtio_gpu_status virtio_gpu_present(virtio_gpu *gpu, uint32_t *pixels,
                                    size_t pixel_count);
int virtio_gpu_shutdown(virtio_gpu *gpu);

#endif
