#ifndef STORAGE_LAYOUT_H
#define STORAGE_LAYOUT_H

#include <stddef.h>

#include "calibration_record.h"
#include "tare_record.h"


/*
 * Fixed non-volatile storage layout.
 *
 * Each record owns a separate, non-overlapping region.
 */
static const size_t CALIBRATION_STORAGE_ADDRESS =
    0U;

static const size_t TARE_STORAGE_ADDRESS =
    CALIBRATION_STORAGE_ADDRESS +
    CALIBRATION_RECORD_SIZE;

static const size_t STORAGE_LAYOUT_REQUIRED_CAPACITY =
    TARE_STORAGE_ADDRESS +
    TARE_RECORD_SIZE;


#endif
