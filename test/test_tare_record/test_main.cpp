#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <unity.h>

#include "tare_record.h"


static const int32_t KNOWN_POSITIVE_OFFSET =
    (int32_t)0x12345678L;


static const uint8_t EXPECTED_POSITIVE_RECORD[
    TARE_RECORD_SIZE
] = {
    0x54U,
    0x41U,
    0x52U,
    0x45U,
    0x01U,
    0x00U,
    0x78U,
    0x56U,
    0x34U,
    0x12U,
    0xA9U,
    0x6BU
};


static const uint8_t EXPECTED_NEGATIVE_ONE_RECORD[
    TARE_RECORD_SIZE
] = {
    0x54U,
    0x41U,
    0x52U,
    0x45U,
    0x01U,
    0x00U,
    0xFFU,
    0xFFU,
    0xFFU,
    0xFFU,
    0x9CU,
    0x22U
};


void setUp(void)
{
}


void tearDown(void)
{
}


static uint16_t reference_crc16_update(
    uint16_t crc,
    uint8_t data
)
{
    crc ^= (uint16_t)data << 8U;

    for (uint8_t bit = 0U;
         bit < 8U;
         ++bit)
    {
        if ((crc & 0x8000U) != 0U)
        {
            crc =
                (uint16_t)(
                    (crc << 1U) ^ 0x1021U
                );
        }
        else
        {
            crc = (uint16_t)(crc << 1U);
        }
    }

    return crc;
}


static uint16_t reference_record_checksum(
    const uint8_t *record_bytes
)
{
    uint16_t crc = 0xFFFFU;

    for (size_t index = 0U;
         index < 10U;
         ++index)
    {
        crc = reference_crc16_update(
            crc,
            record_bytes[index]
        );
    }

    return crc;
}


static void write_uint16_le_for_test(
    uint8_t *destination,
    uint16_t value
)
{
    destination[0] =
        (uint8_t)(value & 0x00FFU);

    destination[1] =
        (uint8_t)((value >> 8U) & 0x00FFU);
}


static void update_record_checksum(
    uint8_t *record_bytes
)
{
    const uint16_t checksum =
        reference_record_checksum(record_bytes);

    write_uint16_le_for_test(
        &record_bytes[10],
        checksum
    );
}


static void encode_valid_record(
    int32_t tare_offset,
    uint8_t *record_bytes
)
{
    TEST_ASSERT_TRUE(
        tare_record_encode(
            tare_offset,
            record_bytes,
            TARE_RECORD_SIZE
        )
    );
}


static void assert_round_trip(
    int32_t expected_offset
)
{
    uint8_t record_bytes[TARE_RECORD_SIZE] = {0U};
    int32_t decoded_offset = 123;

    encode_valid_record(
        expected_offset,
        record_bytes
    );

    TEST_ASSERT_TRUE(
        tare_record_decode(
            record_bytes,
            sizeof(record_bytes),
            &decoded_offset
        )
    );

    TEST_ASSERT_EQUAL_INT32(
        expected_offset,
        decoded_offset
    );
}


static void test_record_size_is_fixed(
    void
)
{
    TEST_ASSERT_EQUAL_UINT32(
        12U,
        TARE_RECORD_SIZE
    );
}


static void test_encode_rejects_null_output(
    void
)
{
    TEST_ASSERT_FALSE(
        tare_record_encode(
            KNOWN_POSITIVE_OFFSET,
            nullptr,
            TARE_RECORD_SIZE
        )
    );
}


static void test_encode_rejects_short_buffer_without_modifying_it(
    void
)
{
    uint8_t record_bytes[TARE_RECORD_SIZE];

    memset(
        record_bytes,
        0xA5,
        sizeof(record_bytes)
    );

    TEST_ASSERT_FALSE(
        tare_record_encode(
            KNOWN_POSITIVE_OFFSET,
            record_bytes,
            TARE_RECORD_SIZE - 1U
        )
    );

    for (size_t index = 0U;
         index < sizeof(record_bytes);
         ++index)
    {
        TEST_ASSERT_EQUAL_HEX8(
            0xA5U,
            record_bytes[index]
        );
    }
}


static void test_encode_produces_known_little_endian_positive_record(
    void
)
{
    uint8_t record_bytes[TARE_RECORD_SIZE] = {0U};

    encode_valid_record(
        KNOWN_POSITIVE_OFFSET,
        record_bytes
    );

    TEST_ASSERT_EQUAL_UINT8_ARRAY(
        EXPECTED_POSITIVE_RECORD,
        record_bytes,
        TARE_RECORD_SIZE
    );
}


