# Componente `lectura_adc`

La lectura puntual de un pin analógico con el ADC del ESP32-S3, con su
calibración. Abre la unidad del pin en modo puntual, configura su canal con la
atenuación pedida y crea el esquema de calibración por ajuste de curva, el
único que admite el ESP32-S3.

El componente **no sabe en qué pin está la señal**: lo recibe al iniciarse y
deduce de él la unidad y el canal. Así el número del pin se escribe en un solo
sitio, el fichero de pines del proyecto, y el componente sirve igual en otro
proyecto.

En el código de `main`, que es quien sabe el pin:

```c
#include "lectura_adc.h"
#include "pines.h"   /* el fichero de pines del proyecto, no del componente */

static lectura_adc_t lectura;

void app_main(void)
{
    ESP_ERROR_CHECK(lectura_adc_iniciar(&lectura, PIN_POTENCIOMETRO,
                                        ADC_ATTEN_DB_12));
    int cuentas = 0;
    int mv = 0;
    ESP_ERROR_CHECK(lectura_adc_leer(&lectura, &cuentas));
    if (lectura_adc_a_mv(&lectura, cuentas, &mv) == ESP_OK) {
        /* hay milivoltios */
    }
    lectura_adc_liberar(&lectura);
}
```

- **Solo la primera unidad.** Si el pin es de la segunda, `lectura_adc_iniciar()`
  devuelve `ESP_ERR_NOT_SUPPORTED`; si el pin no tiene canal de ADC,
  `ESP_ERR_NOT_FOUND`. La segunda unidad la comparte la radio, y con la
  conectividad inalámbrica activa una lectura sobre ella puede fallar.
- **El fallo de la calibración se trata.** Si el chip no trae los datos de
  calibración de fábrica, `lectura_adc_iniciar()` devuelve igualmente
  `ESP_OK`, deja `cali` en `NULL` y `sin_datos_cali` en `true`. Desde ahí,
  `lectura_adc_a_mv()` devuelve `ESP_ERR_INVALID_STATE` y el programa decide
  qué escribir y qué hacer: aquí sigue en cuentas crudas. El componente no
  escribe nada en la traza; quien escribe es ESP-IDF, como se cuenta en el
  `README.md` del ejemplo.
- **El esquema se rehace al cambiar la atenuación.** `lectura_adc_atenuacion()`
  vuelve a configurar el canal y vuelve a crear el esquema, porque el esquema
  se crea para una atenuación concreta. Si no se rehiciera, la conversión
  seguiría funcionando y daría un número equivocado. Si el chip no trae los
  datos, no lo vuelve a intentar. Si algo falla, deja la atenuación anterior.
- **Se libera lo creado.** `lectura_adc_liberar()` borra el esquema y la unidad.
- `lectura_adc_tope()` da la cuenta más alta que puede devolver el conversor,
  la del tope de la escala.

Depende de `esp_adc`, declarado en `REQUIRES` porque sus cabeceras aparecen en
la cabecera pública del componente.
