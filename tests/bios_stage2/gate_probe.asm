; SPDX-License-Identifier: GPL-3.0-only
; Compile/link-only envelope, chưa phải kernel probe nghiệm thu.
BITS 64
section .text.entry
global probe_entry
probe_entry:
    cli
    hlt
    jmp probe_entry
section .rodata
    db 'M05 gate-only; NOT a boot proof', 0
section .data
    dq 0x123456789abcdef0
section .bss
    resb 8192
section .note.lamix align=4
    dd 6, 32, 1
    db 'LAMIX', 0
    align 4, db 0
    db 'LAMIXREQ'
    dw 1, 0
    dd 0
    dq 0, 4
section .note.GNU-stack noalloc noexec nowrite progbits
