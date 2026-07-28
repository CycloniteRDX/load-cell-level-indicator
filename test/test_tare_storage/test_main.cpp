#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <unity.h>

#include "fake_hal_storage.h"
#include "storage_layout.h"
#include "tare_record.h"
#include "tare_storage.h"


static const int32_t KNOWN_POSITIVE_OFFSET =
    (int32_t)0x12345678L;

static const int32_t KNOWN_NEGATIVE_OFFSET =
    -1234567;


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


static const uint8_t CALIBRATION_SENTINEL[
    CALIBRATION_RECORD_SIZE
] = {
    0xC0U,
    0xC1U,
    0xC2U,
    0xC3U,
    0xC4U,
    0xC5U,
    0xC6U,
    0xC7U,
    0xC8U,
    0xC9U,
    0xCAU,
    0xCBU
};


void setUp(void)
{
    fake_hal_storage_reset();
}


void tearDown(void)
{
}


static void encode_valid_tare_record(
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


static void preload_valid_tare_record(
    int32_t tare_offset
)
{
    uint8_t record_bytes[TARE_RECORD_SIZE] = {0U};

    encode_valid_tare_record(
        tare_offset,
        record_bytes
    );

    TEST_ASSERT_TRUE(
        fake_hal_storage_preload(
            TARE_STORAGE_ADDRESS,
            record_bytes,
            sizeof(record_bytes)
        )
    );
}


static void assert_calibration_region_unchanged(
    void
)
{
    uint8_t stored_bytes[CALIBRATION_RECORD_SIZE] = {0U};

    TEST_ASSERT_TRUE(
        fake_hal_storage_copy(
            CALIBRATION_STORAGE_ADDRESS,
            stored_bytes,
            sizeof(stored_bytes)
        )
    );

    TEST_ASSERT_EQUAL_UINT8_ARRAY(
        CALIBRATION_SENTINEL,
        stored_bytes,
        sizeof(stored_bytes)
    );
}


static void preload_calibration_sentinel(
    void
)
{
    TEST_ASSERT_TRUE(
        fake_hal_storage_preload(
            CALIBRATION_STORAGE_ADDRESS,
            CALIBRATION_SENTINEL,
            sizeof(CALIBRATION_SENTINEL)
        )
    );
}


static void test_layout_assigns_non_overlapping_regions(
    void
)
{
    TEST_ASSERT_EQUAL_UINT32(
        0U,
        CALIBRATION_STORAGE_ADDRESS
    );

    TEST_ASSERT_EQUAL_UINT32(
        CALIBRATION_RECORD_SIZE,
        TARE_STORAGE_ADDRESS
    );

    TEST_ASSERT_EQUAL_UINT32(
        CALIBRATION_RECORD_SIZE +
        TARE_RECORD_SIZE,
        STORAGE_LAYOUT_REQUIRED_CAPACITY
    );
}


static void test_load_reads_valid_record_from_tare_region(
    void
)
{
    preload_valid_tare_record(
        KNOWN_POSITIVE_OFFSET
    );

    int32_t loaded_offset = 0;

    TEST_ASSERT_TRUE(
        tare_storage_load(
            &loaded_offset
        )
    );

    TEST_ASSERT_EQUAL_INT32(
        KNOWN_POSITIVE_OFFSET,
        loaded_offset
    );

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hal_storage_read_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        TARE_STORAGE_ADDRESS,
        fake_hal_storage_read_address(1U)
    );

    TEST_ASSERT_EQUAL_UINT32(
        TARE_RECORD_SIZE,
        fake_hal_storage_read_length(1U)
    );

    TEST_ASSERT_FALSE(
        fake_hal_storage_had_invalid_access()
    );
}


static void test_load_rejects_null_output_without_io(
    void
)
{
    TEST_ASSERT_FALSE(
        tare_storage_load(nullptr)
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hal_storage_read_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hal_storage_write_call_count()
    );
}


