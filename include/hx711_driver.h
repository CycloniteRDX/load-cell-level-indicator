#ifndef HX711_DRIVER_H
#define HX711_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    HX711_GAIN_A_128 = 1,
    HX711_GAIN_B_32  = 2,
    HX711_GAIN_A_64  = 3
} hx711_gain_t;

typedef enum
{
    HX711_STATUS_OK = 0,
    HX711_STATUS_INVALID_ARGUMENT,
    HX711_STATUS_NOT_INITIALIZED,
    HX711_STATUS_TIMEOUT
} hx711_status_t;

typedef struct
{
    uint8_t data_pin;
    uint8_t clock_pin;
    hx711_gain_t gain;
    bool initialized;
} hx711_t;

hx711_status_t hx711_init(
    hx711_t *device,
    uint8_t data_pin,
    uint8_t clock_pin
);

bool hx711_is_ready(const hx711_t *device);

hx711_status_t hx711_wait_ready(
    const hx711_t *device,
    uint32_t timeout_ms
);

hx711_status_t hx711_read_raw(
    hx711_t *device,
    int32_t *raw_value
);

hx711_status_t hx711_set_gain(
    hx711_t *device,
    hx711_gain_t gain
);

hx711_status_t hx711_power_down(hx711_t *device);

hx711_status_t hx711_power_up(hx711_t *device);

#ifdef __cplusplus
}
#endif

#endif