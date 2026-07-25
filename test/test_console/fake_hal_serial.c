#include "fake_hal_serial.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "hal_serial.h"


#define FAKE_HAL_SERIAL_BUFFER_CAPACITY 1024U


static uint8_t receive_buffer[
    FAKE_HAL_SERIAL_BUFFER_CAPACITY
];

static size_t receive_length = 0U;
static size_t receive_index = 0U;

static uint8_t transmit_buffer[
    FAKE_HAL_SERIAL_BUFFER_CAPACITY + 1U
];

static size_t transmit_length = 0U;
static bool transmit_overflow = false;

static bool fail_next_read = false;

static uint32_t init_call_count = 0U;
static uint32_t available_call_count = 0U;
static uint32_t read_call_count = 0U;
static uint32_t write_call_count = 0U;

static uint32_t last_baud_rate = 0U;


void fake_hal_serial_reset(void)
{
    receive_length = 0U;
    receive_index = 0U;

    transmit_length = 0U;
    transmit_buffer[0] = '\0';
    transmit_overflow = false;

    fail_next_read = false;

    init_call_count = 0U;
    available_call_count = 0U;
    read_call_count = 0U;
    write_call_count = 0U;

    last_baud_rate = 0U;
}


bool fake_hal_serial_load_input(
    const uint8_t *bytes,
    size_t length
)
{
    if ((bytes == NULL) &&
        (length > 0U))
    {
        return false;
    }

    if (length >
        FAKE_HAL_SERIAL_BUFFER_CAPACITY)
    {
        return false;
    }

    if (length > 0U)
    {
        memcpy(
            receive_buffer,
            bytes,
            length
        );
    }

    receive_length = length;
    receive_index = 0U;

    return true;
}


bool fake_hal_serial_load_input_text(
    const char *text
)
{
    if (text == NULL)
    {
        return false;
    }

    return fake_hal_serial_load_input(
        (const uint8_t *)text,
        strlen(text)
    );
}


void fake_hal_serial_fail_next_read(void)
{
    fail_next_read = true;
}


uint32_t fake_hal_serial_get_init_call_count(void)
{
    return init_call_count;
}


uint32_t fake_hal_serial_get_last_baud_rate(void)
{
    return last_baud_rate;
}


uint32_t
fake_hal_serial_get_available_call_count(void)
{
    return available_call_count;
}


uint32_t fake_hal_serial_get_read_call_count(void)
{
    return read_call_count;
}


uint32_t fake_hal_serial_get_write_call_count(void)
{
    return write_call_count;
}


size_t fake_hal_serial_get_pending_input_length(void)
{
    return receive_length - receive_index;
}


size_t fake_hal_serial_get_output_length(void)
{
    return transmit_length;
}


const uint8_t *fake_hal_serial_get_output_bytes(void)
{
    return transmit_buffer;
}


const char *fake_hal_serial_get_output_text(void)
{
    return (const char *)transmit_buffer;
}


bool fake_hal_serial_output_overflowed(void)
{
    return transmit_overflow;
}


void hal_serial_init(
    uint32_t baud_rate
)
{
    ++init_call_count;
    last_baud_rate = baud_rate;
}


bool hal_serial_rx_available(void)
{
    ++available_call_count;

    return receive_index < receive_length;
}


bool hal_serial_read_byte(
    uint8_t *received_byte
)
{
    ++read_call_count;

    if (received_byte == NULL)
    {
        return false;
    }

    if (fail_next_read)
    {
        fail_next_read = false;
        return false;
    }

    if (receive_index >= receive_length)
    {
        return false;
    }

    *received_byte =
        receive_buffer[receive_index];

    ++receive_index;

    return true;
}


void hal_serial_write_byte(
    uint8_t transmitted_byte
)
{
    ++write_call_count;

    if (transmit_length >=
        FAKE_HAL_SERIAL_BUFFER_CAPACITY)
    {
        transmit_overflow = true;
        return;
    }

    transmit_buffer[transmit_length] =
        transmitted_byte;

    ++transmit_length;

    transmit_buffer[transmit_length] =
        '\0';
}
