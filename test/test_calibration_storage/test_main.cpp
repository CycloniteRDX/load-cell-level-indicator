#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <unity.h>

#include "calibration_record.h"
#include "calibration_storage.h"
#include "fake_hal_storage.h"


static const float
    FLOAT_COMPARISON_TOLERANCE = 0.000000001F;

static const float
    MINIMUM_VALID_FACTOR = 0.000001F;

static const uint8_t EXPECTED_45_5_RECORD[
    CALIBRATION_RECORD_SIZE
] = {
    0x4CU,
    0x41U,
    0x43U,
    0x4CU,
    0x01U,
    0x00U,
    0x00U,
    0x00U,
    0x36U,
    0x42U,
    0x90U,
    0xF3U
};


void setUp(void)
{
    fake_hal_storage_reset();
}


void tearDown(void)
{
}


static uint32_t float_to_bits(
    float value
)
{
    uint32_t bits = 0U;

    memcpy(
        &bits,
        &value,
        sizeof(bits)
    );

    return bits;
}


static void assert_float_bits_equal(
    float expected,
    float actual
)
{
    TEST_ASSERT_EQUAL_HEX32(
        float_to_bits(expected),
        float_to_bits(actual)
    );
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
    float calibration_factor,
    uint8_t *record_bytes
)
{
    TEST_ASSERT_TRUE(
        calibration_record_encode(
            calibration_factor,
            record_bytes,
            CALIBRATION_RECORD_SIZE
        )
    );
}


static void test_factor_validation_accepts_signed_values(
    void
)
{
    TEST_ASSERT_TRUE(
        calibration_record_factor_is_valid(45.5F)
    );

    TEST_ASSERT_TRUE(
        calibration_record_factor_is_valid(-45.5F)
    );

    TEST_ASSERT_TRUE(
        calibration_record_factor_is_valid(1.0F)
    );

    TEST_ASSERT_TRUE(
        calibration_record_factor_is_valid(-1.0F)
    );
}


static void test_factor_validation_accepts_exact_boundaries(
    void
)
{
    TEST_ASSERT_TRUE(
        calibration_record_factor_is_valid(
            MINIMUM_VALID_FACTOR
        )
    );

    TEST_ASSERT_TRUE(
        calibration_record_factor_is_valid(
            -MINIMUM_VALID_FACTOR
        )
    );
}


static void test_factor_validation_rejects_signed_zero(
    void
)
{
    TEST_ASSERT_FALSE(
        calibration_record_factor_is_valid(0.0F)
    );

    TEST_ASSERT_FALSE(
        calibration_record_factor_is_valid(-0.0F)
    );
}


static void test_factor_validation_rejects_non_finite_values(
    void
)
{
    TEST_ASSERT_FALSE(
        calibration_record_factor_is_valid(NAN)
    );

    TEST_ASSERT_FALSE(
        calibration_record_factor_is_valid(INFINITY)
    );

    TEST_ASSERT_FALSE(
        calibration_record_factor_is_valid(-INFINITY)
    );
}


static void test_factor_validation_rejects_below_boundary(
    void
)
{
    const float below_boundary =
        MINIMUM_VALID_FACTOR * 0.5F;

    TEST_ASSERT_FALSE(
        calibration_record_factor_is_valid(
            below_boundary
        )
    );

    TEST_ASSERT_FALSE(
        calibration_record_factor_is_valid(
            -below_boundary
        )
    );
}


static void test_encode_rejects_null_output(
    void
)
{
    TEST_ASSERT_FALSE(
        calibration_record_encode(
            45.5F,
            nullptr,
            CALIBRATION_RECORD_SIZE
        )
    );
}


