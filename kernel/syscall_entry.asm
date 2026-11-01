; SPDX-License-Identifier: GPL-3.0-only
BITS 64
default rel
section .text
%ifdef LAMIX_M24_SMP
%macro M21_SAVE 2
    mov [gs:%2], %1
%endmacro
%macro M21_PUSH 1
    push qword [gs:%1]
%endmacro
%define M21_KERNEL_STACK [gs:128]
%define M21_FAST_RETURN [gs:136]
%define M21_RETURN_RIP [gs:144]
%define M21_RETURN_FLAGS [gs:152]
%define M21_RETURN_RSP [gs:160]
%define M21_RETURN_RAX [gs:168]
%else
extern m21_entry_state
extern m21_kernel_stack
extern m21_fast_return
extern m21_return_rip
extern m21_return_flags
extern m21_return_rsp
extern m21_return_rax
%macro M21_SAVE 2
    mov [m21_entry_state + %2], %1
%endmacro
%macro M21_PUSH 1
    push qword [m21_entry_state + %1]
%endmacro
%define M21_KERNEL_STACK [m21_kernel_stack]
%define M21_FAST_RETURN [m21_fast_return]
%define M21_RETURN_RIP [m21_return_rip]
%define M21_RETURN_FLAGS [m21_return_flags]
%define M21_RETURN_RSP [m21_return_rsp]
%define M21_RETURN_RAX [m21_return_rax]
%endif
extern m21_syscall_dispatch_frame
global m21_syscall_entry

; The BSP has IF masked by IA32_FMASK. This temporary area is therefore not
; shared with an interrupt path; the complete register set is copied before
; switching away from the user stack.
m21_syscall_entry:
%ifdef LAMIX_M24_SMP
    swapgs
%endif
    M21_SAVE rax, 0
    M21_SAVE rcx, 8
    M21_SAVE rdx, 16
    M21_SAVE rbx, 24
    M21_SAVE rbp, 32
    M21_SAVE rsi, 40
    M21_SAVE rdi, 48
    M21_SAVE r8, 56
    M21_SAVE r9, 64
    M21_SAVE r10, 72
    M21_SAVE r11, 80
    M21_SAVE r12, 88
    M21_SAVE r13, 96
    M21_SAVE r14, 104
    M21_SAVE r15, 112
    M21_SAVE rsp, 120
    mov rsp, M21_KERNEL_STACK
    test rsp, rsp
    jz m21_bad_stack

    push qword 0x1b
    M21_PUSH 120
    M21_PUSH 80
    push qword 0x23
    M21_PUSH 8
    push qword 0
    push qword 0
    M21_PUSH 0
    M21_PUSH 8
    M21_PUSH 16
    M21_PUSH 24
    M21_PUSH 32
    M21_PUSH 40
    M21_PUSH 48
    M21_PUSH 56
    M21_PUSH 64
    M21_PUSH 72
    M21_PUSH 80
    M21_PUSH 88
    M21_PUSH 96
    M21_PUSH 104
    M21_PUSH 112
    mov rbx, rsp
    mov rdi, rbx
    and rsp, -16
    call m21_syscall_dispatch_frame
    mov rsp, rbx
    mov rbx, rax
    cmp qword M21_FAST_RETURN, 1
    jne m21_iret_return
    cmp rbx, rsp
    jne m21_iret_return

    mov rax, [rbx + 136]
    mov M21_RETURN_RIP, rax
    mov rax, [rbx + 152]
    mov M21_RETURN_FLAGS, rax
    mov rax, [rbx + 160]
    mov M21_RETURN_RSP, rax
    mov rax, [rbx + 112]
    mov M21_RETURN_RAX, rax
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
    mov rcx, M21_RETURN_RIP
    mov r11, M21_RETURN_FLAGS
    mov rax, M21_RETURN_RAX
    mov rsp, M21_RETURN_RSP
%ifdef LAMIX_M24_SMP
    swapgs
%endif
    o64 sysret

m21_iret_return:
%ifdef LAMIX_M24_SMP
    test byte [rbx + 144], 3
    jz .kernel_return
    swapgs
.kernel_return:
%endif
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
