/* lectura_adc.c - lectura puntual de un pin analógico, con su calibración. */
#include "lectura_adc.h"

#include <stddef.h>

#include "esp_adc/adc_cali_scheme.h"
#include "soc/soc_caps.h"

/* Crea el esquema de calibración por ajuste de curva, el único que admite el
 * ESP32-S3, con la misma unidad y la misma atenuación con las que se lee el
 * canal. Si los datos de fábrica no están, no hay esquema: se anota y se
 * sigue en cuentas crudas, y no se vuelve a intentar, porque los datos no van
 * a aparecer. Quien decide qué decir es el programa, no este componente. */
static esp_err_t crear_cali(lectura_adc_t *l)
{
    l->cali = NULL;
    if (l->sin_datos_cali) {
        return ESP_OK;
    }
    adc_cali_curve_fitting_config_t cfg = {
        .unit_id  = l->unidad,
        .chan     = l->canal,
        .atten    = l->atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    const esp_err_t err = adc_cali_create_scheme_curve_fitting(&cfg, &l->cali);
    if (err == ESP_ERR_NOT_SUPPORTED) {
        l->cali = NULL;
        l->sin_datos_cali = true;
        return ESP_OK;
    }
    return err;
}

static void borrar_cali(lectura_adc_t *l)
{
    if (l->cali != NULL) {
        adc_cali_delete_scheme_curve_fitting(l->cali);
        l->cali = NULL;
    }
}

static esp_err_t configurar_canal(lectura_adc_t *l)
{
    adc_oneshot_chan_cfg_t canal = {
        .atten    = l->atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    return adc_oneshot_config_channel(l->adc, l->canal, &canal);
}

esp_err_t lectura_adc_iniciar(lectura_adc_t *l, int pin, adc_atten_t atten)
{
    if (l == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *l = (lectura_adc_t){ .pin = pin, .atten = atten, .cali = NULL };

    /* La unidad y el canal no se eligen: los fija el pin. */
    esp_err_t err = adc_oneshot_io_to_channel(pin, &l->unidad, &l->canal);
    if (err != ESP_OK) {
        return ESP_ERR_NOT_FOUND;
    }
    if (l->unidad != ADC_UNIT_1) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    adc_oneshot_unit_init_cfg_t uni = {
        .unit_id = l->unidad,
    };
    err = adc_oneshot_new_unit(&uni, &l->adc);
    if (err != ESP_OK) {
        return err;
    }
    err = configurar_canal(l);
    if (err == ESP_OK) {
        err = crear_cali(l);
    }
    if (err != ESP_OK) {
        lectura_adc_liberar(l);
    }
    return err;
}

esp_err_t lectura_adc_atenuacion(lectura_adc_t *l, adc_atten_t atten)
{
    const adc_atten_t anterior = l->atten;
    l->atten = atten;
    esp_err_t err = configurar_canal(l);
    if (err == ESP_OK) {
        /* El esquema se creó para la atenuación anterior. Si no se rehace, la
         * conversión sigue funcionando y devuelve un número equivocado. */
        borrar_cali(l);
        err = crear_cali(l);
    }
    if (err != ESP_OK) {
        /* Se vuelve a la atenuación de antes, con su esquema. */
        l->atten = anterior;
        configurar_canal(l);
        borrar_cali(l);
        crear_cali(l);
    }
    return err;
}

esp_err_t lectura_adc_leer(lectura_adc_t *l, int *cuentas)
{
    return adc_oneshot_read(l->adc, l->canal, cuentas);
}

esp_err_t lectura_adc_a_mv(const lectura_adc_t *l, int cuentas, int *mv)
{
    if (l->cali == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return adc_cali_raw_to_voltage(l->cali, cuentas, mv);
}

int lectura_adc_tope(void)
{
    /* Con ADC_BITWIDTH_DEFAULT el controlador usa la anchura máxima del chip. */
    return (1 << SOC_ADC_RTC_MAX_BITWIDTH) - 1;
}

void lectura_adc_liberar(lectura_adc_t *l)
{
    borrar_cali(l);
    if (l->adc != NULL) {
        adc_oneshot_del_unit(l->adc);
        l->adc = NULL;
    }
}
