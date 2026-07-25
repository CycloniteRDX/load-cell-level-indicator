# Seed del proyecto: aprendizaje progresivo de Arduino a bare-metal con HX711

Estoy desarrollando un proyecto educativo de larga duración para aprender programación de microcontroladores de forma progresiva, sin intentar aprender simultáneamente demasiadas capas.

## Objetivo funcional del proyecto

Construir un sistema que:

* Lea una célula de carga mediante un HX711.
* Convierta la lectura a peso.
* Permita realizar la tara.
* Permita calibrar usando una masa conocida.
* Encienda tres LED dependiendo del peso o nivel medido.
* Sirva como preparación para un futuro proyecto de mezclador de leche automatizado.

La plataforma inicial será probablemente:

* Arduino Nano clásico.
* ATmega328P a 16 MHz.
* VS Code.
* PlatformIO.
* HX711.
* Librería `bogde/HX711`.

El objetivo educativo final es abandonar progresivamente las abstracciones de Arduino, escribir drivers propios, trabajar con registros y terminar portando el proyecto a otra plataforma, probablemente STM32.

---

# Filosofía del proyecto

El proyecto debe avanzar mediante versiones pequeñas y funcionales.

En cada etapa se cambiará una cantidad limitada de cosas para que sea posible localizar los errores. No se debe saltar directamente desde una librería Arduino completamente funcional hasta un proyecto bare-metal completo.

Cada versión funcional debe guardarse mediante un commit de Git y, cuando sea un hito importante, mediante una etiqueta.

Ejemplo:

```bash
git add .
git commit -m "Implement minimal HX711 reading"
git tag v0.1-hx711-minimal
```

No se busca crear desde el principio una arquitectura industrial enorme. La separación de responsabilidades debe ser suficiente para aprender y permitir cambiar componentes, pero evitando la sobrearquitectura.

---

# Ruta de aprendizaje acordada

## Etapa 1: versión mínima con Arduino y la librería Bogde

Crear una versión mínima basada en los ejemplos de `bogde/HX711`.

Objetivos:

* Verificar el cableado.
* Comprobar que el HX711 responde.
* Leer valores ADC sin procesar.
* Comprobar que los valores cambian al aplicar peso.
* Detectar saturación, ruido o conexiones incorrectas.
* Probar la tara.
* Obtener un primer factor de calibración.
* Mostrar valores mediante `Serial`.

En esta etapa se permite utilizar:

* `setup()`.
* `loop()`.
* `Serial`.
* `delay()`.
* La librería `bogde/HX711`.
* Código sencillo e incluso parcialmente monolítico.

La prioridad es conseguir una referencia funcional conocida.

Esta versión debe conservarse sin modificar como programa de diagnóstico del hardware.

Hito sugerido:

```text
v0.1-hx711-minimal
```

---

## Etapa 2: separación de responsabilidades manteniendo Arduino y Bogde

Reescribir la aplicación manteniendo exactamente el mismo comportamiento, pero dividiendo el código en módulos.

Como la librería Bogde está escrita en C++, los módulos que interactúen con ella deben compilarse inicialmente como `.cpp`, no como `.c`.

Puede utilizarse C++ con estilo procedural:

* Funciones.
* `struct`.
* `enum`.
* Variables privadas al módulo.
* Sin necesidad de herencia.
* Sin asignación dinámica.
* Sin clases complejas.
* Evitar `String`.

Arquitectura inicial aproximada:

```text
src/
├── main.cpp
├── config.h
├── app/
│   ├── app.cpp
│   └── app.h
├── scale/
│   ├── scale.cpp
│   └── scale.h
└── level_indicator/
    ├── level_indicator.cpp
    └── level_indicator.h
```

Responsabilidades:

### `main`

Solo inicializa los módulos y ejecuta periódicamente la aplicación.

```cpp
void setup()
{
    scale_init();
    level_indicator_init();
    app_init();
}

void loop()
{
    app_update();
}
```

### `app`

Coordina el comportamiento general:

* Solicita nuevas medidas.
* Decide cuándo actualizar la lectura.
* Entrega el peso al indicador.
* Gestiona estados generales de la aplicación.
* No debe conocer los registros ni los detalles internos del HX711.

### `scale`

Representa la báscula completa:

* Tara.
* Factor de calibración.
* Conversión de cuentas ADC a gramos.
* Filtrado.
* Promediado.
* Validación de medidas.

