#include <Arduino.h>
#include <HX711.h>

static const uint8_t LOADCELL_DOUT_PIN = 2;
static const uint8_t LOADCELL_SCK_PIN = 3;

static const uint8_t TARE_SAMPLES = 20;
static const uint8_t CALIBRATION_SAMPLES = 20;
static const uint8_t WEIGHT_SAMPLES = 10;

/*
 * Sustituye este valor por la masa real utilizada.
 *
 * Ejemplos:
 * 500.0F  para 500 g
 * 1000.0F para 1 kg
 */
static const float KNOWN_MASS_GRAMS = 1500.0F;

HX711 scale;

static void clear_serial_input(void)
{
    while (Serial.available() > 0)
    {
        Serial.read();
    }
}

static void wait_for_serial_input(void)
{
    clear_serial_input();

    while (Serial.available() == 0)
    {
        delay(10);
    }

    clear_serial_input();
}

void setup(void)
{
    Serial.begin(115200);

    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

    Serial.println();
    Serial.println("=== HX711 calibration experiment ===");

    if (!scale.wait_ready_timeout(2000))
    {
        Serial.println("ERROR: HX711 not found.");

        while (true)
        {
            delay(1000);
        }
    }

    /*
     * Factor igual a 1.
     *
     * De esta forma, get_value() representa cuentas netas,
     * todavía no gramos.
     */
    scale.set_scale(1.0F);

    Serial.println();
    Serial.println("Remove all weight from the scale.");
    Serial.println("Wait until the system is stable.");
    Serial.println("Send any character to perform the tare.");

    wait_for_serial_input();

    Serial.println("Taring...");

    scale.tare(TARE_SAMPLES);

    Serial.print("Tare offset: ");
    Serial.println(scale.get_offset());

    Serial.println();
    Serial.print("Place the known mass: ");
    Serial.print(KNOWN_MASS_GRAMS, 2);
    Serial.println(" g");

    Serial.println("Wait until the reading is stable.");
    Serial.println("Send any character to calculate calibration.");

    wait_for_serial_input();

    /*
     * get_value() calcula:
     *
     * media de lecturas - offset de tara
     */
    const float net_counts =
        scale.get_value(CALIBRATION_SAMPLES);

    /*
     * El factor queda expresado en cuentas por gramo.
     */
    const float calibration_factor =
        net_counts / KNOWN_MASS_GRAMS;

    Serial.println();
    Serial.print("Net counts: ");
    Serial.println(net_counts, 2);

    Serial.print("Calibration factor: ");
    Serial.print(calibration_factor, 6);
    Serial.println(" counts/g");

    /*
     * A partir de este momento get_units()
     * devolverá gramos.
     */
    scale.set_scale(calibration_factor);

    Serial.println();
    Serial.println("Calibration completed.");
    Serial.println("Measurements are now expressed in grams.");
}

void loop(void)
{
    if (scale.wait_ready_timeout(1000))
    {
        const float weight_grams =
            scale.get_units(WEIGHT_SAMPLES);

        Serial.print("Weight: ");
        Serial.print(weight_grams, 2);
        Serial.println(" g");
    }
    else
    {
        Serial.println("ERROR: HX711 not ready.");
    }

    delay(500);
}