static void test_encode_uses_twos_complement_for_negative_offset(
    void
)
{
    uint8_t record_bytes[TARE_RECORD_SIZE] = {0U};

    encode_valid_record(
        -1,
        record_bytes
    );

    TEST_ASSERT_EQUAL_UINT8_ARRAY(
        EXPECTED_NEGATIVE_ONE_RECORD,
        record_bytes,
        TARE_RECORD_SIZE
    );
}


static void test_encode_does_not_modify_bytes_after_record(
    void
)
{
    uint8_t record_bytes[TARE_RECORD_SIZE + 2U];

    memset(
        record_bytes,
        0xCC,
        sizeof(record_bytes)
    );

    TEST_ASSERT_TRUE(
        tare_record_encode(
            KNOWN_POSITIVE_OFFSET,
            record_bytes,
            sizeof(record_bytes)
        )
    );

    TEST_ASSERT_EQUAL_HEX8(
        0xCCU,
        record_bytes[TARE_RECORD_SIZE]
    );

    TEST_ASSERT_EQUAL_HEX8(
        0xCCU,
        record_bytes[TARE_RECORD_SIZE + 1U]
    );
}


static void test_decode_rejects_null_input_without_modifying_output(
    void
)
{
    int32_t decoded_offset = 987654;

    TEST_ASSERT_FALSE(
        tare_record_decode(
            nullptr,
            TARE_RECORD_SIZE,
            &decoded_offset
        )
    );

    TEST_ASSERT_EQUAL_INT32(
        987654,
        decoded_offset
    );
}


static void test_decode_rejects_null_output(
    void
)
{
    TEST_ASSERT_FALSE(
        tare_record_decode(
            EXPECTED_POSITIVE_RECORD,
            TARE_RECORD_SIZE,
            nullptr
        )
    );
}


static void test_decode_rejects_short_buffer_without_modifying_output(
    void
)
{
    int32_t decoded_offset = -987654;

    TEST_ASSERT_FALSE(
        tare_record_decode(
            EXPECTED_POSITIVE_RECORD,
            TARE_RECORD_SIZE - 1U,
            &decoded_offset
        )
    );

    TEST_ASSERT_EQUAL_INT32(
        -987654,
        decoded_offset
    );
}


static void test_decode_accepts_known_positive_record(
    void
)
{
    int32_t decoded_offset = 0;

    TEST_ASSERT_TRUE(
        tare_record_decode(
            EXPECTED_POSITIVE_RECORD,
            TARE_RECORD_SIZE,
            &decoded_offset
        )
    );

    TEST_ASSERT_EQUAL_INT32(
        KNOWN_POSITIVE_OFFSET,
        decoded_offset
    );
}


static void test_decode_accepts_known_negative_record(
    void
)
{
    int32_t decoded_offset = 0;

    TEST_ASSERT_TRUE(
        tare_record_decode(
            EXPECTED_NEGATIVE_ONE_RECORD,
            TARE_RECORD_SIZE,
            &decoded_offset
        )
    );

    TEST_ASSERT_EQUAL_INT32(
        -1,
        decoded_offset
    );
}


static void test_round_trip_accepts_zero(
    void
)
{
    assert_round_trip(0);
}


static void test_round_trip_accepts_int32_min(
    void
)
{
    assert_round_trip(INT32_MIN);
}


static void test_round_trip_accepts_int32_max(
    void
)
{
    assert_round_trip(INT32_MAX);
}


static void test_decode_rejects_invalid_magic_even_with_valid_checksum(
    void
)
{
    uint8_t record_bytes[TARE_RECORD_SIZE];

    memcpy(
        record_bytes,
        EXPECTED_POSITIVE_RECORD,
        sizeof(record_bytes)
    );

    record_bytes[0] ^= 0x01U;
    update_record_checksum(record_bytes);

    int32_t decoded_offset = 111;

    TEST_ASSERT_FALSE(
        tare_record_decode(
            record_bytes,
            sizeof(record_bytes),
            &decoded_offset
        )
    );

    TEST_ASSERT_EQUAL_INT32(
        111,
        decoded_offset
    );
}


static void test_decode_rejects_unsupported_version_even_with_valid_checksum(
    void
)
{
    uint8_t record_bytes[TARE_RECORD_SIZE];

    memcpy(
        record_bytes,
        EXPECTED_POSITIVE_RECORD,
        sizeof(record_bytes)
    );

    record_bytes[4] = 0x02U;
    record_bytes[5] = 0x00U;
    update_record_checksum(record_bytes);

    int32_t decoded_offset = 222;

    TEST_ASSERT_FALSE(
        tare_record_decode(
            record_bytes,
            sizeof(record_bytes),
            &decoded_offset
        )
    );

    TEST_ASSERT_EQUAL_INT32(
        222,
        decoded_offset
    );
}


