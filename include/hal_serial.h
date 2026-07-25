#ifndef HAL_SERIAL_H
#define HAL_SERIAL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Initializes the active serial backend using the
 * requested baud rate.
 */
void hal_serial_init(
    uint32_t baud_rate
);


/*
 * Returns true when at least one received byte can be
 * read without blocking.
 */
bool hal_serial_rx_available(void);


/*
 * Reads exactly one received byte.
 *
 * Returns false when:
 *
 * - The output pointer is null.
 * - No received byte is currently available.
 *
 * A failed operation does not modify the caller's
 * output variable.
 */
bool hal_serial_read_byte(
    uint8_t *received_byte
);


/*
 * Transmits exactly one byte.
 *
 * The active backend may block until the byte can be
 * accepted by the physical serial peripheral.
 */
void hal_serial_write_byte(
    uint8_t transmitted_byte
);

#ifdef __cplusplus
}
#endif

#endif