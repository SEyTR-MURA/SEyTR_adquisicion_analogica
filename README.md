# SEyTR · Adquisición analógica

Proyecto de ejemplo de **Sistemas Empotrados y de Tiempo Real**. Es el de la
demostración de la sesión **U2-S4**, ADC y adquisición de señal: un
potenciómetro leído con el conversor analógico-digital en modo puntual, a
período fijo, con la cuenta cruda, los milivoltios calibrados, el valor
filtrado y el período real en la misma línea de traza.

No hace falta cuenta ni permisos para clonarlo: el repositorio es público.

El trabajo propuesto de la sesión te pide escribir **tu propio** programa de
muestreo y hacer tres análisis con él: dispersión, exactitud y filtro. Este
proyecto no lo sustituye: sirve para repetir la demostración y para comparar
tus medidas con las suyas.

## Qué hace

Lee el pin del potenciómetro cada 100 ms, esperando hasta instante absoluto, y
escribe una línea por muestra. Mientras corre, eliges con una tecla, sin
recompilar:

- **La atenuación del canal**, entre las cuatro que admite el ESP32-S3. Al
  cambiarla, el programa vuelve a configurar el canal y vuelve a crear el
  esquema de calibración, porque el esquema depende de la atenuación.
- **Los milivoltios calibrados**, que se escriben en la misma línea que la
  cuenta cruda. El programa crea al arrancar el esquema de calibración por
  ajuste de curva, el único que admite el ESP32-S3. Si el chip no trae los
  datos de calibración de fábrica, lo dice y sigue en cuentas crudas.
- **El filtro**: ninguno, media móvil o mediana, con su ventana N.
- **La medida de la dispersión**: mínimo, máximo, media y desviación típica de
  las cien muestras siguientes.
- **La espera del bucle**: hasta instante absoluto, con `xTaskDelayUntil()`, o
  relativa, con `vTaskDelay()`, con y sin un cuerpo de bucle lento de 20 ms.

## Qué necesitas antes

El entorno instalado y verificado con la guía de instalación de la asignatura,
la placa y su cable USB de datos, conectado al conector **USB-a-UART** de la
placa, el del puente USB-serie («USB-to-UART Port» en la guía del fabricante).
Por el otro conector, el USB nativo del ESP32-S3, verás las trazas, pero las
teclas no le llegan al programa. Conecta solo el cable del conector USB-a-UART.

Un **potenciómetro lineal de 10 kΩ**, montado con la placa desconectada: sus
dos extremos, uno a 3V3 y otro a masa, y el cursor, la patilla central, al pin
que elijas. No lleva nada más.

Para contrastar la medida calibrada, un **multímetro** entre el cursor y masa.

## Cómo obtenerlo

Clona el repositorio en una carpeta de tu equipo con una ruta **corta y sin
espacios ni tildes**, y abre en el editor la carpeta que contiene el
`CMakeLists.txt` de la raíz. Si abres una carpeta por encima o por debajo, el
entorno no reconoce el proyecto y el error no te dirá eso.

## Antes de compilar: el pin

El pin se declara en un solo sitio, `main/pines.h`, y el repositorio llega
**sin pin elegido**, porque elegirlo es parte del trabajo. Sin señal no hay
nada que medir: mientras no elijas el pin, el programa avisa al arrancar y no
hace nada más.

Tiene que ser un pin con canal de la **primera unidad de ADC**. La segunda
unidad la comparte la radio, y con la conectividad inalámbrica activa una
lectura sobre ella puede fallar. La tabla de pines de la hoja de
características del ESP32-S3 dice qué pines tienen canal y de qué unidad.
Descarta además los de arranque y los que tu placa ya usa, y comprueba en su
esquema que el pin sale a la tira. Después escribe su número en
`PIN_POTENCIOMETRO` y anótalo en esta tabla:

| Señal | GPIO | Montaje |
| --- | --- | --- |
| Cursor del potenciómetro | *sin elegir* | Extremos a 3V3 y a masa |

Si eliges un pin sin canal de ADC o de la segunda unidad, el programa lo dice
al arrancar y no sigue.

## Cómo se usa

Compila, graba y abre el monitor. Al arrancar, el programa escribe en qué
unidad y en qué canal está el pin, el tope de la escala, si ha podido crear el
esquema de calibración y la lista de teclas. Empieza con la atenuación máxima,
sin milivoltios y sin filtro.

Si el chip de tu placa no trae los datos de calibración de fábrica, lo verás
antes de esa lista. ESP-IDF escribe, antes de que empiece el programa, ocho
veces la línea

```
W (...) eFuse: calibration efuse version does not match, set default version to 0
```

