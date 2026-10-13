/* pines.h - los pines del proyecto, en un solo sitio.
 *
 * Cada número de pin se escribe aquí y en ningún otro fichero de código. Los
 * componentes lo reciben como parámetro al iniciarse, y la tabla de pines del
 * README tiene que decir lo mismo que este fichero.
 */
#pragma once

#include "driver/gpio.h"

/* Pin al que va el cursor del potenciómetro.
 *
 * GPIO_NUM_NC significa «sin conectar»: el repositorio llega así a propósito,
 * porque el pin lo eliges tú. Sin señal no hay nada que medir, así que,
 * mientras no lo cambies, el programa avisa al arrancar y no hace nada más.
 *
 * Tiene que ser un pin con canal de la PRIMERA unidad de ADC, la que no
 * comparte el conversor con la radio. La tabla de pines de la hoja de
 * características del ESP32-S3 dice qué pines tienen canal y de qué unidad.
 * Descarta además los de arranque y los que tu placa ya usa, y comprueba en su
 * esquema que el pin sale a la tira. Después escribe aquí su número, por
 * ejemplo GPIO_NUM_xx, compila y graba. Si eliges uno sin canal o de la
 * segunda unidad, el programa lo dice al arrancar y no sigue.
 */
#define PIN_POTENCIOMETRO       GPIO_NUM_NC