La aplicación debe pedir algo parecido a:

```c
bool scale_read_grams(float *weight_grams);
```

La aplicación no debe usar directamente objetos de la clase `HX711`.

### `level_indicator`

Controla los tres LED:

* Inicialización de los pines.
* Selección del LED correspondiente.
* Posibles estados de error o espera.

### `config`

Contiene:

* Pines.
* Umbrales.
* Periodos de muestreo.
* Factor inicial de calibración.
* Constantes configurables.

Hito sugerido:

```text
v0.2-structured-bogde
```

---

## Etapa 3: driver propio del HX711 manteniendo el Arduino Core

Eliminar la dependencia de `bogde/HX711`, pero conservar temporalmente:

* `setup()` y `loop()`.
* `Serial`.
* `pinMode()`.
* `digitalRead()`.
* `digitalWrite()`.
* `delayMicroseconds()`.
* El sistema de compilación de Arduino.
* La inicialización realizada por el Arduino Core.

Crear un módulo propio:

```text
hx711/
├── hx711.cpp
└── hx711.h
```

Interfaz aproximada:

```c
void hx711_init(void);
bool hx711_is_ready(void);
bool hx711_read_raw(int32_t *raw_value);
```

El driver debe encargarse únicamente de comunicarse con el HX711.

Debe:

1. Esperar a que `DOUT` pase a nivel bajo.
2. Generar 24 pulsos de reloj.
3. Leer los 24 bits.
4. Generar los pulsos adicionales necesarios para seleccionar canal y ganancia.
5. Realizar correctamente la extensión de signo de 24 a 32 bits.
6. Devolver cuentas ADC sin procesar.
7. Evitar bloquear indefinidamente si el HX711 no responde.
8. Respetar las restricciones temporales indicadas en el datasheet.

El driver del HX711 no debe saber:

* Qué célula de carga está conectada.
* Cuántos gramos representa cada cuenta.
* Cuáles son los umbrales de los LED.
* Cómo funciona la aplicación.
* Cómo se muestran los datos.

La relación correcta debe ser:

```text
app
 ├── scale
 │    └── hx711
 └── level_indicator
```

El módulo `hx711` produce cuentas ADC.

El módulo `scale` convierte esas cuentas a peso:

```text
peso = (lectura_cruda - offset_de_tara) / factor_de_calibración
```

Hito sugerido:

```text
v0.3-custom-hx711-arduino
```

---

## Etapa 4: introducir una HAL propia

Crear una pequeña Hardware Abstraction Layer para evitar que el driver del HX711 dependa directamente de Arduino.

Arquitectura aproximada:

```text
hal/
├── hal_gpio.h
├── hal_gpio.cpp
├── hal_time.h
├── hal_time.cpp
├── hal_uart.h
└── hal_uart.cpp
```

El driver del HX711 debería utilizar funciones semejantes a:

```c
void hal_hx711_clock_write(bool level);
bool hal_hx711_data_read(void);
void hal_delay_microseconds(uint16_t microseconds);
```

Inicialmente, la implementación de la HAL puede usar Arduino:

```cpp
void hal_hx711_clock_write(bool level)
{
    digitalWrite(HX711_SCK_PIN, level ? HIGH : LOW);
}
```

La cadena de dependencias sería:

```text
app
 └── scale
      └── hx711
           └── HAL
                └── Arduino Core
```

El objetivo es que `hx711.cpp` deje de conocer:

* `digitalWrite()`.
* `digitalRead()`.
* Los números de pin Arduino.
* Los registros concretos del ATmega328P.

Hito sugerido:

```text
v0.4-hal-on-arduino
```

---

## Etapa 5: implementar la HAL mediante registros directos

Mantener todavía el Arduino Core y `Serial` para facilitar la depuración, pero reemplazar internamente las funciones Arduino de la HAL por acceso a registros del ATmega328P.

Aprender progresivamente:

* `DDRx`: dirección de los pines.
* `PORTx`: escritura y resistencias pull-up.
* `PINx`: lectura de entradas.
* Máscaras de bits.
* Operaciones AND, OR, XOR y desplazamientos.
* Lectura-modificación-escritura de registros.

Ejemplo conceptual:

```c
DDRD |= (1 << DDD2);
PORTD |= (1 << PORTD2);
PORTD &= ~(1 << PORTD2);

bool level = (PIND & (1 << PIND3)) != 0;
```

