#include <Arduino.h>
#include <HX711.h>

static const uint8_t LOADCELL_DOUT_PIN = 2;
static const uint8_t LOADCELL_SCK_PIN = 3;

static const uint8_t TARE_SAMPLES = 20;
static const uint8_t READING_SAMPLES = 10;

HX711 scale;

static void perform_tare(void)
{
    Serial.println();
    Serial.println("Remove all weight from the scale.");
    Serial.println("Taring...");

    scale.tare(TARE_SAMPLES);

    Serial.print("Tare offset: ");
    Serial.println(scale.get_offset());

    Serial.println("Tare completed.");
    Serial.println();
}

void setup(void)
{
    Serial.begin(115200);

    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

    Serial.println();
    Serial.println("=== HX711 tare experiment ===");

    if (!scale.wait_ready_timeout(2000))
    {
        Serial.println("ERROR: HX711 not found.");

        while (true)
        {
            delay(1000);
        }
    }

    /*
     * Todavía no estamos calibrando.
     * El factor se deja en 1.
     */
    scale.set_scale(1.0F);

    /*
     * Espera para que puedas dejar la báscula
     * completamente descargada tras el arranque.
     */
    Serial.println("Tare will start in 3 seconds.");
    delay(3000);

    perform_tare();

    Serial.println("Commands:");
    Serial.println("  t = perform tare again");
}

void loop(void)
{
    /*
     * Permite repetir la tara escribiendo 't'
     * en el monitor serie.
     */
    if (Serial.available() > 0)
    {
        const char command = Serial.read();

        if ((command == 't') || (command == 'T'))
        {
            perform_tare();
        }
    }

    if (!scale.wait_ready_timeout(1000))
    {
        Serial.println("ERROR: HX711 not ready.");
        delay(500);
        return;
    }

    const long raw_average =
        scale.read_average(READING_SAMPLES);

    const long tare_offset =
        scale.get_offset();

    const long net_counts =
        raw_average - tare_offset;

    Serial.print("Raw: ");
    Serial.print(raw_average);

    Serial.print(" | Offset: ");
    Serial.print(tare_offset);

    Serial.print(" | Net: ");
    Serial.println(net_counts);

    delay(500);
}