static void test_encode_rejects_short_buffer_without_modifying_it(
    void
)
{
    uint8_t record_bytes[CALIBRATION_RECORD_SIZE];

    memset(
        record_bytes,
        0xA5,
        sizeof(record_bytes)
    );

    TEST_ASSERT_FALSE(
        calibration_record_encode(
            45.5F,
            record_bytes,
            CALIBRATION_RECORD_SIZE - 1U
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


static void test_encode_rejects_invalid_factor_without_modifying_buffer(
    void
)
{
    uint8_t record_bytes[CALIBRATION_RECORD_SIZE];

    memset(
        record_bytes,
        0x5A,
        sizeof(record_bytes)
    );

    TEST_ASSERT_FALSE(
        calibration_record_encode(
            0.0F,
            record_bytes,
            sizeof(record_bytes)
        )
    );

    for (size_t index = 0U;
         index < sizeof(record_bytes);
         ++index)
    {
        TEST_ASSERT_EQUAL_HEX8(
            0x5AU,
            record_bytes[index]
        );
    }
}


static void test_encode_produces_known_little_endian_record(
    void
)
{
    uint8_t record_bytes[CALIBRATION_RECORD_SIZE] = {0U};

    encode_valid_record(
        45.5F,
        record_bytes
    );

    TEST_ASSERT_EQUAL_UINT8_ARRAY(
        EXPECTED_45_5_RECORD,
        record_bytes,
        CALIBRATION_RECORD_SIZE
    );
}


static void test_encode_does_not_modify_bytes_after_record(
    void
)
{
    uint8_t record_bytes[
        CALIBRATION_RECORD_SIZE + 2U
    ];

    memset(
        record_bytes,
        0xCC,
        sizeof(record_bytes)
    );

    TEST_ASSERT_TRUE(
        calibration_record_encode(
            45.5F,
            record_bytes,
            sizeof(record_bytes)
        )
    );

    TEST_ASSERT_EQUAL_HEX8(
        0xCCU,
        record_bytes[CALIBRATION_RECORD_SIZE]
    );

    TEST_ASSERT_EQUAL_HEX8(
        0xCCU,
        record_bytes[CALIBRATION_RECORD_SIZE + 1U]
    );
}


static void test_decode_rejects_null_input(
    void
)
{
    float decoded_factor = 123.0F;

    TEST_ASSERT_FALSE(
        calibration_record_decode(
            nullptr,
            CALIBRATION_RECORD_SIZE,
            &decoded_factor
        )
    );

    assert_float_bits_equal(
        123.0F,
        decoded_factor
    );
}


static void test_decode_rejects_null_output(
    void
)
{
    TEST_ASSERT_FALSE(
        calibration_record_decode(
            EXPECTED_45_5_RECORD,
            CALIBRATION_RECORD_SIZE,
            nullptr
        )
    );
}


static void test_decode_rejects_short_buffer_and_preserves_output(
    void
)
{
    float decoded_factor = 123.0F;

    TEST_ASSERT_FALSE(
        calibration_record_decode(
            EXPECTED_45_5_RECORD,
            CALIBRATION_RECORD_SIZE - 1U,
            &decoded_factor
        )
    );

    assert_float_bits_equal(
        123.0F,
        decoded_factor
    );
}


static void test_decode_accepts_known_record(
    void
)
{
    float decoded_factor = 0.0F;

    TEST_ASSERT_TRUE(
        calibration_record_decode(
            EXPECTED_45_5_RECORD,
            CALIBRATION_RECORD_SIZE,
            &decoded_factor
        )
    );

    assert_float_bits_equal(
        45.5F,
        decoded_factor
    );
}


static void test_round_trip_preserves_positive_and_negative_factors(
    void
)
{
    const float factors[] = {
        45.5F,
        -45.5F,
        MINIMUM_VALID_FACTOR,
        -MINIMUM_VALID_FACTOR
    };

    for (size_t index = 0U;
         index <
            (sizeof(factors) / sizeof(factors[0]));
         ++index)
    {
        uint8_t record_bytes[
            CALIBRATION_RECORD_SIZE
        ] = {0U};

        float decoded_factor = 0.0F;

        encode_valid_record(
            factors[index],
            record_bytes
        );

        TEST_ASSERT_TRUE(
            calibration_record_decode(
                record_bytes,
                sizeof(record_bytes),
                &decoded_factor
            )
        );

        assert_float_bits_equal(
            factors[index],
            decoded_factor
        );
    }
}


static void test_decode_rejects_incorrect_magic_and_preserves_output(
    void
)
{
    uint8_t record_bytes[CALIBRATION_RECORD_SIZE];

    memcpy(
        record_bytes,
        EXPECTED_45_5_RECORD,
        sizeof(record_bytes)
    );

    record_bytes[0] ^= 0x01U;

    float decoded_factor = 123.0F;

    TEST_ASSERT_FALSE(
        calibration_record_decode(
            record_bytes,
            sizeof(record_bytes),
            &decoded_factor
        )
    );

    assert_float_bits_equal(
        123.0F,
        decoded_factor
    );
}


static void test_decode_rejects_unsupported_version_and_preserves_output(
    void
)
{
    uint8_t record_bytes[CALIBRATION_RECORD_SIZE];

    memcpy(
        record_bytes,
        EXPECTED_45_5_RECORD,
        sizeof(record_bytes)
    );

    record_bytes[4] = 0x02U;

    float decoded_factor = 123.0F;

    TEST_ASSERT_FALSE(
        calibration_record_decode(
            record_bytes,
            sizeof(record_bytes),
            &decoded_factor
        )
    );

    assert_float_bits_equal(
        123.0F,
        decoded_factor
    );
}


static void test_decode_rejects_corrupted_payload_checksum(
    void
)
{
    uint8_t record_bytes[CALIBRATION_RECORD_SIZE];

    memcpy(
        record_bytes,
        EXPECTED_45_5_RECORD,
        sizeof(record_bytes)
    );

    record_bytes[8] ^= 0x01U;

    float decoded_factor = 123.0F;

    TEST_ASSERT_FALSE(
        calibration_record_decode(
            record_bytes,
            sizeof(record_bytes),
            &decoded_factor
        )
    );

    assert_float_bits_equal(
        123.0F,
        decoded_factor
    );
}


static void test_decode_rejects_corrupted_stored_checksum(
    void
)
{
    uint8_t record_bytes[CALIBRATION_RECORD_SIZE];

    memcpy(
        record_bytes,
        EXPECTED_45_5_RECORD,
        sizeof(record_bytes)
    );

    record_bytes[10] ^= 0x01U;

    float decoded_factor = 123.0F;

    TEST_ASSERT_FALSE(
        calibration_record_decode(
            record_bytes,
            sizeof(record_bytes),
            &decoded_factor
        )
    );

    assert_float_bits_equal(
        123.0F,
        decoded_factor
    );
}


static void test_decode_rejects_invalid_factor_with_valid_checksum(
    void
)
{
    uint8_t record_bytes[CALIBRATION_RECORD_SIZE];

    memcpy(
        record_bytes,
        EXPECTED_45_5_RECORD,
        sizeof(record_bytes)
    );

    record_bytes[6] = 0x00U;
    record_bytes[7] = 0x00U;
    record_bytes[8] = 0x00U;
    record_bytes[9] = 0x00U;

    update_record_checksum(record_bytes);

    float decoded_factor = 123.0F;

    TEST_ASSERT_FALSE(
        calibration_record_decode(
            record_bytes,
            sizeof(record_bytes),
            &decoded_factor
        )
    );

    assert_float_bits_equal(
        123.0F,
        decoded_factor
    );
}


static void test_storage_load_reads_valid_record(
    void
)
{
    TEST_ASSERT_TRUE(
        fake_hal_storage_preload(
            0U,
            EXPECTED_45_5_RECORD,
            CALIBRATION_RECORD_SIZE
        )
    );

    float loaded_factor = 0.0F;

    TEST_ASSERT_EQUAL_INT(
        STORAGE_LOAD_VALID,
        calibration_storage_load(
            &loaded_factor
        )
    );

    assert_float_bits_equal(
        45.5F,
        loaded_factor
    );

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hal_storage_read_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hal_storage_read_address(1U)
    );

    TEST_ASSERT_EQUAL_UINT32(
        CALIBRATION_RECORD_SIZE,
        fake_hal_storage_read_length(1U)
    );

    TEST_ASSERT_FALSE(
        fake_hal_storage_had_invalid_access()
    );
}


static void test_storage_load_rejects_null_output_without_reading(
    void
)
{
    TEST_ASSERT_EQUAL_INT(
        STORAGE_LOAD_ACCESS_ERROR,
        calibration_storage_load(nullptr)
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hal_storage_read_call_count()
    );
}


static void test_storage_load_rejects_insufficient_capacity_without_reading(
    void
)
{
    fake_hal_storage_set_capacity(
        CALIBRATION_RECORD_SIZE - 1U
    );

    float loaded_factor = 123.0F;

    TEST_ASSERT_EQUAL_INT(
        STORAGE_LOAD_ACCESS_ERROR,
        calibration_storage_load(
            &loaded_factor
        )
    );

    assert_float_bits_equal(
        123.0F,
        loaded_factor
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hal_storage_read_call_count()
    );
}


static void test_storage_load_preserves_output_after_read_failure(
    void
)
{
    fake_hal_storage_fail_read_call(1U);

    float loaded_factor = 123.0F;

    TEST_ASSERT_EQUAL_INT(
        STORAGE_LOAD_ACCESS_ERROR,
        calibration_storage_load(
            &loaded_factor
        )
    );

    assert_float_bits_equal(
        123.0F,
        loaded_factor
    );

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hal_storage_read_call_count()
    );
}


static void test_storage_load_reports_absent_for_erased_storage(
    void
)
{
    fake_hal_storage_fill(0xFFU);

    float loaded_factor = 123.0F;

    TEST_ASSERT_EQUAL_INT(
        STORAGE_LOAD_ABSENT,
        calibration_storage_load(
            &loaded_factor
        )
    );

    assert_float_bits_equal(
        123.0F,
        loaded_factor
    );
}


static void test_storage_load_reports_invalid_for_corrupted_record(
    void
)
{
    uint8_t corrupted_record[CALIBRATION_RECORD_SIZE];

    memcpy(
        corrupted_record,
        EXPECTED_45_5_RECORD,
        sizeof(corrupted_record)
    );

    corrupted_record[8] ^= 0x01U;

    TEST_ASSERT_TRUE(
        fake_hal_storage_preload(
            0U,
            corrupted_record,
            sizeof(corrupted_record)
        )
    );

    float loaded_factor = 123.0F;

    TEST_ASSERT_EQUAL_INT(
        STORAGE_LOAD_INVALID,
        calibration_storage_load(
            &loaded_factor
        )
    );

    assert_float_bits_equal(
        123.0F,
        loaded_factor
    );
}


static void test_storage_save_writes_known_record_and_verifies_it(
    void
)
{
    TEST_ASSERT_TRUE(
        calibration_storage_save(45.5F)
    );

    uint8_t stored_record[CALIBRATION_RECORD_SIZE] = {};

    TEST_ASSERT_TRUE(
        fake_hal_storage_copy(
            0U,
            stored_record,
            sizeof(stored_record)
        )
    );

    TEST_ASSERT_EQUAL_UINT8_ARRAY(
        EXPECTED_45_5_RECORD,
        stored_record,
        CALIBRATION_RECORD_SIZE
    );

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hal_storage_write_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hal_storage_write_address(1U)
    );

    TEST_ASSERT_EQUAL_UINT32(
        CALIBRATION_RECORD_SIZE,
        fake_hal_storage_write_length(1U)
    );

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hal_storage_read_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hal_storage_read_address(1U)
    );

    TEST_ASSERT_EQUAL_UINT32(
        CALIBRATION_RECORD_SIZE,
        fake_hal_storage_read_length(1U)
    );
}