En esta etapa:

* `hx711` no debería cambiar.
* `scale` no debería cambiar.
* `app` no debería cambiar.
* Solo debería cambiar la implementación de la HAL.

Esto demostrará que la separación de capas está funcionando.

Hito sugerido:

```text
v0.5-register-hal-arduino-core
```

---

## Etapa 6: eliminar completamente el framework Arduino

Crear un proyecto bare-metal con:

```c
int main(void)
{
    hardware_init();
    app_init();

    while (1)
    {
        app_update();
    }
}
```

Eliminar:

* `setup()`.
* `loop()`.
* `Serial`.
* `millis()`.
* `delay()`.
* `digitalRead()`.
* `digitalWrite()`.
* Dependencia del Arduino Core.

Mantener:

* Compilador AVR-GCC.
* AVR Libc.
* Headers como `<avr/io.h>`.
* Macros de interrupciones de AVR Libc.
* Código de arranque proporcionado por la toolchain, salvo que se decida estudiarlo más adelante.

Implementar progresivamente:

### GPIO

Configuración directa de entradas y salidas.

### UART

Para sustituir `Serial` y conservar la capacidad de depuración.

Funciones aproximadas:

```c
void uart_init(uint32_t baudrate);
void uart_write_byte(uint8_t byte);
void uart_write_string(const char *text);
```

### Base de tiempos

Configurar un timer para generar un tick periódico, por ejemplo cada 1 ms.

Funciones aproximadas:

```c
void time_init(void);
uint32_t time_millis(void);
```

Debe prestarse atención a:

* Prescaler.
* Frecuencia del reloj.
* Modo CTC.
* Registro de comparación.
* Interrupción del timer.
* Acceso atómico a variables de más de 8 bits.
* Uso correcto de `volatile`.
* Desbordamiento del contador.

### Interrupciones

Utilizarlas solo cuando aporten una ventaja clara.

No es obligatorio usar interrupciones para todo. Se puede utilizar polling cuando sea suficiente.

### Reloj

En el Arduino Nano clásico, los fusibles suelen estar ya configurados para trabajar con el reloj de la placa. Definir:

```c
#define F_CPU 16000000UL
```

no configura físicamente el reloj; informa al código y a determinadas librerías sobre la frecuencia esperada.

La configuración de fusibles debe tratarse como un tema separado y con precaución.

Hito sugerido:

```text
v1.0-bare-metal-avr
```

---

## Etapa 7: portar el proyecto a otra plataforma

Portar el proyecto a una plataforma como STM32.

La intención es conservar sin grandes cambios:

* `app`.
* `scale`.
* La lógica de calibración.
* La lógica de los tres LED.
* Parte o la totalidad del driver HX711.

Cambiar principalmente:

* HAL de GPIO.
* HAL de tiempo.
* HAL de UART.
* Inicialización del microcontrolador.
* Configuración del reloj.
* Toolchain y sistema de compilación.

En STM32 se podrá aprender progresivamente:

* CMSIS.
* HAL.
* LL.
* Registros.
* NVIC.
* SysTick.
* Timers.
* GPIO.
* UART.
* DMA.

No es necesario comenzar STM32 haciendo bare-metal absoluto. Puede comenzarse con HAL o LL y bajar progresivamente de nivel.

Hito sugerido:

```text
v2.0-stm32-port
```

---

# Arquitectura objetivo

La arquitectura aproximada final será:

```text
src/
├── main.c
├── config.h
├── app/
│   ├── app.c
│   └── app.h
├── scale/
│   ├── scale.c
│   └── scale.h
├── drivers/
│   ├── hx711.c
│   └── hx711.h
├── indicators/
│   ├── level_indicator.c
│   └── level_indicator.h
└── hal/
    ├── hal_gpio.c
    ├── hal_gpio.h
    ├── hal_time.c
    ├── hal_time.h
    ├── hal_uart.c
    └── hal_uart.h
```

No es obligatorio crear todos estos archivos desde el principio. Deben añadirse únicamente cuando exista una responsabilidad real que separar.

---

# Reglas de dependencia

La aplicación no debe conocer el hardware concreto.

```text
app → scale → hx711 → HAL → microcontrolador
```

La dirección de las dependencias debe mantenerse.

## `app`

