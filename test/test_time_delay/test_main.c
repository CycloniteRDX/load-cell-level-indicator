#include <stdint.h>

#include <unity.h>

#include "fake_hal_time.h"
#include "hal_time.h"


void setUp(void)
{
    fake_hal_time_reset();
}


void tearDown(void)
{
}


static void test_zero_delay_returns_without_reading_time(void)
{
    fake_hal_time_configure(
        100UL,
        1UL,
        1UL
    );

    hal_time_delay_ms(0UL);

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_hal_time_get_read_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        100UL,
        fake_hal_time_get_next_time()
    );
}


static void test_one_millisecond_delay_reaches_deadline(void)
{
    fake_hal_time_configure(
        100UL,
        1UL,
        1UL
    );

    hal_time_delay_ms(1UL);

    TEST_ASSERT_EQUAL_UINT32(
        2UL,
        fake_hal_time_get_read_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        101UL,
        fake_hal_time_get_last_returned_time()
    );
}


static void test_delay_waits_while_clock_value_is_unchanged(void)
{
    fake_hal_time_configure(
        100UL,
        1UL,
        3UL
    );

    hal_time_delay_ms(1UL);

    TEST_ASSERT_EQUAL_UINT32(
        4UL,
        fake_hal_time_get_read_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        101UL,
        fake_hal_time_get_last_returned_time()
    );
}


static void test_multi_millisecond_delay_stops_at_exact_deadline(void)
{
    fake_hal_time_configure(
        100UL,
        1UL,
        1UL
    );

    hal_time_delay_ms(5UL);

    TEST_ASSERT_EQUAL_UINT32(
        6UL,
        fake_hal_time_get_read_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        105UL,
        fake_hal_time_get_last_returned_time()
    );
}


static void test_delay_accepts_time_step_past_deadline(void)
{
    fake_hal_time_configure(
        100UL,
        3UL,
        1UL
    );

    hal_time_delay_ms(5UL);

    TEST_ASSERT_EQUAL_UINT32(
        3UL,
        fake_hal_time_get_read_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        106UL,
        fake_hal_time_get_last_returned_time()
    );
}


static void test_delay_handles_uint32_overflow(void)
{
    fake_hal_time_configure(
        UINT32_MAX - 1UL,
        1UL,
        1UL
    );

    hal_time_delay_ms(3UL);

    TEST_ASSERT_EQUAL_UINT32(
        4UL,
        fake_hal_time_get_read_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_hal_time_get_last_returned_time()
    );
}


int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(
        test_zero_delay_returns_without_reading_time
    );

    RUN_TEST(
        test_one_millisecond_delay_reaches_deadline
    );

    RUN_TEST(
        test_delay_waits_while_clock_value_is_unchanged
    );

    RUN_TEST(
        test_multi_millisecond_delay_stops_at_exact_deadline
    );

    RUN_TEST(
        test_delay_accepts_time_step_past_deadline
    );

    RUN_TEST(
        test_delay_handles_uint32_overflow
    );

    return UNITY_END();
}
