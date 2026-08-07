#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "button.h"
#include "calibration_storage.h"
#include "config.h"
#include "console.h"
#include "fake_app_support.h"
#include "hal_time.h"
#include "indicator_leds.h"
#include "level_indicator.h"
#include "scale.h"
#include "tare_storage.h"


static const size_t CONSOLE_OUTPUT_CAPACITY =
    16384U;

static uint32_t current_time_ms = 0UL;

static bool scale_init_result = true;
static bool scale_ready = false;
static bool scale_factor_result = true;

static uint32_t scale_init_calls = 0UL;
static uint32_t scale_ready_calls = 0UL;
static uint32_t scale_cancel_calls = 0UL;
static uint32_t scale_tare_calls = 0UL;
static uint32_t scale_weight_read_calls = 0UL;

static float active_scale_factor = 1.0F;
static int32_t active_scale_offset = 0;

static uint32_t scale_factor_set_calls = 0UL;
static uint32_t scale_offset_set_calls = 0UL;

static bool calibration_record_available = false;
static float stored_calibration_factor = 0.0F;
static uint32_t calibration_load_calls = 0UL;

static bool tare_record_available = false;
static int32_t stored_tare_offset = 0;
static uint32_t tare_load_calls = 0UL;

static operation_indicator_mode_t operation_mode =
    OPERATION_INDICATOR_NONE;

static uint32_t operation_update_calls = 0UL;
static uint32_t level_reset_calls = 0UL;

static bool console_command_pending = false;
static char pending_console_command = '\0';

static bool queue_command_during_tare_load = false;
static char tare_load_console_command = '\0';

static char console_output[CONSOLE_OUTPUT_CAPACITY];
static size_t console_output_length = 0U;

static bool tare_button_press_pending = false;
static bool calibration_button_press_pending = false;

static uint32_t tare_button_suppression_calls = 0UL;
static uint32_t calibration_button_suppression_calls = 0UL;


static void append_console_text(const char *text)
{
    if (text == NULL)
    {
        return;
    }

    const size_t remaining_capacity =
        CONSOLE_OUTPUT_CAPACITY -
        console_output_length;

    if (remaining_capacity <= 1U)
    {
        return;
    }

    const int characters_written = snprintf(
        &console_output[console_output_length],
        remaining_capacity,
        "%s",
        text
    );

    if (characters_written <= 0)
    {
        return;
    }

    const size_t requested_characters =
        (size_t)characters_written;

    if (requested_characters >= remaining_capacity)
    {
        console_output_length =
            CONSOLE_OUTPUT_CAPACITY - 1U;

        return;
    }

    console_output_length += requested_characters;
}


void fake_app_reset(void)
{
    current_time_ms = 0UL;

    scale_init_result = true;
    scale_ready = false;
    scale_factor_result = true;

    scale_init_calls = 0UL;
    scale_ready_calls = 0UL;
    scale_cancel_calls = 0UL;
    scale_tare_calls = 0UL;
    scale_weight_read_calls = 0UL;

    active_scale_factor = 1.0F;
    active_scale_offset = 0;

    scale_factor_set_calls = 0UL;
    scale_offset_set_calls = 0UL;

    calibration_record_available = false;
    stored_calibration_factor = 0.0F;
    calibration_load_calls = 0UL;

    tare_record_available = false;
    stored_tare_offset = 0;
    tare_load_calls = 0UL;

    operation_mode = OPERATION_INDICATOR_NONE;
    operation_update_calls = 0UL;
    level_reset_calls = 0UL;

    console_command_pending = false;
    pending_console_command = '\0';

    queue_command_during_tare_load = false;
    tare_load_console_command = '\0';

    console_output[0] = '\0';
    console_output_length = 0U;

    tare_button_press_pending = false;
    calibration_button_press_pending = false;

    tare_button_suppression_calls = 0UL;
    calibration_button_suppression_calls = 0UL;
}


void fake_app_set_time_ms(uint32_t time_ms)
{
    current_time_ms = time_ms;
}


void fake_app_advance_time_ms(uint32_t elapsed_ms)
{
    current_time_ms += elapsed_ms;
}


void fake_app_set_scale_init_result(bool result)
{
    scale_init_result = result;
}


void fake_app_set_scale_ready(bool ready)
{
    scale_ready = ready;
}


uint32_t fake_app_scale_init_call_count(void)
{
    return scale_init_calls;
}


