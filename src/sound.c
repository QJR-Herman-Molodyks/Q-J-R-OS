#include "include/sound.h"
#include "include/io.h"

// Функції з audio.c
extern void speaker_play_sound(uint32_t frequency);
extern void speaker_mute(void);

// Калібрована затримка (поки немає PIT-таймера у мілісекундах)
static void sleep_ms(uint32_t ms) {
    for (uint32_t i = 0; i < ms; i++) {
        for (volatile int j = 0; j < 150000; j++) {
            __asm__ volatile ("nop");
        }
    }
}

void sound_play_tone(uint32_t freq, uint32_t duration_ms) {
    if (freq == NOTE_PAUSE) {
        speaker_mute();
    } else {
        speaker_play_sound(freq);
    }
    sleep_ms(duration_ms);
    speaker_mute();
}

void sound_play_melody(const struct note* melody, int length) {
    for (int i = 0; i < length; i++) {
        sound_play_tone(melody[i].frequency, melody[i].duration_ms);
        // Small Micro-Pause between notes so sound not Невеличка мікропауза між нотами, щоб звук не зливався
        sleep_ms(25);
    }
    speaker_mute();
}

// A pleasant ascending sequence upon OS startup (C4 -> E4 -> G4 -> C5) (Приємний перелив під час старту ОС (C4 -> E4 -> G4 -> C5))
void sound_boot(void) {
    static const struct note boot_notes[] = {
        {NOTE_C4, 100},
        {NOTE_E4, 100},
        {NOTE_G4, 100},
        {NOTE_C5, 250}
    };
    sound_play_melody(boot_notes, 4);
}

// Sound of Error or Unknown Command (Low Tones)
void sound_error(void) {
    sound_play_tone(150, 180);
    sleep_ms(40);
    sound_play_tone(110, 240);
}

// OS Shutdown Sound (decreasing sequence) (спадаюча послідовність)
void sound_shutdown(void) {
    static const struct note off_notes[] = {
        {NOTE_G4, 120},
        {NOTE_E4, 120},
        {NOTE_C4, 250}
    };
    sound_play_melody(off_notes, 3);
}