#include <Arduino.h>
#include <EEPROM.h>

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "calibration_storage.h"


/*
 * First EEPROM address used by this module.
 */
static const int CALIBRATION_EEPROM_ADDRESS = 0;


/*
 * Identifies data written by this application.
 *
 * 0x4C43414C represents the characters "LCAL":
 *
 * L = Load
 * CAL = Calibration
 */
static const uint32_t CALIBRATION_MAGIC =
    0x4C43414CUL;


/*
 * Version of the stored data format.
 *
 * If the record structure changes in the future,
 * this version must also change.
 */
static const uint16_t CALIBRATION_FORMAT_VERSION =
    1U;


/*
 * Factors whose absolute value is practically zero
 * are rejected because they would cause an invalid
 * weight conversion.
 */
static const float MINIMUM_ABSOLUTE_FACTOR =
    0.000001F;


typedef struct
{
    uint32_t magic;
    uint16_t version;
    float calibration_factor;
    uint16_t checksum;
} calibration_record_t;


static bool storage_has_enough_space(void)
{
    const int final_address =
        CALIBRATION_EEPROM_ADDRESS +
        (int)sizeof(calibration_record_t);

    return final_address <= (int)EEPROM.length();
}


static bool calibration_factor_is_valid(
    float calibration_factor
)
{
    if (isnan(calibration_factor))
    {
        return false;
    }

    if (isinf(calibration_factor))
    {
        return false;
    }

    if (fabsf(calibration_factor) <
        MINIMUM_ABSOLUTE_FACTOR)
    {
        return false;
    }

    return true;
}


/*
 * Updates a CRC-16/CCITT checksum with one byte.
 */
static uint16_t crc16_update(
    uint16_t crc,
    uint8_t data
)
{
    crc ^= (uint16_t)data << 8U;

    for (uint8_t bit = 0U; bit < 8U; ++bit)
    {
        if ((crc & 0x8000U) != 0U)
        {
            crc =
                (uint16_t)((crc << 1U) ^ 0x1021U);
        }
        else
        {
            crc = (uint16_t)(crc << 1U);
        }
    }

    return crc;
}


static uint16_t crc16_add_uint16(
    uint16_t crc,
    uint16_t value
)
{
    crc = crc16_update(
        crc,
        (uint8_t)(value & 0x00FFU)
    );

    crc = crc16_update(
        crc,
        (uint8_t)((value >> 8U) & 0x00FFU)
    );

    return crc;
}


static uint16_t crc16_add_uint32(
    uint16_t crc,
    uint32_t value
)
{
    crc = crc16_update(
        crc,
        (uint8_t)(value & 0x000000FFUL)
    );

    crc = crc16_update(
        crc,
        (uint8_t)((value >> 8U) & 0x000000FFUL)
    );

    crc = crc16_update(
        crc,
        (uint8_t)((value >> 16U) & 0x000000FFUL)
    );

    crc = crc16_update(
        crc,
        (uint8_t)((value >> 24U) & 0x000000FFUL)
    );

    return crc;
}


static uint16_t calculate_record_checksum(
    const calibration_record_t *record
)
{
    if (record == nullptr)
    {
        return 0U;
    }

    /*
     * On the ATmega328P, float and uint32_t are both
     * four bytes. memcpy avoids violating aliasing rules
     * when inspecting the binary representation.
     */
    static_assert(
        sizeof(float) == sizeof(uint32_t),
        "This storage format requires a 32-bit float."
    );

    uint32_t calibration_factor_bits = 0UL;

    memcpy(
        &calibration_factor_bits,
        &record->calibration_factor,
        sizeof(calibration_factor_bits)
    );

    uint16_t crc = 0xFFFFU;

    crc = crc16_add_uint32(
        crc,
        record->magic
    );

    crc = crc16_add_uint16(
        crc,
        record->version
    );

    crc = crc16_add_uint32(
        crc,
        calibration_factor_bits
    );

    return crc;
}


static bool record_is_valid(
    const calibration_record_t *record
)
{
    if (record == nullptr)
    {
        return false;
    }

    if (record->magic != CALIBRATION_MAGIC)
    {
        return false;
    }

    if (record->version !=
        CALIBRATION_FORMAT_VERSION)
    {
        return false;
    }

    if (!calibration_factor_is_valid(
            record->calibration_factor))
    {
        return false;
    }

    const uint16_t expected_checksum =
        calculate_record_checksum(record);

    if (record->checksum != expected_checksum)
    {
        return false;
    }

    return true;
}


bool calibration_storage_load(
    float *calibration_factor
)
{
    if (calibration_factor == nullptr)
    {
        return false;
    }

    if (!storage_has_enough_space())
    {
        return false;
    }

    calibration_record_t record = {};

    EEPROM.get(
        CALIBRATION_EEPROM_ADDRESS,
        record
    );

    if (!record_is_valid(&record))
    {
        return false;
    }

    *calibration_factor =
        record.calibration_factor;

    return true;
}


bool calibration_storage_save(
    float calibration_factor
)
{
    if (!storage_has_enough_space())
    {
        return false;
    }

    if (!calibration_factor_is_valid(
            calibration_factor))
    {
        return false;
    }

    calibration_record_t record = {};

    record.magic = CALIBRATION_MAGIC;
    record.version =
        CALIBRATION_FORMAT_VERSION;

    record.calibration_factor =
        calibration_factor;

    record.checksum =
        calculate_record_checksum(&record);

    /*
     * EEPROM.put() writes the complete structure,
     * but only physically rewrites bytes that changed.
     */
    EEPROM.put(
        CALIBRATION_EEPROM_ADDRESS,
        record
    );

    /*
     * Read the record back to verify that it was
     * stored correctly.
     */
    calibration_record_t verification_record = {};

    EEPROM.get(
        CALIBRATION_EEPROM_ADDRESS,
        verification_record
    );

    if (!record_is_valid(&verification_record))
    {
        return false;
    }

    if (verification_record.calibration_factor !=
        calibration_factor)
    {
        return false;
    }

    return true;
}


bool calibration_storage_clear(void)
{
    if (!storage_has_enough_space())
    {
        return false;
    }

    /*
     * Invalidating the magic number is enough to make
     * the complete record unusable.
     *
     * We do not need to erase all EEPROM bytes.
     */
    const uint32_t invalid_magic = 0UL;

    EEPROM.put(
        CALIBRATION_EEPROM_ADDRESS,
        invalid_magic
    );

    uint32_t stored_magic = CALIBRATION_MAGIC;

    EEPROM.get(
        CALIBRATION_EEPROM_ADDRESS,
        stored_magic
    );

    return stored_magic != CALIBRATION_MAGIC;
}