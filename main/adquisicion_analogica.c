/* adquisicion_analogica.c - SEyTR, Unidad 2.
 *
 * Qué hace este programa
 * ----------------------
 * Lee un potenciómetro con el ADC en modo puntual, a período fijo, y escribe
 * en cada línea de traza la cuenta cruda y, si se pide, los milivoltios
 * calibrados, el valor filtrado y el período real medido entre dos muestras.
 *
 * Desde el monitor, sin recompilar, se cambia la atenuación del canal, se
 * activa la conversión calibrada, se elige el filtro y su ventana, se mide la
 * dispersión de las lecturas y se compara la espera hasta instante absoluto
 * con la espera relativa, con y sin un cuerpo de bucle lento. Las teclas se
 * listan al arrancar y con «h».
 *
 * Qué hardware necesita
 * ---------------------
 * La placa del curso, conectada por su conector USB-a-UART: es por donde
 * llegan las teclas. Un potenciómetro entre 3V3 y masa, con el cursor en un
 * pin con canal de la primera unidad de ADC; el pin se declara en pines.h.
 *
 * Las tres tareas
 * ---------------
 * - «muestreo» lee el ADC, convierte, filtra y mide el período. Es la de más
 *   prioridad, y es la única que toca el ADC.
 * - «impresora» escribe las trazas. Escribir en la consola es lento, y si lo
 *   hiciera «muestreo», ese tiempo entraría en el cuerpo del bucle y en el
 *   período que se quiere medir.
 * - «teclado» lee las teclas y se las pasa a «muestreo».
 * Las tareas, sus prioridades y las colas entre ellas se estudian en la U3 y
 * en la U4; aquí basta con saber qué hace cada una.
 */

#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "sdkconfig.h"

#include "filtro.h"
#include "lectura_adc.h"
#include "pines.h"

static const char *TAG = "adc";

/* Valores de la configuración del proyecto. */
#define PERIODO_MS          CONFIG_SEYTR_PERIODO_MS
#define VENTANA_INICIAL     CONFIG_SEYTR_VENTANA_INICIAL
#define CUERPO_LENTO_MS     CONFIG_SEYTR_CUERPO_LENTO_MS
#define MUESTRAS_DISPERSION CONFIG_SEYTR_MUESTRAS_DISPERSION

/* Dos comprobaciones al compilar, para que una configuración que no puede
 * funcionar no llegue a la placa. */
#if CONFIG_FREERTOS_HZ < 100
#error "El tic debe ser de al menos 100 Hz: con menos, 10 ms son 0 tics"
#endif
#if CUERPO_LENTO_MS >= PERIODO_MS
#error "El cuerpo lento debe durar menos que el periodo: si no, no cabe en el"
#endif

/* Cuántas teclas caben entre «teclado» y «muestreo», y cuántos mensajes entre
 * «muestreo» e «impresora». */
#define LONGITUD_TECLAS     8
#define LONGITUD_MENSAJES   8

/* La atenuación con la que arranca: la máxima, la de la sesión. */
#define ATENUACION_INICIAL  ADC_ATTEN_DB_12

typedef enum {
    ESPERA_ABSOLUTA,  /* xTaskDelayUntil(): el período no depende del bucle */
    ESPERA_RELATIVA,  /* vTaskDelay(): el período es la espera más el bucle */
} espera_t;

/* Lo que se puede cambiar desde el teclado. Solo lo toca «muestreo». */
typedef struct {
    adc_atten_t   atten;
    bool          calibrar;  /* escribir los milivoltios calibrados */
    filtro_tipo_t filtro;
    int           ventana;
    espera_t      espera;
    bool          lento;     /* cuerpo de bucle lento */
} ajustes_t;

/* Acumuladores de la medida de dispersión. */
typedef struct {
    int     pedidas;    /* 0: no se está midiendo */
    int     n;
    int     min, max;
    int64_t suma;
    int64_t suma_cuadrados;
} acumulador_t;

typedef struct {
    int    n;
    int    min, max;
    double media, desviacion;
} estadistica_t;

/* Lo que «muestreo» le manda a «impresora». */
typedef enum {
    MENSAJE_MUESTRA,
    MENSAJE_AJUSTES,
    MENSAJE_DISPERSION,
    MENSAJE_AVISO,
    MENSAJE_ERROR,
} tipo_mensaje_t;

