#ifndef TIMER_H
#define TIMER_H

#include "stdint.h"

// Initialization PIT on a set frequency (by default: 1000 Hz = 1 ms per tick)
void timer_init(uint32_t frequency_hz);

// Interroption IRQ0 Handler (calling from assembler ISR 32)
void timer_handler_main(void);

// Delays (Затримки)
void sleep_ms(uint32_t milliseconds);
void sleep(int seconds);

// Receiving a count of milliseconds from the moment of the system start
uint32_t timer_get_ticks(void);

#endif // TIMER_H