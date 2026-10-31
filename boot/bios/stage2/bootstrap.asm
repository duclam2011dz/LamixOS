; SPDX-License-Identifier: GPL-3.0-only
; Flat 32 KiB bootstrap. DS=CS cho data; FS=0 cho trạng thái thấp.
BITS 16
ORG 0
%define STATE 0x5000
%define HEADER 0x7fc0
start:
    cli
    cld
    mov ax, cs
    mov ds, ax
    xor ax, ax
    mov fs, ax
    mov ss, ax
    mov esp, 0x7c00
    mov [fs:STATE], dl
    mov [fs:STATE+4], esi
    mov [fs:STATE+8], edi
%ifdef LAMIX_TEST_SEGMENT_LIMITS
    call clamp_limits
%endif
    sti
    ; Header constants, reserved và CRC trước khi dùng sizes.
    cmp dword [HEADER], 'LAMI'
    jne bad_s2
    cmp dword [HEADER+4], 0x00325358
    jne bad_s2
    cmp dword [HEADER+8], 0x00400001
    jne bad_s2
    cmp dword [HEADER+12], 64
    jne bad_s2
    cmp dword [HEADER+24], 0x20000
    jne bad_s2
    mov si, HEADER+40
    mov cx, 24
.zero:
    lodsb
    test al, al
    jnz bad_s2
    loop .zero
    mov ebx, [HEADER+36]
    mov dword [HEADER+36], 0
    mov edi, 0x10000+HEADER
    mov ecx, 64
    call crc32
    mov [HEADER+36], ebx
    cmp eax, ebx
    jne bad_s2
    mov eax, [HEADER+16]
    test eax, eax
    jz bad_s2
    cmp eax, [HEADER+20]
    ja bad_s2
    cmp dword [HEADER+20], 0x40000
    ja bad_s2
    mov edx, [HEADER+28]
    cmp edx, eax
    jae bad_s2
    mov eax, [fs:STATE+4]
    add eax, 64
    jc bad_s2
    mov [next_lba], eax
    mov eax, [HEADER+16]
    add eax, 511
    shr eax, 9
    mov [sectors_left], ax
    add eax, [next_lba]
    jc bad_s2
    dec eax
    cmp eax, [fs:STATE+8]
    ja bad_s2
    ; Thu map trong bootstrap resident (12000..137ff), tokens13800..177ff.
    ; Chỉ copy sang18000 sau khi chứng minh vùng đích là RAM an toàn.
    xor ebx, ebx
    mov ax, 0x1200
    mov es, ax
    xor di, di
    mov word [calls], 0
    mov dword [fs:STATE+16], 0
.e820:
    cmp word [calls], 4096
    jae bad_e820
    cmp dword [fs:STATE+16], 256
    jae bad_e820
    inc word [calls]
    mov dword [es:di+20], 1
    mov eax, 0xe820
    mov edx, 0x534d4150
    mov ecx, 24
    int 0x15
%ifdef LAMIX_TEST_SEGMENT_LIMITS
    call clamp_limits
%endif
    jc bad_e820
%ifdef LAMIX_TEST_E820_BAD
    xor eax, eax
%endif
%ifdef LAMIX_TEST_E820_TRUNCATED
    mov ecx, 16
%endif
    cmp eax, 0x534d4150
    jne bad_e820
    cmp ecx, 20
    je .short_entry
    cmp ecx, 24
    jne bad_e820
    jmp .entry
.short_entry:
    mov dword [es:di+20], 1
.entry:
%ifdef LAMIX_TEST_E820_OVERFLOW
    mov dword [es:di], 0xffffffff
    mov dword [es:di+4], 0xffffffff
    mov dword [es:di+8], 4096
%endif
    ; Overflow kiểm trước khi map adapter nhận dữ liệu.
    mov eax, [es:di]
    mov edx, [es:di+4]
    add eax, [es:di+8]
    adc edx, [es:di+12]
    jc bad_e820
    inc dword [fs:STATE+16]
    add di, 24
    test ebx, ebx
    jz .map_done
    mov si, 0x1800
    mov cx, [calls]
    dec cx
.token:
    jcxz .save_token
    cmp ebx, [es:si]
    je bad_e820
    add si, 4
    loop .token
.save_token:
    mov [es:si], ebx
    jmp .e820
