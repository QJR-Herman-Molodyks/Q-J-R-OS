#include "include/io.h"
#include "include/stdint.h"

#define COM1_PORT 0x3F8

int serial_init(void) {
    outb(COM1_PORT + 1, 0x00); // Вимикаємо всі переривання COM1
    outb(COM1_PORT + 3, 0x80); // Вмикаємо DLAB (Divisor Latch Access Bit)
    outb(COM1_PORT + 0, 0x03); // Дільник частоти: 3 (швидкість 38400 бод, lo byte)
    outb(COM1_PORT + 1, 0x00); // (hi byte)
    outb(COM1_PORT + 3, 0x03); // 8 біт, без парності, 1 стоп-біт (8N1)
    outb(COM1_PORT + 2, 0xC7); // Вмикаємо FIFO, очищення буфера, поріг 14 байтів
    outb(COM1_PORT + 4, 0x0B); // IRQ увімкнено, виставляємо RTS/DSR

    // Тест у режимі Loopback
    outb(COM1_PORT + 4, 0x1E); // Вмикаємо loopback
    outb(COM1_PORT + 0, 0xAE); // Відправляємо тестовий байт

    if (inb(COM1_PORT + 0) != 0xAE) {
        return -1; // Порт несправний або відсутній
    }

    // Повертаємо в нормальний робочий режим
    outb(COM1_PORT + 4, 0x0F);
    return 0;
}

static inline int serial_is_transmit_empty(void) {
    return inb(COM1_PORT + 5) & 0x20;
}

void serial_write_char(char c) {
    while (!serial_is_transmit_empty());
    outb(COM1_PORT, c);
}

void serial_print(const char* str) {
    while (*str) {
        if (*str == '\n') {
            serial_write_char('\r');
        }
        serial_write_char(*str);
        str++;
    }
}