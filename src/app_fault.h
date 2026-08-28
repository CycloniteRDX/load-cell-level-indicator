#ifndef APP_FAULT_H
#define APP_FAULT_H


#ifdef __cplusplus
extern "C" {
#endif


/*
 * Stable application-visible fault codes.
 *
 * The explicit values are part of the serial diagnostic
 * contract for v1.3. Existing meanings must not be
 * renumbered when new faults are added later. The four
 * HX711-named identifiers are retained for source and
 * diagnostic compatibility, but apply to the selected
 * measurement ADC backend.
 */
typedef enum
{
    APP_FAULT_NONE = 0,
    APP_FAULT_HX711_INITIALIZATION = 1,
    APP_FAULT_HX711_STARTUP_TIMEOUT = 2,
    APP_FAULT_HX711_RUNTIME_TIMEOUT = 3,
    APP_FAULT_HX711_READ = 4,
    APP_FAULT_SAMPLE_COLLECTION_TIMEOUT = 5,
    APP_FAULT_SAMPLE_COLLECTION_STATE = 6,
    APP_FAULT_INVALID_ACTIVE_CALIBRATION = 7,
    APP_FAULT_INTERNAL_STATE = 8,
    APP_FAULT_PERSISTENT_STORAGE_ACCESS = 9
} app_fault_code_t;


typedef enum
{
    APP_FAULT_POLICY_NONE,
    APP_FAULT_POLICY_RECOVER_SENSOR,
    APP_FAULT_POLICY_TERMINAL
} app_fault_policy_t;


/*
 * Converts an unknown or invalid value into the stable
 * internal-state fault code.
 */
app_fault_code_t app_fault_normalize_code(
    app_fault_code_t fault_code
);


/*
 * Returns the action assigned to one fault code.
 * Unknown values always receive terminal handling.
 */
app_fault_policy_t app_fault_get_policy(
    app_fault_code_t fault_code
);


#ifdef __cplusplus
}
#endif


#endif
