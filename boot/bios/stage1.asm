; SPDX-License-Identifier: GPL-3.0-only
; M04: Stage1 real-mode, chỉ EDD read, không disk write hoặc kernel entry.
bits 16
org 0x7c00

HEADER equ 0x0600
ENTRIES equ 0x8000

    jmp 0:start
start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    cld
    sti
    mov [drive], dl

    ; COM1 115200 8N1, interrupts UART tắt.
    mov dx, 0x3fb
    mov al, 0x80
    out dx, al
    sub dl, 3
    mov al, 1
    out dx, al
    inc dx
    xor al, al
    out dx, al
    inc dx
    inc dx
    mov al, 3
    out dx, al
    dec dx
    dec dx
    xor al, al
    out dx, al

    mov dl, [drive]
    test dl, 0x80
    jz unsupported
    mov bx, 0x55aa
    mov ah, 0x41
    int 0x13
    jc unsupported
    cmp bx, 0xaa55
    jne unsupported
    test cl, 1
    jz unsupported

    mov si, HEADER
    mov word [si], 26
    mov ah, 0x48
    int 0x13
    jc unsupported
    cmp word [si + 24], 512
    jne unsupported
    cmp dword [si + 20], byte 0
    jne unsupported
    mov eax, [si + 16]
    sub eax, byte 34
    jc unsupported
    mov ebp, eax ; Sống qua read_sectors nhờ PUSHAD/POPAD.
    jmp short profile_ok
unsupported:
    mov al, 'D'
    jmp fail
profile_ok:
    xor eax, eax
    inc ax
    mov bx, si
    mov cx, 1
    call read_sectors
    ; Hai prefix 16 byte: kiểm đủ signature/revision/size và table shape.
    mov di, header_expected
    mov cx, 8
    repe cmpsw
    jne bad_gpt
    add si, byte 56
    mov cx, 8
    repe cmpsw
    jne bad_gpt

    jmp short header_ok
bad_gpt:
    mov al, 'G'
    jmp fail
header_ok:
    inc ax
    mov bx, ENTRIES
    mov cl, 32
    call read_sectors

    mov si, bx
    mov cl, 128
scan:
    ; REP có thể dừng sớm: luôn phục hồi pointer và counter outer loop.
    push si
    push cx
    mov di, bios_guid
    mov cx, 8
    repe cmpsw
    pop cx
    pop si
    je found
next_entry:
    sub si, byte -128
    loop scan
    mov al, 'N'
    jmp fail
found:
    mov eax, [si + 36]
    or eax, [si + 44]
    jnz bad_gpt
    mov eax, [si + 32]
    mov edi, [si + 40]
    cmp eax, byte 34
    jb bad_gpt
    cmp edi, ebp
    ja bad_gpt
    mov edx, edi
    sub edx, eax
    jc bad_gpt
    cmp edx, byte 63
    jb bad_gpt

    mov esi, eax
    push word 0x1000
    pop es
    xor bx, bx
    mov cx, 64
    call read_sectors
    xor ax, ax
    mov es, ax
    mov dl, [drive]
    sti
    jmp 0x1000:0


; EAX=LBA, ES:BX=buffer, CX=count; giữ mọi GPR32 và DS khi thành công.
; Caller SP=7c00: CALL + PUSH DS làm stack căn4 trước PUSHAD/DAP.
; BP chỉ là frame pointer bên trong helper; POPAD phục hồi EBP capacity.
read_sectors:
    push ds
    pushad
    mov bp, sp
    mov di, 3
retry:
    ; DAP mới mỗi attempt, high LBA=0; input AX không phụ thuộc BIOS result.
    push dword 0
    push dword [bp + 28]
    push es
    push bx
    push cx
    push word 16
    mov si, sp
    mov dl, [drive]
    mov ah, 0x42
    int 0x13
    jc read_failed
    cmp [si + 2], cx
    jne read_failed
    add sp, 16
    popad
    pop ds
    ret
read_failed:
    add sp, 16
    dec di
    jz exhausted
    xor ah, ah
    int 0x13
    jmp retry
exhausted:
    mov al, 'R'
    jmp fail

fail:
    ; Chỉ sửa placeholder data, không self-modifying instruction bytes.
    mov [fail_message + 9], al
    mov si, fail_message
    mov di, 11
print_message:
    lodsb
    call putc
    dec di
    jnz print_message
halt:
    cli
    hlt
    jmp halt

; Poll có giới hạn; không mắc kẹt nếu UART không sẵn sàng.
putc:
    mov ah, al
    mov dx, 0x3fd
    mov cx, 0xffff
poll:
    in al, dx
    test al, 0x20
    jnz transmit
    loop poll
    ret
transmit:
    mov al, ah
    mov dx, 0x3f8
    out dx, al
    ret

drive: db 0
fail_message: db 'M04:FAIL', 10, '?', 10
header_expected:
    db 'EFI PART'
    dd 0x00010000, 92
    dq 2
    dd 128, 128
bios_guid:
    dd 0x21686148, 0x6e6f6449, 0x65654e74, 0x49464564
stage1_used equ $ - $$
; Gate bắt buộc: assembler FAIL nếu mã/data vượt vùng boot code PMBR.
times 440 - ($ - $$) db 0
