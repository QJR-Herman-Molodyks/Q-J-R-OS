#include "include/io.h"
#include "include/stdint.h"

#define PIT_BASE_FREQ 1193180

// Увімкнути генерацію частоти на системному динаміку
void speaker_play_sound(uint32_t frequency) {
    if (frequency == 0) return;

    uint32_t div = PIT_BASE_FREQ / frequency;

    // Налаштовуємо канал 2 PIT: режим генератора прямокутної хвилі
    outb(0x43, 0xB6);
    outb(0x42, (uint8_t)(div & 0xFF));        // Low byte
    outb(0x42, (uint8_t)((div >> 8) & 0xFF)); // High byte

    // Підключаємо вихід каналу 2 до динаміка (біти 0 та 1 порту 0x61)
    uint8_t state = inb(0x61);
    if ((state & 0x03) != 0x03) {
        outb(0x61, state | 0x03);
    }
}

// Заглушити динамік
void speaker_mute(void) {
    uint8_t state = inb(0x61) & 0xFC; // Очищуємо біти 0 та 1
    outb(0x61, state);
}

// Короткий системний сигнал (Beep)
void beep(void) {
    speaker_play_sound(1000); // 1000 Гц

    // Проста I/O-затримка (поки PIT таймер відраховує тики)
    for (volatile int i = 0; i < 500000; i++) {
        io_wait();
    }

    speaker_mute();
}