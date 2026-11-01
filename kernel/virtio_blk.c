/* SPDX-License-Identifier: GPL-3.0-only */
#include "virtio_blk.h"

#include <limits.h>

#define DESC_NEXT UINT16_C(1)
#define DESC_WRITE UINT16_C(2)
#define STATUS_ACK UINT32_C(1)
#define STATUS_DRIVER UINT32_C(2)
#define STATUS_DRIVER_OK UINT32_C(4)
#define STATUS_FEATURES_OK UINT32_C(8)
#define STATUS_FAILED UINT32_C(128)
#define FEATURE_RO 5u
#define FEATURE_BLK_SIZE 6u
#define FEATURE_FLUSH 9u
#define FEATURE_VERSION_1 32u
#define REQUEST_IN UINT32_C(0)
#define REQUEST_OUT UINT32_C(1)
#define REQUEST_FLUSH UINT32_C(4)
#define DEVICE_S_OK 0u
#define DEVICE_S_IOERR 1u
#define DEVICE_S_UNSUPP 2u
#define REQUEST_IDLE 0u
#define REQUEST_ACTIVE 1u
#define REQUEST_DONE 2u

typedef struct virtio_blk_header {
  uint32_t type;
  uint32_t reserved;
  uint64_t sector;
} __attribute__((packed)) virtio_blk_header;

_Static_assert(sizeof(virtio_blk_desc) == 16, "VirtIO descriptor layout");
_Static_assert(sizeof(virtio_blk_used_element) == 8, "VirtIO used layout");
_Static_assert(sizeof(virtio_blk_header) == 16, "VirtIO block header layout");

static void fence(void) { __asm__ volatile("mfence" ::: "memory"); }

static int valid_memory(const virtio_blk_ops *ops,
                        const virtio_blk_memory *memory) {
  return ops && ops->read_common && ops->write_common && ops->read_device &&
      ops->notify && ops->dma_address && ops->lock && ops->unlock && memory &&
      memory->descriptors && memory->available && memory->used &&
      memory->headers && memory->statuses &&
      memory->descriptors_size >= VIRTIO_BLK_QUEUE_SIZE *
                                      sizeof(virtio_blk_desc) &&
      memory->available_size >= 6u + VIRTIO_BLK_QUEUE_SIZE * 2u &&
      memory->used_size >= 6u + VIRTIO_BLK_QUEUE_SIZE *
                                   sizeof(virtio_blk_used_element) &&
      memory->headers_size >= VIRTIO_BLK_MAX_REQUESTS *
                                  sizeof(virtio_blk_header) &&
      memory->statuses_size >= VIRTIO_BLK_MAX_REQUESTS;
}

static uint32_t read_common(virtio_blk *driver, uint16_t offset,
                            unsigned width) {
  return driver->operations.read_common(driver->operations.context, offset,
                                         width);
}
static void write_common(virtio_blk *driver, uint16_t offset,
                         unsigned width, uint32_t value) {
  driver->operations.write_common(driver->operations.context, offset, width,
                                  value);
  fence();
}

