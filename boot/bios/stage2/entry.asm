; SPDX-License-Identifier: GPL-3.0-only
BITS 32
section .text.entry
global stage2_core_entry
extern stage2_main
stage2_core_entry:
    cld
    call stage2_main
    cli
.halt: hlt
    jmp .halt
section .text
global stage2_fault32
extern stage2_exception32
stage2_fault32:
    cld
    call stage2_exception32
    cli
.halt: hlt
    jmp .halt

global stage2_enter64
extern enter_context
stage2_enter64:
    cli
    in al, 0x70
    or al, 0x80
    out 0x70, al
    mov eax, [enter_context]
    mov cr3, eax
    mov eax, cr4
    and eax, ~(1<<12)
    or eax, 1<<5
    mov cr4, eax
    mov ecx, 0xc0000080
    rdmsr
    or eax, 1<<8
    cmp dword [enter_context+4], 0
    je .no_nx
    or eax, 1<<11
.no_nx:
    wrmsr
    mov eax, cr0
    or eax, 0x80010001
    mov cr0, eax
    jmp 0x28:long_entry
BITS 64
long_entry:
    lidt [abs enter_context+40]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov rsp, [abs enter_context+8]
    mov ax, 0x30
    ltr ax
    mov al, [abs enter_context+50]
    out 0x70, al
    cld
    mov rdi, [abs enter_context+16]
    mov rsi, 0x50424c58494d414c
    jmp [abs enter_context+24]
%assign i 0
%rep 256
fault_%+i:
    push qword i
    jmp fault64
%assign i i+1
%endrep
fault64:
    cli
    cld
    mov esi, fault_message
    call serial64
    mov rax, [rsp]
    mov ecx, eax
    shr eax, 4
    and ecx, 15
    and eax, 15
    mov al, [rax+hex_digits]
    call put64
    mov al, [rcx+hex_digits]
    call put64
    mov al, 10
    call put64
.halt: hlt
    jmp .halt
serial64:
    lodsb
    test al, al
    jz .done
    call put64
    jmp serial64
.done: ret
put64:
    push rax
    push rcx
    mov ecx, 65535
    mov dx, 0x3fd
.poll:
    in al, dx
    test al, 32
    jnz .send
    loop .poll
.send:
    pop rcx
    pop rax
    mov dx, 0x3f8
    out dx, al
    ret
section .rodata
fault_message: db 'M05:FAIL',10,'EXCEPTION',10,'vector=',0
hex_digits: db '0123456789abcdef'
global stage2_faults64
stage2_faults64:
%assign i 0
%rep 256
    dd fault_%+i
%assign i i+1
%endrep
section .note.GNU-stack noalloc noexec nowrite progbits