static void test_load_rejects_insufficient_capacity_without_io(
    void
)
{
    fake_hal_storage_set_capacity(
        STORAGE_LAYOUT_REQUIRED_CAPACITY - 1U
    );

    int32_t loaded_offset = 456;

    TEST_ASSERT_FALSE(
        tare_storage_load(
            &loaded_offset
        )
    );

    TEST_ASSERT_EQUAL_INT32(
        456,
        loaded_offset
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hal_storage_read_call_count()
    );
}


static void test_load_preserves_output_after_read_failure(
    void
)
{
    preload_valid_tare_record(
        KNOWN_POSITIVE_OFFSET
    );

    fake_hal_storage_fail_read_call(1U);

    int32_t loaded_offset = -789;

    TEST_ASSERT_FALSE(
        tare_storage_load(
            &loaded_offset
        )
    );

    TEST_ASSERT_EQUAL_INT32(
        -789,
        loaded_offset
    );
}


static void test_load_rejects_erased_storage_and_preserves_output(
    void
)
{
    int32_t loaded_offset = 1234;

    TEST_ASSERT_FALSE(
        tare_storage_load(
            &loaded_offset
        )
    );

    TEST_ASSERT_EQUAL_INT32(
        1234,
        loaded_offset
    );
}


static void test_load_rejects_corrupted_record_and_preserves_output(
    void
)
{
    uint8_t record_bytes[TARE_RECORD_SIZE] = {0U};

    encode_valid_tare_record(
        KNOWN_POSITIVE_OFFSET,
        record_bytes
    );

    record_bytes[8] ^= 0x80U;

    TEST_ASSERT_TRUE(
        fake_hal_storage_preload(
            TARE_STORAGE_ADDRESS,
            record_bytes,
            sizeof(record_bytes)
        )
    );

    int32_t loaded_offset = -4321;

    TEST_ASSERT_FALSE(
        tare_storage_load(
            &loaded_offset
        )
    );

    TEST_ASSERT_EQUAL_INT32(
        -4321,
        loaded_offset
    );
}


static void test_save_writes_known_record_and_verifies_it(
    void
)
{
    preload_calibration_sentinel();

    TEST_ASSERT_TRUE(
        tare_storage_save(
            KNOWN_POSITIVE_OFFSET
        )
    );

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hal_storage_write_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        TARE_STORAGE_ADDRESS,
        fake_hal_storage_write_address(1U)
    );

    TEST_ASSERT_EQUAL_UINT32(
        TARE_RECORD_SIZE,
        fake_hal_storage_write_length(1U)
    );

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hal_storage_read_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        TARE_STORAGE_ADDRESS,
        fake_hal_storage_read_address(1U)
    );

    TEST_ASSERT_EQUAL_UINT32(
        TARE_RECORD_SIZE,
        fake_hal_storage_read_length(1U)
    );

    uint8_t stored_record[TARE_RECORD_SIZE] = {0U};

    TEST_ASSERT_TRUE(
        fake_hal_storage_copy(
            TARE_STORAGE_ADDRESS,
            stored_record,
            sizeof(stored_record)
        )
    );

    TEST_ASSERT_EQUAL_UINT8_ARRAY(
        EXPECTED_POSITIVE_RECORD,
        stored_record,
        sizeof(stored_record)
    );

    assert_calibration_region_unchanged();

    TEST_ASSERT_FALSE(
        fake_hal_storage_had_invalid_access()
    );
}


static void test_save_and_load_preserve_negative_offset(
    void
)
{
    TEST_ASSERT_TRUE(
        tare_storage_save(
            KNOWN_NEGATIVE_OFFSET
        )
    );

    int32_t loaded_offset = 0;

    TEST_ASSERT_TRUE(
        tare_storage_load(
            &loaded_offset
        )
    );

    TEST_ASSERT_EQUAL_INT32(
        KNOWN_NEGATIVE_OFFSET,
        loaded_offset
    );
}


