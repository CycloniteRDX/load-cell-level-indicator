#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay_basic.h>

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
 * _delay_loop_2() consumes four CPU cycles
 * per loop iteration.
 *
 * At 16 MHz:
 *
 *     16 cycles/us / 4 cycles/iteration
 *     = 4 iterations/us
 */
#define HAL_TIME_DELAY_LOOP_CYCLES 4UL

#define HAL_TIME_CPU_CYCLES_PER_US \
    (F_CPU / 1000000UL)

#define HAL_TIME_DELAY_ITERATIONS_PER_US \
    (HAL_TIME_CPU_CYCLES_PER_US / \
     HAL_TIME_DELAY_LOOP_CYCLES)

/*
 * _delay_loop_2() accepts a 16-bit iteration count.
 *
 * At four iterations per microsecond:
 *
 *     65535 / 4 = 16383 us
 *
 * Larger requested delays are divided into chunks.
 */
#define HAL_TIME_DELAY_MAX_CHUNK_US \
    (UINT16_MAX / \
     HAL_TIME_DELAY_ITERATIONS_PER_US)

#if \
    (F_CPU % \
     (HAL_TIME_TIMER1_PRESCALER * 1000UL)) != 0UL

#error \
    "Timer1 cannot produce an exact 1 ms interval"

#endif


#if HAL_TIME_TIMER1_TICKS_PER_MS > 65536UL
#error "Timer1 compare value does not fit in 16 bits"
#endif

#if (F_CPU % 1000000UL) != 0UL
#error "F_CPU must contain a whole number of cycles per microsecond"
#endif

#if \
    (HAL_TIME_CPU_CYCLES_PER_US % \
     HAL_TIME_DELAY_LOOP_CYCLES) != 0UL

#error \
    "_delay_loop_2 cannot represent an exact microsecond at this F_CPU"

#endif


/*
 * Project-owned millisecond counter.
 *
 * It is volatile because it is modified asynchronously
 * by the Timer1 compare-match interrupt.
 */
static volatile uint32_t elapsed_milliseconds = 0U;


/*
 * Timer1 Compare Match A interrupt.
 *
 * Timer1 is configured to reach OCR1A once every
 * millisecond. In CTC mode, the timer counter is
 * automatically cleared after the compare match.
 */
ISR(TIMER1_COMPA_vect)
{
    ++elapsed_milliseconds;
}


void hal_time_init(void)
{
    const hal_critical_state_t previous_state =
        hal_critical_enter();

    /*
     * Stop Timer1 and clear its previous configuration.
     *
     * Arduino configures Timer1 during its startup,
     * so the project explicitly takes ownership here.
     */
    TCCR1A = 0U;
    TCCR1B = 0U;
    TCCR1C = 0U;

    /*
     * Disable all Timer1 interrupts while configuring
     * the peripheral.
     */
    TIMSK1 = 0U;

    /*
     * Reset the hardware counter and the project-owned
     * software counter.
     */
    TCNT1 = 0U;
    elapsed_milliseconds = 0U;

    /*
     * At 16 MHz with a prescaler of 64:
     *
     * Timer clock:
     *     16,000,000 / 64 = 250,000 Hz
     *
     * Counts per millisecond:
     *     250,000 / 1,000 = 250
     *
     * Since counting starts at zero:
     *     OCR1A = 250 - 1 = 249
     */
    OCR1A =
        (uint16_t)HAL_TIME_TIMER1_COMPARE_VALUE;

    /*
     * Clear any compare-match flag that may have been
     * pending from the previous Timer1 configuration.
     *
     * AVR interrupt flags are cleared by writing a one
     * to the corresponding bit.
     */
    TIFR1 = _BV(OCF1A);

    /*
     * Enable the Timer1 Compare Match A interrupt.
     *
     * Global interrupts remain in their previous state
     * until hal_critical_exit() is called.
     */
    TIMSK1 = _BV(OCIE1A);

    /*
     * Start Timer1:
     *
     * WGM12 = 1:
     *     CTC mode with OCR1A as TOP.
     *
     * CS11 = 1 and CS10 = 1:
     *     Clock prescaler = 64.
     */
    TCCR1B =
        _BV(WGM12) |
        _BV(CS11) |
        _BV(CS10);

    hal_critical_exit(previous_state);
}


uint32_t hal_time_millis(void)
{
    /*
     * The ATmega328P is an 8-bit microcontroller.
     *
     * Reading a 32-bit value requires several machine
     * instructions, so the read must be protected from
     * interruption by the Timer1 ISR.
     */
    const hal_critical_state_t previous_state =
        hal_critical_enter();

    const uint32_t current_time =
        elapsed_milliseconds;

    hal_critical_exit(previous_state);

    return current_time;
}

void hal_time_delay_us(
    uint16_t microseconds
)
{
    /*
     * Passing zero directly to _delay_loop_2()
     * would not mean zero iterations.
     *
     * Its 16-bit counter would wrap and execute
     * 65536 iterations, so zero must be handled
     * explicitly.
     */
    while (microseconds > 0U)
    {
        uint16_t chunk_us = microseconds;

        if (chunk_us >
            HAL_TIME_DELAY_MAX_CHUNK_US)
        {
            chunk_us =
                (uint16_t)
                HAL_TIME_DELAY_MAX_CHUNK_US;
        }

        const uint16_t loop_iterations =
            (uint16_t)(
                (uint32_t)chunk_us *
                HAL_TIME_DELAY_ITERATIONS_PER_US
            );

        _delay_loop_2(loop_iterations);

        microseconds =
            (uint16_t)(
                microseconds - chunk_us
            );
    }
}