.map_done:
    ; BDA chỉ đọc: không suy ra RAM usable, chỉ thêm giới hạn conventional/EBDA.
    movzx eax, word [fs:0x413]
    shl eax, 10
    cmp eax, 0x60000
    jb bad_e820
    movzx eax, word [fs:0x40e]
    shl eax, 4
    test eax, eax
    jz .ebda_ok
    cmp eax, 0x60000
    jb bad_e820
.ebda_ok:
    mov ebx, 0x5000
.low_pages:
    call safe_page
    add ebx, 4096
    cmp ebx, 0x60000
    jb .low_pages
    mov ebx, 0x105000
    call safe_page
    push ds
    push es
    mov ax, 0x1200
    mov ds, ax
    mov ax, 0x1800
    mov es, ax
    xor si, si
    xor di, di
    mov cx, [fs:STATE+16]
    imul cx, 24
    cld
    rep movsb
    pop es
    pop ds
%ifdef LAMIX_TEST_A20_FAIL
    jmp bad_a20
%endif
    call a20_test
    test al, al
    jnz .a20_ok
    mov ax, 0x2401
    int 0x15
%ifdef LAMIX_TEST_SEGMENT_LIMITS
    call clamp_limits
%endif
    call a20_test
    test al, al
    jnz .a20_ok
    in al, 0x92
    and al, 0xfe
    or al, 2
    out 0x92, al
    call a20_test
    test al, al
    jz bad_a20
.a20_ok:
    ; EDD re-query; không dựa scratch còn lại từ Stage1.
    mov word [fs:0x5100], 30
    push ds
    xor ax, ax
    mov ds, ax
    mov si, 0x5100
    mov dl, [STATE]
    mov ah, 0x48
    int 0x13
%ifdef LAMIX_TEST_SEGMENT_LIMITS
    call clamp_limits
%endif
    pop ds
    jc bad_disk
    cmp word [fs:0x5118], 512
    jne bad_disk
    cmp dword [fs:0x5114], 0
    jne bad_disk
    mov eax, [fs:0x5110]
    mov [fs:STATE+12], eax
    mov eax, [fs:STATE+8]
    add eax, 34
    jc bad_s2
    cmp eax, [fs:STATE+12]
    jae bad_s2
    mov word [load_segment], 0x2000
.load:
    mov ax, [sectors_left]
    cmp ax, 64
    jbe .chunk
    mov ax, 64
.chunk:
    mov [read_count], ax
    call read_chunk
    movzx eax, word [read_count]
    sub [sectors_left], ax
    add [next_lba], eax
    shl ax, 5
    add [load_segment], ax
    cmp word [sectors_left], 0
    jne .load
    mov edi, 0x20000
    mov ecx, [HEADER+16]
    call crc32
    cmp eax, [HEADER+32]
    jne bad_s2
    ; Zero BSS, không dùng linker/firmware assumptions.
    mov edi, 0x20000
    add edi, [HEADER+16]
    mov ecx, [HEADER+20]
    sub ecx, [HEADER+16]
    push es
    mov eax, edi
    shr eax, 4
    mov es, ax
    and di, 15
    jecxz .zero_done
.zero_bss:
    mov byte [es:di], 0
    inc di
    jnz .zero_next
    mov ax, es
    add ax, 0x1000
    mov es, ax
.zero_next:
    dec ecx
    jnz .zero_bss
.zero_done:
    pop es
    mov eax, [HEADER+20]
    add eax, 0x20000
    mov [fs:STATE+20], eax
    mov dword [fs:STATE+32], 0x10000+thunk32
    mov eax, [HEADER+28]
    add eax, 0x20000
    mov [core_jump], eax
    cli
    ; Mask NMI trong transition; core khôi phục sau khi IDT hợp lệ.
    in al, 0x70
    mov [fs:STATE+24], al
    or al, 0x80
    out 0x70, al
    lgdt [gdtr]
    lidt [protected_idtr]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp dword 8:(0x10000+protected_entry)

; EBX=page physical. Mọi overlapping nonusable descriptor đều từ chối;
; yêu cầu ít nhất một usable descriptor bao phủ toàn page.
safe_page:
    push es
    pushad
    mov ax, 0x1200
    mov es, ax
    xor esi, esi
    mov ecx, [fs:STATE+16]
    xor edi, edi
.next:
    test dword [es:esi+20], 1
    jz .skip
    cmp dword [es:esi+4], 0
    jne .skip
    mov eax, [es:esi]
    mov edx, [es:esi+8]
    cmp dword [es:esi+12], 0
    jne .long
    add edx, eax
    jc .long
    cmp edx, ebx
    jbe .skip
    jmp .overlap