static void test_storage_save_and_load_preserve_negative_factor(
    void
)
{
    TEST_ASSERT_TRUE(
        calibration_storage_save(-45.5F)
    );

    float loaded_factor = 0.0F;

    TEST_ASSERT_EQUAL_INT(
        STORAGE_LOAD_VALID,
        calibration_storage_load(
            &loaded_factor
        )
    );

    assert_float_bits_equal(
        -45.5F,
        loaded_factor
    );
}


static void test_storage_save_rejects_invalid_factor_without_io(
    void
)
{
    TEST_ASSERT_FALSE(
        calibration_storage_save(0.0F)
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hal_storage_write_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hal_storage_read_call_count()
    );
}


static void test_storage_save_rejects_insufficient_capacity_without_io(
    void
)
{
    fake_hal_storage_set_capacity(
        CALIBRATION_RECORD_SIZE - 1U
    );

    TEST_ASSERT_FALSE(
        calibration_storage_save(45.5F)
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hal_storage_write_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hal_storage_read_call_count()
    );
}


static void test_storage_save_stops_after_write_failure(
    void
)
{
    fake_hal_storage_fail_write_call(1U);

    TEST_ASSERT_FALSE(
        calibration_storage_save(45.5F)
    );

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hal_storage_write_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hal_storage_read_call_count()
    );
}


