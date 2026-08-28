#ifndef STORAGE_LAYOUT_H
#define STORAGE_LAYOUT_H

#include <stddef.h>

#include "calibration_record.h"
#include "scale_adc_backend.h"
#include "tare_record.h"


/*
 * Fixed non-volatile storage layout.
 *
 * Each record owns a separate, non-overlapping region.
 */
static const size_t HX711_CALIBRATION_STORAGE_ADDRESS =
    0U;

static const size_t HX711_TARE_STORAGE_ADDRESS =
    HX711_CALIBRATION_STORAGE_ADDRESS +
    CALIBRATION_RECORD_SIZE;

static const size_t ADS1232_CALIBRATION_STORAGE_ADDRESS =
    HX711_TARE_STORAGE_ADDRESS +
    TARE_RECORD_SIZE;

static const size_t ADS1232_TARE_STORAGE_ADDRESS =
    ADS1232_CALIBRATION_STORAGE_ADDRESS +
    CALIBRATION_RECORD_SIZE;

#if SCALE_ADC_BACKEND == SCALE_ADC_BACKEND_HX711

static const size_t CALIBRATION_STORAGE_ADDRESS =
    HX711_CALIBRATION_STORAGE_ADDRESS;

static const size_t TARE_STORAGE_ADDRESS =
    HX711_TARE_STORAGE_ADDRESS;

#elif SCALE_ADC_BACKEND == SCALE_ADC_BACKEND_ADS1232

static const size_t CALIBRATION_STORAGE_ADDRESS =
    ADS1232_CALIBRATION_STORAGE_ADDRESS;

static const size_t TARE_STORAGE_ADDRESS =
    ADS1232_TARE_STORAGE_ADDRESS;

#endif

static const size_t STORAGE_LAYOUT_REQUIRED_CAPACITY =
    TARE_STORAGE_ADDRESS +
    TARE_RECORD_SIZE;

static const size_t STORAGE_LAYOUT_TOTAL_CAPACITY =
    ADS1232_TARE_STORAGE_ADDRESS +
    TARE_RECORD_SIZE;


#endif
