idf_component_register(
  SRCS main.c app_state.c buzzer.c display.c fleet.c fusb302.c hw_i2c.c lines_hw.c loadtest.c net.c ota.c
       ota_cmd.c prov.c tester.c
  INCLUDE_DIRS .
  PRIV_REQUIRES katooni_core nvs_flash esp_driver_gpio esp_driver_i2c esp_driver_spi esp_driver_ledc
                esp_lcd esp_timer esp_wifi esp_netif esp_event esp_app_format app_update esp_http_client
                mqtt json mbedtls espcoredump
  EMBED_FILES assets/logo_240.rgb565
  # Amazon Root CA 1 (public), for IoT Core and the update download. The device's own certificate,
  # key and Wi-Fi are never in the image: they're in the prov partition (prov.h).
  EMBED_TXTFILES ${COMPONENT_DIR}/../AmazonRootCA1.pem
)
