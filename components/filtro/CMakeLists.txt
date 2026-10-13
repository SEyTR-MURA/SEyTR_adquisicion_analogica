# CMakeLists.txt de componente. El filtro es C puro: no depende de ningun otro
# componente, y por eso no lleva REQUIRES ni PRIV_REQUIRES.
idf_component_register(SRCS "filtro.c"
                       INCLUDE_DIRS "include")
