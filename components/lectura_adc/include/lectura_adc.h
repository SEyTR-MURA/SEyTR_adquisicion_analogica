/* lectura_adc.h - lectura puntual de un pin analógico, con su calibración.
 *
 * Reúne en un componente lo que la sesión enseña por partes: la unidad y el
 * canal en modo puntual, la atenuación del canal, el esquema de calibración
 * por ajuste de curva y su fallo tratado, y la conversión de cuentas a
 * milivoltios.
 */
#pragma once

#include <stdbool.h>

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"

typedef struct {
    int                       pin;     /* GPIO de la señal */
    adc_unit_t                unidad;  /* la deduce del pin */
    adc_channel_t             canal;   /* también */
    adc_atten_t               atten;   /* atenuación en uso */
    adc_oneshot_unit_handle_t adc;     /* la unidad abierta en modo puntual */
    adc_cali_handle_t         cali;    /* NULL si no hay esquema de calibración */
    bool                      sin_datos_cali; /* el chip no trae los datos de fábrica */
} lectura_adc_t;

/* Abre la unidad del pin en modo puntual, configura su canal con la
 * atenuación pedida e intenta crear el esquema de calibración.
 *
 * Devuelve:
 * - ESP_OK, aunque el esquema no se haya podido crear porque el chip no trae
 *   los datos de fábrica: entonces l->cali queda en NULL, l->sin_datos_cali
 *   en true, y se sigue en cuentas crudas.
 * - ESP_ERR_INVALID_ARG si el puntero es nulo.
 * - ESP_ERR_NOT_FOUND si el pin no tiene canal de ADC.
 * - ESP_ERR_NOT_SUPPORTED si el pin es de la segunda unidad: este componente
 *   solo usa la primera, la que no comparte el conversor con la radio.
 * - El error de la función de ESP-IDF que falle, en cualquier otro caso. */
esp_err_t lectura_adc_iniciar(lectura_adc_t *l, int pin, adc_atten_t atten);

/* Cambia la atenuación del canal y vuelve a crear el esquema de calibración
 * con la nueva, porque el esquema depende de ella. Si algo falla, deja la
 * atenuación anterior. */
esp_err_t lectura_adc_atenuacion(lectura_adc_t *l, adc_atten_t atten);

/* Una conversión: la cuenta cruda. Devuelve el error de adc_oneshot_read(). */
esp_err_t lectura_adc_leer(lectura_adc_t *l, int *cuentas);

/* Si hay esquema de calibración, convierte la cuenta en milivoltios.
 * Sin esquema devuelve ESP_ERR_INVALID_STATE y no toca *mv. */
esp_err_t lectura_adc_a_mv(const lectura_adc_t *l, int cuentas, int *mv);

/* La cuenta más alta que puede dar el conversor: el tope de la escala. */
int lectura_adc_tope(void);

/* Libera lo creado: el esquema de calibración y la unidad. */
void lectura_adc_liberar(lectura_adc_t *l);
