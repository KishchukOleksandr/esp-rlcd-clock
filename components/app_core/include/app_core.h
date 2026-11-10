#ifndef APP_CORE_H
#define APP_CORE_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize all application subsystems (NVS config_store, HAL button, Wi-Fi AP+STA, Web Server).
 *
 * @return ESP_OK on success.
 */
esp_err_t app_core_init(void);

/**
 * @brief Log periodic runtime health and FreeRTOS stack high-water mark diagnostics.
 */
void app_core_log_diagnostics(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_CORE_H */
