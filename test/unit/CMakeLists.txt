idf_component_register(
    SRCS "test_config_store.c" "test_wifi_fsm.c" "test_http_validation.c"
    INCLUDE_DIRS "."
    REQUIRES unity config_store wifi_manager web_server
)
