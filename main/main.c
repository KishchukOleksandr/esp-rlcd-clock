#include "app_core.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "main";

#define MAIN_LOOP_FEED_INTERVAL_MS   2000U
#define DIAG_LOG_EVERY_N_CYCLES      15U

void app_main(void)
{
    ESP_LOGI(TAG, "Waveshare ESP32-C6-DEV-KIT-N8 booting...");

    ESP_ERROR_CHECK(app_core_init());

    /* Subscribe main task to Task Watchdog Timer per Constitution Principle III */
    esp_err_t wdt_err = esp_task_wdt_add(NULL);
    if (wdt_err != ESP_OK && wdt_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "TWDT subscription note: %s", esp_err_to_name(wdt_err));
    }

    uint32_t cycle = 0U;
    while (1) {
        if (wdt_err == ESP_OK) {
            esp_task_wdt_reset();
        }
        vTaskDelay(pdMS_TO_TICKS(MAIN_LOOP_FEED_INTERVAL_MS));
        cycle++;
        if ((cycle % DIAG_LOG_EVERY_N_CYCLES) == 0U) {
            app_core_log_diagnostics();
        }
    }
}
