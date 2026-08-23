
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

    ; Завантажуємо kernel починаючи з сектора 2
    ; mov ah, 0x02
    ; mov al, 54              ; 54 сектори
    ; mov ch, 0
    ; mov cl, 2
    ; mov dh, 0
    ; mov dl, [boot_drive]

    ; mov bx, 0x1000          ; ES:BX = 0000:1000
    ; int 0x13

    ; Завантажуємо 127 секторів ядра (до кінця файлу os.img розміром 64KB)
    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, dap
    int 0x13

    jc disk_error

    ; Увімкнення A20
    in al, 0x92
    or al, 00000010b
    out 0x92, al

    ; Завантаження GDT
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

    ; kernel_entry буде завантажений за 0x10000
    jmp 0x10000

error_msg db "Disk error!", 0
boot_drive db 0

align 4
dap:
    db 0x10      ; Розмір структури DAP (16 байт)
    db 0         ; Завжди нуль
    dw 127       ; Кількість секторів для читання (64 КБ - 1 сектор bootloader)
    dw 0x0000    ; Offset: 0
    dw 0x1000    ; Segment: 0x1000 (0x1000:0x0000 = фізична адреса 0x10000)
    dq 1         ; Початковий LBA (сектор 2 на диску має індекс 1)


times 510 - ($ - $$) db 0
dw 0xAA55
