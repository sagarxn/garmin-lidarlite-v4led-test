set(sources
    "sensor.c"
    "lidarlite_v4led.c")

set(includes
    "include")

set(requires
    board)

idf_component_register(SRCS ${sources}
                       INCLUDE_DIRS ${includes}
                       REQUIRES ${requires})
