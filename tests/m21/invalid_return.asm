; SPDX-License-Identifier: GPL-3.0-only
BITS 64
org 0x0000008000000000
mov rax, 0x0000800000000000
mov rsp, rax
xor eax, eax
syscall
ud2
