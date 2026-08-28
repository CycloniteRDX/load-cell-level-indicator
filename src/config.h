#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

#include "scale_adc_backend.h"

/*
 * HX711 connections.
 */
static const uint8_t HX711_DOUT_PIN = 2U;
static const uint8_t HX711_SCK_PIN = 3U;

/*
 * ADS1232 connections.
 *
 * Nano A0 and A1 are used as digital pins 14 and 15.
 * Module A0 (channel select) and SPEED remain strapped
 * to GND and are not microcontroller connections.
 */
static const uint8_t ADS1232_DOUT_PIN = 2U;
static const uint8_t ADS1232_SCLK_PIN = 3U;
static const uint8_t ADS1232_PDWN_PIN = 9U;
static const uint8_t ADS1232_GAIN0_PIN = 14U;
static const uint8_t ADS1232_GAIN1_PIN = 15U;

/*
 * User input.
 */
static const uint8_t TARE_BUTTON_PIN = 4U;
static const uint8_t CALIBRATION_BUTTON_PIN = 8U;

/*
 * Level indicator LEDs.
 */
static const uint8_t LOW_LEVEL_LED_PIN = 5U;
static const uint8_t MEDIUM_LEVEL_LED_PIN = 6U;
static const uint8_t HIGH_LEVEL_LED_PIN = 7U;

/*
 * Scale sampling configuration.
 */
static const uint8_t TARE_SAMPLES = 20U;

/*
 * Console configuration.
 */
static const uint32_t CONSOLE_BAUD_RATE =
    115200UL;

/*
 * Timing configuration.
 */
static const uint32_t BUTTON_DEBOUNCE_MS = 40UL;

static const uint32_t TARE_START_HOLD_MS =
    3000UL;

static const uint32_t CALIBRATION_START_HOLD_MS =
    3000UL;

static const uint32_t PRINT_PERIOD_MS = 500UL;


/*
 * Maximum time allowed for the first ADC conversion
 * to become ready after pin configuration.
 */
static const uint32_t SCALE_STARTUP_TIMEOUT_MS =
    2000UL;


/*
 * Maximum time that normal operation may receive only
 * SCALE_READ_NO_DATA results. Every valid measurement
 * starts a fresh window. States that intentionally do
 * not consume normal measurements do not use it.
 */
static const uint32_t
    SCALE_RUNTIME_READY_TIMEOUT_MS = 2000UL;


/*
 * Maximum total time allowed for one incremental
 * multi-sample ADC collection.
 *
 * Twenty samples require approximately two seconds at
 * the current 10 SPS rate. Five seconds leaves margin
 * while still detecting a stalled or disconnected ADC.
 */
static const uint32_t
    SCALE_SAMPLE_COLLECTION_TIMEOUT_MS = 5000UL;


/*
 * Cooperative ADC recovery policy.
 *
 * Every attempt starts only after a finite backoff.
 * A successful power cycle must then produce a ready
 * conversion before the ready timeout expires.
 */
static const uint32_t
    FAULT_RECOVERY_BACKOFF_MS = 500UL;

static const uint32_t
    FAULT_RECOVERY_READY_TIMEOUT_MS = 2000UL;

static const uint8_t
    FAULT_RECOVERY_MAX_ATTEMPTS = 3U;


/*
 * Recovery indication.
 *
 * LOW and HIGH alternate every 250 ms so recovery is
 * visually distinct from every normal and fault mode.
 */
static const uint32_t
    FAULT_RECOVERY_INDICATOR_PERIOD_MS = 250UL;


/*
 * Slow blinking used while waiting for the user
 * during the calibration workflow.
 */
static const uint32_t
    OPERATION_INDICATOR_BLINK_PERIOD_MS = 500UL;


/*
 * Slow all-LED blinking used when no valid tare
 * offset is available.
 */
static const uint32_t
    TARE_REQUIRED_BLINK_PERIOD_MS = 1000UL;


/*
 * Faster blinking used for temporary success and
 * error feedback.
 */
static const uint32_t
    OPERATION_RESULT_BLINK_PERIOD_MS = 150UL;


/*
 * Number of visible flashes for temporary results.
 */
static const uint8_t
    CALIBRATION_SUCCESS_FLASH_COUNT = 2U;

static const uint8_t
    CALIBRATION_ERROR_FLASH_COUNT = 3U;

/*
 * Very-low level warning.
 *
 * The LOW LED changes state every 250 ms.
 * One complete on/off cycle therefore lasts 500 ms.
 */
static const uint32_t
    VERY_LOW_BLINK_PERIOD_MS = 250UL;

/*
 * Provisional fallback calibration factor.
 *
 * It will be used when no valid calibration
 * has been stored in non-volatile memory.
 */
#if SCALE_ADC_BACKEND == SCALE_ADC_BACKEND_HX711

static const float DEFAULT_CALIBRATION_FACTOR =
    45.589332F;

#elif SCALE_ADC_BACKEND == SCALE_ADC_BACKEND_ADS1232

/*
 * No transferable ADS1232 counts-per-gram factor exists
 * before physical calibration. One keeps arithmetic valid
 * without reusing HX711 data; the operator must calibrate.
 */
static const float DEFAULT_CALIBRATION_FACTOR =
    1.0F;

#endif

/*
 * Provisional level thresholds.
 */
static const float VERY_LOW_LOW_THRESHOLD_GRAMS =
    100.0F;

static const float LOW_MEDIUM_THRESHOLD_GRAMS =
    500.0F;

static const float MEDIUM_HIGH_THRESHOLD_GRAMS =
    1000.0F;

/*
 * Prevents rapid level changes around the thresholds.
 */
static const float LEVEL_HYSTERESIS_GRAMS = 20.0F;

/*
 * Provisional reference mass used during calibration.
 *
 * This value must match the real calibration mass.
 */
static const float CALIBRATION_MASS_GRAMS =
    1500.0F;


/*
 * Number of ADC samples used to calculate
 * the calibration factor.
 */
static const uint8_t CALIBRATION_SAMPLES =
    20U;


/*
 * Prevents calibration when no meaningful load
 * has been applied.
 *
 * This is provisional for the current test setup.
 */
static const float MINIMUM_CALIBRATION_SIGNAL_COUNTS =
    5000.0F;

#endif
