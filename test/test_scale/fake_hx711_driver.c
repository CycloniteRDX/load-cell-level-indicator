#include "fake_hx711_driver.h"

#include <stddef.h>

#define FAKE_HX711_MAX_READINGS 256U

typedef struct
{
    hx711_status_t status;
    int32_t raw_value;
} fake_hx711_reading_t;

static hx711_status_t init_status =
    HX711_STATUS_OK;

static hx711_status_t wait_ready_status =
    HX711_STATUS_OK;

static bool ready_state = true;

static fake_hx711_reading_t readings[
    FAKE_HX711_MAX_READINGS
];

static uint16_t prepared_reading_count = 0U;
static uint16_t consumed_reading_count = 0U;
static bool sequence_exhausted = false;

static uint32_t init_call_count = 0U;
static uint32_t wait_ready_call_count = 0U;
static uint32_t is_ready_call_count = 0U;
static uint32_t read_raw_call_count = 0U;

static uint8_t last_data_pin = 0U;
static uint8_t last_clock_pin = 0U;
static uint32_t last_timeout_ms = 0U;


void fake_hx711_driver_reset(void)
{
    init_status = HX711_STATUS_OK;
    wait_ready_status = HX711_STATUS_OK;
    ready_state = true;

    prepared_reading_count = 0U;
    consumed_reading_count = 0U;
    sequence_exhausted = false;

    init_call_count = 0U;
    wait_ready_call_count = 0U;
    is_ready_call_count = 0U;
    read_raw_call_count = 0U;

    last_data_pin = 0U;
    last_clock_pin = 0U;
    last_timeout_ms = 0U;
}


void fake_hx711_driver_set_init_status(
    hx711_status_t status
)
{
    init_status = status;
}


void fake_hx711_driver_set_wait_ready_status(
    hx711_status_t status
)
{
    wait_ready_status = status;
}


void fake_hx711_driver_set_ready(
    bool ready
)
{
    ready_state = ready;
}


bool fake_hx711_driver_push_reading(
    hx711_status_t status,
    int32_t raw_value
)
{
    if (prepared_reading_count >=
        FAKE_HX711_MAX_READINGS)
    {
        return false;
    }

    readings[prepared_reading_count].status =
        status;

    readings[prepared_reading_count].raw_value =
        raw_value;

    ++prepared_reading_count;

    return true;
}


uint32_t fake_hx711_driver_get_init_call_count(void)
{
    return init_call_count;
}


uint32_t
fake_hx711_driver_get_wait_ready_call_count(void)
{
    return wait_ready_call_count;
}


uint32_t fake_hx711_driver_get_is_ready_call_count(void)
{
    return is_ready_call_count;
}


uint32_t fake_hx711_driver_get_read_raw_call_count(void)
{
    return read_raw_call_count;
}


uint8_t fake_hx711_driver_get_last_data_pin(void)
{
    return last_data_pin;
}


uint8_t fake_hx711_driver_get_last_clock_pin(void)
{
    return last_clock_pin;
}


uint32_t fake_hx711_driver_get_last_timeout_ms(void)
{
    return last_timeout_ms;
}


uint16_t
fake_hx711_driver_get_consumed_reading_count(void)
{
    return consumed_reading_count;
}


bool fake_hx711_driver_sequence_was_exhausted(void)
{
    return sequence_exhausted;
}


hx711_status_t hx711_init(
    hx711_t *device,
    uint8_t data_pin,
    uint8_t clock_pin
)
{
    ++init_call_count;

    last_data_pin = data_pin;
    last_clock_pin = clock_pin;

    if ((init_status == HX711_STATUS_OK) &&
        (device != NULL))
    {
        device->data_pin = data_pin;
        device->clock_pin = clock_pin;
        device->gain = HX711_GAIN_A_128;
        device->initialized = true;
    }

    return init_status;
}


bool hx711_is_ready(
    const hx711_t *device
)
{
    (void)device;

    ++is_ready_call_count;

    return ready_state;
}


hx711_status_t hx711_wait_ready(
    const hx711_t *device,
    uint32_t timeout_ms
)
{
    (void)device;

    ++wait_ready_call_count;
    last_timeout_ms = timeout_ms;

    return wait_ready_status;
}


hx711_status_t hx711_read_raw(
    hx711_t *device,
    int32_t *raw_value
)
{
    (void)device;

    ++read_raw_call_count;

    if (consumed_reading_count >=
        prepared_reading_count)
    {
        sequence_exhausted = true;
        return HX711_STATUS_TIMEOUT;
    }

    const fake_hx711_reading_t reading =
        readings[consumed_reading_count];

    ++consumed_reading_count;

    if ((reading.status == HX711_STATUS_OK) &&
        (raw_value != NULL))
    {
        *raw_value = reading.raw_value;
    }

    return reading.status;
}


hx711_status_t hx711_set_gain(
    hx711_t *device,
    hx711_gain_t gain
)
{
    if (device != NULL)
    {
        device->gain = gain;
    }

    return HX711_STATUS_OK;
}


hx711_status_t hx711_power_down(
    hx711_t *device
)
{
    (void)device;
    return HX711_STATUS_OK;
}


hx711_status_t hx711_power_up(
    hx711_t *device
)
{
    (void)device;
    return HX711_STATUS_OK;
}
