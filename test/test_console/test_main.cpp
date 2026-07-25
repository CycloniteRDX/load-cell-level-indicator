#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include <unity.h>

#include "console.h"
#include "fake_hal_serial.h"


static const uint32_t TEST_BAUD_RATE =
    115200UL;


void setUp(void)
{
    fake_hal_serial_reset();
}


void tearDown(void)
{
}


static void assert_output_is(
    const char *expected_output
)
{
    TEST_ASSERT_FALSE(
        fake_hal_serial_output_overflowed()
    );

    TEST_ASSERT_EQUAL_STRING(
        expected_output,
        fake_hal_serial_get_output_text()
    );

    TEST_ASSERT_EQUAL_UINT32(
        (uint32_t)fake_hal_serial_get_output_length(),
        fake_hal_serial_get_write_call_count()
    );
}


static void test_console_init_forwards_baud_rate(void)
{
    console_init(TEST_BAUD_RATE);

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hal_serial_get_init_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        TEST_BAUD_RATE,
        fake_hal_serial_get_last_baud_rate()
    );
}


static void test_input_available_is_false_when_empty(void)
{
    TEST_ASSERT_FALSE(
        console_input_available()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hal_serial_get_available_call_count()
    );
}


static void test_input_available_is_true_when_data_waits(void)
{
    TEST_ASSERT_TRUE(
        fake_hal_serial_load_input_text("c")
    );

    TEST_ASSERT_TRUE(
        console_input_available()
    );
}


static void test_read_char_rejects_null_without_reading(void)
{
    TEST_ASSERT_TRUE(
        fake_hal_serial_load_input_text("t")
    );

    TEST_ASSERT_FALSE(
        console_read_char(NULL)
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hal_serial_get_read_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        (uint32_t)
        fake_hal_serial_get_pending_input_length()
    );
}


static void test_read_char_preserves_output_when_empty(void)
{
    char character = 'Z';

    TEST_ASSERT_FALSE(
        console_read_char(&character)
    );

    TEST_ASSERT_EQUAL_CHAR('Z', character);

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hal_serial_get_read_call_count()
    );
}


static void test_read_char_consumes_one_character(void)
{
    TEST_ASSERT_TRUE(
        fake_hal_serial_load_input_text("c")
    );

    char character = '?';

    TEST_ASSERT_TRUE(
        console_read_char(&character)
    );

    TEST_ASSERT_EQUAL_CHAR('c', character);

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        (uint32_t)
        fake_hal_serial_get_pending_input_length()
    );
}


static void test_read_char_preserves_input_order(void)
{
    TEST_ASSERT_TRUE(
        fake_hal_serial_load_input_text("cqt")
    );

    char character = '?';

    TEST_ASSERT_TRUE(
        console_read_char(&character)
    );
    TEST_ASSERT_EQUAL_CHAR('c', character);

    TEST_ASSERT_TRUE(
        console_read_char(&character)
    );
    TEST_ASSERT_EQUAL_CHAR('q', character);

    TEST_ASSERT_TRUE(
        console_read_char(&character)
    );
    TEST_ASSERT_EQUAL_CHAR('t', character);

    TEST_ASSERT_FALSE(
        console_read_char(&character)
    );

    TEST_ASSERT_EQUAL_CHAR('t', character);
}


static void test_discard_input_consumes_all_waiting_bytes(void)
{
    TEST_ASSERT_TRUE(
        fake_hal_serial_load_input_text("tcqsx")
    );

    console_discard_input();

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        (uint32_t)
        fake_hal_serial_get_pending_input_length()
    );

    TEST_ASSERT_EQUAL_UINT32(
        5U,
        fake_hal_serial_get_read_call_count()
    );
}


static void test_discard_input_handles_empty_buffer(void)
{
    console_discard_input();

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hal_serial_get_read_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hal_serial_get_available_call_count()
    );
}


