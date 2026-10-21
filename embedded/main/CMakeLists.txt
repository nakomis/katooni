idf_component_register(
  SRCS main.c app_state.c buzzer.c display.c fusb302.c hw_i2c.c lines_hw.c loadtest.c net.c tester.c
  INCLUDE_DIRS .
  PRIV_REQUIRES katooni_core nvs_flash esp_driver_gpio esp_driver_i2c esp_driver_spi esp_driver_ledc
                esp_lcd esp_timer esp_wifi esp_netif esp_event esp_app_format
  EMBED_FILES assets/logo_240.rgb565
)
