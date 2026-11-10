idf_component_register(
    SRCS "app_core.c"
    INCLUDE_DIRS "include"
    REQUIRES board_hal config_store wifi_manager web_server esp_timer log
)
