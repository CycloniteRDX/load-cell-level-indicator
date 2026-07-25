#ifndef TEST_CONSOLE_FAKE_HAL_SERIAL_H
#define TEST_CONSOLE_FAKE_HAL_SERIAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void fake_hal_serial_reset(void);

bool fake_hal_serial_load_input(
    const uint8_t *bytes,
    size_t length
);

bool fake_hal_serial_load_input_text(
    const char *text
);

void fake_hal_serial_fail_next_read(void);

uint32_t fake_hal_serial_get_init_call_count(void);
uint32_t fake_hal_serial_get_last_baud_rate(void);
uint32_t fake_hal_serial_get_available_call_count(void);
uint32_t fake_hal_serial_get_read_call_count(void);
uint32_t fake_hal_serial_get_write_call_count(void);

size_t fake_hal_serial_get_pending_input_length(void);
size_t fake_hal_serial_get_output_length(void);

const uint8_t *fake_hal_serial_get_output_bytes(void);
const char *fake_hal_serial_get_output_text(void);

bool fake_hal_serial_output_overflowed(void);

#ifdef __cplusplus
}
#endif

#endif
