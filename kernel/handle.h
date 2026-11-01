/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_KERNEL_HANDLE_H
#define LAMIX_KERNEL_HANDLE_H
#include <stddef.h>
#include <stdint.h>

#define LAMIX_HANDLE_INITIAL 16u
#define LAMIX_HANDLE_MAX 256u
#define LAMIX_RIGHT_TRANSFER (1u << 0)
#define LAMIX_RIGHT_CHANNEL_SEND (1u << 1)
#define LAMIX_RIGHT_CHANNEL_RECEIVE (1u << 2)
#define LAMIX_RIGHT_SHM_MAP (1u << 3)
#define LAMIX_RIGHT_SHM_READ (1u << 4)
#define LAMIX_RIGHT_SHM_WRITE (1u << 5)
#define LAMIX_RIGHT_ALL ((1u << 6) - 1)

typedef enum lamix_object_type { LAMIX_OBJECT_NONE, LAMIX_OBJECT_CHANNEL,
  LAMIX_OBJECT_SHM } lamix_object_type;
typedef struct lamix_object lamix_object;
typedef struct lamix_handle_entry {
  lamix_object *object;
  uint32_t generation, rights;
  uint8_t retired;
} lamix_handle_entry;
typedef struct lamix_handle_table {
  lamix_handle_entry *entries;
  size_t capacity;
  void *context;
  void *(*allocate)(void *, size_t);
  void (*deallocate)(void *, void *);
  uintptr_t (*lock)(void *);
  void (*unlock)(void *, uintptr_t);
} lamix_handle_table;
struct lamix_object {
  volatile uint32_t references;
  lamix_object_type type;
  void *context;
  void (*destroy)(lamix_object *);
};
typedef enum lamix_handle_status { LH_OK, LH_INVALID, LH_NOMEM, LH_BADF,
  LH_ACCESS, LH_TOO_MANY } lamix_handle_status;

void lamix_object_init(lamix_object *, lamix_object_type, void *, void (*)(lamix_object *));
void lamix_object_retain(lamix_object *);
void lamix_object_release(lamix_object *);
lamix_handle_status lamix_handle_table_init(lamix_handle_table *, void *,
  void *(*)(void *, size_t), void (*)(void *, void *), uintptr_t (*)(void *),
  void (*)(void *, uintptr_t));
void lamix_handle_table_destroy(lamix_handle_table *);
lamix_handle_status lamix_handle_install(lamix_handle_table *, lamix_object *, uint32_t, uint64_t *);
lamix_handle_status lamix_handle_lookup(lamix_handle_table *, uint64_t, lamix_object_type, uint32_t, lamix_object **);
lamix_handle_status lamix_handle_rights(lamix_handle_table *, uint64_t, uint32_t *);
lamix_handle_status lamix_handle_close(lamix_handle_table *, uint64_t);
lamix_handle_status lamix_handle_duplicate(lamix_handle_table *, uint64_t, uint32_t, uint64_t *);
lamix_handle_status lamix_handle_remove(lamix_handle_table *, uint64_t, lamix_object **);
/* Atomically validates and transfers a batch of unique source handles. MOVE
 * returns the table-owned reference; DUP returns a newly retained reference. */
lamix_handle_status lamix_handle_transfer_batch(lamix_handle_table *,
  const uint64_t *, const uint32_t *, const uint8_t *, lamix_object *const *,
  size_t, lamix_object **);
#endif