typedef struct {
    tipo_mensaje_t tipo;
    ajustes_t      ajustes;
    bool           hay_cali;    /* existe el esquema de calibración */
    /* MENSAJE_MUESTRA */
    int            cuentas;
    bool           hay_mv;
    int            mv;
    int            filtrado;
    int64_t        periodo_us;  /* 0 en la primera muestra */
    /* MENSAJE_DISPERSION */
    estadistica_t  disp_cuentas;
    bool           disp_hay_mv;
    estadistica_t  disp_mv;
    /* MENSAJE_AVISO */
    const char    *aviso;       /* siempre un literal: no hay que liberarlo */
    /* MENSAJE_ERROR */
    esp_err_t      error;
    /* Siempre: los mensajes que no cupieron en la cola de la impresora, y las
     * vueltas en que el bucle llegó tarde al instante que le tocaba. */
    uint32_t       perdidos;
    uint32_t       incumplidos;
} mensaje_t;

static QueueHandle_t s_teclas;    /* de «teclado» a «muestreo» */
static QueueHandle_t s_mensajes;  /* de «muestreo» a «impresora» */

static lectura_adc_t s_lectura;

/* ------------------------------------------------------------------------
 * Pequeñas utilidades.
 * ------------------------------------------------------------------------ */

static void acumulador_empezar(acumulador_t *a, int pedidas)
{
    *a = (acumulador_t){ .pedidas = pedidas, .min = INT32_MAX, .max = INT32_MIN };
}

static void acumulador_anadir(acumulador_t *a, int x)
{
    a->n++;
    a->suma += x;
    a->suma_cuadrados += (int64_t)x * x;
    if (x < a->min) a->min = x;
    if (x > a->max) a->max = x;
}

/* Media y desviación típica muestral, con n - 1. */
static estadistica_t acumulador_resultado(const acumulador_t *a)
{
    estadistica_t e = { .n = a->n, .min = a->min, .max = a->max };
    if (a->n > 0) {
        e.media = (double)a->suma / a->n;
    }
    if (a->n > 1) {
        const double var = ((double)a->suma_cuadrados
                            - (double)a->suma * a->suma / a->n) / (a->n - 1);
        e.desviacion = (var > 0.0) ? sqrt(var) : 0.0;
    }
    return e;
}

/* Atenuación en texto, sin coma flotante: «0», «2.5», «6» o «12». */
static const char *texto_atenuacion(adc_atten_t atten)
{
    switch (atten) {
    case ADC_ATTEN_DB_0:   return "0";
    case ADC_ATTEN_DB_2_5: return "2.5";
    case ADC_ATTEN_DB_6:   return "6";
    case ADC_ATTEN_DB_12:  return "12";
    default:               return "?";
    }
}

/* ------------------------------------------------------------------------
 * La tarea «muestreo».
 * ------------------------------------------------------------------------ */

typedef struct {
    ajustes_t    ajustes;
    filtro_t     filtro;
    acumulador_t disp_cuentas;
    acumulador_t disp_mv;
    bool         disp_mv_valida;  /* todas las muestras tenían milivoltios */
    TickType_t   referencia;      /* de xTaskDelayUntil() */
    uint32_t     perdidos;
    uint32_t     incumplidos;
} estado_t;

static estado_t s_estado;

/* Manda un mensaje a «impresora» sin esperar: si su cola está llena, el
 * mensaje se pierde y se cuenta, pero «muestreo» no se detiene por la
 * consola. */
static void enviar(estado_t *s, mensaje_t *m)
{
    m->ajustes = s->ajustes;
    m->hay_cali = (s_lectura.cali != NULL);
    m->perdidos = s->perdidos;
    m->incumplidos = s->incumplidos;
    if (xQueueSend(s_mensajes, m, 0) != pdTRUE) {
        s->perdidos++;
    }
}

static void avisar_ajustes(estado_t *s)
{
    mensaje_t m = { .tipo = MENSAJE_AJUSTES };
    enviar(s, &m);
}

static void avisar(estado_t *s, const char *aviso)
{
    mensaje_t m = { .tipo = MENSAJE_AVISO, .aviso = aviso };
    enviar(s, &m);
}

