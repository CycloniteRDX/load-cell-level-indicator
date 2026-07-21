#include <Arduino.h>
#include <HX711.h>

static const uint8_t LOADCELL_DOUT_PIN = 2;
static const uint8_t LOADCELL_SCK_PIN = 3;

static const uint8_t TARE_SAMPLES = 20;
static const uint8_t WEIGHT_SAMPLES = 10;

/*
 * Factor provisional obtenido con:
 *
 * Masa conocida: 1500 g
 * Cuentas netas: 68384
 *
 * Debe recalibrarse cuando el montaje mecánico sea definitivo.
 */
static const float CALIBRATION_FACTOR = 45.589332F;

HX711 scale;

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
    Serial.println("Do not apply the load to be measured.");

    scale.tare(TARE_SAMPLES);

    Serial.print("New tare offset: ");
    Serial.println(scale.get_offset());

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

    /*
     * Elimina caracteres pendientes, como retorno de carro
     * y salto de línea enviados por el monitor serie.
     */
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

void setup(void)
{
    Serial.begin(115200);

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

    /*
     * Configura la relación entre cuentas ADC y gramos.
     */
    scale.set_scale(CALIBRATION_FACTOR);

    Serial.print("Calibration factor: ");
    Serial.print(CALIBRATION_FACTOR, 6);
    Serial.println(" counts/g");

    /*
     * Por ahora hacemos una tara automática.
     * En una etapa posterior se sustituirá o complementará
     * con un pulsador físico.
     */
    Serial.println();
    Serial.println("Automatic tare will start in 3 seconds.");
    Serial.println("Leave the scale unloaded or with the empty container.");

    delay(3000);

    perform_tare();

    Serial.println("Command: t = perform tare");
}

void loop(void)
{
    process_serial_commands();

    if (!scale.wait_ready_timeout(1000))
    {
        Serial.println("ERROR: HX711 not ready.");
        delay(500);
        return;
    }

    const float weight_grams =
        scale.get_units(WEIGHT_SAMPLES);

    Serial.print("Weight: ");
    Serial.print(weight_grams, 2);
    Serial.println(" g");

    delay(500);
}