static void test_decode_rejects_corrupted_offset(
    void
)
{
    uint8_t record_bytes[TARE_RECORD_SIZE];

    memcpy(
        record_bytes,
        EXPECTED_POSITIVE_RECORD,
        sizeof(record_bytes)
    );

    record_bytes[8] ^= 0x80U;

    int32_t decoded_offset = 333;

    TEST_ASSERT_FALSE(
        tare_record_decode(
            record_bytes,
            sizeof(record_bytes),
            &decoded_offset
        )
    );

    TEST_ASSERT_EQUAL_INT32(
        333,
        decoded_offset
    );
}


static void test_decode_rejects_corrupted_checksum(
    void
)
{
    uint8_t record_bytes[TARE_RECORD_SIZE];

    memcpy(
        record_bytes,
        EXPECTED_POSITIVE_RECORD,
        sizeof(record_bytes)
    );

    record_bytes[10] ^= 0x01U;

    int32_t decoded_offset = 444;

    TEST_ASSERT_FALSE(
        tare_record_decode(
            record_bytes,
            sizeof(record_bytes),
            &decoded_offset
        )
    );

    TEST_ASSERT_EQUAL_INT32(
        444,
        decoded_offset
    );
}


static void test_decode_does_not_modify_output_after_invalid_magic(
    void
)
{
    uint8_t record_bytes[TARE_RECORD_SIZE];

    memcpy(
        record_bytes,
        EXPECTED_POSITIVE_RECORD,
        sizeof(record_bytes)
    );

    record_bytes[3] = 0x00U;

    int32_t decoded_offset = INT32_MIN;

    TEST_ASSERT_FALSE(
        tare_record_decode(
            record_bytes,
            sizeof(record_bytes),
            &decoded_offset
        )
    );

    TEST_ASSERT_EQUAL_INT32(
        INT32_MIN,
        decoded_offset
    );
}


static void test_decode_ignores_bytes_after_record(
    void
)
{
    uint8_t record_bytes[TARE_RECORD_SIZE + 3U];

    memset(
        record_bytes,
        0x7EU,
        sizeof(record_bytes)
    );

    memcpy(
        record_bytes,
        EXPECTED_POSITIVE_RECORD,
        TARE_RECORD_SIZE
    );

    int32_t decoded_offset = 0;

    TEST_ASSERT_TRUE(
        tare_record_decode(
            record_bytes,
            sizeof(record_bytes),
            &decoded_offset
        )
    );

    TEST_ASSERT_EQUAL_INT32(
        KNOWN_POSITIVE_OFFSET,
        decoded_offset
    );
}


int main(
    int argc,
    char **argv
)
{
    (void)argc;
    (void)argv;

    UNITY_BEGIN();

    RUN_TEST(
        test_record_size_is_fixed
    );

    RUN_TEST(
        test_encode_rejects_null_output
    );

    RUN_TEST(
        test_encode_rejects_short_buffer_without_modifying_it
    );

    RUN_TEST(
        test_encode_produces_known_little_endian_positive_record
    );

    RUN_TEST(
        test_encode_uses_twos_complement_for_negative_offset
    );

    RUN_TEST(
        test_encode_does_not_modify_bytes_after_record
    );

    RUN_TEST(
        test_decode_rejects_null_input_without_modifying_output
    );

    RUN_TEST(
        test_decode_rejects_null_output
    );

    RUN_TEST(
        test_decode_rejects_short_buffer_without_modifying_output
    );

    RUN_TEST(
        test_decode_accepts_known_positive_record
    );

    RUN_TEST(
        test_decode_accepts_known_negative_record
    );

    RUN_TEST(
        test_round_trip_accepts_zero
    );

    RUN_TEST(
        test_round_trip_accepts_int32_min
    );

    RUN_TEST(
        test_round_trip_accepts_int32_max
    );

    RUN_TEST(
        test_decode_rejects_invalid_magic_even_with_valid_checksum
    );

    RUN_TEST(
        test_decode_rejects_unsupported_version_even_with_valid_checksum
    );

    RUN_TEST(
        test_decode_rejects_corrupted_offset
    );

    RUN_TEST(
        test_decode_rejects_corrupted_checksum
    );

    RUN_TEST(
        test_decode_does_not_modify_output_after_invalid_magic
    );

    RUN_TEST(
        test_decode_ignores_bytes_after_record
    );

    return UNITY_END();
}