Sabe que existe una báscula, pero no sabe cómo funciona el HX711.

## `scale`

Sabe que recibe cuentas ADC de un conversor, pero gestiona gramos, tara, calibración y filtrado.

## `hx711`

Sabe cómo comunicarse con el chip, pero no conoce gramos ni la lógica del sistema.

## HAL

Sabe cómo controlar GPIO, timers y UART en la plataforma concreta.

## Plataforma

Contiene los registros específicos del ATmega328P, STM32 u otro microcontrolador.

---

# Principios importantes

## No cambiar demasiadas cosas simultáneamente

Cada etapa debe conservar una referencia funcional anterior.

## No reescribir por reescribir

Cada refactor debe tener un objetivo claro:

* Mejor aislamiento.
* Mayor capacidad de prueba.
* Sustituir una dependencia.
* Aprender una capa concreta.
* Facilitar el futuro portado.

## Evitar la sobrearquitectura

Separación de responsabilidades no significa crear muchos archivos.

Un módulo debe existir porque tiene un motivo propio para cambiar.

## Mantener el programa observable

Conservar durante el mayor tiempo posible:

* Salida serie.
* Indicadores de error.
* Valores crudos.
* Peso calculado.
* Estado del HX711.
* Información de calibración.

## Trabajar desde abajo mediante pruebas pequeñas

Antes de integrar un componente:

1. Probar GPIO.
2. Probar temporización.
3. Probar UART.
4. Probar lectura cruda del HX711.
5. Probar tara.
6. Probar calibración.
7. Probar filtrado.
8. Probar umbrales.
9. Integrar la aplicación completa.

---

# Posibles ramas de Git

Puede utilizarse una rama estable y ramas educativas:

```text
main
feature/minimal-hx711
refactor/separation-of-responsibilities
feature/custom-hx711-driver
feature/hal
feature/register-gpio
feature/bare-metal-avr
feature/stm32-port
```

No es obligatorio mantener todas las ramas indefinidamente. Lo importante es disponer de commits claros y versiones recuperables.

---

# Estado inicial al retomar el proyecto

Cuando se retome el proyecto, primero hay que identificar en qué etapa se quedó.

Preguntas que debe resolver el asistente:

1. ¿Existe ya una lectura funcional con `bogde/HX711`?
2. ¿Está calibrada la báscula?
3. ¿Se conocen los pines utilizados?
4. ¿Qué placa exacta se está usando?
5. ¿Qué estructura de archivos existe?
6. ¿Qué módulos están ya separados?
7. ¿Se sigue utilizando la librería Bogde?
8. ¿Existe ya un driver propio?
9. ¿La HAL usa Arduino o registros?
10. ¿Se conserva `Serial`?
11. ¿Cuál es el último commit o tag funcional?
12. ¿Qué error o siguiente objetivo concreto existe?

No se debe reiniciar el proyecto desde cero si ya existe una etapa funcional.

---

# Estado actual del proyecto

## v0.3: calibración persistente completada

El proyecto utiliza actualmente Arduino Nano, ATmega328P, Arduino Core y la librería `bogde/HX711`.

La aplicación está separada en módulos:

```text
src/
├── main.cpp
├── app.cpp
├── app.h
├── button.cpp
├── button.h
├── calibration_storage.cpp
├── calibration_storage.h
├── config.h
├── indicator_leds.cpp
├── indicator_leds.h
├── level_indicator.cpp
├── level_indicator.h
├── operation_indicator.cpp
├── operation_indicator.h
├── scale.cpp
└── scale.h
```

Funcionalidades completadas:

* Lectura de la célula de carga mediante HX711.
* Conversión de las cuentas ADC a gramos.
* Tara automática durante el arranque.
* Tara manual mediante pulsador físico o puerto serie.
* Tres niveles de peso con histéresis.
* Separación entre la lógica de nivel y el control físico de los LED.
* Factor de calibración modificable durante la ejecución.
* Almacenamiento persistente del factor en EEPROM.
* Registro EEPROM con identificador, versión y CRC.
* Validación de factores inválidos o datos EEPROM corruptos.
* Recuperación del factor predeterminado cuando no existe una calibración válida.
* Máquina de estados de calibración.
* Calibración mediante puerto serie.
* Pulsador físico de calibración.
* Pulsación larga para evitar iniciar una calibración accidentalmente.
* Cancelación de la calibración mediante el pulsador de tara o el comando serie.
* Señalización LED de tara, estados de calibración, éxito y error.
* Literales de diagnóstico almacenados en flash mediante `F()` para reducir el consumo de SRAM.