static void avisar_error(estado_t *s, esp_err_t err)
{
    mensaje_t m = { .tipo = MENSAJE_ERROR, .error = err };
    enviar(s, &m);
}

/* Aplica una tecla. Las que cambian lo que se mide vacían el filtro, porque
 * mezclar en la misma ventana valores de antes y de después del cambio daría
 * una salida que no es de ninguno de los dos. */
static void aplicar_tecla(estado_t *s, char tecla)
{
    ajustes_t *a = &s->ajustes;
    bool vaciar_filtro = true;

    switch (tecla) {
    case '0': case '1': case '2': case '3': {
        static const adc_atten_t ATENUACIONES[] = {
            ADC_ATTEN_DB_0, ADC_ATTEN_DB_2_5, ADC_ATTEN_DB_6, ADC_ATTEN_DB_12,
        };
        const esp_err_t err =
            lectura_adc_atenuacion(&s_lectura, ATENUACIONES[tecla - '0']);
        if (err != ESP_OK) {
            /* lectura_adc_atenuacion() ha dejado la atenuación de antes. */
            avisar_error(s, err);
            return;
        }
        a->atten = ATENUACIONES[tecla - '0'];
        /* Una dispersión medida a medias con dos atenuaciones no es la de
         * ninguna de las dos: se cancela. */
        if (s->disp_cuentas.pedidas > 0) {
            s->disp_cuentas.pedidas = 0;
            avisar(s, "medida de dispersion cancelada: ha cambiado la atenuacion");
        }
        break;
    }
    case 'c': a->calibrar = !a->calibrar;                   break;
    case 'x': a->filtro = FILTRO_NINGUNO;                   break;
    case 'm': a->filtro = FILTRO_MEDIA;                     break;
    case 'd': a->filtro = FILTRO_MEDIANA;                   break;
    case '+':
        a->ventana = (a->ventana * 2 > FILTRO_VENTANA_MAX) ? FILTRO_VENTANA_MAX
                                                          : a->ventana * 2;
        break;
    case '-':
        a->ventana = (a->ventana / 2 < 2) ? 2 : a->ventana / 2;
        break;
    case 'a':
        /* La referencia se toma ahora: si se dejara la de antes, después de
         * un rato con espera relativa xTaskDelayUntil() intentaría recuperar
         * de golpe los instantes que «se han perdido». */
        a->espera = ESPERA_ABSOLUTA;
        s->referencia = xTaskGetTickCount();
        vaciar_filtro = false;
        break;
    case 'r': a->espera = ESPERA_RELATIVA; vaciar_filtro = false; break;
    case 'l':
        /* También se toma la referencia, para que el cambio de duración del
         * bucle empiece a contar desde ahora. */
        a->lento = !a->lento;
        s->referencia = xTaskGetTickCount();
        vaciar_filtro = false;
        break;
    case 's':
        acumulador_empezar(&s->disp_cuentas, MUESTRAS_DISPERSION);
        acumulador_empezar(&s->disp_mv, MUESTRAS_DISPERSION);
        s->disp_mv_valida = true;
        avisar(s, "midiendo la dispersion: deja quieto el potenciometro");
        return;  /* no cambia los ajustes: no se escriben */
    default:
        return;
    }

    /* Sin esquema de calibración no hay milivoltios: se sigue en cuentas. */
    if (a->calibrar && s_lectura.cali == NULL) {
        a->calibrar = false;
    }
    if (vaciar_filtro) {
        filtro_iniciar(&s->filtro, a->filtro, a->ventana);
        a->ventana = s->filtro.ventana;  /* la que el filtro ha aceptado */
    }
    avisar_ajustes(s);
}