static block_status driver_submit(block_device *device,
                                  block_request *request) {
  virtio_blk *driver = (virtio_blk *)device->driver;
  if (!driver || !request) return BLOCK_INVALID;
  uint64_t data_bytes = 0, sector = 0;
  if (request->operation == BLOCK_FLUSH && !driver->flush_supported)
    return BLOCK_UNSUPPORTED;
  if (request->operation != BLOCK_FLUSH) {
    if (request->block_count > UINT32_MAX / device->logical_block_size)
      return BLOCK_RANGE;
    data_bytes = (uint64_t)request->block_count * device->logical_block_size;
    uint64_t sectors_per_block = device->logical_block_size / 512u;
    if (request->lba > UINT64_MAX / sectors_per_block)
      return BLOCK_RANGE;
    sector = request->lba * sectors_per_block;
    if (data_bytes > UINT32_MAX || sector > UINT64_MAX - data_bytes / 512u)
      return BLOCK_RANGE;
  }

  uint64_t data_physical = 0, header_physical = 0, status_physical = 0;
  virtio_blk_header *header = 0;
  uint8_t *status = 0;
  uint16_t slot_id = VIRTIO_BLK_MAX_REQUESTS;
  driver->operations.lock(driver->operations.context);
  if (!driver->initialized || !driver->accepting || driver->offline) {
    driver->operations.unlock(driver->operations.context);
    return BLOCK_DEVICE_ERROR;
  }
  for (uint16_t i = 0; i < VIRTIO_BLK_MAX_REQUESTS; ++i)
    if (!driver->slots[i].active) { slot_id = i; break; }
  if (slot_id == VIRTIO_BLK_MAX_REQUESTS) {
    driver->operations.unlock(driver->operations.context);
    return BLOCK_BUSY;
  }
  header = (virtio_blk_header *)(void *)(driver->memory.headers +
               (size_t)slot_id * sizeof(virtio_blk_header));
  status = driver->memory.statuses + slot_id;
  if (!driver->operations.dma_address(driver->operations.context, header,
          sizeof(*header), &header_physical) ||
      !driver->operations.dma_address(driver->operations.context, status, 1,
          &status_physical) ||
      (request->operation != BLOCK_FLUSH &&
       !driver->operations.dma_address(driver->operations.context,
           request->buffer, (size_t)data_bytes, &data_physical))) {
    driver->operations.unlock(driver->operations.context);
    return BLOCK_DEVICE_ERROR;
  }
  uint16_t head = (uint16_t)(slot_id * 3u);
  uint16_t data_desc = (uint16_t)(head + 1u);
  uint16_t status_desc = (uint16_t)(head + 2u);
  *header = (virtio_blk_header){
      request->operation == BLOCK_READ ? REQUEST_IN :
      request->operation == BLOCK_WRITE ? REQUEST_OUT : REQUEST_FLUSH,
      0, sector};
  *status = UINT8_C(0xff);
  driver->descriptor_table[head] = (virtio_blk_desc){header_physical,
      sizeof(*header), DESC_NEXT, data_desc};
  if (request->operation == BLOCK_FLUSH) {
    driver->descriptor_table[head] = (virtio_blk_desc){header_physical,
        sizeof(*header), DESC_NEXT, status_desc};
  } else {
    driver->descriptor_table[data_desc] = (virtio_blk_desc){data_physical,
        (uint32_t)data_bytes,
        (uint16_t)(DESC_NEXT | (request->operation == BLOCK_READ ?
                                DESC_WRITE : 0)), status_desc};
  }
  driver->descriptor_table[status_desc] = (virtio_blk_desc){status_physical,
      1, DESC_WRITE, 0};
  driver->slots[slot_id] = (virtio_blk_slot){request,
      request->operation == BLOCK_READ ? (uint32_t)data_bytes : 0,
      head, (uint8_t)request->operation, 1};
  volatile uint16_t *ring = driver->available_ring;
  uint16_t available = *driver->available_index;
  ring[available % driver->queue_size] = head;
  fence();
  *driver->available_index = (uint16_t)(available + 1u);
  fence();
  driver->operations.notify(driver->operations.context, 0,
                            driver->notify_offset);
  driver->operations.unlock(driver->operations.context);
  return BLOCK_OK;
}

static block_status reset_internal(virtio_blk *driver) {
  struct completion { block_request *request; block_status status; } done[VIRTIO_BLK_MAX_REQUESTS];
  size_t count = 0;
  driver->operations.lock(driver->operations.context);
  driver->accepting = 0;
  write_common(driver, 20, 1, 0);
  uint32_t value = UINT32_MAX;
  for (uint32_t spin = 0; spin < 1000000u; ++spin) {
    value = read_common(driver, 20, 1);
    if (!value) break;
    if (driver->operations.wait_hint)
      driver->operations.wait_hint(driver->operations.context);
  }
  if (value) {
    driver->offline = 1;
    driver->operations.unlock(driver->operations.context);
    return BLOCK_RESET_TIMEOUT;
  }
  for (size_t i = 0; i < VIRTIO_BLK_MAX_REQUESTS; ++i) {
    if (!driver->slots[i].active) continue;
    done[count++] = (struct completion){driver->slots[i].request,
                                          BLOCK_TIMEOUT};
    driver->slots[i] = (virtio_blk_slot){0};
  }
  driver->initialized = 0;
  driver->offline = 1;
  driver->operations.unlock(driver->operations.context);
  for (size_t i = 0; i < count; ++i)
    block_request_complete(done[i].request, done[i].status);
  return BLOCK_OK;
}

block_status virtio_blk_reset(virtio_blk *driver) {
  if (!driver || !driver->initialized) return BLOCK_INVALID;
  return reset_internal(driver);
}

static block_status driver_reset(block_device *device) {
  if (!device || !device->driver) return BLOCK_INVALID;
  return reset_internal((virtio_blk *)device->driver);
}

