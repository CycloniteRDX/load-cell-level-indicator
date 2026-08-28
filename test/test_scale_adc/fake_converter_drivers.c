#include "fake_converter_drivers.h"

#include <stddef.h>

#include "hx711_driver.h"

static bool init_success = true;
static bool ready_state = true;
static bool read_success = true;
static bool power_down_success = true;
static bool power_up_success = true;
static int32_t prepared_raw_value = 0;

static uint32_t init_call_count = 0U;
static uint32_t ready_call_count = 0U;
static uint32_t read_call_count = 0U;
static uint32_t power_down_call_count = 0U;
static uint32_t power_up_call_count = 0U;

static uint8_t captured_data_pin = 0U;
static uint8_t captured_clock_pin = 0U;
static uint8_t captured_power_down_pin = 0U;
static uint8_t captured_gain0_pin = 0U;
static uint8_t captured_gain1_pin = 0U;
static ads1232_gain_t captured_ads1232_gain =
    ADS1232_GAIN_1;

void fake_converter_drivers_reset(void)
{
    init_success = true;
    ready_state = true;
    read_success = true;
    power_down_success = true;
    power_up_success = true;
    prepared_raw_value = 0;

    init_call_count = 0U;
    ready_call_count = 0U;
    read_call_count = 0U;
    power_down_call_count = 0U;
    power_up_call_count = 0U;

    captured_data_pin = 0U;
    captured_clock_pin = 0U;
    captured_power_down_pin = 0U;
    captured_gain0_pin = 0U;
    captured_gain1_pin = 0U;
    captured_ads1232_gain = ADS1232_GAIN_1;
}

void fake_converter_set_init_success(
    bool success
)
{
    init_success = success;
}

void fake_converter_set_ready(
    bool ready
)
{
    ready_state = ready;
}

void fake_converter_set_read_result(
    bool success,
    int32_t raw_value
)
{
    read_success = success;
    prepared_raw_value = raw_value;
}

void fake_converter_set_power_down_success(
    bool success
)
{
    power_down_success = success;
}

void fake_converter_set_power_up_success(
    bool success
)
{
    power_up_success = success;
}

uint32_t fake_converter_get_init_call_count(void)
{
    return init_call_count;
}

uint32_t fake_converter_get_ready_call_count(void)
{
    return ready_call_count;
}

uint32_t fake_converter_get_read_call_count(void)
{
    return read_call_count;
}

uint32_t fake_converter_get_power_down_call_count(void)
{
    return power_down_call_count;
}

uint32_t fake_converter_get_power_up_call_count(void)
{
    return power_up_call_count;
}

uint8_t fake_converter_get_data_pin(void)
{
    return captured_data_pin;
}

uint8_t fake_converter_get_clock_pin(void)
{
    return captured_clock_pin;
}

uint8_t fake_converter_get_power_down_pin(void)
{
    return captured_power_down_pin;
}

uint8_t fake_converter_get_gain0_pin(void)
{
    return captured_gain0_pin;
}

uint8_t fake_converter_get_gain1_pin(void)
{
    return captured_gain1_pin;
}

ads1232_gain_t fake_converter_get_ads1232_gain(void)
{
    return captured_ads1232_gain;
}

hx711_status_t hx711_init(
    hx711_t *device,
    uint8_t data_pin,
    uint8_t clock_pin
)
{
    ++init_call_count;
    captured_data_pin = data_pin;
    captured_clock_pin = clock_pin;

    if (!init_success)
    {
        return HX711_STATUS_INVALID_ARGUMENT;
    }

    if (device != NULL)
    {
        device->data_pin = data_pin;
        device->clock_pin = clock_pin;
        device->gain = HX711_GAIN_A_128;
        device->initialized = true;
    }

    return HX711_STATUS_OK;
}

bool hx711_is_ready(
    const hx711_t *device
)
{
    (void)device;
    ++ready_call_count;
    return ready_state;
}

hx711_status_t hx711_read_raw(
    hx711_t *device,
    int32_t *raw_value
)
{
    (void)device;
    ++read_call_count;

    if (!read_success || (raw_value == NULL))
    {
        return HX711_STATUS_TIMEOUT;
    }

    *raw_value = prepared_raw_value;
    return HX711_STATUS_OK;
}

hx711_status_t hx711_power_down(
    hx711_t *device
)
{
    (void)device;
    ++power_down_call_count;

    return power_down_success ?
        HX711_STATUS_OK :
        HX711_STATUS_TIMEOUT;
}

hx711_status_t hx711_power_up(
    hx711_t *device
)
{
    (void)device;
    ++power_up_call_count;

    return power_up_success ?
        HX711_STATUS_OK :
        HX711_STATUS_TIMEOUT;
}

ads1232_status_t ads1232_init(
    ads1232_t *device,
    uint8_t data_pin,
    uint8_t clock_pin,
    uint8_t power_down_pin,
    uint8_t gain0_pin,
    uint8_t gain1_pin,
    ads1232_gain_t gain
)
{
    ++init_call_count;
    captured_data_pin = data_pin;
    captured_clock_pin = clock_pin;
    captured_power_down_pin = power_down_pin;
    captured_gain0_pin = gain0_pin;
    captured_gain1_pin = gain1_pin;
    captured_ads1232_gain = gain;

    if (!init_success)
    {
        return ADS1232_STATUS_INVALID_ARGUMENT;
    }

    if (device != NULL)
    {
        device->data_pin = data_pin;
        device->clock_pin = clock_pin;
        device->power_down_pin = power_down_pin;
        device->gain0_pin = gain0_pin;
        device->gain1_pin = gain1_pin;
        device->gain = gain;
        device->initialized = true;
        device->powered_down = false;
    }

    return ADS1232_STATUS_OK;
}

bool ads1232_is_ready(
    const ads1232_t *device
)
{
    (void)device;
    ++ready_call_count;
    return ready_state;
}

ads1232_status_t ads1232_read_raw(
    ads1232_t *device,
    int32_t *raw_value
)
{
    (void)device;
    ++read_call_count;

    if (!read_success || (raw_value == NULL))
    {
        return ADS1232_STATUS_TIMEOUT;
    }

    *raw_value = prepared_raw_value;
    return ADS1232_STATUS_OK;
}

ads1232_status_t ads1232_power_down(
    ads1232_t *device
)
{
    (void)device;
    ++power_down_call_count;

    return power_down_success ?
        ADS1232_STATUS_OK :
        ADS1232_STATUS_TIMEOUT;
}

ads1232_status_t ads1232_power_up(
    ads1232_t *device
)
{
    (void)device;
    ++power_up_call_count;

    return power_up_success ?
        ADS1232_STATUS_OK :
        ADS1232_STATUS_TIMEOUT;
}
