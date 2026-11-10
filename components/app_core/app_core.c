#include "app_core.h"

#include "config_store.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal_button.h"
#include "web_server.h"
#include "wifi_manager.h"

static const char *TAG = "app_core";

static void on_boot_button_long_press(void *user_ctx)
{
    (void)user_ctx;
    ESP_LOGW(TAG, "Hardware BOOT button factory reset triggered (FR-012)!");
    esp_err_t err = wifi_manager_apply_factory_reset();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Factory reset via BOOT button failed: %s", esp_err_to_name(err));
    }
}

esp_err_t app_core_init(void)
{
    ESP_LOGI(TAG, "Starting ESP Weather Clock application core...");

    esp_err_t err = config_store_init();
    if (err != ESP_OK) {
        return err;
    }

    wifi_stored_config_t stored_cfg;
    err = config_store_load(&stored_cfg);
    if (err != ESP_OK) {
        return err;
    }

    err = wifi_manager_init(&stored_cfg);
    if (err != ESP_OK) {
        return err;
    }

    err = web_server_start();
    if (err != ESP_OK) {
        return err;
    }

    err = hal_button_init(HAL_BUTTON_BOOT_GPIO, HAL_BUTTON_FACTORY_RESET_MS,
                          &on_boot_button_long_press, NULL);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "BOOT button initialization warning: %s", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "ESP Weather Clock ready: SoftAP='%s' (http://%s/), mDNS=http://%s/",
             WIFI_AP_FIXED_SSID, WIFI_AP_DEFAULT_IP, WIFI_MDNS_FQDN);
    return ESP_OK;
}

void app_core_log_diagnostics(void)
{
    UBaseType_t stack_hwm = uxTaskGetStackHighWaterMark(NULL);
    wifi_runtime_status_t st = {0};
    wifi_manager_get_status(&st);

    ESP_LOGI(TAG, "Health: fsm=%s, sta_ip=%s, ap_clients=%u, free_heap=%lu B, min_heap=%lu B, stack_hwm=%u B",
             wifi_fsm_state_to_str(st.fsm_state),
             st.sta_ip,
             (unsigned)st.ap_client_count,
             (unsigned long)esp_get_free_heap_size(),
             (unsigned long)esp_get_minimum_free_heap_size(),
             (unsigned)(stack_hwm * sizeof(StackType_t)));
}