static void test_storage_save_rejects_verification_read_failure(
    void
)
{
    fake_hal_storage_fail_read_call(1U);

    TEST_ASSERT_FALSE(
        calibration_storage_save(45.5F)
    );

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hal_storage_write_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hal_storage_read_call_count()
    );
}


static void test_storage_save_rejects_corrupted_verification_record(
    void
)
{
    uint8_t corrupted_record[CALIBRATION_RECORD_SIZE];

    memcpy(
        corrupted_record,
        EXPECTED_45_5_RECORD,
        sizeof(corrupted_record)
    );

    corrupted_record[8] ^= 0x01U;

    TEST_ASSERT_TRUE(
        fake_hal_storage_set_read_override(
            1U,
            corrupted_record,
            sizeof(corrupted_record)
        )
    );

    TEST_ASSERT_FALSE(
        calibration_storage_save(45.5F)
    );
}


static void test_storage_save_rejects_mismatched_valid_factor(
    void
)
{
    uint8_t different_record[CALIBRATION_RECORD_SIZE] = {};

    encode_valid_record(
        46.5F,
        different_record
    );

    TEST_ASSERT_TRUE(
        fake_hal_storage_set_read_override(
            1U,
            different_record,
            sizeof(different_record)
        )
    );

    TEST_ASSERT_FALSE(
        calibration_storage_save(45.5F)
    );
}


