; SPDX-License-Identifier: GPL-3.0-only
BITS 64
section .text.start
global _start
extern lamix_crt_start
_start:
  cld
  mov rdi, rsp
  and rsp, -16
  call lamix_crt_start
  ud2
