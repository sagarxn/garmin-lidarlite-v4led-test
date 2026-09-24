set(web_files
    "index.html"
    "settings.html"
    "console.html"
    "style.css"
    "script.js"
    "favicon.ico"
    "logo.png"
    "icons/microchip.svg"
    "icons/setting.svg"
    "icons/schedule.svg"
    "icons/terminal.svg"
    "icons/laser.svg"
    "icons/wifi.svg"
    "icons/router.svg"
    "icons/reboot.svg"
    "icons/eye.svg"
    "icons/eye_slash.svg")

idf_component_register(SRCS "webserver.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver esp_netif esp_wifi esp_event esp_http_server json
                    EMBED_FILES "${web_files}")
