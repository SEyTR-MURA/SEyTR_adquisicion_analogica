/* filtro.c - media móvil y mediana sobre las N últimas muestras. */
#include "filtro.h"

#include <string.h>

void filtro_iniciar(filtro_t *f, filtro_tipo_t tipo, int ventana)
{
    if (ventana < 1) {
        ventana = 1;
    }
    if (ventana > FILTRO_VENTANA_MAX) {
        ventana = FILTRO_VENTANA_MAX;
    }
    memset(f, 0, sizeof(*f));
    f->tipo = tipo;
    f->ventana = ventana;
}

/* División entera redondeada al más cercano, también con negativos. */
static int dividir_redondeando(int64_t a, int b)
{
    return (int)((a >= 0) ? (a + b / 2) / b : (a - b / 2) / b);
}

/* La mediana de las que hay: se copian, se ordenan por inserción y se toma la
 * central. Con N par, la media de las dos centrales. Con N = 64 son unas pocas
 * miles de comparaciones, nada a diez muestras por segundo. */
static int mediana(const filtro_t *f)
{
    int v[FILTRO_VENTANA_MAX];
    const int n = f->cuantas;
    for (int i = 0; i < n; i++) {
        const int x = f->muestras[i];
        int j = i;
        while (j > 0 && v[j - 1] > x) {
            v[j] = v[j - 1];
            j--;
        }
        v[j] = x;
    }
    if (n % 2 == 1) {
        return v[n / 2];
    }
    return dividir_redondeando((int64_t)v[n / 2 - 1] + v[n / 2], 2);
}

int filtro_aplicar(filtro_t *f, int muestra)
{
    if (f->tipo == FILTRO_NINGUNO) {
        return muestra;
    }

    /* Si la ventana está llena, la muestra más antigua sale de la suma. */
    if (f->cuantas == f->ventana) {
        f->suma -= f->muestras[f->siguiente];
    } else {
        f->cuantas++;
    }
    f->muestras[f->siguiente] = muestra;
    f->suma += muestra;
    f->siguiente = (f->siguiente + 1) % f->ventana;

    if (f->tipo == FILTRO_MEDIA) {
        return dividir_redondeando(f->suma, f->cuantas);
    }
    return mediana(f);
}

const char *filtro_nombre(filtro_tipo_t tipo)
{
    switch (tipo) {
    case FILTRO_MEDIA:   return "media";
    case FILTRO_MEDIANA: return "mediana";
    default:             return "sin filtro";
    }
}
