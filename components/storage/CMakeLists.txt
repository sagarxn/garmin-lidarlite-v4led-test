idf_component_register(SRCS "storage.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver nvs_flash sdmmc fatfs littlefs)
