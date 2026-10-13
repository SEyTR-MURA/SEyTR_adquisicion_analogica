/* filtro.h - filtros de media móvil y de mediana sobre las N últimas muestras.
 *
 * C puro, sin nada del SDK: se compila igual en la placa que en el ordenador,
 * y por eso se puede probar en el ordenador con muestras inventadas.
 */
#pragma once

#include <stdint.h>

/* Ventana máxima. La memoria de las muestras va dentro de la estructura, sin
 * reservas dinámicas, así que este número fija su tamaño. */
#define FILTRO_VENTANA_MAX 64

typedef enum {
    FILTRO_NINGUNO,  /* la salida es la entrada */
    FILTRO_MEDIA,    /* media de las N últimas muestras */
    FILTRO_MEDIANA,  /* valor central de las N últimas muestras */
} filtro_tipo_t;

typedef struct {
    filtro_tipo_t tipo;
    int     ventana;                     /* N, entre 1 y FILTRO_VENTANA_MAX */
    int     muestras[FILTRO_VENTANA_MAX]; /* las N últimas, en anillo */
    int     cuantas;                     /* cuántas hay ya, hasta N */
    int     siguiente;                   /* dónde va la próxima */
    int64_t suma;                        /* suma de las que hay, para la media */
} filtro_t;

/* Deja el filtro vacío, con su tipo y su ventana. Una ventana fuera de rango
 * se lleva al extremo más cercano. */
void filtro_iniciar(filtro_t *f, filtro_tipo_t tipo, int ventana);

/* Mete una muestra y devuelve la salida del filtro. Mientras no hay N
 * muestras, filtra con las que hay: justo después de iniciarlo, la primera
 * salida es la primera muestra. */
int filtro_aplicar(filtro_t *f, int muestra);

/* Nombre corto del tipo, para las trazas: "sin filtro", "media", "mediana". */
const char *filtro_nombre(filtro_tipo_t tipo);
