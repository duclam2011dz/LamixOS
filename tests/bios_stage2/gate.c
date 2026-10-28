/* SPDX-License-Identifier: GPL-3.0-only */
#include "lamix/boot/contract.h"
/* Artifact compile/link-only. Không được đặt vào fixture bootable. Buộc linker
 * kéo các API M03 vào ELF32 để phát hiện runtime/compiler helper thiếu. */
__attribute__((section(".text.entry"), noreturn)) void stage2_core_entry(void) {
  lbp_memory input = {0}, output = {0}, scratch[10];
  lbp_view view;
  uint64_t needed = 0;
  volatile lbp_status status;
  status = lbp_decode_e820(&input, 24, 24, &output, 1, &needed);
  status = lbp_normalize(&input, 1, 0, 0, scratch, 10, &output, 1, &needed);
  status = lbp_validate_note(&input, 32, 0);
  status = lbp_validate(&input, 32, 0, 0, &view);
  status = lbp_record_at(&view, 0, &needed);
  (void)status;
  for (;;)
    __asm__ volatile("cli; hlt");
}
