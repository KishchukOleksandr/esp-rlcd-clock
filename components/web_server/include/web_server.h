#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <stddef.h>
#include "config_store.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the built-in HTTP web server on port 80 across both AP and STA interfaces (FR-003).
 *
 * @return ESP_OK on success.
 */
esp_err_t web_server_start(void);

/**
 * @brief Stop the built-in HTTP web server if running.
 */
void web_server_stop(void);

/**
 * @brief Parse and validate a JSON payload for `/api/wifi/connect` (exposed for unit testing).
 *
 * @param[in]  json_str     Null-terminated JSON string.
 * @param[out] out_ssid     Output buffer of size >= `WIFI_SSID_MAX_LEN + 1`.
 * @param[in]  ssid_buf_len Size of `out_ssid`.
 * @param[out] out_pass     Output buffer of size >= `WIFI_PASS_MAX_LEN + 1`.
 * @param[in]  pass_buf_len Size of `out_pass`.
 * @return ESP_OK if valid, ESP_ERR_INVALID_ARG otherwise.
 */
esp_err_t web_server_parse_sta_connect_json(const char *json_str,
                                            char *out_ssid, size_t ssid_buf_len,
                                            char *out_pass, size_t pass_buf_len);

/**
 * @brief Parse and validate a JSON payload for `/api/wifi/ap-password` (exposed for unit testing).
 *
 * @param[in]  json_str     Null-terminated JSON string.
 * @param[out] out_pass     Output buffer of size >= `WIFI_PASS_MAX_LEN + 1`.
 * @param[in]  pass_buf_len Size of `out_pass`.
 * @return ESP_OK if valid, ESP_ERR_INVALID_ARG otherwise.
 */
esp_err_t web_server_parse_ap_password_json(const char *json_str,
                                            char *out_pass, size_t pass_buf_len);

/**
 * @brief Parse and validate a JSON payload for `POST /api/location` (exposed for unit testing).
 *
 * @param[in]  json_str     Null-terminated JSON string containing `name`, `latitude`, and `longitude`.
 * @param[out] out_name     Output buffer of size >= `LOC_NAME_MAX_LEN + 1`.
 * @param[in]  name_buf_len Size of `out_name`.
 * @param[out] out_lat      Output pointer for parsed latitude [-90.0 .. 90.0].
 * @param[out] out_lon      Output pointer for parsed longitude [-180.0 .. 180.0].
 * @return ESP_OK if valid, ESP_ERR_INVALID_ARG otherwise.
 */
esp_err_t web_server_parse_location_json(const char *json_str,
                                         char *out_name, size_t name_buf_len,
                                         double *out_lat, double *out_lon);

#ifdef __cplusplus
}
#endif

#endif /* WEB_SERVER_H */
