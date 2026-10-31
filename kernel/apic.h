/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_KERNEL_APIC_H
#define LAMIX_KERNEL_APIC_H
#include <stdint.h>

typedef enum apic_status { APIC_OK, APIC_INVALID, APIC_RANGE } apic_status;
typedef struct apic_irq_mode { uint8_t active_low, level_triggered; } apic_irq_mode;

apic_status apic_decode_isa_override(uint16_t flags, apic_irq_mode *mode);
apic_status apic_make_redirection(uint8_t vector, uint8_t destination,
                                  apic_irq_mode mode, int masked,
                                  uint64_t *value);
apic_status apic_elapsed_counts(uint32_t start_count, uint32_t end_count,
                                uint32_t *elapsed_counts);
apic_status apic_calibrate_periodic(uint32_t elapsed_lapic_counts,
                                    uint32_t reference_ticks,
                                    uint16_t pit_divisor,
                                    uint32_t target_hz,
                                    uint32_t *initial_count);
#endif
