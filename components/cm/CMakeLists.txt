idf_component_register(SRCS "cm.c" "wifi.c" "time_sync.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_wifi esp_netif esp_event lwip setting)
