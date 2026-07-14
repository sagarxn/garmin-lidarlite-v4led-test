idf_component_register(SRCS "cm.c" "wifi.c" "cm_sntp.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_wifi esp_netif esp_event lwip setting)
