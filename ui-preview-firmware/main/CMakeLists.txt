idf_component_register(SRCS "main.c" "buttons.c" "relays.c" "voice.c" "voice_wire.c" "device_web.c" "cert_wire.c" "fonts/font20.c" "fonts/status_font.c" "fonts/EN_BIG.c" "fonts/ui_small.c" "fonts/ui_call.c" INCLUDE_DIRS "." REQUIRES esp_hw_support driver esp_timer lvgl lv_porting lvgl_esp32_drivers esp_wifi esp_netif esp_event esp_http_server esp_http_client nvs_flash lwip json mqtt opus mbedtls monocypher bootloader_support EMBED_TXTFILES "web.html")

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-deprecated-declarations)
