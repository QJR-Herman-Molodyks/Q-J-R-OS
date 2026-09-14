BITS 16
ORG 0x7C00

start:
    cli

    ; Налаштування сегментних регістрів у реальному режимі
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00          ; Стек росте вниз від 0x7C00

    ; Зберігаємо номер завантажувального накопичувача від BIOS
    mov [boot_drive], dl

    ; 1. Перевірка підтримки розширень BIOS LBA (INT 13h Extensions)
    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    ; 2. Завантаження ядра за допомогою DAP (LBA 1..70 за адресою 0x10000)
    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, dap
    int 0x13
    jc disk_error

    ; 3. Увімкнення адресної лінії A20 (Fast A20 Gate)
    in al, 0x92
    or al, 00000010b
    out 0x92, al

    ; 4. Завантаження таблиці глобальних дескрипторів (GDT)
    lgdt [gdt_descriptor]

    ; 5. Перехід у захищений 32-бітний режим (Protected Mode)
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; Far jump для очищення конвеєра інструкцій CPU
    jmp CODE_SEG:protected_mode

disk_error:
    mov si, error_msg

.print:
    lodsb
    or al, al
    jz .halt
    mov ah, 0x0E
    int 0x10
    jmp .print

.halt:
    cli
    hlt
    jmp .halt


; ===================================================
; Global Descriptor Table (GDT)
; ===================================================

gdt_start:

gdt_null:
    dq 0

gdt_code:
    dw 0xFFFF               ; Limit 0:15
    dw 0                    ; Base 0:15
    db 0                    ; Base 16:23
    db 10011010b            ; Access: Present, Ring 0, Executable, Readable
    db 11001111b            ; Granularity: 4KB blocks, 32-bit PM
    db 0                    ; Base 24:31

gdt_data:
    dw 0xFFFF               ; Limit 0:15
    dw 0                    ; Base 0:15
    db 0                    ; Base 16:23
    db 10010010b            ; Access: Present, Ring 0, Writable
    db 11001111b            ; Granularity: 4KB blocks, 32-bit PM
    db 0                    ; Base 24:31

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start


; ===================================================
; Protected Mode (32-bit)
; ===================================================

BITS 32

protected_mode:
    ; Завантажуємо селектор сегмента даних у всі сегментні регістри
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    ; Безпечний стек для 32-бітного ядра
    mov esp, 0x90000

    ; Передача керування у точку входу ядра (linker.ld -> . = 0x10000)
    jmp 0x10000


; ===================================================
; Змінні та структури
; ===================================================

error_msg db "Boot error: could not read kernel sectors via LBA!", 0
boot_drive db 0

align 4
dap:
    db 0x10                 ; Розмір структури DAP (16 байтів)
    db 0                    ; Зарезервовано (завжди 0)
    dw 70                   ; Кількість секторів (70 * 512 = 35 840 байтів, з запасом під kernel.bin)
    dw 0x0000               ; Зміщення (Offset: 0x0000)
    dw 0x1000               ; Сегмент (Segment: 0x1000 -> 0x1000:0x0000 = фізична адреса 0x10000)
    dq 1                    ; Стартовий LBA (сектор 2 на диску = індекс 1)


; ===================================================
; Завершення завантажувального сектора (512 байтів)
; ===================================================

times 510 - ($ - $$) db 0
dw 0xAA55