Controles actuales:

```text
D4:
    funcionamiento normal → tara
    calibración activa    → cancelar

D8:
    mantener pulsado      → iniciar calibración
    pulsación breve       → confirmar paso

Puerto serie:
    t → tara
    c → iniciar o confirmar calibración
    q → cancelar calibración
    s → guardar el factor activo
    x → invalidar la calibración almacenada
```

El factor de calibración actual sigue siendo provisional porque la plataforma mecánica definitiva todavía no está construida.

El valor `DEFAULT_CALIBRATION_FACTOR` es un respaldo compilado en el firmware. La calibración específica y más reciente de cada dispositivo se guarda en EEPROM.

## v0.4: advertencia de nivel muy bajo completada

* Cuatro estados de nivel: VERY_LOW, LOW, MEDIUM y HIGH.
* Histéresis entre todos los niveles.
* Aviso de recipiente muy vacío mediante parpadeo no bloqueante del LED LOW.
* Frecuencias visuales diferenciadas:
  - VERY_LOW: 250 ms por cambio.
  - Espera de calibración: 500 ms por cambio.
  - Éxito y error de calibración: 150 ms por cambio.
* Prioridad de los patrones de operación sobre la indicación normal de nivel.

Niveles provisionales:

VERY_LOW:
    menos de 100 g

LOW:
    de 100 a 500 g

MEDIUM:
    de 500 a 1000 g

HIGH:
    más de 1000 g

Con histéresis de 20g:

VERY_LOW → LOW:
    120 g o más

LOW → VERY_LOW:
    80 g o menos

## Siguiente gran etapa educativa

Después de completar las funcionalidades de aplicación previstas, se eliminará la dependencia de `bogde/HX711` y se escribirá un driver propio del HX711 manteniendo inicialmente el Arduino Core.

El driver propio se desarrollará en una rama independiente:

```text
feature/custom-hx711-driver
```

No se cambiarán simultáneamente la lógica de aplicación y el driver de comunicación.


## Custom HX711 driver milestone

The project now uses a custom HX711 driver instead of the Bogde HX711 library.

The HX711 protocol logic is implemented primarily in C and is separated from the temporary Arduino platform adapter. The driver supports initialization, readiness checking, finite timeouts, signed 24-bit raw readings, channel and gain selection, and power control.

The scale module preserves the previous tare, averaging and calibration behaviour. Persistent calibration, the physical tare button, the serial calibration workflow and LED level indication continue to operate correctly.

The Bogde dependency has been removed from `platformio.ini`, and the project has been successfully compiled, uploaded and physically tested on the Arduino Nano.

The current platform backend is `hx711_platform_arduino.cpp`. A future custom HAL should replace this backend without rewriting the HX711 protocol implementation.

Dedicated physical testing of the power-down and power-up functions and automated driver tests remain future improvements.

## Project HAL milestone

The `feature/project-hal` milestone introduced a project-specific hardware abstraction layer while retaining the Arduino framework as the temporary platform backend.

The project now provides C-compatible HAL interfaces for:

* Digital GPIO.
* Millisecond timing.
* Microsecond delays.
* Critical sections.

The implemented backend files are:

```text
hal_gpio_arduino.cpp
hal_time_arduino.cpp
hal_critical_avr.c
```

The HX711 platform adapter now uses the project HAL and no longer depends directly on Arduino or AVR functions.

The following modules were migrated:

* HX711 platform adapter.
* Physical indicator LED module.
* Button module.
* Level-indicator timing.
* Operation-indicator timing.
* Periodic application timing.

The HX711 protocol driver itself remained unchanged.

The existing behaviour was preserved:

* Weight measurement.
* Initial and physical tare.
* Persistent calibration.
* Button debounce.
* Long-hold calibration entry.
* Very-low-level blinking.
* Low, medium and high level indication.
* Operation-indicator patterns.
* Periodic serial output.

The project still uses the Arduino framework.

The remaining direct Arduino dependencies are primarily:

* Arduino startup through `setup()` and `loop()`.
* Serial communication and flash-string support.
* EEPROM storage.
* Temporary Arduino GPIO and time backends.

The critical-section backend is currently AVR-specific by design.

