if(IDF_TARGET STREQUAL "esp32s3")
    set(BOARD_SOURCES "boards/xingzhi/peripherals.c")
    set(BOARD_DEPS esp_driver_i2s esp_driver_usb_serial_jtag esp_lcd esp_adc)
elseif(IDF_TARGET STREQUAL "esp32")
    set(BOARD_SOURCES "battery.c" "side_leds.c")
    set(BOARD_DEPS led_strip lvgl_esp32_drivers)
else()
    message(FATAL_ERROR "Select a supported FMO board for this chip")
endif()
idf_component_register(SRCS "main.c" ${BOARD_SOURCES} "buttons.c" "relays.c" "discovery.c" "voice.c" "voice_wire.c" "device_web.c" "cert_wire.c" "fonts/font20.c" "fonts/history_font.c" "fonts/status_font.c" "fonts/EN_BIG.c" "fonts/ui_small.c" "fonts/ui_call.c" "fonts/ui_clock.c" "fonts/relay_font.c" INCLUDE_DIRS "." REQUIRES fmo_board ${BOARD_DEPS} esp_hw_support driver esp_timer lvgl lv_porting esp_wifi esp_netif esp_event esp_http_server esp_http_client nvs_flash lwip json mqtt opus mbedtls monocypher bootloader_support EMBED_TXTFILES "web.html")

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-deprecated-declarations)

# Optimize only real-time audio/DSP, leaving UI and transport build settings unchanged.
set_source_files_properties(voice.c voice_wire.c PROPERTIES COMPILE_OPTIONS "-O2")
