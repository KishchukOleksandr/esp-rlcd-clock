#include "hal_button.h"

#include <stdbool.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "hal_button";

#define BUTTON_POLL_INTERVAL_MS     50U

typedef struct {
    int gpio_num;
    uint32_t hold_ms;
    uint32_t pressed_accum_ms;
    bool triggered_for_current_press;
    hal_button_long_press_cb_t callback;
    void *user_ctx;
    esp_timer_handle_t poll_timer;
} hal_button_state_t;

static hal_button_state_t s_btn_state = {0};

static void button_poll_timer_cb(void *arg)
{
    hal_button_state_t *st = (hal_button_state_t *)arg;
    if (st == NULL) {
        return;
    }

    /* Active LOW on GPIO9 (BOOT button on ESP32-C6-DEV-KIT-N8) */
    int level = gpio_get_level((gpio_num_t)st->gpio_num);
    if (level == 0) {
        if (!st->triggered_for_current_press) {
            st->pressed_accum_ms += BUTTON_POLL_INTERVAL_MS;
            if (st->pressed_accum_ms >= st->hold_ms) {
                st->triggered_for_current_press = true;
                ESP_LOGW(TAG, "BOOT button held for %u ms -> triggering long-press action",
                         (unsigned)st->pressed_accum_ms);
                if (st->callback != NULL) {
                    st->callback(st->user_ctx);
                }
            }
        }
    } else {
        st->pressed_accum_ms = 0U;
        st->triggered_for_current_press = false;
    }
}

esp_err_t hal_button_init(int gpio_num, uint32_t hold_ms, hal_button_long_press_cb_t cb, void *user_ctx)
{
    if (hold_ms < BUTTON_POLL_INTERVAL_MS) {
        return ESP_ERR_INVALID_ARG;
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << (unsigned)gpio_num),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO%d: %s", gpio_num, esp_err_to_name(err));
        return err;
    }

    s_btn_state.gpio_num = gpio_num;
    s_btn_state.hold_ms = hold_ms;
    s_btn_state.pressed_accum_ms = 0U;
    s_btn_state.triggered_for_current_press = false;
    s_btn_state.callback = cb;
    s_btn_state.user_ctx = user_ctx;

    const esp_timer_create_args_t timer_args = {
        .callback = &button_poll_timer_cb,
        .arg = &s_btn_state,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "hal_btn_poll",
    };

    err = esp_timer_create(&timer_args, &s_btn_state.poll_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create button poll timer: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_timer_start_periodic(s_btn_state.poll_timer, (uint64_t)BUTTON_POLL_INTERVAL_MS * 1000ULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start button poll timer: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Initialized BOOT button on GPIO%d (long-press threshold: %u ms)",
             gpio_num, (unsigned)hold_ms);
    return ESP_OK;
}
