
#include "include/io.h"
#include "include/stdint.h"

#define KBD_BUFFER_SIZE 64

static volatile uint8_t kbd_buffer[KBD_BUFFER_SIZE];
static volatile int kbd_head = 0;
static volatile int kbd_tail = 0;

// Викликається з вашого обробника переривання (ISR 33 / IRQ1)
void keyboard_handler_main(void) {
    uint8_t status = inb(0x64);

    // Перевіряємо біт 0: чи є дані в вихідному буфері контролера
    if (status & 0x01) {
        uint8_t scancode = inb(0x60);
        int next = (kbd_head + 1) % KBD_BUFFER_SIZE;

        // Якщо буфер не переповнений — зберігаємо сканкод
        if (next != kbd_tail) {
            kbd_buffer[kbd_head] = scancode;
            kbd_head = next;
        }
    }

    // Відправляємо End-of-Interrupt (EOI) до Master PIC
    outb(0x20, 0x20);
}

int keyboard_has_char(void) {
    return kbd_head != kbd_tail;
}

unsigned char keyboard_pop_scancode(void) {
    if (kbd_head == kbd_tail) {
        return 0;
    }

    uint8_t scancode = kbd_buffer[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUFFER_SIZE;
    return scancode;
}


