idf_component_register(
    SRCS "geo_search.c" "geo_search_parser.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client esp-tls mbedtls cjson esp_timer lwip log config_store
)