y, cuando el programa intenta crear el esquema, otra igual y esta:

```
E (...) adc_cali: adc_cali_create_scheme_curve_fitting(80): Calibration required eFuse bits not burnt
```

Después el programa escribe «esquema de calibracion por ajuste de curva: no
disponible en este chip» y sigue en cuentas crudas: la tecla `c` no da
milivoltios. No es un fallo del programa: es el caso que el programa trata. Con
los datos de fábrica presentes, ninguna de esas líneas sale.

| Tecla | Qué hace |
| --- | --- |
| `0` `1` `2` `3` | Atenuación de 0, 2,5, 6 o 12 dB |
| `c` | Escribe o deja de escribir los milivoltios calibrados |
| `x` | Sin filtro |
| `m` | Media móvil |
| `d` | Mediana |
| `+` `-` | Ventana N del filtro: el doble o la mitad, entre 2 y 64 |
| `s` | Dispersión de las cien muestras siguientes. Si cambias de atenuación a mitad, se cancela |
| `a` | Espera hasta instante absoluto, `xTaskDelayUntil()` |
| `r` | Espera relativa, `vTaskDelay()` |
| `l` | Cuerpo de bucle lento, 20 ms: sí o no |
| `h` `?` | Vuelve a escribir la lista |

Las teclas se pulsan sin Intro. Los cambios de atenuación, de milivoltios, de
tipo de filtro y de ventana vacían el filtro: mezclar en la misma ventana valores de antes y de
después del cambio daría una salida que no es de ninguno de los dos.

## Qué deberías ver

Cada tecla que cambia un ajuste escribe dos líneas con lo que está en marcha,
empezando por `---`; `s` escribe una, al empezar a medir. Después, una línea
por muestra. Las cifras de los ejemplos son
ilustrativas: las de tu placa serán otras.

```
I (12345) adc: 2051 cuentas | periodo 100.0 ms
```

**La cuenta cruda.** Gira el potenciómetro de un extremo al otro: la cuenta va
de 0 al tope de la escala, que el programa escribe al arrancar. Lo que sale es
un número, no una tensión.

**La atenuación.** Cambia de atenuación con `0` a `3` y repite el recorrido.
Con poca atenuación el rango medible es estrecho: la cuenta llega antes al
tope y se queda ahí el resto del recorrido. La línea lo dice:

```
I (23456) adc: 4095 cuentas (tope de escala) | periodo 100.0 ms
```

Es la **saturación**: la tensión del pin se sale del rango de la atenuación
elegida, y la cuenta ya no responde al giro. Con la atenuación máxima pasa lo
mismo cerca del final del recorrido, porque el rango medible termina por debajo
de los 3,3 V de la alimentación. Gira despacio hasta que aparezca el tope y
mide con el multímetro la tensión del cursor en ese punto: es el final del
rango en tu placa. Con la atenuación ya al máximo, para medir más arriba hay
que dividir la tensión antes de que entre.

**Los milivoltios.** Con `c`, cada línea lleva también los milivoltios
calibrados:

```
I (34567) adc: 2051 cuentas = 1650 mV | periodo 100.0 ms
```

Compáralos con el multímetro en el tramo medio del recorrido, lejos del tope.
Si la diferencia se mantiene igual al repetir la medida, es sistemática, y un
filtro no la corrige.

**La dispersión.** Deja el potenciómetro quieto y pulsa `s`. Al cabo de cien
muestras, diez segundos, el programa escribe el resumen:

```
I (45678) adc: dispersion de 100 muestras seguidas, atenuacion 12 dB:
I (45678) adc:     en cuentas: minimo 2040, maximo 2063, rango 23, media 2051.4, desviacion tipica 4.2
I (45678) adc:     en mV: minimo 1641, maximo 1659, rango 18, media 1650.3, desviacion tipica 3.4
```

La línea en mV solo sale si los milivoltios han estado activos, con `c`,
durante toda la medida. Repítelo con otra atenuación y compara. La dispersión es la **precisión**: lo
que se parecen entre sí las lecturas. Lo que se parecen al multímetro es la
**exactitud**, y es otra cosa.

**El filtro.** Con `m`, la línea añade la salida de la media móvil:

```
I (56789) adc: 2051 cuentas = 1650 mV | media N=8: 1648 mV | periodo 100.0 ms
```

Con el potenciómetro quieto, la salida filtrada salta menos que la medida.
Gíralo de golpe y mira cuántas líneas tarda la salida filtrada en llegar al
valor nuevo: con N muestras y un período de 100 ms, la media tarda N muestras
en dejar atrás las antiguas. Sube la ventana con `+` y repite: el ruido baja y
el retardo crece. Con `d`, la mediana: un valor suelto muy distinto de los
demás no la mueve, y a la media sí.

**La espera.** El período de cada línea es el tiempo real entre esa muestra y
la anterior, medido con el reloj del sistema en microsegundos. Con `l`, cada
vuelta del bucle trabaja 20 ms más. Con la espera hasta instante absoluto, la
de partida, el período sigue en 100 ms. Pulsa `r`: con la espera relativa, el
período pasa a ser la espera más lo que tarda el bucle, unos 120 ms. Vuelve con
`a`. Si alguna vez una vuelta del bucle dura más que el período, con la espera
hasta instante absoluto, el programa lo avisa con una línea de «periodos
incumplidos».

Si el monitor muestra las trazas pero no responde a las teclas, mira el
conector: tiene que ser el USB-a-UART.

## Qué puedes cambiar sin tocar el código

En la configuración del proyecto, en el menú **«SEyTR - Adquisicion
analogica»**, que está en el nivel superior, junto a los de ESP-IDF, y no
dentro de la configuración de componentes:

| Opción | Qué hace |
| --- | --- |
| Periodo de muestreo | Intervalo entre dos muestras, en milisegundos. Parte de 100 |
| Ventana inicial de los filtros | La N con la que arrancan la media y la mediana. Parte de 8 |
| Duración del cuerpo de bucle lento | El trabajo que añade la tecla `l`, en milisegundos. Parte de 20. Tiene que ser menor que el período: si no, el proyecto no compila |
| Muestras para medir la dispersión | Cuántas toma la tecla `s`. Parte de 100 |

El proyecto fija el tic del sistema operativo a 1000 Hz en
`sdkconfig.defaults`, como todos los de la asignatura. Si el programa avisa al
arrancar de que el tic no está a 1000 Hz, tu `sdkconfig` trae otro valor: borra
el fichero `sdkconfig` y vuelve a fijar el destino, para que se genere de nuevo
a partir de `sdkconfig.defaults`.

## Estructura

```
CMakeLists.txt              define el proyecto
sdkconfig.defaults          configuración de partida; se escribe a mano y se versiona
main/
  CMakeLists.txt            registra las fuentes del componente principal
  Kconfig.projbuild         las opciones de la tabla de arriba
  pines.h                   el pin del potenciómetro, en un solo sitio
  adquisicion_analogica.c   el programa: el bucle de muestreo, las trazas y las teclas
components/
  lectura_adc/              componente propio: la unidad, el canal, la atenuación y la calibración
  filtro/                   componente propio: la media móvil y la mediana
```

El programa tiene tres tareas: una muestrea, otra escribe las trazas y otra lee
las teclas. La que muestrea no escribe nada en la consola, porque escribir es
lento y ese tiempo entraría en el cuerpo del bucle y en el período que se
quiere medir: le pasa lo que hay que escribir a la otra, que tiene menos
prioridad. Las tareas y sus prioridades se estudian en la U3, y las colas entre
ellas, en la U4.

El componente `lectura_adc` no sabe en qué pin está la señal: se lo dice `main`
al iniciarlo, y él deduce la unidad y el canal. El de `filtro` es C puro, sin
nada del SDK.

Hay dos cosas que **no** se versionan y que por eso no están aquí: el fichero
`sdkconfig`, que genera la herramienta a partir de `sdkconfig.defaults`, y la
carpeta `build/`.

## Documentación

- Controlador del ADC en modo puntual:
  [ADC Oneshot Mode Driver, ESP-IDF v6.1 para ESP32-S3](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/api-reference/peripherals/adc/adc_oneshot.html).
- Calibración:
  [ADC Calibration Driver, ESP-IDF v6.1 para ESP32-S3](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/api-reference/peripherals/adc/adc_calibration.html).
- Espera hasta instante absoluto y espera relativa:
  [FreeRTOS (IDF), ESP-IDF v6.1 para ESP32-S3](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/api-reference/system/freertos_idf.html).
- Pines con canal de ADC y características del conversor:
  [hoja de características del ESP32-S3](https://documentation.espressif.com/esp32-s3_datasheet_en.pdf).
  La documentación de la versión del curso remite a ella para el rango de cada
  atenuación. Como referencia, la
  [guía de ESP-IDF v4.4.6 para el ESP32-S3](https://docs.espressif.com/projects/esp-idf/en/v4.4.6/esp32s3/api-reference/peripherals/adc.html)
  da para la atenuación máxima un rango medible de 0 a 3100 mV.

## Un aviso

Este repositorio es material de la asignatura. El repositorio de tu **proyecto
final** es otro distinto, con su propio nombre y sus propias reglas, y llega más
adelante en el curso.
