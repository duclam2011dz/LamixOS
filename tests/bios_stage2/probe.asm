; SPDX-License-Identifier: GPL-3.0-only
BITS 64
section .text.entry
global probe_entry
extern probe_snapshot, probe_main
probe_entry:
    db 0xf3,0x0f,0x1e,0xfa
    mov [rel probe_snapshot], rdi
    mov [rel probe_snapshot+8], rsi
    mov [rel probe_snapshot+16], rsp
    pushfq
    pop rax
    mov [rel probe_snapshot+24], rax
    mov rax, cr0
    mov [rel probe_snapshot+32], rax
    mov rax, cr3
    mov [rel probe_snapshot+40], rax
    mov rax, cr4
    mov [rel probe_snapshot+48], rax
    mov ecx, 0xc0000080
    rdmsr
    shl rdx, 32
    or rax, rdx
    mov [rel probe_snapshot+56], rax
    mov ax, cs
    mov [rel probe_snapshot+64], ax
    mov ax, ss
    mov [rel probe_snapshot+66], ax
    mov ax, ds
    mov [rel probe_snapshot+68], ax
    mov ax, es
    mov [rel probe_snapshot+70], ax
    sub rsp, 8
    call probe_main
    cli
.halt: hlt
    jmp .halt
section .note.lamix align=4
    dd 6,32,1
    db 'LAMIX',0
    align 4,db 0
    db 'LAMIXREQ'
    dw 1,0
    dd 0
    dq 0,4
section .note.GNU-stack noalloc noexec nowrite progbits