uint32_t fake_app_scale_ready_call_count(void)
{
    return scale_ready_calls;
}


uint32_t fake_app_scale_cancel_call_count(void)
{
    return scale_cancel_calls;
}


uint32_t fake_app_scale_tare_call_count(void)
{
    return scale_tare_calls;
}


uint32_t fake_app_scale_weight_read_call_count(void)
{
    return scale_weight_read_calls;
}


void fake_app_set_calibration_record(
    bool available,
    float calibration_factor
)
{
    calibration_record_available = available;
    stored_calibration_factor = calibration_factor;
}


void fake_app_set_scale_factor_result(bool result)
{
    scale_factor_result = result;
}


uint32_t fake_app_calibration_load_call_count(void)
{
    return calibration_load_calls;
}


uint32_t fake_app_scale_factor_set_call_count(void)
{
    return scale_factor_set_calls;
}


float fake_app_last_scale_factor(void)
{
    return active_scale_factor;
}


void fake_app_set_tare_record(
    bool available,
    int32_t tare_offset
)
{
    tare_record_available = available;
    stored_tare_offset = tare_offset;
}


uint32_t fake_app_tare_load_call_count(void)
{
    return tare_load_calls;
}


uint32_t fake_app_scale_offset_set_call_count(void)
{
    return scale_offset_set_calls;
}


int32_t fake_app_last_scale_offset(void)
{
    return active_scale_offset;
}


operation_indicator_mode_t
fake_app_operation_indicator_mode(void)
{
    return operation_mode;
}


uint32_t fake_app_operation_indicator_update_call_count(void)
{
    return operation_update_calls;
}


uint32_t fake_app_level_reset_call_count(void)
{
    return level_reset_calls;
}


void fake_app_queue_console_command(char command)
{
    pending_console_command = command;
    console_command_pending = true;
}


void fake_app_queue_console_command_during_tare_load(
    char command
)
{
    tare_load_console_command = command;
    queue_command_during_tare_load = true;
}


bool fake_app_console_input_is_pending(void)
{
    return console_command_pending;
}


const char *fake_app_console_output(void)
{
    return console_output;
}


void fake_app_press_tare_button(void)
{
    tare_button_press_pending = true;
}


void fake_app_press_calibration_button(void)
{
    calibration_button_press_pending = true;
}


uint32_t fake_app_tare_button_suppression_count(void)
{
    return tare_button_suppression_calls;
}


uint32_t fake_app_calibration_button_suppression_count(void)
{
    return calibration_button_suppression_calls;
}


void hal_time_init(void)
{
}


uint32_t hal_time_millis(void)
{
    return current_time_ms;
}


void button_init(
    button_t *button,
    uint8_t pin,
    uint32_t debounce_ms
)
{
    if (button == NULL)
    {
        return;
    }

    button->pin = pin;
    button->debounce_ms = debounce_ms;
}


bool button_was_pressed(button_t *button)
{
    if (button == NULL)
    {
        return false;
    }

    if (button->pin == TARE_BUTTON_PIN)
    {
        const bool was_pressed =
            tare_button_press_pending;

        tare_button_press_pending = false;
        return was_pressed;
    }

    if (button->pin == CALIBRATION_BUTTON_PIN)
    {
        const bool was_pressed =
            calibration_button_press_pending;

        calibration_button_press_pending = false;
        return was_pressed;
    }

    return false;
}


bool button_was_held(
    button_t *button,
    uint32_t hold_ms
)
{
    (void)button;
    (void)hold_ms;

    return false;
}


void button_suppress_hold_until_release(
    button_t *button
)
{
    if (button == NULL)
    {
        return;
    }

    if (button->pin == TARE_BUTTON_PIN)
    {
        ++tare_button_suppression_calls;
    }
    else if (button->pin == CALIBRATION_BUTTON_PIN)
    {
        ++calibration_button_suppression_calls;
    }
}


void console_init(uint32_t baud_rate)
{
    (void)baud_rate;
}


bool console_input_available(void)
{
    return console_command_pending;
}


bool console_read_char(char *character)
{
    if ((character == NULL) ||
        !console_command_pending)
    {
        return false;
    }

    *character = pending_console_command;
    console_command_pending = false;

    return true;
}


void console_discard_input(void)
{
    console_command_pending = false;
}


void console_newline(void)
{
    append_console_text("\r\n");
}


