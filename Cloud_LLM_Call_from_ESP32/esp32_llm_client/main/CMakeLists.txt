idf_component_register(SRCS "esp32_llm_client.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi esp_event nvs_flash esp_http_client json mbedtls)
