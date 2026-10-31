; SPDX-License-Identifier: GPL-3.0-only
BITS 64
section .text.entry
global kernel_entry
extern kernel_main
kernel_entry:
    cli
    cld
    mov r12, rdi
    mov r13, rsi
    call kernel_main
.halt: cli
    hlt
    jmp .halt

section .note.lamix align=4
    dd 6,32,1
    db 'LAMIX',0
    align 4,db 0
    db 'LAMIXREQ'
    dw 1,0
    dd 0
%ifdef LAMIX_M14_APIC
    dq 2,0
%else
    dq 0,0
%endif
section .note.GNU-stack noalloc noexec nowrite progbits
