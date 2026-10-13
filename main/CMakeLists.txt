# CMakeLists.txt de componente. Registra las fuentes y las cabeceras publicas.
# Si anades un fichero .c, anadelo tambien a SRCS.
# PRIV_REQUIRES dice de que componentes depende este: los dos del propio
# proyecto; esp_timer, que da el reloj del sistema en microsegundos con el que
# se mide el periodo real; esp_rom, por la espera activa del cuerpo lento; y
# esp_driver_gpio, por GPIO_NUM_NC en pines.h.
idf_component_register(SRCS "adquisicion_analogica.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES lectura_adc filtro esp_timer esp_rom esp_driver_gpio)
