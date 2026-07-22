#include <Arduino.h>
#include <HX711.h>

#include "config.h"


typedef enum
{
    LEVEL_UNKNOWN,
    LEVEL_LOW,
    LEVEL_MEDIUM,
    LEVEL_HIGH
} level_state_t;


HX711 scale;

static float latest_weight_grams = 0.0F;
static bool measurement_available = false;

static bool last_raw_button_state = HIGH;
static bool stable_button_state = HIGH;

static unsigned long last_button_change_ms = 0;
static unsigned long last_print_ms = 0;

static level_state_t current_level = LEVEL_UNKNOWN;


static void clear_serial_input(void)
{
    while (Serial.available() > 0)
    {
        Serial.read();
    }
}


static void set_level_leds(level_state_t level)
{
    /*
     * Primero apagamos todos los LED.
     */
    digitalWrite(LOW_LEVEL_LED_PIN, LOW);
    digitalWrite(MEDIUM_LEVEL_LED_PIN, LOW);
    digitalWrite(HIGH_LEVEL_LED_PIN, LOW);

    /*
     * Después encendemos solamente el correspondiente.
     */
    switch (level)
    {
        case LEVEL_LOW:
            digitalWrite(LOW_LEVEL_LED_PIN, HIGH);
            break;

        case LEVEL_MEDIUM:
            digitalWrite(MEDIUM_LEVEL_LED_PIN, HIGH);
            break;

        case LEVEL_HIGH:
            digitalWrite(HIGH_LEVEL_LED_PIN, HIGH);
            break;

        case LEVEL_UNKNOWN:
        default:
            /*
             * Si todavía no existe una medida válida,
             * permanecen todos apagados.
             */
            break;
    }
}


static void update_level_indicator(float weight_grams)
{
    /*
     * En la primera medida todavía no existe un estado anterior.
     * Elegimos directamente el nivel correspondiente.
     */
    if (current_level == LEVEL_UNKNOWN)
    {
        if (weight_grams < LOW_MEDIUM_THRESHOLD_GRAMS)
        {
            current_level = LEVEL_LOW;
        }
        else if (weight_grams < MEDIUM_HIGH_THRESHOLD_GRAMS)
        {
            current_level = LEVEL_MEDIUM;
        }
        else
        {
            current_level = LEVEL_HIGH;
        }

        set_level_leds(current_level);
        return;
    }

    switch (current_level)
    {
        case LEVEL_LOW:

            /*
             * Permitimos saltar directamente a HIGH si el peso
             * aumenta mucho entre dos medidas.
             */
            if (weight_grams >=
                (MEDIUM_HIGH_THRESHOLD_GRAMS +
                 LEVEL_HYSTERESIS_GRAMS))
            {
                current_level = LEVEL_HIGH;
            }
            else if (weight_grams >=
                     (LOW_MEDIUM_THRESHOLD_GRAMS +
                      LEVEL_HYSTERESIS_GRAMS))
            {
                current_level = LEVEL_MEDIUM;
            }

            break;

        case LEVEL_MEDIUM:

            if (weight_grams <=
                (LOW_MEDIUM_THRESHOLD_GRAMS -
                 LEVEL_HYSTERESIS_GRAMS))
            {
                current_level = LEVEL_LOW;
            }
            else if (weight_grams >=
                     (MEDIUM_HIGH_THRESHOLD_GRAMS +
                      LEVEL_HYSTERESIS_GRAMS))
            {
                current_level = LEVEL_HIGH;
            }

            break;

        case LEVEL_HIGH:

            /*
             * También permitimos un salto directo a LOW si
             * retiramos prácticamente toda la carga.
             */
            if (weight_grams <=
                (LOW_MEDIUM_THRESHOLD_GRAMS -
                 LEVEL_HYSTERESIS_GRAMS))
            {
                current_level = LEVEL_LOW;
            }
            else if (weight_grams <=
                     (MEDIUM_HIGH_THRESHOLD_GRAMS -
                      LEVEL_HYSTERESIS_GRAMS))
            {
                current_level = LEVEL_MEDIUM;
            }

            break;

        case LEVEL_UNKNOWN:
        default:
            current_level = LEVEL_UNKNOWN;
            break;
    }

    set_level_leds(current_level);
}


