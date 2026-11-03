/* SPDX-License-Identifier: GPL-3.0-only */
#include "lamix/services.h"

lamix_services_status lamix_services_order(const lamix_service_spec *specs,
                                            size_t count,
                                            uint8_t *order) {
  if (!specs || !order || !count || count > LAMIX_SERVICE_MAX_COUNT)
    return LAMIX_SERVICES_INVALID;
  uint32_t valid = count == 32 ? UINT32_MAX : ((1u << count) - 1u);
  uint32_t done = 0;
  for (size_t i = 0; i < count; ++i) {
    if (!specs[i].name || !specs[i].path ||
        (specs[i].dependencies & (1u << i)))
      return LAMIX_SERVICES_INVALID;
  }
  for (size_t at = 0; at < count; ++at) {
    size_t found = count;
    for (size_t i = 0; i < count; ++i) {
      if ((done & (1u << i)) || (specs[i].dependencies & ~done))
        continue;
      found = i;
      break;
    }
    if (found == count) {
      for (size_t i = 0; i < count; ++i)
        if (specs[i].dependencies & ~valid)
          return LAMIX_SERVICES_MISSING_DEPENDENCY;
      return LAMIX_SERVICES_CYCLE;
    }
    order[at] = (uint8_t)found;
    done |= 1u << found;
  }
  return LAMIX_SERVICES_OK;
}

int lamix_service_restart_due(lamix_service_runtime *runtime,
                              uint8_t restart_limit, uint64_t now,
                              uint32_t *delay_ticks) {
  if (!runtime || !delay_ticks || runtime->restart_count >= restart_limit)
    return 0;
  uint32_t delay = 1u << runtime->restart_count;
  if (delay > 8)
    delay = 8;
  runtime->restart_after_tick = now + delay;
  runtime->state = LAMIX_SERVICE_READY;
  ++runtime->restart_count;
  *delay_ticks = delay;
  return 1;
}
