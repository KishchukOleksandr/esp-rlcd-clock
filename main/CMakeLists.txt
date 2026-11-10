idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES app_core nvs_flash esp_timer
)
