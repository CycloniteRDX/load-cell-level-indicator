#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

/*
 * HX711 connections.
 */
static const uint8_t LOADCELL_DOUT_PIN = 2U;
static const uint8_t LOADCELL_SCK_PIN = 3U;

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
 * HX711 sampling configuration.
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
 * Maximum time allowed for the first HX711 conversion
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
 * multi-sample HX711 collection.
 *
 * Twenty samples require approximately two seconds at
 * the current 10 SPS rate. Five seconds leaves margin
 * while still detecting a stalled or disconnected ADC.
 */
static const uint32_t
    SCALE_SAMPLE_COLLECTION_TIMEOUT_MS = 5000UL;


/*
 * Cooperative HX711 recovery policy.
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
static const float DEFAULT_CALIBRATION_FACTOR =
    45.589332F;

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
 * Number of HX711 samples used to calculate
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
