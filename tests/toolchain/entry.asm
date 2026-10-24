; SPDX-License-Identifier: GPL-3.0-only
; Chỉ compile/link: không phải bootloader hoặc entry ABI của kernel.
bits 64
section .text
global m01_probe_entry
extern m01_c_probe
m01_probe_entry:
    call m01_c_probe
.stop:
    jmp .stop
section .note.GNU-stack noalloc noexec nowrite progbits
