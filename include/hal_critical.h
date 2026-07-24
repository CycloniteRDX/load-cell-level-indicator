#ifndef HAL_CRITICAL_H
#define HAL_CRITICAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uintptr_t hal_critical_state_t;

hal_critical_state_t hal_critical_enter(void);

void hal_critical_exit(
    hal_critical_state_t previous_state
);

#ifdef __cplusplus
}
#endif

#endif