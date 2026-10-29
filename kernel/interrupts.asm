; SPDX-License-Identifier: GPL-3.0-only
BITS 64
section .text
extern interrupt_dispatch
global interrupt_common
%macro ISR_NOERR 1
global isr_%1
isr_%1: push qword 0
        push qword %1
        jmp interrupt_common
%endmacro
%macro ISR_ERR 1
global isr_%1
isr_%1: push qword %1
        jmp interrupt_common
%endmacro
global interrupt_vectors
%assign i 0
%rep 256
  %if i=8 || i=10 || i=11 || i=12 || i=13 || i=14 || i=17 || i=21 || i=29 || i=30
    ISR_ERR i
  %else
    ISR_NOERR i
  %endif
  %assign i i+1
%endrep
section .rodata
interrupt_vectors:
%assign i 0
%rep 256
    dq isr_%+i
    %assign i i+1
%endrep
section .text
interrupt_common:
    cld
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    mov rbx, rsp
    mov rdi, rbx
    and rsp, -16
    call interrupt_dispatch
    mov rsp, rbx
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax
    add rsp, 16
    iretq
section .note.GNU-stack noalloc noexec nowrite progbits
