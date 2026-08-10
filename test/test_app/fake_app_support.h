#ifndef TEST_APP_FAKE_SUPPORT_H
#define TEST_APP_FAKE_SUPPORT_H

#include <stdbool.h>
#include <stdint.h>

#include "operation_indicator.h"
#include "scale.h"


void fake_app_reset(void);


void fake_app_set_time_ms(uint32_t time_ms);
void fake_app_advance_time_ms(uint32_t elapsed_ms);


void fake_app_set_scale_init_result(bool result);
void fake_app_set_scale_ready(bool ready);
void fake_app_set_scale_collection_start_result(
    bool result
);
void fake_app_set_scale_collection_status(
    scale_sample_collection_status_t status
);
void fake_app_set_scale_sample_average(
    bool available,
    int32_t average_raw
);

uint32_t fake_app_scale_init_call_count(void);
uint32_t fake_app_scale_ready_call_count(void);
uint32_t fake_app_scale_cancel_call_count(void);
uint32_t fake_app_scale_collection_start_call_count(void);
uint32_t fake_app_scale_collection_update_call_count(void);
uint32_t fake_app_scale_average_take_call_count(void);
uint8_t fake_app_last_requested_sample_count(void);
uint32_t fake_app_scale_weight_read_call_count(void);


void fake_app_set_calibration_record(
    bool available,
    float calibration_factor
);

void fake_app_set_scale_factor_result(bool result);
void fake_app_set_calibration_save_result(bool result);

uint32_t fake_app_calibration_load_call_count(void);
uint32_t fake_app_calibration_save_call_count(void);
uint32_t fake_app_scale_factor_set_call_count(void);
float fake_app_last_scale_factor(void);
float fake_app_last_saved_calibration_factor(void);
float fake_app_scale_factor_when_calibration_was_saved(void);


void fake_app_set_tare_record(
    bool available,
    int32_t tare_offset
);

uint32_t fake_app_tare_load_call_count(void);
void fake_app_set_tare_save_result(bool result);
uint32_t fake_app_tare_save_call_count(void);
int32_t fake_app_last_saved_tare_offset(void);
int32_t fake_app_scale_offset_when_tare_was_saved(void);
uint32_t fake_app_scale_offset_set_call_count(void);
int32_t fake_app_last_scale_offset(void);


operation_indicator_mode_t
fake_app_operation_indicator_mode(void);

operation_indicator_mode_t
fake_app_operation_indicator_return_mode(void);

void fake_app_complete_operation_pattern(void);

uint32_t fake_app_operation_indicator_update_call_count(void);
uint32_t fake_app_level_reset_call_count(void);


void fake_app_queue_console_command(char command);
void fake_app_queue_console_command_during_tare_load(
    char command
);
void fake_app_queue_console_command_during_tare_save(
    char command
);
void fake_app_queue_console_command_during_calibration_save(
    char command
);
bool fake_app_console_input_is_pending(void);
const char *fake_app_console_output(void);


void fake_app_press_tare_button(void);
void fake_app_hold_tare_button(void);
void fake_app_press_calibration_button(void);
void fake_app_hold_calibration_button(void);

uint32_t fake_app_tare_button_suppression_count(void);
uint32_t fake_app_calibration_button_suppression_count(void);


#endif
