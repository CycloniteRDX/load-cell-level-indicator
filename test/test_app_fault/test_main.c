#include <unity.h>

#include "app_fault.h"


void setUp(void)
{
}


void tearDown(void)
{
}


static void test_fault_codes_keep_stable_numeric_values(void)
{
    TEST_ASSERT_EQUAL_INT(0, APP_FAULT_NONE);
    TEST_ASSERT_EQUAL_INT(1, APP_FAULT_HX711_INITIALIZATION);
    TEST_ASSERT_EQUAL_INT(2, APP_FAULT_HX711_STARTUP_TIMEOUT);
    TEST_ASSERT_EQUAL_INT(3, APP_FAULT_HX711_RUNTIME_TIMEOUT);
    TEST_ASSERT_EQUAL_INT(4, APP_FAULT_HX711_READ);
    TEST_ASSERT_EQUAL_INT(5, APP_FAULT_SAMPLE_COLLECTION_TIMEOUT);
    TEST_ASSERT_EQUAL_INT(6, APP_FAULT_SAMPLE_COLLECTION_STATE);
    TEST_ASSERT_EQUAL_INT(7, APP_FAULT_INVALID_ACTIVE_CALIBRATION);
    TEST_ASSERT_EQUAL_INT(8, APP_FAULT_INTERNAL_STATE);
    TEST_ASSERT_EQUAL_INT(9, APP_FAULT_PERSISTENT_STORAGE_ACCESS);
}


static void test_sensor_faults_have_recovery_policy(void)
{
    TEST_ASSERT_EQUAL_INT(
        APP_FAULT_POLICY_RECOVER_SENSOR,
        app_fault_get_policy(
            APP_FAULT_HX711_STARTUP_TIMEOUT
        )
    );

    TEST_ASSERT_EQUAL_INT(
        APP_FAULT_POLICY_RECOVER_SENSOR,
        app_fault_get_policy(
            APP_FAULT_HX711_RUNTIME_TIMEOUT
        )
    );

    TEST_ASSERT_EQUAL_INT(
        APP_FAULT_POLICY_RECOVER_SENSOR,
        app_fault_get_policy(
            APP_FAULT_HX711_READ
        )
    );

    TEST_ASSERT_EQUAL_INT(
        APP_FAULT_POLICY_RECOVER_SENSOR,
        app_fault_get_policy(
            APP_FAULT_SAMPLE_COLLECTION_TIMEOUT
        )
    );
}


static void test_internal_and_configuration_faults_are_terminal(void)
{
    TEST_ASSERT_EQUAL_INT(
        APP_FAULT_POLICY_TERMINAL,
        app_fault_get_policy(
            APP_FAULT_HX711_INITIALIZATION
        )
    );

    TEST_ASSERT_EQUAL_INT(
        APP_FAULT_POLICY_TERMINAL,
        app_fault_get_policy(
            APP_FAULT_SAMPLE_COLLECTION_STATE
        )
    );

    TEST_ASSERT_EQUAL_INT(
        APP_FAULT_POLICY_TERMINAL,
        app_fault_get_policy(
            APP_FAULT_INVALID_ACTIVE_CALIBRATION
        )
    );

    TEST_ASSERT_EQUAL_INT(
        APP_FAULT_POLICY_TERMINAL,
        app_fault_get_policy(
            APP_FAULT_INTERNAL_STATE
        )
    );
}


static void test_none_and_unknown_values_fail_safely(void)
{
    TEST_ASSERT_EQUAL_INT(
        APP_FAULT_POLICY_NONE,
        app_fault_get_policy(APP_FAULT_NONE)
    );

    const app_fault_code_t unknown_code =
        (app_fault_code_t)99;

    TEST_ASSERT_EQUAL_INT(
        APP_FAULT_INTERNAL_STATE,
        app_fault_normalize_code(unknown_code)
    );

    TEST_ASSERT_EQUAL_INT(
        APP_FAULT_POLICY_TERMINAL,
        app_fault_get_policy(unknown_code)
    );
}


static void test_persistent_storage_access_fault_is_terminal(void)
{
    TEST_ASSERT_EQUAL_INT(
        APP_FAULT_PERSISTENT_STORAGE_ACCESS,
        app_fault_normalize_code(
            APP_FAULT_PERSISTENT_STORAGE_ACCESS
        )
    );

    TEST_ASSERT_EQUAL_INT(
        APP_FAULT_POLICY_TERMINAL,
        app_fault_get_policy(
            APP_FAULT_PERSISTENT_STORAGE_ACCESS
        )
    );
}


int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_fault_codes_keep_stable_numeric_values);
    RUN_TEST(test_sensor_faults_have_recovery_policy);
    RUN_TEST(test_internal_and_configuration_faults_are_terminal);
    RUN_TEST(test_none_and_unknown_values_fail_safely);
    RUN_TEST(test_persistent_storage_access_fault_is_terminal);

    return UNITY_END();
}