static void perform_tare(void)
{
    Serial.println();
    Serial.println("Taring...");
    Serial.println("Leave only the empty platform or container.");

    scale.tare(TARE_SAMPLES);

    Serial.print("New tare offset: ");
    Serial.println(scale.get_offset());

    /*
     * La medida anterior deja de ser válida porque el offset
     * ha cambiado.
     */
    measurement_available = false;
    current_level = LEVEL_UNKNOWN;

    set_level_leds(LEVEL_UNKNOWN);

    Serial.println("Tare completed.");
    Serial.println();
}


static void process_serial_commands(void)
{
    if (Serial.available() == 0)
    {
        return;
    }

    const char command = Serial.read();

    clear_serial_input();

    switch (command)
    {
        case 't':
        case 'T':
            perform_tare();
            break;

        default:
            Serial.println("Unknown command.");
            Serial.println("Available command: t = tare");
            break;
    }
}


static bool tare_button_pressed(void)
{
    const unsigned long now = millis();
    const bool raw_button_state =
        digitalRead(TARE_BUTTON_PIN);

    /*
     * Cuando cambia la lectura instantánea, reiniciamos
     * el periodo de debounce.
     */
    if (raw_button_state != last_raw_button_state)
    {
        last_raw_button_state = raw_button_state;
        last_button_change_ms = now;
    }

    /*
     * Solo aceptamos el nuevo estado si permanece estable.
     */
    if ((now - last_button_change_ms) >=
        BUTTON_DEBOUNCE_MS)
    {
        if (raw_button_state != stable_button_state)
        {
            stable_button_state = raw_button_state;

            /*
             * Con INPUT_PULLUP:
             *
             * HIGH = liberado
             * LOW  = pulsado
             */
            if (stable_button_state == LOW)
            {
                return true;
            }
        }
    }

    return false;
}


static void update_weight_measurement(void)
{
    if (!scale.is_ready())
    {
        return;
    }

    latest_weight_grams =
        scale.get_units(WEIGHT_SAMPLES);

    measurement_available = true;

    update_level_indicator(latest_weight_grams);
}


static const char *level_to_string(level_state_t level)
{
    switch (level)
    {
        case LEVEL_LOW:
            return "LOW";

        case LEVEL_MEDIUM:
            return "MEDIUM";

        case LEVEL_HIGH:
            return "HIGH";

        case LEVEL_UNKNOWN:
        default:
            return "UNKNOWN";
    }
}


static void print_weight_periodically(void)
{
    const unsigned long now = millis();

    if ((now - last_print_ms) < PRINT_PERIOD_MS)
    {
        return;
    }

    last_print_ms = now;

    if (!measurement_available)
    {
        Serial.println("Waiting for weight measurement...");
        return;
    }

    Serial.print("Weight: ");
    Serial.print(latest_weight_grams, 2);
    Serial.print(" g | Level: ");
    Serial.println(level_to_string(current_level));
}


void setup(void)
{
    Serial.begin(115200);

    pinMode(TARE_BUTTON_PIN, INPUT_PULLUP);

    pinMode(LOW_LEVEL_LED_PIN, OUTPUT);
    pinMode(MEDIUM_LEVEL_LED_PIN, OUTPUT);
    pinMode(HIGH_LEVEL_LED_PIN, OUTPUT);

    /*
     * Estado seguro al arrancar.
     */
    set_level_leds(LEVEL_UNKNOWN);

    scale.begin(
        LOADCELL_DOUT_PIN,
        LOADCELL_SCK_PIN
    );

    Serial.println();
    Serial.println("=== Load cell level indicator ===");

    if (!scale.wait_ready_timeout(2000))
    {
        Serial.println("ERROR: HX711 not found.");

        while (true)
        {
            delay(1000);
        }
    }

    scale.set_scale(CALIBRATION_FACTOR);

    Serial.print("Calibration factor: ");
    Serial.print(CALIBRATION_FACTOR, 6);
    Serial.println(" counts/g");

    Serial.println();
    Serial.println(
        "Automatic tare will start in 3 seconds."
    );
    Serial.println(
        "Leave the scale unloaded or with the empty container."
    );

    delay(3000);

    perform_tare();

    Serial.println("Controls:");
    Serial.println("  Physical button on D4 = tare");
    Serial.println("  Serial command 't'    = tare");

    Serial.println();
    Serial.println("Provisional levels:");
    Serial.println("  LOW:    below 500 g");
    Serial.println("  MEDIUM: 500 to 1000 g");
    Serial.println("  HIGH:   above 1000 g");
}


void loop(void)
{
    process_serial_commands();

    if (tare_button_pressed())
    {
        perform_tare();
    }

    update_weight_measurement();
    print_weight_periodically();
}
