#include "fake_scale_adc.h"

#include <stddef.h>

#define FAKE_SCALE_ADC_MAX_READINGS 256U

typedef struct
{
    scale_adc_status_t status;
    int32_t raw_value;
} fake_scale_adc_reading_t;

static scale_adc_status_t init_status =
    SCALE_ADC_STATUS_OK;

static scale_adc_status_t power_down_status =
    SCALE_ADC_STATUS_OK;

static scale_adc_status_t power_up_status =
    SCALE_ADC_STATUS_OK;

static bool ready_state = true;

static fake_scale_adc_reading_t readings[
    FAKE_SCALE_ADC_MAX_READINGS
];

static uint16_t prepared_reading_count = 0U;
static uint16_t consumed_reading_count = 0U;
static bool sequence_exhausted = false;

static uint32_t init_call_count = 0U;
static uint32_t is_ready_call_count = 0U;
static uint32_t read_raw_call_count = 0U;
static uint32_t power_down_call_count = 0U;
static uint32_t power_up_call_count = 0U;

static uint32_t adc_call_sequence = 0U;
static uint32_t power_down_call_order = 0U;
static uint32_t power_up_call_order = 0U;

void fake_scale_adc_reset(void)
{
    init_status = SCALE_ADC_STATUS_OK;
    power_down_status = SCALE_ADC_STATUS_OK;
    power_up_status = SCALE_ADC_STATUS_OK;
    ready_state = true;

    prepared_reading_count = 0U;
    consumed_reading_count = 0U;
    sequence_exhausted = false;

    init_call_count = 0U;
    is_ready_call_count = 0U;
    read_raw_call_count = 0U;
    power_down_call_count = 0U;
    power_up_call_count = 0U;

    adc_call_sequence = 0U;
    power_down_call_order = 0U;
    power_up_call_order = 0U;
}

void fake_scale_adc_set_init_status(
    scale_adc_status_t status
)
{
    init_status = status;
}

void fake_scale_adc_set_ready(
    bool ready
)
{
    ready_state = ready;
}

void fake_scale_adc_set_power_down_status(
    scale_adc_status_t status
)
{
    power_down_status = status;
}

void fake_scale_adc_set_power_up_status(
    scale_adc_status_t status
)
{
    power_up_status = status;
}

bool fake_scale_adc_push_reading(
    scale_adc_status_t status,
    int32_t raw_value
)
{
    if (prepared_reading_count >=
        FAKE_SCALE_ADC_MAX_READINGS)
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

uint32_t fake_scale_adc_get_init_call_count(void)
{
    return init_call_count;
}

uint32_t fake_scale_adc_get_is_ready_call_count(void)
{
    return is_ready_call_count;
}

uint32_t fake_scale_adc_get_read_raw_call_count(void)
{
    return read_raw_call_count;
}

uint32_t
fake_scale_adc_get_power_down_call_count(void)
{
    return power_down_call_count;
}

uint32_t fake_scale_adc_get_power_up_call_count(void)
{
    return power_up_call_count;
}

bool fake_scale_adc_power_down_preceded_power_up(
    void
)
{
    return (power_down_call_order != 0U) &&
           (power_up_call_order != 0U) &&
           (power_down_call_order < power_up_call_order);
}

uint16_t
fake_scale_adc_get_consumed_reading_count(void)
{
    return consumed_reading_count;
}

bool fake_scale_adc_sequence_was_exhausted(void)
{
    return sequence_exhausted;
}

scale_adc_status_t scale_adc_init(void)
{
    ++init_call_count;
    return init_status;
}

bool scale_adc_is_ready(void)
{
    ++is_ready_call_count;
    return ready_state;
}

scale_adc_status_t scale_adc_read_raw(
    int32_t *raw_value
)
{
    ++read_raw_call_count;

    if (consumed_reading_count >=
        prepared_reading_count)
    {
        sequence_exhausted = true;
        return SCALE_ADC_STATUS_ERROR;
    }

    const fake_scale_adc_reading_t reading =
        readings[consumed_reading_count];

    ++consumed_reading_count;

    if ((reading.status == SCALE_ADC_STATUS_OK) &&
        (raw_value != NULL))
    {
        *raw_value = reading.raw_value;
    }

    return reading.status;
}

scale_adc_status_t scale_adc_power_down(void)
{
    ++power_down_call_count;
    power_down_call_order = ++adc_call_sequence;

    return power_down_status;
}

scale_adc_status_t scale_adc_power_up(void)
{
    ++power_up_call_count;
    power_up_call_order = ++adc_call_sequence;

    return power_up_status;
}
