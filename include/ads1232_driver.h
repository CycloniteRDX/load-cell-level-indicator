#ifndef ADS1232_DRIVER_H
#define ADS1232_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * GAIN1:GAIN0 encoding from the ADS1232 data sheet.
 */
typedef enum
{
    ADS1232_GAIN_1   = 0,
    ADS1232_GAIN_2   = 1,
    ADS1232_GAIN_64  = 2,
    ADS1232_GAIN_128 = 3
} ads1232_gain_t;

typedef enum
{
    ADS1232_STATUS_OK = 0,
    ADS1232_STATUS_INVALID_ARGUMENT,
    ADS1232_STATUS_NOT_INITIALIZED,
    ADS1232_STATUS_POWERED_DOWN,
    ADS1232_STATUS_TIMEOUT
} ads1232_status_t;

typedef struct
{
    uint8_t data_pin;
    uint8_t clock_pin;
    uint8_t power_down_pin;
    uint8_t gain0_pin;
    uint8_t gain1_pin;
    ads1232_gain_t gain;
    bool initialized;
    bool powered_down;
} ads1232_t;

/*
 * Initializes the connected control pins and performs the ADS1232
 * power-up reset sequence. Channel and sample rate are board-level
 * straps in the current prototype and are intentionally outside this
 * interface.
 */
ads1232_status_t ads1232_init(
    ads1232_t *device,
    uint8_t data_pin,
    uint8_t clock_pin,
    uint8_t power_down_pin,
    uint8_t gain0_pin,
    uint8_t gain1_pin,
    ads1232_gain_t gain
);

bool ads1232_is_ready(const ads1232_t *device);

ads1232_status_t ads1232_wait_ready(
    const ads1232_t *device,
    uint32_t timeout_ms
);

ads1232_status_t ads1232_read_raw(
    ads1232_t *device,
    int32_t *raw_value
);

/*
 * Consumes one ready conversion and emits the 26th SCLK pulse that
 * starts the ADS1232 internal offset calibration. The calibration is
 * asynchronous; completion is reported later by DOUT becoming LOW.
 */
ads1232_status_t ads1232_start_offset_calibration(
    ads1232_t *device
);

ads1232_status_t ads1232_power_down(
    ads1232_t *device
);

ads1232_status_t ads1232_power_up(
    ads1232_t *device
);

#ifdef __cplusplus
}
#endif

#endif
