idf_component_register(
    SRCS "web_server.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server cjson esp_timer log wifi_manager config_store
    EMBED_TXTFILES "assets/index.html" "assets/style.css" "assets/app.js"
)