A future custom AVR backend should replace the Arduino GPIO and time backends without requiring changes to the HX711 driver or migrated application modules.

The next recommended major milestone is to add automated host-side tests using fake or simulated HAL backends before replacing additional platform infrastructure.

## Native unit tests — v0.7

The project now includes native host-side unit tests using PlatformIO, GCC and Unity.

Four isolated test environments were added:

* `native_button`
* `native_hx711`
* `native_level_indicator`
* `native_operation_indicator`

The native implementations replace hardware dependencies with fake GPIO, time, LED, HX711 platform and critical-section backends.

Current automated coverage:

* Button: 10 tests.
* HX711 driver: 18 tests.
* Level indicator: 14 tests.
* Operation indicator: 14 tests.
* Total: 56 native tests.

The tests cover debounce, long presses, contact bounce, timer overflow, HX711 bit reconstruction, signed 24-bit conversion, timeouts, gain pulses, critical sections, power control, level thresholds, hysteresis, blinking and operation-status patterns.

All native tests pass and the normal Arduino Nano firmware continues to compile.

The completed milestone will be tagged as `v0.7-native-unit-tests`.

The next planned milestone is to introduce direct AVR HAL backends incrementally while preserving the Arduino implementation as a reference and using the native tests to prevent regressions.


## Direct AVR GPIO backend — v0.8

The project now uses a direct ATmega328P register backend for digital GPIO.

The public project HAL remains unchanged:

```text
hal_gpio_configure_input()
hal_gpio_configure_input_pullup()
hal_gpio_configure_output()
hal_gpio_read()
hal_gpio_write()
```

The production GPIO path is now:

```text
Application modules
        |
        v
Project GPIO HAL
        |
        v
hal_gpio_avr.c
        |
        v
DDRx / PORTx / PINx
        |
        v
ATmega328P hardware
```

The direct AVR backend supports Arduino-compatible digital pin identifiers:

```text
D0-D7   -> PORTD
D8-D13  -> PORTB
D14-D19 -> PORTC
```

Nano pins A6 and A7 are not accepted as digital GPIO because they are analog-input-only pins.

The backend provides:

* Safe translation from Arduino pin identifiers to AVR registers and bit masks.
* Input configuration through `DDRx`.
* Internal pull-up control through `PORTx`.
* Output configuration through `DDRx`.
* Digital input reads through `PINx`.
* Digital output writes through `PORTx`.
* Safe handling of invalid pin identifiers.
* Critical-section protection for register read-modify-write operations.
* Preservation of unrelated bits in shared port registers.

The previous Arduino implementation remains available in:

```text
hal_gpio_arduino.cpp
```

but is excluded from the production Nano build through `platformio.ini`.

No changes were required in:

* Button logic.
* Indicator LED logic.
* Level-indicator logic.
* Operation-indicator logic.
* HX711 driver.
* HX711 platform adapter.
* Scale module.
* Application state machine.

This confirms that the project GPIO abstraction boundary works correctly.

Validation completed successfully:

* 56 native tests pass.
* Arduino Nano firmware compiles.
* HX711 communication works physically.
* Weight readings remain functional.
* Tare and calibration buttons work.
* Internal pull-ups work.
* Short-press and long-press detection work.
* All three LEDs work.
* Very-low warning blinking works.
* Tare, calibration, success and error patterns work.
* Persistent calibration continues loading.
* Complete calibration flow works.

Memory comparison:

```text
Arduino GPIO backend:
RAM:   744 bytes
Flash: 12406 bytes

Direct AVR GPIO backend:
RAM:   744 bytes
Flash: 12450 bytes
```

The direct backend therefore leaves SRAM usage unchanged and increases flash usage by 44 bytes in the current build.

The completed milestone is tagged:

```text
v0.8-avr-gpio-backend
```

The next planned milestone is a direct AVR time backend. It should replace `hal_time_arduino.cpp` incrementally while retaining the Arduino Core for startup, Serial and EEPROM during the transition.

## Direct AVR time backend — v0.9

The project now uses a direct ATmega328P time backend for project timing.

The public time HAL provides:

```text
hal_time_init()
hal_time_millis()
hal_time_delay_us()
```

The active production backend is:

```text
src/hal_time_avr.c
```

The previous Arduino implementation remains available as a reference:

```text
src/hal_time_arduino.cpp
```

