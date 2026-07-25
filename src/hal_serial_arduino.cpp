#include <Arduino.h>

#include <stdbool.h>
#include <stdint.h>

#include "hal_serial.h"


extern "C" void hal_serial_init(
    uint32_t baud_rate
)
{
    Serial.begin(
        (unsigned long)baud_rate
    );
}


extern "C" bool hal_serial_rx_available(void)
{
    return Serial.available() > 0;
}


extern "C" bool hal_serial_read_byte(
    uint8_t *received_byte
)
{
    /*
     * Preserve the caller's output value when the
     * supplied pointer is invalid.
     */
    if (received_byte == nullptr)
    {
        return false;
    }

    /*
     * Avoid calling Serial.read() when no byte is
     * available.
     */
    if (!hal_serial_rx_available())
    {
        return false;
    }

    /*
     * Arduino Serial.read() returns an int so it can
     * use -1 to represent an unavailable byte.
     *
     * Availability was already checked, but retaining
     * this validation keeps the wrapper defensive.
     */
    const int received_value =
        Serial.read();

    if (received_value < 0)
    {
        return false;
    }

    *received_byte =
        (uint8_t)received_value;

    return true;
}


extern "C" void hal_serial_write_byte(
    uint8_t transmitted_byte
)
{
    /*
     * Serial.write() sends the byte value itself.
     *
     * Serial.print() would instead format some values
     * as human-readable decimal text.
     */
    (void)Serial.write(
        transmitted_byte
    );
}