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

## Próximo paso inmediato

Añadir una advertencia de recipiente muy vacío como una feature independiente:

```text
feature/very-low-warning
```

Comportamiento previsto:

```text
VERY_LOW → LED rojo parpadeando
LOW      → LED rojo fijo
MEDIUM   → LED intermedio fijo
HIGH     → LED superior fijo
```

El parpadeo de `VERY_LOW` utilizará una frecuencia diferente de los patrones de calibración para que ambos estados puedan distinguirse claramente.

## Siguiente gran etapa educativa

Después de completar las funcionalidades de aplicación previstas, se eliminará la dependencia de `bogde/HX711` y se escribirá un driver propio del HX711 manteniendo inicialmente el Arduino Core.

El driver propio se desarrollará en una rama independiente:

```text
feature/custom-hx711-driver
```

No se cambiarán simultáneamente la lógica de aplicación y el driver de comunicación.
