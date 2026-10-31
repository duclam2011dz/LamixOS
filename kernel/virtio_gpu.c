/* SPDX-License-Identifier: GPL-3.0-only */
#include "virtio_gpu.h"

#define DESC_NEXT UINT16_C(1)
#define DESC_WRITE UINT16_C(2)
#define VIRTIO_STATUS_ACK UINT32_C(1)
#define VIRTIO_STATUS_DRIVER UINT32_C(2)
#define VIRTIO_STATUS_DRIVER_OK UINT32_C(4)
#define VIRTIO_STATUS_FEATURES_OK UINT32_C(8)
#define VIRTIO_STATUS_FAILED UINT32_C(128)
#define VIRTIO_F_VERSION_1 32u
#define GPU_CMD_GET_DISPLAY_INFO UINT32_C(0x0100)
#define GPU_CMD_RESOURCE_CREATE_2D UINT32_C(0x0101)
#define GPU_CMD_RESOURCE_UNREF UINT32_C(0x0102)
#define GPU_CMD_SET_SCANOUT UINT32_C(0x0103)
#define GPU_CMD_RESOURCE_FLUSH UINT32_C(0x0104)
#define GPU_CMD_TRANSFER_TO_HOST_2D UINT32_C(0x0105)
#define GPU_CMD_RESOURCE_ATTACH_BACKING UINT32_C(0x0106)
#define GPU_RESP_OK_NODATA UINT32_C(0x1100)
#define GPU_RESP_OK_DISPLAY_INFO UINT32_C(0x1101)
#define GPU_RESP_ERR_UNSPEC UINT32_C(0x1200)
#define GPU_RESP_ERR_INVALID_RESOURCE UINT32_C(0x1203)
#define GPU_RESP_ERR_INVALID_PARAMETER UINT32_C(0x1205)
#define GPU_RESOURCE_ID UINT32_C(1)
#define GPU_MAX_WIDTH 4096u
#define GPU_MAX_HEIGHT 2160u

typedef struct __attribute__((packed)) gpu_header {
  uint32_t type, flags;
  uint64_t fence_id;
  uint32_t context_id;
  uint8_t ring_index;
  uint8_t padding[3];
} gpu_header;

typedef struct __attribute__((packed)) gpu_rect {
  uint32_t x, y, width, height;
} gpu_rect;

typedef struct __attribute__((packed)) gpu_display_mode {
  gpu_rect rect;
  uint32_t enabled, flags;
} gpu_display_mode;

typedef struct __attribute__((packed)) gpu_display_response {
  gpu_header header;
  gpu_display_mode modes[16];
} gpu_display_response;

typedef struct __attribute__((packed)) gpu_create_request {
  gpu_header header;
  uint32_t resource_id, format, width, height;
} gpu_create_request;

typedef struct __attribute__((packed)) gpu_scanout_request {
  gpu_header header;
  gpu_rect rect;
  uint32_t scanout_id, resource_id;
} gpu_scanout_request;

typedef struct __attribute__((packed)) gpu_transfer_request {
  gpu_header header;
  gpu_rect rect;
  uint64_t offset;
  uint32_t resource_id;
  uint32_t padding;
} gpu_transfer_request;

typedef struct __attribute__((packed)) gpu_flush_request {
  gpu_header header;
  gpu_rect rect;
  uint32_t resource_id;
  uint32_t padding;
} gpu_flush_request;

typedef struct __attribute__((packed)) gpu_attach_request {
  gpu_header header;
  uint32_t resource_id, entry_count;
} gpu_attach_request;

typedef struct __attribute__((packed)) gpu_memory_entry {
  uint64_t address;
  uint32_t length, padding;
} gpu_memory_entry;

static void barrier(void) { __asm__ volatile("mfence" ::: "memory"); }
static void clear_bytes(void *pointer, size_t length) {
  uint8_t *bytes = (uint8_t *)pointer;
  for (size_t i = 0; i < length; ++i) bytes[i] = 0;
}

static uint32_t common_read(const virtio_gpu_ops *ops, uint16_t offset,
                            unsigned width) {
  return ops->read_common(ops->context, offset, width);
}

static void common_write(const virtio_gpu_ops *ops, uint16_t offset,
                         unsigned width, uint32_t value) {
  ops->write_common(ops->context, offset, width, value);
  barrier();
}

