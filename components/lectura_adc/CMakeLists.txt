# REQUIRES lleva los componentes cuyas cabeceras aparecen en la cabecera
# publica de este: lectura_adc.h incluye las del ADC, que son de esp_adc.
idf_component_register(SRCS "lectura_adc.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_adc)