but is excluded from the Arduino Nano production build.

The project time architecture is:

```text
Application timing
        |
        v
Project time HAL
        |
        +--> Timer1 CTC interrupt every 1 ms
        |        |
        |        v
        |   volatile uint32_t counter
        |
        +--> calibrated AVR busy loop
                 |
                 v
          microsecond delays
```

Timer1 configuration:

```text
CPU frequency:    16 MHz
Prescaler:        64
Timer frequency:  250 kHz
OCR1A:            249
Interrupt period: 1 ms
Interrupt vector: TIMER1_COMPA_vect
```

The 32-bit millisecond counter is incremented by a short Timer1 Compare Match A ISR.

`hal_time_millis()` reads the counter inside a critical section because the ATmega328P cannot read a 32-bit value atomically.

Natural `uint32_t` overflow is preserved, and application modules continue using unsigned subtraction for overflow-safe timing.

`hal_time_delay_us()` no longer calls Arduino `delayMicroseconds()`.

It uses AVR libc `_delay_loop_2()` with a calculation specific to the Nano's 16 MHz CPU frequency.

Large delays are divided into safe chunks, and a zero-microsecond request is handled explicitly.

The project now reserves Timer1.

PWM on D9 and D10, Timer1-based Servo implementations and other Timer1-dependent libraries must not be introduced without redesigning the timebase.

Timer0 remains controlled by Arduino during the transition and continues supporting remaining direct calls to:

```text
millis()
micros()
delay()
```

The active direct AVR low-level backends are now:

```text
hal_gpio_avr.c
hal_time_avr.c
hal_critical_avr.c
```

The Arduino Core remains temporarily for:

```text
startup
setup() and loop()
Serial
EEPROM
remaining direct delay() calls
```

Validation completed successfully:

* 56 native tests pass.
* Arduino Nano firmware compiles.
* Firmware starts normally.
* Serial remains functional.
* HX711 communication works.
* Weight readings remain functional.
* Tare works.
* Short and long button presses work.
* Button debounce remains correct.
* Level and operation-indicator timing remains correct.
* Complete calibration works.
* Persistent calibration continues loading after restart.

Memory usage with the direct AVR GPIO and time backends:

```text
RAM:   748 bytes
Flash: 12620 bytes
```

The completed milestone will be tagged:

```text
v0.9-avr-timebase
```

The next recommended milestone is to add native tests for the `scale` module before replacing persistent storage or Serial communication.

The likely following architectural stages are:

```text
v0.10: native tests for scale
v0.11: storage HAL and testable calibration records
v0.12: direct AVR EEPROM backend
v0.13: console/UART abstraction
v1.0: remove the Arduino Core and provide a project-owned main()
```

## Native scale tests — v0.10

The project now contains a native host-side unit-test suite for the scale module.

The environment is:

```text
native_scale
```

It compiles the real production implementation:

```text
src/scale.cpp
```

against a fake implementation of the public HX711 driver API:

```text
test/test_scale/fake_hx711_driver.c
```

The test architecture is:

```text
scale.cpp
    |
    v
hx711_driver.h
    |
    v
fake_hx711_driver.c
    |
    v
controlled readings, errors and call records
```

The fake HX711 driver can:

* Control initialization results.
* Control startup readiness results.
* Control current readiness.
* Supply sequences of raw readings.
* Inject read failures at selected positions.
* Record initialization pins.
* Record timeout values.
* Count driver calls.
* Count consumed readings.
* Detect unexpected extra reads.

The scale suite covers:

* Successful initialization.
* HX711 initialization failure.
* HX711 startup timeout.
* State reset after successful initialization.
* State preservation after failed initialization.
* Positive calibration factors.
* Negative calibration factors.
* Rejection of zero, NaN and infinity.
* Rejection of factors below the minimum magnitude.
* Acceptance of exact boundary factors.
* Preservation of the previous valid factor.
* Successful tare using 20 samples.
* Positive, negative and mixed tare readings.
* Failed tare at the first, intermediate and final sample.
* Preservation of the previous offset after failed tare.
* Repeated tare.
* Single-sample net-count calculations.
* Multi-sample net-count calculations.
* Signed integer truncation.
* Null output pointers.
* Zero sample counts.
* Read failures at multiple positions.
* Preservation of caller output values after failures.
* Positive weight conversions.
* Negative weight conversions.
* Positive and negative calibration-factor signs.
* HX711-not-ready behaviour.
* Weight-read failures.
* Arithmetic limits using 255 maximum or minimum HX711 readings.

The scale suite contains:

```text
32 tests
0 failures
```

The complete native regression now contains:

```text
native_button:              10 tests
native_hx711:               18 tests
native_level_indicator:     14 tests
native_operation_indicator: 14 tests
native_scale:               32 tests

Total:                      88 tests
Failures:                    0
```

The production Arduino Nano firmware continues compiling successfully.

This branch does not modify the public scale API or production behaviour.

The current test-layer architecture is:

```text
Application logic
    |
    +--> native_button
    +--> native_level_indicator
    +--> native_operation_indicator
    +--> native_scale

Hardware protocol logic
    |
    +--> native_hx711
```

The completed milestone is tagged:

```text
v0.10-native-scale-tests
```

The next recommended milestone is to separate calibration-record validation from physical EEPROM access.

A suitable progression is:

```text
v0.11  Testable calibration-record format and storage HAL
v0.12  Direct AVR EEPROM backend
v0.13  Console abstraction
v0.14  Direct AVR UART backend
v1.0   Project-owned main() without the Arduino Core
```

## Calibration storage HAL — v0.11

The project now separates calibration-record logic from physical non-volatile storage.

The architecture is:

```text
calibration_storage.cpp
        |
        +--> calibration_record.cpp
        |
        +--> hal_storage.h
                 |
                 v
        hal_storage_arduino.cpp
                 |
                 v
        Arduino EEPROM library
```

The public calibration-storage API remains unchanged:

```text
calibration_storage_load()
calibration_storage_save()
calibration_storage_clear()
```

The storage HAL provides:

```text
hal_storage_capacity()
hal_storage_read()
hal_storage_write()
```

It operates on byte ranges and does not understand calibration records.

The active production backend is:

```text
src/hal_storage_arduino.cpp
```

It uses:

```text
EEPROM.length()
EEPROM.read()
EEPROM.update()
```

The calibration record is now an explicit fixed-size binary format:

```text
Size: 12 bytes

Offset  Size  Field
0       4     Magic
4       2     Version
6       4     Calibration-factor bits
10      2     CRC-16/CCITT
```

Multibyte values use explicit little-endian encoding.

Preserved format values:

```text
Magic:       0x4C43414C
Version:     1
CRC:         CRC-16/CCITT
Polynomial:  0x1021
Initial CRC: 0xFFFF
```

For `45.5F`, the complete record is:

```text
4C 41 43 4C 01 00 00 00 36 42 90 F3
```

The explicit format remains compatible with calibration records written by the previous implementation on the ATmega328P.

The native environment:

```text
native_calibration_storage
```

tests the real:

```text
src/calibration_record.cpp
src/calibration_storage.cpp
```

against:

```text
test/test_calibration_storage/fake_hal_storage.cpp
```

The suite covers:

* Record encoding and decoding.
* Byte ordering.
* CRC generation and validation.
* Magic and version validation.
* Calibration-factor validation.
* Output preservation after failures.
* Successful load, save and clear operations.
* Insufficient storage capacity.
* Simulated read and write failures.
* Verification failures.
* Corrupted read-back data.
* Mismatched saved factors.
* Exact access addresses and lengths.
* Magic-only record invalidation.

Test totals:

```text
native_button:                       10
native_hx711:                        18
native_level_indicator:              14
native_operation_indicator:          14
native_scale:                        32
native_calibration_storage:          40

Total:                              128
Failures:                             0
```

Physical validation confirms:

* An existing calibration remains readable.
* The old and new record formats are compatible.
* A new calibration can be saved.
* Calibration persists across restart.
* Persistent calibration can be cleared.
* The default factor is restored after clearing.
* Existing scale, button, indicator and Serial behaviour remains functional.

Production memory usage:

```text
RAM:   <RAM_BYTES> bytes
Flash: <FLASH_BYTES> bytes
```

The completed milestone is tagged:

```text
v0.11-calibration-storage-hal
```

The next recommended milestone is:

```text
v0.12-direct-avr-eeprom
```

It will replace `hal_storage_arduino.cpp` with a direct AVR EEPROM backend while preserving:

```text
calibration_storage.cpp
calibration_record.cpp
hal_storage.h
native calibration-storage tests
```
