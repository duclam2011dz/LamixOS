; SPDX-License-Identifier: GPL-3.0-only
BITS 16
ORG 0

%define MBOX_SEGMENT 0xfeed

trampoline_start:
    cli
    cld
    mov ax, cs
    mov ds, ax
    mov eax, cs
    shl eax, 4
    add eax, trampoline_gdt
    mov [trampoline_gdtr + 2], eax
    lgdt [trampoline_gdtr]

    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax
    mov ax, MBOX_SEGMENT
    mov es, ax
    mov ss, ax
    mov sp, 0x1000
    mov eax, [es:0]
    mov cr3, eax

    mov ecx, 0xc0000080
    rdmsr
    or eax, (1 << 8) | (1 << 11)
    wrmsr

    mov eax, cr0
    or eax, (1 << 31) | 1
    mov cr0, eax
    mov eax, cs
    shl eax, 4
    add eax, trampoline_long_mode
    push word 0x08
    push dword eax
    o32 retf

align 8
trampoline_gdt:
    dq 0
    dq 0x00af9a000000ffff
    dq 0x00cf92000000ffff
trampoline_gdt_end:
trampoline_gdtr:
    dw trampoline_gdt_end - trampoline_gdt - 1
    dd 0

BITS 64
trampoline_long_mode:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esi, MBOX_SEGMENT
    shl esi, 4
    mov rsp, [rsi + 8]
    mov edi, [rsi + 16]
    mov rax, [rsi + 24]
    jmp rax

trampoline_end:
