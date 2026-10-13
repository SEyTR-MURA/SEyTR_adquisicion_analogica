# Componente `filtro`

Dos filtros sobre las N últimas muestras: la **media móvil** y la **mediana**.
Es C puro, sin nada del SDK, así que se compila igual en la placa que en el
ordenador.

```c
#include "filtro.h"

static filtro_t f;

filtro_iniciar(&f, FILTRO_MEDIA, 8);       /* o FILTRO_MEDIANA, o FILTRO_NINGUNO */
int salida = filtro_aplicar(&f, muestra);  /* una vez por muestra */
```

- **N va de 1 a 64.** Las muestras se guardan dentro de la estructura, sin
  reservas dinámicas, y `FILTRO_VENTANA_MAX` fija su tamaño. Una ventana fuera
  de ese rango se lleva al extremo más cercano.
- **Mientras se llena, filtra con lo que hay.** Justo después de
  `filtro_iniciar()`, la primera salida es la primera muestra, y la media de N
  no está completa hasta la muestra N.
- **La media** lleva la suma de la ventana y, en cada muestra, resta la que
  sale y suma la que entra. El resultado se redondea al entero más cercano.
- **La mediana** ordena una copia de la ventana y toma el valor central; con N
  par, la media de los dos centrales. Un valor suelto muy distinto de los demás
  arrastra a la media durante toda la ventana, y a la mediana no.
- Para cambiar el tipo o la ventana, se vuelve a llamar a `filtro_iniciar()`,
  que deja el filtro vacío.

No depende de ningún componente.
