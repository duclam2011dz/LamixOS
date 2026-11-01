; SPDX-License-Identifier: GPL-3.0-only
BITS 64
default rel
section .text
extern m21_entry_state
extern m21_kernel_stack
extern m21_fast_return
extern m21_return_rip
extern m21_return_flags
extern m21_return_rsp
extern m21_return_rax
extern m21_syscall_dispatch_frame
global m21_syscall_entry

; The BSP has IF masked by IA32_FMASK. This temporary area is therefore not
; shared with an interrupt path; the complete register set is copied before
; switching away from the user stack.
m21_syscall_entry:
    mov [m21_entry_state + 0], rax
    mov [m21_entry_state + 8], rcx
    mov [m21_entry_state + 16], rdx
    mov [m21_entry_state + 24], rbx
    mov [m21_entry_state + 32], rbp
    mov [m21_entry_state + 40], rsi
    mov [m21_entry_state + 48], rdi
    mov [m21_entry_state + 56], r8
    mov [m21_entry_state + 64], r9
    mov [m21_entry_state + 72], r10
    mov [m21_entry_state + 80], r11
    mov [m21_entry_state + 88], r12
    mov [m21_entry_state + 96], r13
    mov [m21_entry_state + 104], r14
    mov [m21_entry_state + 112], r15
    mov [m21_entry_state + 120], rsp
    mov rsp, [m21_kernel_stack]
    test rsp, rsp
    jz m21_bad_stack

    push qword 0x1b
    push qword [m21_entry_state + 120]
    push qword [m21_entry_state + 80]
    push qword 0x23
    push qword [m21_entry_state + 8]
    push qword 0
    push qword 0
    push qword [m21_entry_state + 0]
    push qword [m21_entry_state + 8]
    push qword [m21_entry_state + 16]
    push qword [m21_entry_state + 24]
    push qword [m21_entry_state + 32]
    push qword [m21_entry_state + 40]
    push qword [m21_entry_state + 48]
    push qword [m21_entry_state + 56]
    push qword [m21_entry_state + 64]
    push qword [m21_entry_state + 72]
    push qword [m21_entry_state + 80]
    push qword [m21_entry_state + 88]
    push qword [m21_entry_state + 96]
    push qword [m21_entry_state + 104]
    push qword [m21_entry_state + 112]
    mov rbx, rsp
    mov rdi, rbx
    and rsp, -16
    call m21_syscall_dispatch_frame
    mov rsp, rbx
    mov rbx, rax
    cmp qword [m21_fast_return], 1
    jne m21_iret_return
    cmp rbx, rsp
    jne m21_iret_return

    mov rax, [rbx + 136]
    mov [m21_return_rip], rax
    mov rax, [rbx + 152]
    mov [m21_return_flags], rax
    mov rax, [rbx + 160]
    mov [m21_return_rsp], rax
    mov rax, [rbx + 112]
    mov [m21_return_rax], rax
    mov r15, [rbx + 0]
    mov r14, [rbx + 8]
    mov r13, [rbx + 16]
    mov r12, [rbx + 24]
    mov r10, [rbx + 40]
    mov r9,  [rbx + 48]
    mov r8,  [rbx + 56]
    mov rdi, [rbx + 64]
    mov rsi, [rbx + 72]
    mov rbp, [rbx + 80]
    mov rdx, [rbx + 96]
    mov rbx, [rbx + 88]
    mov rcx, [m21_return_rip]
    mov r11, [m21_return_flags]
    mov rax, [m21_return_rax]
    mov rsp, [m21_return_rsp]
    o64 sysret

m21_iret_return:
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
m21_bad_stack:
    cli
    hlt
    jmp m21_bad_stack

section .note.GNU-stack noalloc noexec nowrite progbits
