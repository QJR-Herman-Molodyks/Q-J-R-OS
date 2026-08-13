
BITS 16
ORG 0x7C00

start:
    cli

    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    mov [boot_drive], dl

    mov ah, 0x02
    mov al, 16              
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, [boot_drive]

    mov bx, 0x1000          ; ES:BX = 0000:1000
    int 0x13

    jc disk_error

    ; Enabling A20
    in al, 0x92
    or al, 00000010b
    out 0x92, al

    ; Loading GDT
    lgdt [gdt_descriptor]

    ; Protected Mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; Far jump очищує pipeline CPU
    jmp CODE_SEG:protected_mode

disk_error:
    mov si, error_msg

.print:
    lodsb
    or al, al
    jz $
    mov ah, 0x0E
    int 0x10
    jmp .print

error_msg db "Disk error!", 0

boot_drive db 0


; =========================
; GDT
; =========================

gdt_start:

gdt_null:
    dq 0

gdt_code:
    dw 0xFFFF
    dw 0
    db 0
    db 10011010b
    db 11001111b
    db 0

gdt_data:
    dw 0xFFFF
    dw 0
    db 0
    db 10010010b
    db 11001111b
    db 0

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start


; =========================
; Protected Mode
; =========================

BITS 32

protected_mode:

    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    mov esp, 0x90000

    ; kernel_entry буде завантажений за 0x1000
    jmp 0x1000


times 510 - ($ - $$) db 0
dw 0xAA55