static int valid_ops(const virtio_gpu_ops *ops) {
  return ops && ops->read_common && ops->write_common && ops->notify &&
         ops->dma_address;
}

static int storage_contains(const uint8_t *storage, size_t capacity,
                            const void *pointer, size_t length) {
  uintptr_t base = (uintptr_t)storage, address = (uintptr_t)pointer;
  if (!storage || !pointer || !length || address < base ||
      capacity > UINTPTR_MAX - base || length > UINTPTR_MAX - address)
    return 0;
  return address + length <= base + capacity;
}

virtio_gpu_status virtio_gpu_queue_init(
    virtio_gpu_queue *queue, const virtio_gpu_ops *ops, uint16_t size,
    uint8_t *descriptor_storage, size_t descriptor_storage_size,
    uint64_t descriptor_physical, uint8_t *available_storage,
    size_t available_storage_size, uint64_t available_physical,
    uint8_t *used_storage, size_t used_storage_size, uint64_t used_physical) {
  if (!queue || !valid_ops(ops) || !descriptor_storage ||
      !available_storage || !used_storage || !size || size > 32768 ||
      (size & (size - 1)) ||
      descriptor_storage_size < (size_t)size * sizeof(virtio_gpu_desc) ||
      available_storage_size < 6u + (size_t)size * sizeof(uint16_t) ||
      used_storage_size < 6u + (size_t)size * sizeof(virtio_gpu_used_element) ||
      (descriptor_physical & 15u) || (available_physical & 1u) ||
      (used_physical & 3u))
    return VIRTIO_GPU_INVALID_ARGUMENT;
  clear_bytes(descriptor_storage, (size_t)size * sizeof(virtio_gpu_desc));
  clear_bytes(available_storage, 6u + (size_t)size * sizeof(uint16_t));
  clear_bytes(used_storage,
              6u + (size_t)size * sizeof(virtio_gpu_used_element));
  *queue = (virtio_gpu_queue){
      .descriptors = (virtio_gpu_desc *)descriptor_storage,
      .available_flags = (volatile uint16_t *)available_storage,
      .available_index = (volatile uint16_t *)(available_storage + 2),
      .available_ring = (volatile uint16_t *)(available_storage + 4),
      .used_flags = (volatile uint16_t *)used_storage,
      .used_index = (volatile uint16_t *)(used_storage + 2),
      .used_ring = (volatile virtio_gpu_used_element *)(used_storage + 4),
      .descriptors_physical = descriptor_physical,
      .available_physical = available_physical,
      .used_physical = used_physical,
      .size = size,
      .last_used = 0,
      .ready = 1};
  return VIRTIO_GPU_OK;
}

virtio_gpu_status virtio_gpu_submit(virtio_gpu *gpu, const void *request,
                                    size_t request_size, void *response,
                                    size_t response_size) {
  if (!gpu || !gpu->memory || !gpu->memory->queue.ready || !request ||
      !request_size || !response || response_size < sizeof(gpu_header) ||
      request_size > UINT32_MAX || response_size > UINT32_MAX ||
      !storage_contains(gpu->memory->command_storage,
                        gpu->memory->command_storage_size, request,
                        request_size) ||
      !storage_contains(gpu->memory->response_storage,
                        gpu->memory->response_storage_size, response,
                        response_size))
    return VIRTIO_GPU_INVALID_ARGUMENT;
  virtio_gpu_queue *queue = &gpu->memory->queue;
  uint64_t request_physical = 0, response_physical = 0;
  if (!gpu->ops.dma_address(gpu->ops.context, request, request_size,
                            &request_physical) ||
      !gpu->ops.dma_address(gpu->ops.context, response, response_size,
                            &response_physical))
    return VIRTIO_GPU_DMA_ERROR;
  queue->descriptors[0] = (virtio_gpu_desc){0};
  queue->descriptors[1] = (virtio_gpu_desc){0};
  queue->descriptors[0] = (virtio_gpu_desc){request_physical,
      (uint32_t)request_size, DESC_NEXT, 1};
  queue->descriptors[1] = (virtio_gpu_desc){response_physical,
      (uint32_t)response_size, DESC_WRITE, 0};
  uint16_t available = *queue->available_index;
  queue->available_ring[available % queue->size] = 0;
  barrier();
  *queue->available_index = (uint16_t)(available + 1);
  barrier();
  gpu->ops.notify(gpu->ops.context, 0, queue->notify_offset);
  for (uint32_t spin = 0; spin < 50000000u; ++spin) {
    barrier();
    uint16_t current = *queue->used_index;
    if (current != queue->last_used) {
      virtio_gpu_used_element used = queue->used_ring[
          queue->last_used % queue->size];
      ++queue->last_used;
      if (used.id != 0 || used.length < sizeof(gpu_header) ||
          used.length > response_size)
        return VIRTIO_GPU_BAD_QUEUE;
      barrier();
      gpu_header *header = (gpu_header *)response;
      if (header->type != GPU_RESP_OK_NODATA &&
          header->type != GPU_RESP_OK_DISPLAY_INFO)
        return VIRTIO_GPU_DEVICE_ERROR;
      return VIRTIO_GPU_OK;
    }
    if (gpu->ops.wait_hint)
      gpu->ops.wait_hint(gpu->ops.context);
    else
      __asm__ volatile("pause");
  }
  return VIRTIO_GPU_TIMEOUT;
}

