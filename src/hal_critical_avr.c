#include <avr/interrupt.h>
#include <avr/io.h>

#include "hal_critical.h"

hal_critical_state_t hal_critical_enter(void)
{
    const uint8_t previous_state = SREG;

    cli();

    return (hal_critical_state_t)previous_state;
}

void hal_critical_exit(
    hal_critical_state_t previous_state
)
{
    SREG = (uint8_t)previous_state;
}