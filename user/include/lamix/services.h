/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_USER_SERVICES_H
#define LAMIX_USER_SERVICES_H

#include <stddef.h>
#include <stdint.h>

#define LAMIX_SERVICE_MAX_COUNT 32u
#define LAMIX_SERVICE_RESTART_LIMIT 3u

typedef enum lamix_service_state {
  LAMIX_SERVICE_STOPPED,
  LAMIX_SERVICE_READY,
  LAMIX_SERVICE_RUNNING,
  LAMIX_SERVICE_FAILED
} lamix_service_state;

typedef struct lamix_service_spec {
  const char *name;
  const char *path;
  uint32_t dependencies;
  uint8_t restart_limit;
  uint32_t permissions;
} lamix_service_spec;

typedef struct lamix_service_runtime {
  uint32_t process_id;
  uint8_t restart_count;
  lamix_service_state state;
  uint64_t restart_after_tick;
} lamix_service_runtime;

typedef enum lamix_services_status {
  LAMIX_SERVICES_OK,
  LAMIX_SERVICES_INVALID,
  LAMIX_SERVICES_MISSING_DEPENDENCY,
  LAMIX_SERVICES_CYCLE
} lamix_services_status;

lamix_services_status lamix_services_order(const lamix_service_spec *specs,
                                            size_t count,
                                            uint8_t *order);
int lamix_service_restart_due(lamix_service_runtime *runtime,
                              uint8_t restart_limit, uint64_t now,
                              uint32_t *delay_ticks);

#endif
