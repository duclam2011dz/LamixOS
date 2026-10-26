/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_BOOT_CONTRACT_H
#define LAMIX_BOOT_CONTRACT_H
#include "lbp.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Đây là API native của thư viện, không phải wire structs. */
typedef enum lbp_status {
  LBP_OK,
  LBP_INVALID,
  LBP_TRUNCATED,
  LBP_OVERFLOW,
  LBP_UNSUPPORTED,
  LBP_CONFLICT,
  LBP_BUFFER_TOO_SMALL
} lbp_status;
typedef struct lbp_span {
  uint64_t physical;
  const uint8_t *bytes;
  uint64_t size;
} lbp_span;
typedef struct lbp_view {
  const uint8_t *bytes;
  uint64_t size;
  uint32_t count;
} lbp_view;
lbp_status lbp_validate_note(const void *bytes, uint64_t size,
                             uint64_t available_features);
lbp_status lbp_validate(const void *bytes, uint64_t size, const lbp_span *spans,
                        uint64_t span_count, lbp_view *view);
/* View chỉ lấy từ lbp_validate thành công và blob phải còn bất biến. */
lbp_status lbp_record_at(const lbp_view *view, uint32_t index,
                         uint64_t *offset);
/* Decode một map firmware vào caller-owned array, không firmware I/O. */
lbp_status lbp_decode_e820(const void *bytes, uint64_t size, uint32_t stride,
                           lbp_memory *output, uint64_t capacity,
                           uint64_t *needed);
lbp_status lbp_decode_uefi(const void *bytes, uint64_t size, uint32_t stride,
                           uint32_t version, lbp_memory *output,
                           uint64_t capacity, uint64_t *needed);
/* Scratch và output không alias input/nhau; required scratch = 4*(n+r)+2
 * descriptors. */
lbp_status lbp_normalize(const lbp_memory *input, uint64_t count,
                         const lbp_reservation *reservations,
                         uint64_t reservation_count, lbp_memory *scratch,
                         uint64_t scratch_capacity, lbp_memory *output,
                         uint64_t capacity, uint64_t *needed);
#ifdef __cplusplus
}
#endif
#endif