static void test_storage_clear_invalidates_only_magic_bytes(
    void
)
{
    TEST_ASSERT_TRUE(
        fake_hal_storage_preload(
            0U,
            EXPECTED_45_5_RECORD,
            CALIBRATION_RECORD_SIZE
        )
    );

    TEST_ASSERT_TRUE(
        calibration_storage_clear()
    );

    uint8_t stored_record[CALIBRATION_RECORD_SIZE] = {};

    TEST_ASSERT_TRUE(
        fake_hal_storage_copy(
            0U,
            stored_record,
            sizeof(stored_record)
        )
    );

    for (size_t index = 0U;
         index < 4U;
         ++index)
    {
        TEST_ASSERT_EQUAL_HEX8(
            0x00U,
            stored_record[index]
        );
    }

    TEST_ASSERT_EQUAL_UINT8_ARRAY(
        &EXPECTED_45_5_RECORD[4],
        &stored_record[4],
        CALIBRATION_RECORD_SIZE - 4U
    );

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hal_storage_write_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hal_storage_write_address(1U)
    );

    TEST_ASSERT_EQUAL_UINT32(
        4U,
        fake_hal_storage_write_length(1U)
    );

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hal_storage_read_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hal_storage_read_address(1U)
    );

    TEST_ASSERT_EQUAL_UINT32(
        4U,
        fake_hal_storage_read_length(1U)
    );
}


static void test_storage_clear_rejects_insufficient_capacity_without_io(
    void
)
{
    fake_hal_storage_set_capacity(
        CALIBRATION_RECORD_SIZE - 1U
    );

    TEST_ASSERT_FALSE(
        calibration_storage_clear()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hal_storage_write_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hal_storage_read_call_count()
    );
}


static void test_storage_clear_stops_after_write_failure(
    void
)
{
    fake_hal_storage_fail_write_call(1U);

    TEST_ASSERT_FALSE(
        calibration_storage_clear()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hal_storage_write_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hal_storage_read_call_count()
    );
}


static void test_storage_clear_rejects_verification_read_failure(
    void
)
{
    fake_hal_storage_fail_read_call(1U);

    TEST_ASSERT_FALSE(
        calibration_storage_clear()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hal_storage_write_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hal_storage_read_call_count()
    );
}


static void test_storage_clear_rejects_unmodified_magic(
    void
)
{
    TEST_ASSERT_TRUE(
        fake_hal_storage_preload(
            0U,
            EXPECTED_45_5_RECORD,
            CALIBRATION_RECORD_SIZE
        )
    );

    fake_hal_storage_discard_writes(true);

    TEST_ASSERT_FALSE(
        calibration_storage_clear()
    );
}