static void test_discard_input_stops_after_backend_read_failure(void)
{
    TEST_ASSERT_TRUE(
        fake_hal_serial_load_input_text("abc")
    );

    fake_hal_serial_fail_next_read();

    console_discard_input();

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hal_serial_get_read_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        3U,
        (uint32_t)
        fake_hal_serial_get_pending_input_length()
    );
}


static void test_newline_emits_crlf(void)
{
    console_newline();

    assert_output_is("\r\n");
}


static void test_print_ignores_null_text(void)
{
    console_print(NULL);

    assert_output_is("");
}


static void test_print_handles_empty_text(void)
{
    console_print("");

    assert_output_is("");
}


static void test_print_emits_ram_text(void)
{
    console_print("Weight: ");

    assert_output_is("Weight: ");
}


static void test_println_emits_text_and_crlf(void)
{
    console_println("Ready");

    assert_output_is("Ready\r\n");
}


static void test_println_null_emits_only_crlf(void)
{
    console_println(NULL);

    assert_output_is("\r\n");
}


static void test_print_progmem_ignores_null_text(void)
{
    console_print_progmem(NULL);

    assert_output_is("");
}


static void test_print_progmem_emits_text(void)
{
    console_print_progmem(
        CONSOLE_PROGMEM("Calibration")
    );

    assert_output_is("Calibration");
}


static void test_println_progmem_emits_text_and_crlf(void)
{
    console_println_progmem(
        CONSOLE_PROGMEM("Saved")
    );

    assert_output_is("Saved\r\n");
}


static void test_console_print_macro_emits_literal(void)
{
    CONSOLE_PRINT("Level: ");

    assert_output_is("Level: ");
}


static void test_console_println_macro_emits_literal_and_crlf(void)
{
    CONSOLE_PRINTLN("Complete");

    assert_output_is("Complete\r\n");
}


static void test_multiple_output_operations_are_concatenated(void)
{
    CONSOLE_PRINT("Weight: ");
    console_print_float(12.5F, 2U);
    console_newline();

    assert_output_is("Weight: 12.50\r\n");
}


static void test_print_int32_formats_zero(void)
{
    console_print_int32(0);

    assert_output_is("0");
}


static void test_print_int32_formats_positive_value(void)
{
    console_print_int32(1234567890L);

    assert_output_is("1234567890");
}


static void test_print_int32_formats_negative_value(void)
{
    console_print_int32(-1234567890L);

    assert_output_is("-1234567890");
}


static void test_print_int32_formats_maximum_value(void)
{
    console_print_int32(INT32_MAX);

    assert_output_is("2147483647");
}


static void test_print_int32_formats_minimum_value(void)
{
    console_print_int32(INT32_MIN);

    assert_output_is("-2147483648");
}


static void test_print_float_formats_zero_with_two_decimals(void)
{
    console_print_float(0.0F, 2U);

    assert_output_is("0.00");
}


static void test_print_float_formats_zero_with_six_decimals(void)
{
    console_print_float(0.0F, 6U);

    assert_output_is("0.000000");
}


static void test_print_float_formats_positive_value(void)
{
    console_print_float(45.589332F, 6U);

    assert_output_is("45.589332");
}


static void test_print_float_formats_negative_value(void)
{
    console_print_float(-10.0F, 2U);

    assert_output_is("-10.00");
}


static void test_print_float_preserves_trailing_zeros(void)
{
    console_print_float(1500.0F, 2U);

    assert_output_is("1500.00");
}


static void test_print_float_rounds_with_zero_decimal_places(void)
{
    console_print_float(1.6F, 0U);

    assert_output_is("2");
}


static void test_print_float_rounds_down(void)
{
    console_print_float(1.2344F, 3U);

    assert_output_is("1.234");
}


static void test_print_float_rounds_up(void)
{
    console_print_float(1.2346F, 3U);

    assert_output_is("1.235");
}


static void test_print_float_rounding_carries_into_integer_part(void)
{
    console_print_float(1.9996F, 3U);

    assert_output_is("2.000");
}


