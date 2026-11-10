#ifndef HAL_BUTTON_H
#define HAL_BUTTON_H

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Default BOOT button GPIO on Waveshare ESP32-C6-DEV-KIT-N8 (GPIO9, active LOW).
 */
#define HAL_BUTTON_BOOT_GPIO            9
#define HAL_BUTTON_FACTORY_RESET_MS     3000U

/**
 * @brief Callback invoked when the button has been held continuously for at least `hold_ms`.
 */
typedef void (*hal_button_long_press_cb_t)(void *user_ctx);

/**
 * @brief Initialize hardware button monitoring with periodic non-blocking timer debounce (FR-012).
 *
 * @param[in] gpio_num   GPIO pin number (e.g. HAL_BUTTON_BOOT_GPIO = 9).
 * @param[in] hold_ms    Required hold duration in milliseconds (e.g. 3000 ms).
 * @param[in] cb         Callback function triggered once per long-press event.
 * @param[in] user_ctx   Optional user context passed to callback.
 * @return ESP_OK on success.
 */
esp_err_t hal_button_init(int gpio_num, uint32_t hold_ms, hal_button_long_press_cb_t cb, void *user_ctx);

#ifdef __cplusplus
}
#endif

#endif /* HAL_BUTTON_H */