static void tarea_muestreo(void *argumento)
{
    (void)argumento;
    estado_t *s = &s_estado;
    int64_t anterior_us = 0;

    filtro_iniciar(&s->filtro, s->ajustes.filtro, s->ajustes.ventana);
    s->ajustes.ventana = s->filtro.ventana;
    s->referencia = xTaskGetTickCount();
    avisar_ajustes(s);

    for (;;) {
        /* El instante de la muestra, y el período real: el tiempo desde la
         * muestra anterior, medido con el reloj del sistema en microsegundos,
         * que no depende del tic. Se toma lo primero, al despertar, para que
         * no le sume lo que tarden las teclas. */
        const int64_t ahora_us = esp_timer_get_time();
        const int64_t periodo_us = (anterior_us != 0) ? ahora_us - anterior_us : 0;
        anterior_us = ahora_us;

        char tecla;
        while (xQueueReceive(s_teclas, &tecla, 0) == pdTRUE) {
            aplicar_tecla(s, tecla);
        }

        int cuentas = 0;
        const esp_err_t err = lectura_adc_leer(&s_lectura, &cuentas);
        if (err != ESP_OK) {
            /* El retorno se comprueba: una lectura fallida no es una medida. */
            avisar_error(s, err);
        } else {
            mensaje_t m = { .tipo = MENSAJE_MUESTRA, .cuentas = cuentas,
                            .periodo_us = periodo_us };
            int valor = cuentas;
            if (s->ajustes.calibrar &&
                lectura_adc_a_mv(&s_lectura, cuentas, &m.mv) == ESP_OK) {
                m.hay_mv = true;
                valor = m.mv;
            }
            m.filtrado = filtro_aplicar(&s->filtro, valor);

            if (s->disp_cuentas.pedidas > 0) {
                acumulador_anadir(&s->disp_cuentas, cuentas);
                if (m.hay_mv) {
                    acumulador_anadir(&s->disp_mv, m.mv);
                } else {
                    s->disp_mv_valida = false;
                }
                if (s->disp_cuentas.n == s->disp_cuentas.pedidas) {
                    mensaje_t d = { .tipo = MENSAJE_DISPERSION };
                    d.disp_cuentas = acumulador_resultado(&s->disp_cuentas);
                    d.disp_hay_mv = s->disp_mv_valida;
                    if (d.disp_hay_mv) {
                        d.disp_mv = acumulador_resultado(&s->disp_mv);
                    }
                    s->disp_cuentas.pedidas = 0;
                    enviar(s, &d);
                }
            }

            /* El cuerpo lento: trabajo de CPU que tarda, como un procesado
             * pesado. Es una espera activa a propósito: ocupa la CPU, que es
             * lo que hace un cálculo, y no cede el turno. */
            if (s->ajustes.lento) {
                esp_rom_delay_us(CUERPO_LENTO_MS * 1000U);
            }

            enviar(s, &m);
        }

        if (s->ajustes.espera == ESPERA_ABSOLUTA) {
            /* Devuelve pdFALSE si el instante que tocaba ya había pasado: la
             * vuelta ha durado más que el período y no ha esperado nada. Se
             * cuenta, y la referencia se vuelve a tomar para no intentar
             * recuperar los instantes perdidos de golpe. */
            if (xTaskDelayUntil(&s->referencia, pdMS_TO_TICKS(PERIODO_MS)) == pdFALSE) {
                s->incumplidos++;
                s->referencia = xTaskGetTickCount();
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(PERIODO_MS));
        }
    }
}

/* ------------------------------------------------------------------------
 * La tarea «impresora».
 * ------------------------------------------------------------------------ */

/* Escribe en linea a partir de la posición n, sin pasarse del tamaño, y
 * devuelve la nueva posición. Si no cabe, la línea se queda cortada. */
static size_t anadir(char *linea, size_t tam, size_t n, const char *formato, ...)
{
    if (n >= tam) {
        return n;
    }
    va_list args;
    va_start(args, formato);
    const int escritos = vsnprintf(linea + n, tam - n, formato, args);
    va_end(args);
    if (escritos < 0) {
        return n;
    }
    n += (size_t)escritos;
    return (n < tam) ? n : tam - 1;
}

static void imprimir_estadistica(const char *unidad, const estadistica_t *e)
{
    ESP_LOGI(TAG, "    en %s: minimo %d, maximo %d, rango %d, media %.1f, "
                  "desviacion tipica %.1f",
             unidad, e->min, e->max, e->max - e->min, e->media, e->desviacion);
}

