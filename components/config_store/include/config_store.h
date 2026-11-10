#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief NVS Schema & Default Constants (FR-002, FR-006, FR-007)
 */
#define CONFIG_STORE_NVS_NAMESPACE      "wifi_cfg"
#define CONFIG_STORE_SCHEMA_VERSION     1U

#define WIFI_AP_FIXED_SSID              "ESP Weather Clock"
#define WIFI_AP_DEFAULT_PASSWORD        "weather1"

#define WIFI_SSID_MAX_LEN               32U
#define WIFI_PASS_MIN_LEN               8U
#define WIFI_PASS_MAX_LEN               63U

/**
 * @brief Stored Wi-Fi configuration persisted in NVS ("wifi_cfg").
 */
typedef struct {
    uint8_t schema_version;                     /**< Schema version for forward migration */
    bool sta_configured;                        /**< True if user configured a target STA network */
    char sta_ssid[WIFI_SSID_MAX_LEN + 1U];      /**< Target router SSID (1..32 bytes) */
    char sta_password[WIFI_PASS_MAX_LEN + 1U];  /**< Target router password (0 or 8..63 chars) */
    char ap_password[WIFI_PASS_MAX_LEN + 1U];   /**< SoftAP password (8..63 chars, default "weather1") */
} wifi_stored_config_t;

/**
 * @brief Initialize NVS flash and ensure configuration namespace is ready.
 *
 * @return ESP_OK on success, or an esp_err_t code.
 */
esp_err_t config_store_init(void);

/**
 * @brief Populate a configuration struct with factory default values.
 *
 * @param[out] out_cfg Pointer to configuration structure.
 */
void config_store_get_defaults(wifi_stored_config_t *out_cfg);

/**
 * @brief Validate a Wi-Fi SSID string (1..32 bytes, non-empty).
 *
 * @param[in] ssid Null-terminated SSID string.
 * @return true if valid, false otherwise.
 */
bool config_store_validate_ssid(const char *ssid);

/**
 * @brief Validate a Wi-Fi password string.
 *
 * @param[in] password   Null-terminated password string.
 * @param[in] allow_open If true, an empty password ("") is valid (for open STA networks).
 *                       If false (e.g., SoftAP password), password must be 8..63 printable ASCII chars.
 * @return true if valid, false otherwise.
 */
bool config_store_validate_password(const char *password, bool allow_open);

/**
 * @brief Load persisted configuration from NVS, falling back to factory defaults if uninitialized.
 *
 * @param[out] out_cfg Pointer to destination structure.
 * @return ESP_OK on success.
 */
esp_err_t config_store_load(wifi_stored_config_t *out_cfg);

/**
 * @brief Validate and atomically save STA credentials to NVS (FR-006, FR-007).
 *
 * @param[in] ssid     Target STA SSID (1..32 chars).
 * @param[in] password Target STA password (0 or 8..63 printable ASCII chars).
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if validation fails.
 */
esp_err_t config_store_save_sta(const char *ssid, const char *password);

/**
 * @brief Validate and atomically save SoftAP password to NVS (FR-002, FR-006, FR-007).
 *
 * @param[in] ap_password New SoftAP password (8..63 printable ASCII chars).
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if validation fails.
 */
esp_err_t config_store_save_ap_password(const char *ap_password);

/**
 * @brief Reset stored configuration in NVS back to factory defaults ("weather1", no STA) (FR-012).
 *
 * @return ESP_OK on success.
 */
esp_err_t config_store_factory_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_STORE_H */