static virtio_gpu_status command(virtio_gpu *gpu, const void *request,
                                 size_t request_size, void *response,
                                 size_t response_size, uint32_t expected) {
  clear_bytes(response, response_size);
  virtio_gpu_status status = virtio_gpu_submit(
      gpu, request, request_size, response, response_size);
  if (status != VIRTIO_GPU_OK)
    return status;
  if (((gpu_header *)response)->type != expected)
    return VIRTIO_GPU_DEVICE_ERROR;
  return VIRTIO_GPU_OK;
}

static gpu_header header(uint32_t type) {
  return (gpu_header){.type = type};
}

static virtio_gpu_status prepare_backing(virtio_gpu *gpu, uint32_t *pixels,
                                         size_t pixel_count) {
  size_t byte_count = pixel_count * sizeof(uint32_t);
  size_t page_count = (byte_count + 4095u) / 4096u;
  if (page_count > gpu->memory->backing_page_capacity ||
      gpu->memory->command_storage_size <
          sizeof(gpu_attach_request) + page_count * sizeof(gpu_memory_entry))
    return VIRTIO_GPU_OUT_OF_RANGE;
  gpu_attach_request *attach =
      (gpu_attach_request *)gpu->memory->command_storage;
  attach->header = header(GPU_CMD_RESOURCE_ATTACH_BACKING);
  attach->resource_id = gpu->resource_id;
  attach->entry_count = (uint32_t)page_count;
  gpu_memory_entry *entries = (gpu_memory_entry *)(attach + 1);
  for (size_t i = 0; i < page_count; ++i) {
    uintptr_t pointer = (uintptr_t)pixels + i * 4096u;
    size_t remaining = byte_count - i * 4096u;
    size_t length = remaining < 4096u ? remaining : 4096u;
    uint64_t physical = 0;
    if (!gpu->ops.dma_address(gpu->ops.context, (const void *)pointer,
                              length, &physical))
      return VIRTIO_GPU_DMA_ERROR;
    entries[i] = (gpu_memory_entry){physical, (uint32_t)length, 0};
  }
  uint8_t *response = gpu->memory->response_storage;
  return command(gpu, attach, sizeof(*attach) + page_count * sizeof(*entries),
                 response, sizeof(gpu_header), GPU_RESP_OK_NODATA);
}

static virtio_gpu_status transfer_and_flush(virtio_gpu *gpu) {
  gpu_transfer_request *transfer =
      (gpu_transfer_request *)gpu->memory->command_storage;
  *transfer = (gpu_transfer_request){
      .header = header(GPU_CMD_TRANSFER_TO_HOST_2D),
      .rect = {0, 0, gpu->width, gpu->height}, .offset = 0,
      .resource_id = gpu->resource_id};
  uint8_t *response = gpu->memory->response_storage;
  virtio_gpu_status status = command(gpu, transfer, sizeof(*transfer),
                                     response, sizeof(gpu_header),
                                     GPU_RESP_OK_NODATA);
  if (status != VIRTIO_GPU_OK)
    return status;
  gpu_flush_request *flush = (gpu_flush_request *)gpu->memory->command_storage;
  *flush = (gpu_flush_request){
      .header = header(GPU_CMD_RESOURCE_FLUSH),
      .rect = {0, 0, gpu->width, gpu->height},
      .resource_id = gpu->resource_id};
  return command(gpu, flush, sizeof(*flush), response, sizeof(gpu_header),
                 GPU_RESP_OK_NODATA);
}

