#ifndef SCALE_ADC_BACKEND_H
#define SCALE_ADC_BACKEND_H

/*
 * Stable preprocessor values used by PlatformIO build flags.
 */
#define SCALE_ADC_BACKEND_HX711    1
#define SCALE_ADC_BACKEND_ADS1232  2

/*
 * Preserve the validated HX711 firmware as the default build.
 */
#ifndef SCALE_ADC_BACKEND
#define SCALE_ADC_BACKEND SCALE_ADC_BACKEND_HX711
#endif

#if SCALE_ADC_BACKEND == SCALE_ADC_BACKEND_HX711

#define SCALE_ADC_NAME_LITERAL "HX711"

#elif SCALE_ADC_BACKEND == SCALE_ADC_BACKEND_ADS1232

#define SCALE_ADC_NAME_LITERAL "ADS1232"

#else

#error "Unsupported SCALE_ADC_BACKEND value."

#endif

#endif
