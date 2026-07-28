#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "tare_record.h"


/*
 * Identifies tare data written by this project.
 *
 * Its explicit little-endian byte sequence is the
 * ASCII text: 54 41 52 45 ("TARE").
 */
static const uint32_t TARE_MAGIC =
    0x45524154UL;


/*
 * Version of the stored binary format.
 */
static const uint16_t TARE_FORMAT_VERSION =
    1U;


/*
 * Explicit byte offsets inside the 12-byte record.
 */
static const size_t MAGIC_OFFSET = 0U;
static const size_t VERSION_OFFSET = 4U;
static const size_t TARE_OFFSET_OFFSET = 6U;
static const size_t CHECKSUM_OFFSET = 10U;


/*
 * The persistent format stores the exact 32-bit
 * two's-complement representation used by the project.
 */
static_assert(
    sizeof(int32_t) == sizeof(uint32_t),
    "The tare format requires 32-bit signed offsets."
);

#if INT32_MIN != (-2147483647 - 1)

#error \
    "The tare format requires two's-complement int32_t."

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


bool tare_record_encode(
    int32_t tare_offset,
    uint8_t *record_bytes,
    size_t record_size
)
{
    if (record_bytes == nullptr)
    {
        return false;
    }

    if (record_size < TARE_RECORD_SIZE)
    {
        return false;
    }

    uint32_t tare_offset_bits = 0UL;

    memcpy(
        &tare_offset_bits,
        &tare_offset,
        sizeof(tare_offset_bits)
    );

    write_uint32_le(
        &record_bytes[MAGIC_OFFSET],
        TARE_MAGIC
    );

    write_uint16_le(
        &record_bytes[VERSION_OFFSET],
        TARE_FORMAT_VERSION
    );

    write_uint32_le(
        &record_bytes[TARE_OFFSET_OFFSET],
        tare_offset_bits
    );

    const uint16_t checksum =
        calculate_checksum(record_bytes);

    write_uint16_le(
        &record_bytes[CHECKSUM_OFFSET],
        checksum
    );

    return true;
}


bool tare_record_decode(
    const uint8_t *record_bytes,
    size_t record_size,
    int32_t *tare_offset
)
{
    if (record_bytes == nullptr)
    {
        return false;
    }

    if (tare_offset == nullptr)
    {
        return false;
    }

    if (record_size < TARE_RECORD_SIZE)
    {
        return false;
    }

    const uint32_t stored_magic =
        read_uint32_le(
            &record_bytes[MAGIC_OFFSET]
        );

    if (stored_magic != TARE_MAGIC)
    {
        return false;
    }

    const uint16_t stored_version =
        read_uint16_le(
            &record_bytes[VERSION_OFFSET]
        );

    if (stored_version != TARE_FORMAT_VERSION)
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

    const uint32_t tare_offset_bits =
        read_uint32_le(
            &record_bytes[TARE_OFFSET_OFFSET]
        );

    int32_t decoded_tare_offset = 0;

    memcpy(
        &decoded_tare_offset,
        &tare_offset_bits,
        sizeof(decoded_tare_offset)
    );

    *tare_offset = decoded_tare_offset;

    return true;
}