void console_print(const char *text)
{
    append_console_text(text);
}


void console_println(const char *text)
{
    append_console_text(text);
    console_newline();
}


void console_print_progmem(
    console_progmem_string_t text
)
{
    append_console_text(text);
}


void console_println_progmem(
    console_progmem_string_t text
)
{
    append_console_text(text);
    console_newline();
}


void console_print_int32(int32_t value)
{
    char buffer[16];

    snprintf(
        buffer,
        sizeof(buffer),
        "%ld",
        (long)value
    );

    append_console_text(buffer);
}


void console_print_float(
    float value,
    uint8_t decimal_places
)
{
    char buffer[32];

    snprintf(
        buffer,
        sizeof(buffer),
        "%.*f",
        (int)decimal_places,
        (double)value
    );

    append_console_text(buffer);
}


void indicator_leds_init(void)
{
}


void level_indicator_init(void)
{
}


void level_indicator_reset(void)
{
    ++level_reset_calls;
}


void level_indicator_update(float weight_grams)
{
    (void)weight_grams;
}


void level_indicator_update_visual(void)
{
}


const char *level_indicator_get_state_name(void)
{
    return "UNKNOWN";
}


void operation_indicator_init(void)
{
    operation_mode = OPERATION_INDICATOR_NONE;
}


void operation_indicator_set_mode(
    operation_indicator_mode_t mode
)
{
    operation_mode = mode;
}


void operation_indicator_show_success(void)
{
    operation_mode = OPERATION_INDICATOR_SUCCESS;
}


void operation_indicator_show_error(
    operation_indicator_mode_t return_mode
)
{
    (void)return_mode;
    operation_mode = OPERATION_INDICATOR_ERROR;
}


bool operation_indicator_is_temporary_active(void)
{
    return
        (operation_mode == OPERATION_INDICATOR_SUCCESS) ||
        (operation_mode == OPERATION_INDICATOR_ERROR);
}


void operation_indicator_update(void)
{
    ++operation_update_calls;
}


void operation_indicator_clear(void)
{
    operation_mode = OPERATION_INDICATOR_NONE;
}


bool scale_init(void)
{
    ++scale_init_calls;
    return scale_init_result;
}


bool scale_is_ready(void)
{
    ++scale_ready_calls;
    return scale_ready;
}


bool scale_set_calibration_factor(
    float calibration_factor
)
{
    ++scale_factor_set_calls;

    if (!scale_factor_result)
    {
        return false;
    }

    active_scale_factor = calibration_factor;
    return true;
}


void scale_set_offset(int32_t tare_offset)
{
    ++scale_offset_set_calls;
    active_scale_offset = tare_offset;
}


void scale_cancel_sample_collection(void)
{
    ++scale_cancel_calls;
}


bool scale_tare(void)
{
    ++scale_tare_calls;
    return false;
}


bool scale_try_read_weight(float *weight_grams)
{
    (void)weight_grams;

    ++scale_weight_read_calls;
    return false;
}


bool scale_read_net_counts(
    float *net_counts,
    uint8_t samples
)
{
    (void)net_counts;
    (void)samples;

    return false;
}


int32_t scale_get_offset(void)
{
    return active_scale_offset;
}


float scale_get_calibration_factor(void)
{
    return active_scale_factor;
}


bool calibration_storage_load(
    float *calibration_factor
)
{
    ++calibration_load_calls;

    if (!calibration_record_available)
    {
        return false;
    }

    if (calibration_factor != NULL)
    {
        *calibration_factor =
            stored_calibration_factor;
    }

    return true;
}


bool calibration_storage_save(
    float calibration_factor
)
{
    (void)calibration_factor;
    return true;
}


bool calibration_storage_clear(void)
{
    return true;
}


bool tare_storage_load(int32_t *tare_offset)
{
    ++tare_load_calls;

    if (queue_command_during_tare_load)
    {
        pending_console_command =
            tare_load_console_command;

        console_command_pending = true;
        queue_command_during_tare_load = false;
    }

    if (!tare_record_available)
    {
        return false;
    }

    if (tare_offset != NULL)
    {
        *tare_offset = stored_tare_offset;
    }

    return true;
}


bool tare_storage_save(int32_t tare_offset)
{
    (void)tare_offset;
    return true;
}


bool tare_storage_clear(void)
{
    return true;
}