static void test_print_float_does_not_print_negative_zero(void)
{
    console_print_float(-0.0F, 2U);

    assert_output_is("0.00");
}


static void test_print_float_formats_nan(void)
{
    console_print_float(NAN, 2U);

    assert_output_is("nan");
}


static void test_print_float_formats_positive_infinity(void)
{
    console_print_float(INFINITY, 2U);

    assert_output_is("inf");
}


static void test_print_float_formats_negative_infinity(void)
{
    console_print_float(-INFINITY, 2U);

    assert_output_is("-inf");
}


static void test_print_float_reports_positive_overflow(void)
{
    console_print_float(4.30e9F, 2U);

    assert_output_is("ovf");
}


static void test_print_float_reports_negative_overflow(void)
{
    console_print_float(-4.30e9F, 2U);

    assert_output_is("ovf");
}


static void test_print_float_limits_precision_to_six_places(void)
{
    console_print_float(1.0F, 9U);

    assert_output_is("1.000000");
}


int main(
    int argc,
    char **argv
)
{
    (void)argc;
    (void)argv;

    UNITY_BEGIN();

    RUN_TEST(test_console_init_forwards_baud_rate);

    RUN_TEST(test_input_available_is_false_when_empty);
    RUN_TEST(test_input_available_is_true_when_data_waits);
    RUN_TEST(test_read_char_rejects_null_without_reading);
    RUN_TEST(test_read_char_preserves_output_when_empty);
    RUN_TEST(test_read_char_consumes_one_character);
    RUN_TEST(test_read_char_preserves_input_order);
    RUN_TEST(test_discard_input_consumes_all_waiting_bytes);
    RUN_TEST(test_discard_input_handles_empty_buffer);
    RUN_TEST(test_discard_input_stops_after_backend_read_failure);

    RUN_TEST(test_newline_emits_crlf);
    RUN_TEST(test_print_ignores_null_text);
    RUN_TEST(test_print_handles_empty_text);
    RUN_TEST(test_print_emits_ram_text);
    RUN_TEST(test_println_emits_text_and_crlf);
    RUN_TEST(test_println_null_emits_only_crlf);
    RUN_TEST(test_print_progmem_ignores_null_text);
    RUN_TEST(test_print_progmem_emits_text);
    RUN_TEST(test_println_progmem_emits_text_and_crlf);
    RUN_TEST(test_console_print_macro_emits_literal);
    RUN_TEST(test_console_println_macro_emits_literal_and_crlf);
    RUN_TEST(test_multiple_output_operations_are_concatenated);

    RUN_TEST(test_print_int32_formats_zero);
    RUN_TEST(test_print_int32_formats_positive_value);
    RUN_TEST(test_print_int32_formats_negative_value);
    RUN_TEST(test_print_int32_formats_maximum_value);
    RUN_TEST(test_print_int32_formats_minimum_value);

    RUN_TEST(test_print_float_formats_zero_with_two_decimals);
    RUN_TEST(test_print_float_formats_zero_with_six_decimals);
    RUN_TEST(test_print_float_formats_positive_value);
    RUN_TEST(test_print_float_formats_negative_value);
    RUN_TEST(test_print_float_preserves_trailing_zeros);
    RUN_TEST(test_print_float_rounds_with_zero_decimal_places);
    RUN_TEST(test_print_float_rounds_down);
    RUN_TEST(test_print_float_rounds_up);
    RUN_TEST(test_print_float_rounding_carries_into_integer_part);
    RUN_TEST(test_print_float_does_not_print_negative_zero);
    RUN_TEST(test_print_float_formats_nan);
    RUN_TEST(test_print_float_formats_positive_infinity);
    RUN_TEST(test_print_float_formats_negative_infinity);
    RUN_TEST(test_print_float_reports_positive_overflow);
    RUN_TEST(test_print_float_reports_negative_overflow);
    RUN_TEST(test_print_float_limits_precision_to_six_places);

    return UNITY_END();
}
