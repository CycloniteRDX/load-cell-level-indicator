#include <avr/interrupt.h>
#include <avr/io.h>

#include <stdint.h>

#include "hal_critical.h"
#include "hal_time.h"


#ifndef F_CPU
#error "F_CPU must be defined"
#endif

#if F_CPU != 16000000UL
#error "hal_time_avr.c currently requires F_CPU = 16 MHz"
#endif


#define HAL_TIME_TIMER1_PRESCALER 64UL

#define HAL_TIME_TIMER1_TICKS_PER_MS \
    (F_CPU / HAL_TIME_TIMER1_PRESCALER / 1000UL)

#define HAL_TIME_TIMER1_COMPARE_VALUE \
    (HAL_TIME_TIMER1_TICKS_PER_MS - 1UL)


/*
 * Direct Timer1 timekeeping will be implemented
 * incrementally in the following commits.
 *
 * Expected configuration:
 *
 * Timer clock:
 *     16 MHz / 64 = 250 kHz
 *
 * Timer ticks per millisecond:
 *     250000 / 1000 = 250
 *
 * OCR1A:
 *     250 - 1 = 249
 */