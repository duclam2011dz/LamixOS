; SPDX-License-Identifier: GPL-3.0-only
; Probe real-mode M04, không phải Stage2 hoặc kernel.
bits 16
org 0
%ifndef EXPECTED_START
%define EXPECTED_START 2048
%endif
%ifndef EXPECTED_END
%define EXPECTED_END 6143
%endif
%ifndef EXPECTED_DRIVE
%define EXPECTED_DRIVE 0x80
%endif
    cmp sp, 0x7c00
    jne fail
    mov ax, cs
    cmp ax, 0x1000
    jne fail
    mov ax, ds
    test ax, ax
    jnz fail
    mov ax, es
    test ax, ax
    jnz fail
    mov ax, ss
    test ax, ax
    jnz fail
    pushf
    pop ax
    test ax, 0x0400
    jnz fail
    test ax, 0x0200
    jz fail
    cmp dl, EXPECTED_DRIVE
    jne fail
    cmp esi, EXPECTED_START
    jne fail
    cmp edi, EXPECTED_END
    jne fail
    push cs
    pop ds
    push cs
    pop es
    mov si, 512
    mov bx, 1
sector:
    mov eax, 0x4d040000
    mov ax, bx
    cmp [si], eax
    jne fail
    add si, 4
    mov cx, 508
    cmp bx, 63
    jne pattern
    sub cx, 16
pattern:
    mov al, bl
    xor al, 0xa5
pattern_byte:
    cmp [si], al
    jne fail
    inc si
    loop pattern_byte
    inc bx
    cmp bx, 64
    jne sector
    mov di, trailer_expected
    mov cx, 16
    repe cmpsb
    jne fail
    mov si, success
    jmp print
fail:
    cld
    push cs
    pop ds
    mov si, failure
print:
    lodsb
    test al, al
    jz halt
    mov ah, al
    mov dx, 0x3fd
    mov cx, 0xffff
poll:
    in al, dx
    test al, 0x20
    jnz send
    loop poll
    jmp print
send:
    mov al, ah
    mov dx, 0x3f8
    out dx, al
    jmp print
halt:
    cli
    hlt
    jmp halt
success: db 'M04:PASS', 10, 0
failure: db 'M04:FAIL', 10, 'P', 10, 0
trailer_expected: db 'M04:END:32K!!!!!'
times 512-($-$$) db 0
%assign sector_index 1
%rep 63
    dd 0x4d040000 + sector_index
%if sector_index == 63
    times 492 db (sector_index ^ 0xa5)
    db 'M04:END:32K!!!!!'
%else
    times 508 db (sector_index ^ 0xa5)
%endif
%assign sector_index sector_index + 1
%endrep
%if ($-$$) != 32768
%error Probe phải đúng 32 KiB
%endif
