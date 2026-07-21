#include <Arduino.h>
#include <HX711.h>

static const uint8_t LOADCELL_DOUT_PIN = 2;
static const uint8_t LOADCELL_SCK_PIN = 3;
static const uint8_t TARE_BUTTON_PIN = 4;

static const uint8_t TARE_SAMPLES = 20;

/*
 * Usamos una lectura individual para que el programa vuelva rápidamente
 * a comprobar el pulsador.
 *
 * Más adelante implementaremos un filtrado propio sin bloquear el programa
 * durante muchas conversiones consecutivas.
 */
static const uint8_t WEIGHT_SAMPLES = 1;

static const unsigned long BUTTON_DEBOUNCE_MS = 40;
static const unsigned long PRINT_PERIOD_MS = 500;

/*
 * Factor provisional obtenido con el montaje mecánico actual.
 */
static const float CALIBRATION_FACTOR = 45.589332F;

HX711 scale;

static float latest_weight_grams = 0.0F;
static bool measurement_available = false;

static bool last_raw_button_state = HIGH;
static bool stable_button_state = HIGH;

static unsigned long last_button_change_ms = 0;
static unsigned long last_print_ms = 0;


static void clear_serial_input(void)
{
    while (Serial.available() > 0)
    {
        Serial.read();
    }
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
     * La medida anterior ya no utiliza el offset actual.
     * La descartamos hasta obtener una conversión nueva.
     */
    measurement_available = false;

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
    const bool raw_button_state = digitalRead(TARE_BUTTON_PIN);

    /*
     * Si la lectura instantánea cambia, comienza un nuevo periodo
     * de estabilización.
     */
    if (raw_button_state != last_raw_button_state)
    {
        last_raw_button_state = raw_button_state;
        last_button_change_ms = now;
    }

    /*
     * El nuevo estado solo se acepta si permanece sin cambios
     * durante BUTTON_DEBOUNCE_MS.
     */
    if ((now - last_button_change_ms) >= BUTTON_DEBOUNCE_MS)
    {
        if (raw_button_state != stable_button_state)
        {
            stable_button_state = raw_button_state;

            /*
             * INPUT_PULLUP:
             *
             * HIGH = pulsador liberado
             * LOW  = pulsador presionado
             *
             * Solo devolvemos true durante la transición a LOW.
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
    /*
     * is_ready() no espera bloqueando.
     * Si todavía no existe una conversión, volvemos inmediatamente.
     */
    if (!scale.is_ready())
    {
        return;
    }

    latest_weight_grams = scale.get_units(WEIGHT_SAMPLES);
    measurement_available = true;
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
    Serial.println(" g");
}


void setup(void)
{
    Serial.begin(115200);

    pinMode(TARE_BUTTON_PIN, INPUT_PULLUP);

    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

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
    Serial.println("Automatic tare will start in 3 seconds.");
    Serial.println("Leave the scale unloaded or with the empty container.");

    delay(3000);

    perform_tare();

    Serial.println("Controls:");
    Serial.println("  Physical button on D4 = tare");
    Serial.println("  Serial command 't'    = tare");
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