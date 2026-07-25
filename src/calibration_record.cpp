#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "calibration_record.h"


/*
 * Identifies calibration data written by this project.
 *
 * This preserves the numeric magic value used by the
 * existing EEPROM format. Its explicit little-endian
 * byte sequence is: 4C 41 43 4C.
 */
static const uint32_t CALIBRATION_MAGIC =
    0x4C43414CUL;


/*
 * Version of the stored binary format.
 */
static const uint16_t CALIBRATION_FORMAT_VERSION =
    1U;


/*
 * Factors whose absolute value is practically zero
 * are rejected because they would make weight
 * conversion invalid or excessively sensitive.
 */
static const float MINIMUM_ABSOLUTE_FACTOR =
    0.000001F;


/*
 * Explicit byte offsets inside the 12-byte record.
 */
static const size_t MAGIC_OFFSET = 0U;
static const size_t VERSION_OFFSET = 4U;
static const size_t FACTOR_OFFSET = 6U;
static const size_t CHECKSUM_OFFSET = 10U;


/*
 * The stored format requires an IEEE-754 binary32
 * representation, which is used by both the ATmega328P
 * toolchain and the native development environment.
 */
static_assert(
    sizeof(float) == sizeof(uint32_t),
    "The calibration format requires a 32-bit float."
);

#if \
    (FLT_RADIX != 2) || \
    (FLT_MANT_DIG != 24) || \
    (FLT_MAX_EXP != 128)

#error \
    "The calibration format requires IEEE-754 binary32 float."

#endif


static void write_uint16_le(
    uint8_t *destination,
    uint16_t value
)
{
    destination[0] =
        (uint8_t)(value & 0x00FFU);

    destination[1] =
        (uint8_t)((value >> 8U) & 0x00FFU);
}


static void write_uint32_le(
    uint8_t *destination,
    uint32_t value
)
{
    destination[0] =
        (uint8_t)(value & 0x000000FFUL);

    destination[1] =
        (uint8_t)((value >> 8U) & 0x000000FFUL);

    destination[2] =
        (uint8_t)((value >> 16U) & 0x000000FFUL);

    destination[3] =
        (uint8_t)((value >> 24U) & 0x000000FFUL);
}


static uint16_t read_uint16_le(
    const uint8_t *source
)
{
    return
        (uint16_t)source[0] |
        ((uint16_t)source[1] << 8U);
}


static uint32_t read_uint32_le(
    const uint8_t *source
)
{
    return
        (uint32_t)source[0] |
        ((uint32_t)source[1] << 8U) |
        ((uint32_t)source[2] << 16U) |
        ((uint32_t)source[3] << 24U);
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


static uint16_t calculate_checksum(
    const uint8_t *record_bytes
)
{
    uint16_t crc = 0xFFFFU;

    for (size_t index = 0U;
         index < CHECKSUM_OFFSET;
         ++index)
    {
        crc = crc16_update(
            crc,
            record_bytes[index]
        );
    }

    return crc;
}


bool calibration_record_factor_is_valid(
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


bool calibration_record_encode(
    float calibration_factor,
    uint8_t *record_bytes,
    size_t record_size
)
{
    if (record_bytes == nullptr)
    {
        return false;
    }

    if (record_size < CALIBRATION_RECORD_SIZE)
    {
        return false;
    }

    if (!calibration_record_factor_is_valid(
            calibration_factor))
    {
        return false;
    }

    uint32_t calibration_factor_bits = 0UL;

    memcpy(
        &calibration_factor_bits,
        &calibration_factor,
        sizeof(calibration_factor_bits)
    );

    write_uint32_le(
        &record_bytes[MAGIC_OFFSET],
        CALIBRATION_MAGIC
    );

    write_uint16_le(
        &record_bytes[VERSION_OFFSET],
        CALIBRATION_FORMAT_VERSION
    );

    write_uint32_le(
        &record_bytes[FACTOR_OFFSET],
        calibration_factor_bits
    );

    const uint16_t checksum =
        calculate_checksum(record_bytes);

    write_uint16_le(
        &record_bytes[CHECKSUM_OFFSET],
        checksum
    );

    return true;
}


bool calibration_record_decode(
    const uint8_t *record_bytes,
    size_t record_size,
    float *calibration_factor
)
{
    if (record_bytes == nullptr)
    {
        return false;
    }

    if (calibration_factor == nullptr)
    {
        return false;
    }

    if (record_size < CALIBRATION_RECORD_SIZE)
    {
        return false;
    }

    const uint32_t stored_magic =
        read_uint32_le(
            &record_bytes[MAGIC_OFFSET]
        );

    if (stored_magic != CALIBRATION_MAGIC)
    {
        return false;
    }

    const uint16_t stored_version =
        read_uint16_le(
            &record_bytes[VERSION_OFFSET]
        );

    if (stored_version !=
        CALIBRATION_FORMAT_VERSION)
    {
        return false;
    }

    const uint16_t stored_checksum =
        read_uint16_le(
            &record_bytes[CHECKSUM_OFFSET]
        );

    const uint16_t expected_checksum =
        calculate_checksum(record_bytes);

    if (stored_checksum != expected_checksum)
    {
        return false;
    }

    const uint32_t calibration_factor_bits =
        read_uint32_le(
            &record_bytes[FACTOR_OFFSET]
        );

    float decoded_factor = 0.0F;

    memcpy(
        &decoded_factor,
        &calibration_factor_bits,
        sizeof(decoded_factor)
    );

    if (!calibration_record_factor_is_valid(
            decoded_factor))
    {
        return false;
    }

    *calibration_factor = decoded_factor;

    return true;
}
