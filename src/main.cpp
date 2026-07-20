#include <Arduino.h>

void setup(void)
{
    pinMode(LED_BUILTIN, OUTPUT);
    Serial.begin(115200);
    Serial.println("Program started");
}

void loop(void)
{
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println("LED on");
    delay(500);

    digitalWrite(LED_BUILTIN, LOW);
    Serial.println("LED off");
    delay(500);
}