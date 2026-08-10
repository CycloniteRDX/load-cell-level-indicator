#include "app_fault.h"


app_fault_code_t app_fault_normalize_code(
    app_fault_code_t fault_code
)
{
    switch (fault_code)
    {
        case APP_FAULT_NONE:
        case APP_FAULT_HX711_INITIALIZATION:
        case APP_FAULT_HX711_STARTUP_TIMEOUT:
        case APP_FAULT_HX711_RUNTIME_TIMEOUT:
        case APP_FAULT_HX711_READ:
        case APP_FAULT_SAMPLE_COLLECTION_TIMEOUT:
        case APP_FAULT_SAMPLE_COLLECTION_STATE:
        case APP_FAULT_INVALID_ACTIVE_CALIBRATION:
        case APP_FAULT_INTERNAL_STATE:
            return fault_code;

        default:
            return APP_FAULT_INTERNAL_STATE;
    }
}


app_fault_policy_t app_fault_get_policy(
    app_fault_code_t fault_code
)
{
    switch (app_fault_normalize_code(fault_code))
    {
        case APP_FAULT_NONE:
            return APP_FAULT_POLICY_NONE;

        case APP_FAULT_HX711_STARTUP_TIMEOUT:
        case APP_FAULT_HX711_RUNTIME_TIMEOUT:
        case APP_FAULT_HX711_READ:
        case APP_FAULT_SAMPLE_COLLECTION_TIMEOUT:
            return APP_FAULT_POLICY_RECOVER_SENSOR;

        case APP_FAULT_HX711_INITIALIZATION:
        case APP_FAULT_SAMPLE_COLLECTION_STATE:
        case APP_FAULT_INVALID_ACTIVE_CALIBRATION:
        case APP_FAULT_INTERNAL_STATE:
        default:
            return APP_FAULT_POLICY_TERMINAL;
    }
}