static void tarea_impresora(void *argumento)
{
    (void)argumento;
    mensaje_t m;
    uint32_t perdidos_avisados = 0;
    uint32_t incumplidos_avisados = 0;
    const int tope = lectura_adc_tope();

    for (;;) {
        if (xQueueReceive(s_mensajes, &m, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        const ajustes_t *a = &m.ajustes;

        switch (m.tipo) {
        case MENSAJE_AJUSTES:
            ESP_LOGI(TAG, "--- atenuacion %s dB | milivoltios: %s | filtro: %s",
                     texto_atenuacion(a->atten),
                     a->calibrar ? "si"
                                 : (m.hay_cali ? "no" : "no, chip sin datos de calibracion"),
                     filtro_nombre(a->filtro));
            ESP_LOGI(TAG, "--- ventana N=%d | espera: %s | cuerpo lento: %s",
                     a->ventana,
                     (a->espera == ESPERA_ABSOLUTA)
                         ? "hasta instante absoluto, xTaskDelayUntil()"
                         : "relativa, vTaskDelay()",
                     a->lento ? "si" : "no");
            break;

        case MENSAJE_MUESTRA: {
            /* La línea se compone por partes. Con 160 caracteres sobra, y aun
             * así cada parte se escribe solo en el espacio que queda. */
            char linea[160];
            size_t n = 0;
            n = anadir(linea, sizeof(linea), n, "%4d cuentas", m.cuentas);
            if (m.hay_mv) {
                n = anadir(linea, sizeof(linea), n, " = %4d mV", m.mv);
            }
            if (m.cuentas >= tope) {
                n = anadir(linea, sizeof(linea), n, " (tope de escala)");
            }
            if (a->filtro != FILTRO_NINGUNO) {
                n = anadir(linea, sizeof(linea), n, " | %s N=%d: %d %s",
                           filtro_nombre(a->filtro), a->ventana, m.filtrado,
                           m.hay_mv ? "mV" : "cuentas");
            }
            if (m.periodo_us > 0) {
                /* En décimas de milisegundo, redondeado. */
                const int64_t decimas = (m.periodo_us + 50) / 100;
                n = anadir(linea, sizeof(linea), n,
                           " | periodo %" PRId64 ".%" PRId64 " ms",
                           decimas / 10, decimas % 10);
            }
            ESP_LOGI(TAG, "%s", linea);
            break;
        }

        case MENSAJE_DISPERSION:
            ESP_LOGI(TAG, "dispersion de %d muestras seguidas, atenuacion %s dB:",
                     m.disp_cuentas.n, texto_atenuacion(a->atten));
            imprimir_estadistica("cuentas", &m.disp_cuentas);
            if (m.disp_hay_mv) {
                imprimir_estadistica("mV", &m.disp_mv);
            }
            break;

        case MENSAJE_AVISO:
            ESP_LOGI(TAG, "--- %s", m.aviso);
            break;

        case MENSAJE_ERROR:
            ESP_LOGE(TAG, "error del ADC: %s", esp_err_to_name(m.error));
            break;
        }

        if (m.perdidos != perdidos_avisados) {
            ESP_LOGW(TAG, "%" PRIu32 " lineas de traza perdidas por cola llena",
                     m.perdidos - perdidos_avisados);
            perdidos_avisados = m.perdidos;
        }
        if (m.incumplidos != incumplidos_avisados) {
            ESP_LOGW(TAG, "%" PRIu32 " periodos incumplidos: la vuelta del bucle "
                          "ha durado mas que el periodo",
                     m.incumplidos - incumplidos_avisados);
            incumplidos_avisados = m.incumplidos;
        }
    }
}

/* ------------------------------------------------------------------------
 * La tarea «teclado».
 * ------------------------------------------------------------------------ */

static void mostrar_ayuda(void)
{
    printf("\n"
           "Teclas (con el monitor abierto, sin pulsar Intro):\n"
           "  0 1 2 3  atenuacion de 0, 2.5, 6 o 12 dB\n"
           "  c        milivoltios calibrados: si o no\n"
           "  x        sin filtro\n"
           "  m        media movil\n"
           "  d        mediana\n"
           "  + -      ventana N del filtro: el doble o la mitad, entre 2 y %d\n"
           "  s        dispersion de las %d muestras siguientes\n"
           "  a        espera hasta instante absoluto, xTaskDelayUntil()\n"
           "  r        espera relativa, vTaskDelay()\n"
           "  l        cuerpo de bucle lento, %d ms: si o no\n"
           "  h ?      esta ayuda\n"
           "Periodo de muestreo: %d ms. Los cambios de atenuacion, de\n"
           "milivoltios, de tipo de filtro y de ventana vacian el filtro.\n\n",
           FILTRO_VENTANA_MAX, MUESTRAS_DISPERSION, CUERPO_LENTO_MS, PERIODO_MS);
}

static void tarea_teclado(void *argumento)
{
    (void)argumento;
    for (;;) {
        /* Mientras no se instale el controlador de la UART, y aquí no se
         * instala, read() sobre la consola no se queda esperando: si no ha
         * llegado nada, devuelve -1 enseguida. Por eso se consulta cada
         * 20 ms. */
        char tecla;
        if (read(STDIN_FILENO, &tecla, 1) == 1) {
            if (tecla == 'h' || tecla == '?') {
                mostrar_ayuda();
            } else {
                xQueueSend(s_teclas, &tecla, 0);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/* ------------------------------------------------------------------------
 * Arranque.
 * ------------------------------------------------------------------------ */

void app_main(void)
{
    ESP_LOGI(TAG, "SEyTR - adquisicion analogica. Tic a %d Hz", configTICK_RATE_HZ);

    if (configTICK_RATE_HZ != 1000) {
        ESP_LOGW(TAG, "el tic no esta a 1000 Hz: el periodo se ajusta en "
                      "multiplos de %d ms", 1000 / configTICK_RATE_HZ);
    }

    if (PIN_POTENCIOMETRO == GPIO_NUM_NC) {
        ESP_LOGE(TAG, "no hay pin para el potenciometro: eligelo en main/pines.h. "
                      "Sin senal no hay nada que medir");
        return;
    }

    const esp_err_t err =
        lectura_adc_iniciar(&s_lectura, PIN_POTENCIOMETRO, ATENUACION_INICIAL);
    if (err == ESP_ERR_NOT_FOUND) {
        ESP_LOGE(TAG, "el GPIO %d no tiene canal de ADC: elige otro en main/pines.h",
                 PIN_POTENCIOMETRO);
        return;
    }
    if (err == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGE(TAG, "el GPIO %d es de la segunda unidad de ADC: elige uno de la "
                      "primera en main/pines.h", PIN_POTENCIOMETRO);
        return;
    }
    ESP_ERROR_CHECK(err);

    ESP_LOGI(TAG, "GPIO %d: unidad %d, canal %d. Tope de escala: %d cuentas",
             s_lectura.pin, (int)s_lectura.unidad + 1, (int)s_lectura.canal,
             lectura_adc_tope());
    ESP_LOGI(TAG, "esquema de calibracion por ajuste de curva: %s",
             (s_lectura.cali != NULL) ? "creado" : "no disponible en este chip");

    s_estado.ajustes = (ajustes_t){
        .atten = ATENUACION_INICIAL,
        .calibrar = false,
        .filtro = FILTRO_NINGUNO,
        .ventana = VENTANA_INICIAL,
        .espera = ESPERA_ABSOLUTA,
        .lento = false,
    };

    s_teclas = xQueueCreate(LONGITUD_TECLAS, sizeof(char));
    s_mensajes = xQueueCreate(LONGITUD_MENSAJES, sizeof(mensaje_t));
    if (s_teclas == NULL || s_mensajes == NULL) {
        ESP_LOGE(TAG, "no hay memoria para las colas");
        return;
    }

    mostrar_ayuda();

    /* «muestreo» tiene la prioridad más alta de las tres, para que ni la
     * consola ni el teclado retrasen la lectura. */
    const BaseType_t impresora =
        xTaskCreate(tarea_impresora, "impresora", 4096, NULL, 3, NULL);
    const BaseType_t muestreo =
        xTaskCreate(tarea_muestreo, "muestreo", 4096, NULL, 10, NULL);
    const BaseType_t teclado =
        xTaskCreate(tarea_teclado, "teclado", 3072, NULL, 5, NULL);
    if (impresora != pdPASS || muestreo != pdPASS || teclado != pdPASS) {
        ESP_LOGE(TAG, "no hay memoria para las tareas");
        return;
    }

    /* app_main termina aquí; las tres tareas siguen. La unidad y el esquema
     * de calibración no se liberan porque el programa no termina: en uno que
     * deja de medir, se liberan con lectura_adc_liberar(). */
}