static void test_storage_load_reports_absent_after_successful_clear(
    void
)
{
    TEST_ASSERT_TRUE(
        fake_hal_storage_preload(
            0U,
            EXPECTED_45_5_RECORD,
            CALIBRATION_RECORD_SIZE
        )
    );

    TEST_ASSERT_TRUE(
        calibration_storage_clear()
    );

    float loaded_factor = 123.0F;

    TEST_ASSERT_EQUAL_INT(
        STORAGE_LOAD_ABSENT,
        calibration_storage_load(
            &loaded_factor
        )
    );

    assert_float_bits_equal(
        123.0F,
        loaded_factor
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
        test_factor_validation_accepts_signed_values
    );

    RUN_TEST(
        test_factor_validation_accepts_exact_boundaries
    );

    RUN_TEST(
        test_factor_validation_rejects_signed_zero
    );

    RUN_TEST(
        test_factor_validation_rejects_non_finite_values
    );

    RUN_TEST(
        test_factor_validation_rejects_below_boundary
    );

    RUN_TEST(
        test_encode_rejects_null_output
    );

    RUN_TEST(
        test_encode_rejects_short_buffer_without_modifying_it
    );

    RUN_TEST(
        test_encode_rejects_invalid_factor_without_modifying_buffer
    );

    RUN_TEST(
        test_encode_produces_known_little_endian_record
    );

    RUN_TEST(
        test_encode_does_not_modify_bytes_after_record
    );

    RUN_TEST(
        test_decode_rejects_null_input
    );

    RUN_TEST(
        test_decode_rejects_null_output
    );

    RUN_TEST(
        test_decode_rejects_short_buffer_and_preserves_output
    );

    RUN_TEST(
        test_decode_accepts_known_record
    );

    RUN_TEST(
        test_round_trip_preserves_positive_and_negative_factors
    );

    RUN_TEST(
        test_decode_rejects_incorrect_magic_and_preserves_output
    );

    RUN_TEST(
        test_decode_rejects_unsupported_version_and_preserves_output
    );

    RUN_TEST(
        test_decode_rejects_corrupted_payload_checksum
    );

    RUN_TEST(
        test_decode_rejects_corrupted_stored_checksum
    );

    RUN_TEST(
        test_decode_rejects_invalid_factor_with_valid_checksum
    );


    RUN_TEST(
        test_storage_load_reads_valid_record
    );

    RUN_TEST(
        test_storage_load_rejects_null_output_without_reading
    );

    RUN_TEST(
        test_storage_load_rejects_insufficient_capacity_without_reading
    );

    RUN_TEST(
        test_storage_load_preserves_output_after_read_failure
    );

    RUN_TEST(
        test_storage_load_reports_absent_for_erased_storage
    );

    RUN_TEST(
        test_storage_load_reports_invalid_for_corrupted_record
    );

    RUN_TEST(
        test_storage_save_writes_known_record_and_verifies_it
    );

    RUN_TEST(
        test_storage_save_and_load_preserve_negative_factor
    );

    RUN_TEST(
        test_storage_save_rejects_invalid_factor_without_io
    );

    RUN_TEST(
        test_storage_save_rejects_insufficient_capacity_without_io
    );

    RUN_TEST(
        test_storage_save_stops_after_write_failure
    );

    RUN_TEST(
        test_storage_save_rejects_verification_read_failure
    );

    RUN_TEST(
        test_storage_save_rejects_corrupted_verification_record
    );

    RUN_TEST(
        test_storage_save_rejects_mismatched_valid_factor
    );

    RUN_TEST(
        test_storage_clear_invalidates_only_magic_bytes
    );

    RUN_TEST(
        test_storage_clear_rejects_insufficient_capacity_without_io
    );

    RUN_TEST(
        test_storage_clear_stops_after_write_failure
    );

    RUN_TEST(
        test_storage_clear_rejects_verification_read_failure
    );

    RUN_TEST(
        test_storage_clear_rejects_unmodified_magic
    );

    RUN_TEST(
        test_storage_load_reports_absent_after_successful_clear
    );

    return UNITY_END();
}