.long:
    mov edx, 0xffffffff
.overlap:
    mov ebp, ebx
    add ebp, 4096
    cmp eax, ebp
    jae .skip
    cmp dword [es:esi+16], 1
    jne bad_e820
    test dword [es:esi+20], 2
    jnz bad_e820
    cmp eax, ebx
    ja .skip
    cmp edx, ebp
    jb .skip
    mov edi, 1
.skip:
    add esi, 24
    loop .next
    test edi, edi
    jz bad_e820
    popad
    pop es
    ret

a20_test:
    push ds
    push es
    xor ax, ax
    mov ds, ax
    mov ax, 0xffff
    mov es, ax
    mov al, [0x5000]
    mov ah, [es:0x5010]
    push ax
    mov byte [0x5000], 0
    mov byte [es:0x5010], 0xff
    mov dl, [0x5000]
    pop ax
    mov [es:0x5010], ah
    mov [0x5000], al
    xor ax, ax
    cmp dl, 0xff
    sete al
    xor al, 1
    pop es
    pop ds
    ret

read_chunk:
    mov byte [attempts], 3
.retry:
    ; DAP được dựng mới, aligned 4, count không dùng lại sau BIOS.
    mov word [fs:0x5120], 16
    mov ax, [read_count]
    mov [fs:0x5122], ax
    mov word [fs:0x5124], 0
    mov ax, [load_segment]
    mov [fs:0x5126], ax
    mov eax, [next_lba]
    mov [fs:0x5128], eax
    mov dword [fs:0x512c], 0
    push ds
    xor ax, ax
    mov ds, ax
    mov si, 0x5120
    mov dl, [STATE]
    mov ah, 0x42
    int 0x13
%ifdef LAMIX_TEST_SEGMENT_LIMITS
    call clamp_limits
%endif
    pop ds
    jc .failed
    mov ax, [read_count]
    cmp [fs:0x5122], ax
    je .done
.failed:
    dec byte [attempts]
    jz bad_disk
    mov dl, [fs:STATE]
    xor ax, ax
    int 0x13
%ifdef LAMIX_TEST_SEGMENT_LIMITS
    call clamp_limits
%endif
    jmp .retry
.done:
    ret

; EDI physical, ECX byte count, return EAX IEEE CRC32.
crc32:
    push ds
    mov edx, edi
    shr edx, 4
    mov ds, dx
    mov si, di
    and si, 15
    mov eax, 0xffffffff
.byte:
    xor al, [si]
    inc si
    jnz .same_segment
    push ax
    mov ax, ds
    add ax, 0x1000
    mov ds, ax
    pop ax
.same_segment:
    mov dx, 8
.bit:
    shr eax, 1
    jnc .no_xor
    xor eax, 0xedb88320
.no_xor:
    dec dx
    jnz .bit
    dec ecx
    jnz .byte
    not eax
    pop ds
    ret

bad_s2: mov si, reason_s2
    jmp fatal
bad_e820: mov si, reason_e820
    jmp fatal
bad_a20: mov si, reason_a20
    jmp fatal
bad_disk: mov si, reason_disk
fatal:
    mov ax, 0x1000
    mov ds, ax
    cld
    push si
    mov si, failure
    call puts
    pop si
    call puts
    cli
.halt: hlt
    jmp .halt
puts:
    lodsb
    test al, al
    jz .done
    push ax
    mov cx, 0xffff
    mov dx, 0x3fd
.poll:
    in al, dx
    test al, 0x20
    jnz .send
    loop .poll
.send:
    pop ax
    mov dx, 0x3f8
    out dx, al
    jmp puts
.done: ret

%ifdef LAMIX_TEST_SEGMENT_LIMITS
; Adapter test-only: giữ GPR/flags/visible selectors, reset hidden limits64KiB
; trước và sau firmware; SS=0 là invariant bootstrap/thunk.
clamp_limits:
    pushfd
    cli
    pushad
    push ds
    push es
    push fs
    push gs
    in al, 0x70
    mov [cs:limit_nmi], al
    or al, 0x80
    out 0x70, al
    lgdt [cs:gdtr]
    lidt [cs:protected_idtr]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x18:limits_protected
limits_protected:
    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov eax, cr0
    and eax, 0xfffffffe
    mov cr0, eax
    jmp 0x1000:limits_real
limits_real:
    xor ax, ax
    mov ss, ax
    lidt [cs:real_idtr]
    mov al, [cs:limit_nmi]
    out 0x70, al
    pop gs
    pop fs
    pop es
    pop ds
    popad
    popfd
    ret
