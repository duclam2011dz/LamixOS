; SPDX-License-Identifier: GPL-3.0-only
BITS 64
section .text
global efi_transition, tramp_begin, tramp_end, tramp_gdt, tramp_gdtr_base
global tramp_idtr, tramp_idtr_base, tramp_idt, tramp_stub_table
efi_transition:
    ; Microsoft x64 ABI: fifth argument is the prepared low trampoline page.
    mov rax, [rsp+40]
    jmp rax
tramp_begin:
    cli
    cld
    mov rax, cr4
    and rax, ~(1 << 12)
    or rax, (1 << 5)
    mov cr4, rax
    lidt [rel tramp_idtr]
    mov cr3, rcx
    mov rsp, rdx
    mov rax, cr0
    or rax, (1 << 16)
    mov cr0, rax
    lgdt [rel tramp_gdtr]
    mov rdi, r8
    mov rsi, 0x50424c58494d414c
    jmp r9
tramp_stub_table:
%assign vector 0
%rep 256
    push strict dword vector
    jmp near tramp_fault_common
    times 6 db 0
%assign vector vector+1
%endrep
tramp_fault_common:
    cli
    pop rax
    mov dx, 0x3f8
    mov al, '!'
    out dx, al
    mov rbx, rax
    shr rax, 4
    and al, 15
    add al, '0'
    cmp al, '9'
    jbe .high
    add al, 7
.high:
    out dx, al
    mov rax, rbx
    and al, 15
    add al, '0'
    cmp al, '9'
    jbe .low
    add al, 7
.low:
    out dx, al
    mov rax, [rsp+8]
    mov rbx, rax
    mov dx, 0x3f8
    mov al, '<'
    out dx, al
    mov ecx, 12
.rip_hex:
    mov rax, rbx
    shr rax, cl
    and al, 15
    add al, '0'
    cmp al, '9'
    jbe .rip_digit
    add al, 7
.rip_digit:
    out dx, al
    sub ecx, 4
    jns .rip_hex
.fault_halt:
    hlt
    jmp .fault_halt
align 8
tramp_gdt:
    times 32 dq 0
tramp_gdtr:
    dw tramp_gdtr-tramp_gdt-1
tramp_gdtr_base:
    dq tramp_gdt
tramp_idtr:
    dw 4095
tramp_idtr_base:
    dq tramp_idt
align 16
tramp_idt:
    times 4096 db 0
tramp_end:
section .note.GNU-stack noalloc noexec nowrite progbits