size_t virtio_blk_poll(virtio_blk *driver, size_t budget) {
  if (!driver || !budget) return 0;
  struct completion { block_request *request; block_status status; };
  struct completion done[VIRTIO_BLK_MAX_REQUESTS];
  size_t count = 0;
  int malformed = 0;
  if (budget > VIRTIO_BLK_MAX_REQUESTS) budget = VIRTIO_BLK_MAX_REQUESTS;
  driver->operations.lock(driver->operations.context);
  if (!driver->initialized || driver->offline) {
    driver->operations.unlock(driver->operations.context);
    return 0;
  }
  while (count < budget) {
    fence();
    uint16_t current = *driver->used_index;
    uint16_t pending = (uint16_t)(current - driver->last_used);
    if (pending > driver->queue_size) {
      driver->offline = 1;
      driver->accepting = 0;
      malformed = 1;
      break;
    }
    if (current == driver->last_used) break;
    virtio_blk_used_element used = driver->used_ring[
        driver->last_used % driver->queue_size];
    ++driver->last_used;
    if (used.id >= VIRTIO_BLK_MAX_REQUESTS * 3u || used.id % 3u ||
        !driver->slots[used.id / 3u].active ||
        driver->slots[used.id / 3u].head != used.id) {
      driver->offline = 1;
      driver->accepting = 0;
      malformed = 1;
      break;
    }
    uint16_t slot_id = (uint16_t)(used.id / 3u);
    virtio_blk_slot slot = driver->slots[slot_id];
    uint8_t device_status = driver->memory.statuses[slot_id];
    uint32_t expected = slot.operation == BLOCK_READ ? slot.bytes + 1u : 1u;
    block_status status = BLOCK_OK;
    if (used.length != expected) {
      driver->offline = 1;
      driver->accepting = 0;
      malformed = 1;
      break;
    }
    if (device_status == DEVICE_S_IOERR)
      status = BLOCK_DEVICE_ERROR;
    else if (device_status == DEVICE_S_UNSUPP)
      status = BLOCK_UNSUPPORTED;
    else if (device_status != DEVICE_S_OK)
      status = BLOCK_DEVICE_ERROR;
    driver->slots[slot_id] = (virtio_blk_slot){0};
    done[count++] = (struct completion){slot.request, status};
  }
  driver->operations.unlock(driver->operations.context);
  for (size_t i = 0; i < count; ++i)
    block_request_complete(done[i].request, done[i].status);
  if (malformed) (void)reset_internal(driver);
  return count;
}

static size_t driver_poll(block_device *device, size_t budget) {
  return device && device->driver
      ? virtio_blk_poll((virtio_blk *)device->driver, budget) : 0;
}

static const block_device_ops block_ops = {
    driver_submit, driver_poll, driver_reset};

block_status virtio_blk_initialize(virtio_blk *driver,
                                   const virtio_blk_ops *operations,
                                   const virtio_blk_memory *memory) {
  if (!driver || !valid_memory(operations, memory) ||
      (memory->descriptors_physical & 15u) ||
      (memory->available_physical & 1u) ||
      (memory->used_physical & 3u))
    return BLOCK_INVALID;
  *driver = (virtio_blk){0};
  driver->operations = *operations;
  driver->memory = *memory;
  driver->descriptor_table = (virtio_blk_desc *)(void *)memory->descriptors;
  driver->available_flags = (volatile uint16_t *)(void *)memory->available;
  driver->available_index = (volatile uint16_t *)(void *)(memory->available + 2);
  driver->available_ring = (volatile uint16_t *)(void *)(memory->available + 4);
  driver->used_flags = (volatile uint16_t *)(void *)memory->used;
  driver->used_index = (volatile uint16_t *)(void *)(memory->used + 2);
  driver->used_ring = (volatile virtio_blk_used_element *)(void *)(memory->used + 4);

  write_common(driver, 20, 1, 0);
  uint32_t status = UINT32_MAX;
  for (uint32_t spin = 0; spin < 1000000u; ++spin) {
    status = read_common(driver, 20, 1);
    if (!status) break;
    if (operations->wait_hint) operations->wait_hint(operations->context);
  }
  if (status) return BLOCK_DEVICE_ERROR;
  write_common(driver, 20, 1, STATUS_ACK);
  write_common(driver, 20, 1, STATUS_ACK | STATUS_DRIVER);

  write_common(driver, 0, 4, 0);
  uint32_t features_low = read_common(driver, 4, 4);
  write_common(driver, 0, 4, 1);
  uint32_t features_high = read_common(driver, 4, 4);
  uint64_t features = ((uint64_t)features_high << 32) | features_low;
  if (!(features & (UINT64_C(1) << FEATURE_VERSION_1))) goto unsupported;
  uint64_t accepted = UINT64_C(1) << FEATURE_VERSION_1;
  if (features & (UINT64_C(1) << FEATURE_RO)) accepted |= UINT64_C(1) << FEATURE_RO;
  if (features & (UINT64_C(1) << FEATURE_BLK_SIZE)) accepted |= UINT64_C(1) << FEATURE_BLK_SIZE;
  if (features & (UINT64_C(1) << FEATURE_FLUSH)) accepted |= UINT64_C(1) << FEATURE_FLUSH;
  write_common(driver, 8, 4, 0);
  write_common(driver, 12, 4, (uint32_t)accepted);
  write_common(driver, 8, 4, 1);
  write_common(driver, 12, 4, (uint32_t)(accepted >> 32));
  write_common(driver, 20, 1,
               STATUS_ACK | STATUS_DRIVER | STATUS_FEATURES_OK);
  if (!(read_common(driver, 20, 1) & STATUS_FEATURES_OK)) goto unsupported;

  driver->flush_supported = !!(accepted & (UINT64_C(1) << FEATURE_FLUSH));
  uint8_t read_only = !!(accepted & (UINT64_C(1) << FEATURE_RO));
  uint32_t logical_size = 512;
  uint64_t capacity_sectors = 0;
  int stable_config = 0;
  for (unsigned attempt = 0; attempt < 8; ++attempt) {
    uint32_t before = read_common(driver, 21, 1);
    uint32_t low = operations->read_device(operations->context, 0, 4);
    uint32_t high = operations->read_device(operations->context, 4, 4);
    uint32_t after = read_common(driver, 21, 1);
    if (before == after) {
      capacity_sectors = (uint64_t)low | ((uint64_t)high << 32);
      stable_config = 1;
      break;
    }
  }
  if (accepted & (UINT64_C(1) << FEATURE_BLK_SIZE)) {
    uint32_t advertised = operations->read_device(operations->context, 20, 4);
    if (advertised < 512 || advertised > 65536 ||
        (advertised & (advertised - 1)) || (advertised % 512))
      goto bad_device;
    logical_size = advertised;
  }
  uint64_t sectors_per_block = logical_size / 512u;
  if (!stable_config || !capacity_sectors ||
      capacity_sectors % sectors_per_block)
    goto bad_device;

  write_common(driver, 22, 2, 0);
  uint16_t maximum = (uint16_t)read_common(driver, 24, 2);
  if (maximum < 24) goto bad_queue;
  driver->queue_size = maximum < VIRTIO_BLK_QUEUE_SIZE
                           ? maximum : VIRTIO_BLK_QUEUE_SIZE;
  if (driver->queue_size < 24 || (driver->queue_size &
                                   (driver->queue_size - 1)))
    goto bad_queue;
  for (size_t i = 0; i < driver->memory.descriptors_size; ++i)
    driver->memory.descriptors[i] = 0;
  for (size_t i = 0; i < 6u + driver->queue_size * 2u; ++i)
    driver->memory.available[i] = 0;
  for (size_t i = 0; i < 6u + driver->queue_size *
                              sizeof(virtio_blk_used_element); ++i)
    driver->memory.used[i] = 0;
  driver->notify_offset = (uint16_t)read_common(driver, 30, 2);
  write_common(driver, 32, 4, (uint32_t)memory->descriptors_physical);
  write_common(driver, 36, 4, (uint32_t)(memory->descriptors_physical >> 32));
  write_common(driver, 40, 4, (uint32_t)memory->available_physical);
  write_common(driver, 44, 4, (uint32_t)(memory->available_physical >> 32));
  write_common(driver, 48, 4, (uint32_t)memory->used_physical);
  write_common(driver, 52, 4, (uint32_t)(memory->used_physical >> 32));
  write_common(driver, 28, 2, 1);
  if (!read_common(driver, 28, 2)) goto bad_queue;
  write_common(driver, 20, 1, STATUS_ACK | STATUS_DRIVER |
                                   STATUS_FEATURES_OK | STATUS_DRIVER_OK);
  if (!(read_common(driver, 20, 1) & STATUS_DRIVER_OK)) goto bad_device;
  driver->logical_block_size = logical_size;
  driver->initialized = 1;
  driver->accepting = 1;
  if (block_device_init(&driver->device, &block_ops, driver,
          capacity_sectors / sectors_per_block, logical_size, read_only) !=
      BLOCK_OK) goto bad_device;
  return BLOCK_OK;

unsupported:
  write_common(driver, 20, 1, STATUS_ACK | STATUS_DRIVER | STATUS_FAILED);
  return BLOCK_UNSUPPORTED;
bad_queue:
  write_common(driver, 20, 1, STATUS_ACK | STATUS_DRIVER | STATUS_FAILED);
  return BLOCK_INVALID;
bad_device:
  write_common(driver, 20, 1, STATUS_ACK | STATUS_DRIVER | STATUS_FAILED);
  return BLOCK_DEVICE_ERROR;
}