static void test_save_and_load_preserve_int32_boundaries(
    void
)
{
    TEST_ASSERT_TRUE(
        tare_storage_save(INT32_MIN)
    );

    int32_t loaded_offset = 0;

    TEST_ASSERT_TRUE(
        tare_storage_load(
            &loaded_offset
        )
    );

    TEST_ASSERT_EQUAL_INT32(
        INT32_MIN,
        loaded_offset
    );

    fake_hal_storage_reset();

    TEST_ASSERT_TRUE(
        tare_storage_save(INT32_MAX)
    );

    loaded_offset = 0;

    TEST_ASSERT_TRUE(
        tare_storage_load(
            &loaded_offset
        )
    );

    TEST_ASSERT_EQUAL_INT32(
        INT32_MAX,
        loaded_offset
    );
}


static void test_save_rejects_insufficient_capacity_without_io(
    void
)
{
    fake_hal_storage_set_capacity(
        STORAGE_LAYOUT_REQUIRED_CAPACITY - 1U
    );

    TEST_ASSERT_FALSE(
        tare_storage_save(
            KNOWN_POSITIVE_OFFSET
        )
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


static void test_save_stops_after_write_failure(
    void
)
{
    fake_hal_storage_fail_write_call(1U);

    TEST_ASSERT_FALSE(
        tare_storage_save(
            KNOWN_POSITIVE_OFFSET
        )
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


static void test_save_rejects_verification_read_failure(
    void
)
{
    fake_hal_storage_fail_read_call(1U);

    TEST_ASSERT_FALSE(
        tare_storage_save(
            KNOWN_POSITIVE_OFFSET
        )
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


static void test_save_rejects_corrupted_verification_record(
    void
)
{
    uint8_t corrupted_record[TARE_RECORD_SIZE] = {0U};

    encode_valid_tare_record(
        KNOWN_POSITIVE_OFFSET,
        corrupted_record
    );

    corrupted_record[7] ^= 0x01U;

    TEST_ASSERT_TRUE(
        fake_hal_storage_set_read_override(
            1U,
            corrupted_record,
            sizeof(corrupted_record)
        )
    );

    TEST_ASSERT_FALSE(
        tare_storage_save(
            KNOWN_POSITIVE_OFFSET
        )
    );
}


static void test_save_rejects_mismatched_valid_offset(
    void
)
{
    uint8_t other_record[TARE_RECORD_SIZE] = {0U};

    encode_valid_tare_record(
        KNOWN_NEGATIVE_OFFSET,
        other_record
    );

    TEST_ASSERT_TRUE(
        fake_hal_storage_set_read_override(
            1U,
            other_record,
            sizeof(other_record)
        )
    );

    TEST_ASSERT_FALSE(
        tare_storage_save(
            KNOWN_POSITIVE_OFFSET
        )
    );
}


static void test_clear_invalidates_only_tare_magic_bytes(
    void
)
{
    preload_calibration_sentinel();

    uint8_t original_record[TARE_RECORD_SIZE] = {0U};

    encode_valid_tare_record(
        KNOWN_POSITIVE_OFFSET,
        original_record
    );

    TEST_ASSERT_TRUE(
        fake_hal_storage_preload(
            TARE_STORAGE_ADDRESS,
            original_record,
            sizeof(original_record)
        )
    );

    TEST_ASSERT_TRUE(
        tare_storage_clear()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hal_storage_write_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        TARE_STORAGE_ADDRESS,
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
        TARE_STORAGE_ADDRESS,
        fake_hal_storage_read_address(1U)
    );

    TEST_ASSERT_EQUAL_UINT32(
        4U,
        fake_hal_storage_read_length(1U)
    );

    uint8_t cleared_record[TARE_RECORD_SIZE] = {0U};

    TEST_ASSERT_TRUE(
        fake_hal_storage_copy(
            TARE_STORAGE_ADDRESS,
            cleared_record,
            sizeof(cleared_record)
        )
    );

    for (size_t index = 0U;
         index < 4U;
         ++index)
    {
        TEST_ASSERT_EQUAL_HEX8(
            0x00U,
            cleared_record[index]
        );
    }

    TEST_ASSERT_EQUAL_UINT8_ARRAY(
        &original_record[4],
        &cleared_record[4],
        TARE_RECORD_SIZE - 4U
    );

    assert_calibration_region_unchanged();
}


static void test_clear_rejects_insufficient_capacity_without_io(
    void
)
{
    fake_hal_storage_set_capacity(
        STORAGE_LAYOUT_REQUIRED_CAPACITY - 1U
    );

    TEST_ASSERT_FALSE(
        tare_storage_clear()
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


static void test_clear_stops_after_write_failure(
    void
)
{
    preload_valid_tare_record(
        KNOWN_POSITIVE_OFFSET
    );

    fake_hal_storage_fail_write_call(1U);

    TEST_ASSERT_FALSE(
        tare_storage_clear()
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


static void test_clear_rejects_verification_read_failure(
    void
)
{
    preload_valid_tare_record(
        KNOWN_POSITIVE_OFFSET
    );

    fake_hal_storage_fail_read_call(1U);

    TEST_ASSERT_FALSE(
        tare_storage_clear()
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


static void test_clear_rejects_unmodified_magic(
    void
)
{
    preload_valid_tare_record(
        KNOWN_POSITIVE_OFFSET
    );

    fake_hal_storage_discard_writes(true);

    TEST_ASSERT_FALSE(
        tare_storage_clear()
    );
}


static void test_load_fails_after_successful_clear(
    void
)
{
    preload_valid_tare_record(
        KNOWN_POSITIVE_OFFSET
    );

    TEST_ASSERT_TRUE(
        tare_storage_clear()
    );

    int32_t loaded_offset = 2468;

    TEST_ASSERT_FALSE(
        tare_storage_load(
            &loaded_offset
        )
    );

    TEST_ASSERT_EQUAL_INT32(
        2468,
        loaded_offset
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
        test_layout_assigns_non_overlapping_regions
    );

    RUN_TEST(
        test_load_reads_valid_record_from_tare_region
    );

    RUN_TEST(
        test_load_rejects_null_output_without_io
    );

    RUN_TEST(
        test_load_rejects_insufficient_capacity_without_io
    );

    RUN_TEST(
        test_load_preserves_output_after_read_failure
    );

    RUN_TEST(
        test_load_rejects_erased_storage_and_preserves_output
    );

    RUN_TEST(
        test_load_rejects_corrupted_record_and_preserves_output
    );

    RUN_TEST(
        test_save_writes_known_record_and_verifies_it
    );

    RUN_TEST(
        test_save_and_load_preserve_negative_offset
    );

    RUN_TEST(
        test_save_and_load_preserve_int32_boundaries
    );

    RUN_TEST(
        test_save_rejects_insufficient_capacity_without_io
    );

    RUN_TEST(
        test_save_stops_after_write_failure
    );

    RUN_TEST(
        test_save_rejects_verification_read_failure
    );

    RUN_TEST(
        test_save_rejects_corrupted_verification_record
    );

    RUN_TEST(
        test_save_rejects_mismatched_valid_offset
    );

    RUN_TEST(
        test_clear_invalidates_only_tare_magic_bytes
    );

    RUN_TEST(
        test_clear_rejects_insufficient_capacity_without_io
    );

    RUN_TEST(
        test_clear_stops_after_write_failure
    );

    RUN_TEST(
        test_clear_rejects_verification_read_failure
    );

    RUN_TEST(
        test_clear_rejects_unmodified_magic
    );

    RUN_TEST(
        test_load_fails_after_successful_clear
    );

    return UNITY_END();
}
