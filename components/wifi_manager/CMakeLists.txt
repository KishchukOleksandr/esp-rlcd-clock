idf_component_register(
    SRCS "wifi_manager.c" "captive_dns.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi esp_netif esp_event esp_timer lwip log config_store
    PRIV_REQUIRES mdns
)