virtio_gpu_status virtio_gpu_initialize(virtio_gpu *gpu,
                                        const virtio_gpu_ops *ops,
                                        virtio_gpu_memory *memory,
                                        uint32_t width, uint32_t height,
                                        uint32_t *backbuffer,
                                        size_t backbuffer_size) {
  if (!gpu || !valid_ops(ops) || !memory || !backbuffer || !width ||
      !height || width > GPU_MAX_WIDTH || height > GPU_MAX_HEIGHT ||
      !memory->command_storage ||
      memory->command_storage_size < sizeof(gpu_transfer_request) ||
      !memory->response_storage ||
      memory->response_storage_size < sizeof(gpu_display_response) ||
      (uint64_t)width * height > SIZE_MAX / sizeof(uint32_t) ||
      backbuffer_size < (uint64_t)width * height * sizeof(uint32_t))
    return VIRTIO_GPU_INVALID_ARGUMENT;
  clear_bytes(gpu, sizeof(*gpu));
  gpu->ops = *ops;
  gpu->memory = memory;
  gpu->width = width;
  gpu->height = height;
  (void)common_read(ops, 20, 1);
  common_write(ops, 20, 1, 0);
  uint32_t reset = 0;
  for (uint32_t spin = 0; spin < 1000000u; ++spin) {
    reset = common_read(ops, 20, 1);
    if (!reset) break;
    if (ops->wait_hint) ops->wait_hint(ops->context);
  }
  if (reset) return VIRTIO_GPU_BAD_DEVICE;
  common_write(ops, 20, 1, VIRTIO_STATUS_ACK);
  common_write(ops, 20, 1, VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER);
  common_write(ops, 8, 4, 1);
  uint32_t features_low = common_read(ops, 4, 4);
  (void)features_low;
  common_write(ops, 0, 4, 1);
  uint32_t features_high = common_read(ops, 4, 4);
  if (!(features_high & (UINT32_C(1) << (VIRTIO_F_VERSION_1 - 32))))
    goto unsupported;
  common_write(ops, 8, 4, 0);
  common_write(ops, 12, 4, 0);
  common_write(ops, 8, 4, 1);
  common_write(ops, 12, 4, UINT32_C(1) << (VIRTIO_F_VERSION_1 - 32));
  common_write(ops, 20, 1,
               VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER |
                   VIRTIO_STATUS_FEATURES_OK);
  if (!(common_read(ops, 20, 1) & VIRTIO_STATUS_FEATURES_OK))
    goto unsupported;
  common_write(ops, 22, 2, 0);
  uint16_t queue_size = (uint16_t)common_read(ops, 24, 2);
  if (!queue_size || queue_size < 2 || queue_size > VIRTIO_GPU_QUEUE_SIZE)
    goto bad_queue;
  virtio_gpu_status qs = virtio_gpu_queue_init(
      &memory->queue, ops, queue_size, memory->descriptor_storage,
      memory->descriptor_storage_size, memory->queue.descriptors_physical,
      memory->available_storage, memory->available_storage_size,
      memory->queue.available_physical, memory->used_storage,
      memory->used_storage_size, memory->queue.used_physical);
  if (qs != VIRTIO_GPU_OK)
    goto bad_queue;
  memory->queue.notify_offset = (uint16_t)common_read(ops, 30, 2);
  common_write(ops, 32, 4, (uint32_t)memory->queue.descriptors_physical);
  common_write(ops, 36, 4, (uint32_t)(memory->queue.descriptors_physical >> 32));
  common_write(ops, 40, 4, (uint32_t)memory->queue.available_physical);
  common_write(ops, 44, 4, (uint32_t)(memory->queue.available_physical >> 32));
  common_write(ops, 48, 4, (uint32_t)memory->queue.used_physical);
  common_write(ops, 52, 4, (uint32_t)(memory->queue.used_physical >> 32));
  common_write(ops, 28, 2, 1);
  if (!common_read(ops, 28, 2))
    goto bad_queue;
  common_write(ops, 20, 1,
               VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER |
                   VIRTIO_STATUS_FEATURES_OK | VIRTIO_STATUS_DRIVER_OK);
  if (!(common_read(ops, 20, 1) & VIRTIO_STATUS_DRIVER_OK))
    goto bad_device;
  gpu->initialized = 1;
  gpu_header *request = (gpu_header *)memory->command_storage;
  *request = header(GPU_CMD_GET_DISPLAY_INFO);
  gpu_display_response *display =
      (gpu_display_response *)memory->response_storage;
  virtio_gpu_status result = command(gpu, request, sizeof(*request), display,
      sizeof(*display), GPU_RESP_OK_DISPLAY_INFO);
  if (result != VIRTIO_GPU_OK)
    goto fail;
  if (!display->modes[0].enabled || !display->modes[0].rect.width ||
      !display->modes[0].rect.height)
    goto unsupported_after_ready;
  if (gpu->width > display->modes[0].rect.width)
    gpu->width = display->modes[0].rect.width;
  if (gpu->height > display->modes[0].rect.height)
    gpu->height = display->modes[0].rect.height;
  gpu_create_request *create = (gpu_create_request *)memory->command_storage;
  *create = (gpu_create_request){.header = header(GPU_CMD_RESOURCE_CREATE_2D),
      .resource_id = GPU_RESOURCE_ID, .format = VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM,
      .width = gpu->width, .height = gpu->height};
  uint8_t *response = memory->response_storage;
  result = command(gpu, create, sizeof(*create), response, sizeof(gpu_header),
                   GPU_RESP_OK_NODATA);
  if (result != VIRTIO_GPU_OK)
    goto fail;
  gpu->resource_id = GPU_RESOURCE_ID;
  result = prepare_backing(gpu, backbuffer, (size_t)width * height);
  if (result != VIRTIO_GPU_OK) goto fail;
  gpu_scanout_request *scanout = (gpu_scanout_request *)memory->command_storage;
  *scanout = (gpu_scanout_request){.header = header(GPU_CMD_SET_SCANOUT),
      .rect = {0, 0, gpu->width, gpu->height}, .scanout_id = 0,
      .resource_id = GPU_RESOURCE_ID};
  result = command(gpu, scanout, sizeof(*scanout), response, sizeof(gpu_header),
                   GPU_RESP_OK_NODATA);
  if (result != VIRTIO_GPU_OK)
    goto fail;
  result = transfer_and_flush(gpu);
  if (result != VIRTIO_GPU_OK)
    goto fail;
  return VIRTIO_GPU_OK;

unsupported:
  common_write(ops, 20, 1,
               common_read(ops, 20, 1) | VIRTIO_STATUS_FAILED);
  return VIRTIO_GPU_UNSUPPORTED;
bad_queue:
  common_write(ops, 20, 1,
               common_read(ops, 20, 1) | VIRTIO_STATUS_FAILED);
  return VIRTIO_GPU_BAD_QUEUE;
bad_device:
  common_write(ops, 20, 1, common_read(ops, 20, 1) | VIRTIO_STATUS_FAILED);
  return VIRTIO_GPU_BAD_DEVICE;
unsupported_after_ready:
  result = VIRTIO_GPU_UNSUPPORTED;
fail:
  if (!virtio_gpu_shutdown(gpu)) return VIRTIO_GPU_RESET_TIMEOUT;
  return result;
}

virtio_gpu_status virtio_gpu_present(virtio_gpu *gpu, uint32_t *pixels,
                                    size_t pixel_count) {
  if (!gpu || !gpu->initialized || !pixels ||
      pixel_count != (size_t)gpu->width * gpu->height)
    return VIRTIO_GPU_INVALID_ARGUMENT;
  virtio_gpu_status status = transfer_and_flush(gpu);
  if (status != VIRTIO_GPU_OK)
    return status;
  return VIRTIO_GPU_OK;
}

int virtio_gpu_shutdown(virtio_gpu *gpu) {
  if (!gpu || !gpu->ops.write_common || !gpu->memory)
    return 0;
  /* Reset is synchronous; only after status reaches zero may caller free DMA. */
  common_write(&gpu->ops, 20, 1, 0);
  for (uint32_t spin = 0; spin < 1000000u; ++spin) {
    if (!common_read(&gpu->ops, 20, 1)) {
      gpu->memory->queue.ready = 0;
      gpu->initialized = 0;
      return 1;
    }
    if (gpu->ops.wait_hint)
      gpu->ops.wait_hint(gpu->ops.context);
  }
  gpu->initialized = 0;
  return 0;
}
