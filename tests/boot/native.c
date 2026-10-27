/* SPDX-License-Identifier: GPL-3.0-only */
#include <lamix/boot/contract.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "FAIL line %d\n", __LINE__);                             \
      return 1;                                                                \
    }                                                                          \
  } while (0)
int main(int argc, char **argv) {
  lbp_memory input[2] = {{1, 0x8fff, 1, 0, 0}, {0x3000, 0x1000, 2, 0, 0}},
             tmp[18], out[18];
  uint64_t n = 0;
  CHECK(lbp_normalize(input, 2, NULL, 0, tmp, 18, out, 18, &n) == LBP_OK);
  CHECK(n == 4 && out[0].type == LBP_RESERVED && out[1].base == 4096 &&
        out[1].type == LBP_USABLE);
  uint8_t note[32] = {'L', 'A', 'M', 'I', 'X', 'R', 'E', 'Q', 1};
  CHECK(lbp_validate_note(note, 32, 0) == LBP_OK);
  for (unsigned i = 0; i < 32; i++)
    for (unsigned j = 0; j < 8; j++) {
      note[i] ^= (uint8_t)(1u << j);
      (void)lbp_validate_note(note, 32, 7);
      note[i] ^= (uint8_t)(1u << j);
    }
  uint8_t raw[96] = {0};
  uint64_t needed;
  for (unsigned i = 0; i < sizeof(raw); i++) {
    raw[i] = 255;
    (void)lbp_decode_uefi(raw, 96, 48, 1, out, 18, &needed);
    raw[i] = 0;
  }
  if (argc == 2) {
    FILE *f = fopen(argv[1], "rb");
    CHECK(f);
    uint8_t blob[4096];
    size_t size = fread(blob, 1, sizeof(blob), f);
    CHECK(!ferror(f));
    fclose(f);
    lbp_view view;
    CHECK(lbp_validate(blob, size, NULL, 0, &view) == LBP_OK);
    for (size_t len = 0; len < size; len++)
      (void)lbp_validate(blob, len, NULL, 0, &view);
    for (size_t i = 0; i < size; i++)
      for (unsigned j = 0; j < 8; j++) {
        blob[i] ^= (uint8_t)(1u << j);
        (void)lbp_validate(blob, size, NULL, 0, &view);
        blob[i] ^= (uint8_t)(1u << j);
      }
  }
  uint32_t seed = 20261026u;
  for (unsigned run = 0; run < 256; run++) {
    lbp_memory maps[4], work[18], result[18];
    for (unsigned i = 0; i < 4; i++) {
      seed = seed * 1664525u + 1013904223u;
      maps[i] = (lbp_memory){seed % 65536u, (seed >> 16) + 1u, (seed % 9u) + 1u,
                             0, 0};
    }
    lbp_status status =
        lbp_normalize(maps, 4, NULL, 0, work, 18, result, run % 19u, &needed);
    CHECK(status == LBP_OK || status == LBP_BUFFER_TOO_SMALL);
    if (status == LBP_OK) {
      for (uint64_t i = 0; i < needed; i++) {
        CHECK(result[i].base % 4096 == 0 && result[i].length % 4096 == 0);
        if (i)
          CHECK(result[i - 1].base + result[i - 1].length <= result[i].base);
      }
    }
  }
  puts("PASS: native normalization, note/descriptor mutations, optional "
       "BootInfo mutations");
  return 0;
}
