#include "include/timer.h"
#include "include/io.h"

#define PIT_BASE_FREQ 1193182

// Змінна обов'язково має бути volatile,
// щоб компілятор не оптимізував цикл очікування в sleep_ms
static volatile uint32_t timer_ticks = 0;

void timer_init(uint32_t frequency_hz) {
    if (frequency_hz == 0) frequency_hz = 1000;

    uint32_t divisor = PIT_BASE_FREQ / frequency_hz;

    // Регістр команд 0x43:
    // Канал 0 (00) | Доступ lo/hi byte (11) | Mode 3 (Square Wave: 011) | 16-бітний двійковий (0)
    outb(0x43, 0x36);

    // Запис 16-бітного дільника в порт Каналу 0 (0x40)
    outb(0x40, (uint8_t)(divisor & 0xFF));        // Low byte
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF)); // High byte
}

// Викликається при кожному перериванні IRQ0 (Вектор 32 / 0x20)
void timer_handler_main(void) {
    timer_ticks++;

    // Відправляємо EOI (End of Interrupt) контролеру Master PIC
    outb(0x20, 0x20);
}

uint32_t timer_get_ticks(void) {
    return timer_ticks;
}

// Затримка в мілісекундах
void sleep_ms(uint32_t milliseconds) {
    uint32_t target_ticks = timer_ticks + milliseconds;

    while (timer_ticks < target_ticks) {
        // Зупиняємо процесор до наступного переривання (IRQ),
        // щоб не навантажувати ядро даремним опитуванням
        __asm__ volatile ("hlt");
    }
}

// Затримка в секундах
void sleep(int seconds) {
    if (seconds <= 0) return;
    sleep_ms((uint32_t)seconds * 1000);
}