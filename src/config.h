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
static const uint8_t WEIGHT_SAMPLES = 1U;

/*
 * Timing configuration.
 */
static const unsigned long BUTTON_DEBOUNCE_MS = 40UL;
static const unsigned long CALIBRATION_START_HOLD_MS =
    3000UL;
static const unsigned long PRINT_PERIOD_MS = 500UL;
/*
 * Time between LED state changes while showing
 * a blinking operation status.
 */
static const unsigned long
    OPERATION_INDICATOR_BLINK_PERIOD_MS = 500UL;

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
static const float LOW_MEDIUM_THRESHOLD_GRAMS = 500.0F;
static const float MEDIUM_HIGH_THRESHOLD_GRAMS = 1000.0F;

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