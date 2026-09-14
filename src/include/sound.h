#ifndef SOUND_H
#define SOUND_H

#include "stdint.h"

// Частоти базових нот 4-ї та 5-ї октав (Гц)
#define NOTE_C4  261
#define NOTE_CS4 277
#define NOTE_D4  293
#define NOTE_DS4 311
#define NOTE_E4  329
#define NOTE_F4  349
#define NOTE_FS4 370
#define NOTE_G4  392
#define NOTE_GS4 415
#define NOTE_A4  440
#define NOTE_AS4 466
#define NOTE_B4  493

#define NOTE_C5  523
#define NOTE_CS5 554
#define NOTE_D5  587
#define NOTE_DS5 622
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_G5  784
#define NOTE_A5  880
#define NOTE_B5  987

#define NOTE_PAUSE 0

struct note {
    uint32_t frequency;
    uint32_t duration_ms;
};

void sound_play_tone(uint32_t freq, uint32_t duration_ms);
void sound_play_melody(const struct note* melody, int length);

// Системні звукові ефекти
void sound_boot(void);
void sound_error(void);
void sound_shutdown(void);

#endif