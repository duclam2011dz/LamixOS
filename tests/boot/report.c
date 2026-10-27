/* SPDX-License-Identifier: GPL-3.0-only */
#include <lamix/boot/gpt.h>
#include <lamix/boot/lbp.h>
#include <stdio.h>
int main(void) {
  printf("{\"lbp_header\":%zu,\"lbp_record\":%zu,\"lbp_table\":%zu,\"lbp_"
         "memory\":%zu,\"lbp_kernel\":%zu,\"lbp_segment\":%zu,\"lbp_"
         "reservation\":%zu,\"lbp_entry\":%zu,\"lbp_device\":%zu,\"lbp_"
         "module\":%zu,\"lbp_framebuffer\":%zu,\"lbp_acpi\":%zu,\"lbp_note\":%"
         "zu,\"gpt_header\":%zu,\"gpt_partition\":%zu}\n",
         sizeof(lbp_header), sizeof(lbp_record), sizeof(lbp_table),
         sizeof(lbp_memory), sizeof(lbp_kernel), sizeof(lbp_segment),
         sizeof(lbp_reservation), sizeof(lbp_entry), sizeof(lbp_device),
         sizeof(lbp_module), sizeof(lbp_framebuffer), sizeof(lbp_acpi),
         sizeof(lbp_note), sizeof(lamix_gpt_header),
         sizeof(lamix_gpt_partition));
  return 0;
}