%endif
align 8
gdt:
    dq 0, 0x00cf9a000000ffff, 0x00cf92000000ffff
    ; Core thunk selectors 18/20 có base10000, limit64KiB.
    dq 0x00009a010000ffff, 0x000092000000ffff
gdt_end:
gdtr: dw gdt_end-gdt-1
    dd 0x10000+gdt
core_jump: dd 0
    dw 8
next_lba: dd 0
sectors_left: dw 0
read_count: dw 0
load_segment: dw 0
calls: dw 0
attempts: db 0
%ifdef LAMIX_TEST_SEGMENT_LIMITS
limit_nmi: db 0
%endif
failure: db 'M05:FAIL',10,0
reason_s2: db 'S2',10,0
reason_e820: db 'E820',10,0
reason_a20: db 'A20',10,0
reason_disk: db 'DISK',10,0
BITS 32
protected_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    mov esp, 0x7c00
    jmp far [0x10000+core_jump]
BITS 32
; C ABI: state5040=LBA, 5044=count; luôn đọc vào bounce8000.
thunk32:
    pushad
    sgdt [0x5060]
    sidt [0x5068]
    in al, 0x70
    mov [0x5070], al
    or al, 0x80
    out 0x70, al
    jmp word 0x18:thunk16
BITS 16
thunk16:
    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov ss, ax
    lidt [cs:real_idtr]
    mov eax, cr0
    and eax, 0xfffffffe
    mov cr0, eax
    jmp 0x1000:thunk_real
thunk_real:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov al, [0x5070]
    out 0x70, al
    sti
    mov byte [0x5050], 3
.retry:
%ifdef LAMIX_M16_VIDEO
    cmp byte [0x5080], 1
    jne .disk_read
    mov ax, [0x5082]
    mov bx, [0x5084]
    mov cx, [0x5086]
    mov dx, [0x5088]
    mov es, [0x508a]
    mov di, [0x508c]
    int 0x10
    mov [0x508e], ax
    mov dword [0x5034], 1
    jmp .done
.disk_read:
%endif
    mov dword [0x5034], 0
    mov word [0x5120], 16
    mov ax, [0x5044]
    mov [0x5122], ax
    mov dword [0x5124], 0x08000000
    mov eax, [0x5040]
    mov [0x5128], eax
    mov dword [0x512c], 0
    mov si, 0x5120
    mov dl, [0x5000]
    mov ah, 0x42
    int 0x13
%ifdef LAMIX_TEST_SEGMENT_LIMITS
    call clamp_limits
%endif
    jc .failed
    mov ax, [0x5044]
    cmp [0x5122], ax
    jne .failed
    mov dword [0x5034], 1
    jmp .done
.failed:
    dec byte [0x5050]
    jz .done
    mov dl, [0x5000]
    xor ax, ax
    int 0x13
%ifdef LAMIX_TEST_SEGMENT_LIMITS
    call clamp_limits
%endif
    jmp .retry
.done:
    cli
    in al, 0x70
    or al, 0x80
    out 0x70, al
    lgdt [0x5060]
    lidt [0x5068]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp dword 8:(0x10000+thunk_return)
BITS 32
thunk_return:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    lidt [0x5068]
    mov al, [0x5070]
    out 0x70, al
    cld
    popad
    mov eax, [0x5034]
    ret
BITS 32
transition_fault:
    cli
    cld
    mov esi, 0x10000+transition_message
.print:
    lodsb
    test al, al
    jz .halt
    push eax
    mov ecx, 65535
    mov dx, 0x3fd
.poll:
    in al, dx
    test al, 32
    jnz .send
    loop .poll
.send:
    pop eax
    mov dx, 0x3f8
    out dx, al
    jmp .print
.halt: hlt
    jmp .halt
BITS 16
align 8
protected_idt:
%rep 256
    dw transition_fault-$$,8
    db 0,0x8e
    dw 1
%endrep
protected_idtr: dw 2047
    dd 0x10000+protected_idt
transition_message: db 'M05:FAIL',10,'EXCEPTION',10,0
real_idtr: dw 1023
    dd 0
BITS 16
bootstrap_used equ $-$$
; Không để instruction/data tĩnh đè E820 scratch bên trong cửa sổ resident.
times 0x2000-($-$$) db 0
times HEADER-($-$$) db 0
; Header được packer tạo riêng; builder không thay instruction bytes.
times 64 db 0
