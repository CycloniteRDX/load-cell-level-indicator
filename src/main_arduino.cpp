#include <Arduino.h>

#include "app.h"
#include "hal_watchdog.h"


void setup(void)
{
    app_init();
    hal_watchdog_enable();
}


void loop(void)
{
    app_update();
    hal_watchdog_kick();
}
