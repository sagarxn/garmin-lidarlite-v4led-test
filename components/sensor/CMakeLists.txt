set(sources
    "sensor.c"
    "lidarlite_v4led.c"
    "lidar_lite.c")

set(includes
    "include")

set(requires
    board
    storage
    wscada
    util)

idf_component_register(SRCS ${sources}
                       INCLUDE_DIRS ${includes}
                       REQUIRES ${requires})
