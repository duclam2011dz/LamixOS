/* SPDX-License-Identifier: GPL-3.0-only */
#include "apic.h"

apic_status apic_decode_isa_override(uint16_t flags, apic_irq_mode *mode) {
  if (!mode || (flags & 0xfff0u)) return APIC_INVALID;
  uint16_t polarity = flags & 3u, trigger = (flags >> 2) & 3u;
  if (polarity == 2 || trigger == 2) return APIC_INVALID;
  mode->active_low = polarity == 3;
  mode->level_triggered = trigger == 3;
  return APIC_OK;
}

apic_status apic_make_redirection(uint8_t vector, uint8_t destination,
                                  apic_irq_mode mode, int masked,
                                  uint64_t *value) {
  if (!value || vector < 32 || vector == 0xff ||
      mode.active_low > 1 || mode.level_triggered > 1)
    return APIC_INVALID;
  uint64_t result = vector | ((uint64_t)destination << 56);
  if (mode.active_low) result |= UINT64_C(1) << 13;
  if (mode.level_triggered) result |= UINT64_C(1) << 15;
  if (masked) result |= UINT64_C(1) << 16;
  *value = result;
  return APIC_OK;
}

apic_status apic_elapsed_counts(uint32_t start_count, uint32_t end_count,
                                uint32_t *elapsed_counts) {
  if (!elapsed_counts) return APIC_INVALID;
  if (end_count >= start_count) return APIC_RANGE;
  *elapsed_counts = start_count - end_count;
  return APIC_OK;
}

apic_status apic_calibrate_periodic(uint32_t elapsed_lapic_counts,
                                    uint32_t reference_ticks,
                                    uint16_t pit_divisor,
                                    uint32_t target_hz,
                                    uint32_t *initial_count) {
  if (!elapsed_lapic_counts || !reference_ticks || reference_ticks > 10000 ||
      !pit_divisor || !target_hz || target_hz > 1000 || !initial_count)
    return APIC_INVALID;
  uint64_t numerator = (uint64_t)elapsed_lapic_counts * 1193182u;
  uint64_t denominator = (uint64_t)reference_ticks * pit_divisor * target_hz;
  uint64_t count = (numerator + denominator / 2) / denominator;
  if (count < 16 || count > UINT32_MAX) return APIC_RANGE;
  *initial_count = (uint32_t)count;
  return APIC_OK;
}
