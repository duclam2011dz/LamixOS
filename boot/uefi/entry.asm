; SPDX-License-Identifier: GPL-3.0-only
BITS 64
default rel
section .text
global efi_entry
extern efi_main
efi_entry:
    sub rsp, 40
    call efi_main
.halt:
    cli
    hlt
    jmp .halt
section .note.GNU-stack noalloc noexec nowrite progbits
