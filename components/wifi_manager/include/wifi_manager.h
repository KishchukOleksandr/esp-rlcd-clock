#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "config_store.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_AP_DEFAULT_IP          "192.168.4.1"
#define WIFI_MDNS_HOSTNAME          "weather-clock"
#define WIFI_MDNS_FQDN              "weather-clock.local"
#define WIFI_MAX_SCAN_RESULTS       16U
#define WIFI_AP_MAX_CONN            4U

/**
 * @brief Wi-Fi Finite State Machine states (Constitution Principle II).
 */
typedef enum {
    WIFI_FSM_UNINITIALIZED = 0,
    WIFI_FSM_AP_PROVISIONING_MODE,
    WIFI_FSM_CONNECTING,
    WIFI_FSM_CONNECTED_STA,
    WIFI_FSM_DISCONNECTED,
    WIFI_FSM_RECONNECT_BACKOFF
} wifi_fsm_state_t;

/**
 * @brief FSM input events for deterministic state transition calculation.
 */
typedef enum {
    WIFI_FSM_EV_BOOT_NO_STA_CFG = 0,
    WIFI_FSM_EV_BOOT_WITH_STA_CFG,
    WIFI_FSM_EV_USER_CONNECT_REQ,
    WIFI_FSM_EV_STA_GOT_IP,
    WIFI_FSM_EV_STA_DISCONNECTED,
    WIFI_FSM_EV_BACKOFF_TIMER_EXPIRED,
    WIFI_FSM_EV_AP_CLIENT_CONNECTED,
    WIFI_FSM_EV_AP_ALL_CLIENTS_LEFT,
    WIFI_FSM_EV_FACTORY_RESET
} wifi_fsm_event_t;

/**
 * @brief Runtime network status across both SoftAP and STA interfaces.
 */
typedef struct {
    wifi_fsm_state_t fsm_state;
    bool sta_configured;
    bool sta_connected;
    char sta_ssid[WIFI_SSID_MAX_LEN + 1U];
    char sta_ip[16];
    int8_t sta_rssi;
    uint8_t last_error_reason;
    uint16_t retry_count;
    char ap_ssid[WIFI_SSID_MAX_LEN + 1U];
    char ap_ip[16];
    uint8_t ap_client_count;
    uint8_t ap_channel;
} wifi_runtime_status_t;

/**
 * @brief Scanned 2.4 GHz Wi-Fi network entry (FR-005).
 */
typedef struct {
    char ssid[WIFI_SSID_MAX_LEN + 1U];
    int8_t rssi;
    uint8_t channel;
    uint8_t authmode;
    bool secure;
} wifi_scan_item_t;

/**
 * @brief Pure function computing the next FSM state given current state, event, and context (FR-010).
 */
wifi_fsm_state_t wifi_fsm_compute_next_state(wifi_fsm_state_t current_state,
                                             wifi_fsm_event_t event,
                                             uint8_t ap_client_count,
                                             bool sta_configured);

/**
 * @brief Compute exponential backoff delay in milliseconds (2s .. 60s max).
 */
uint32_t wifi_fsm_compute_backoff_ms(uint16_t retry_count);

/**
 * @brief Convert FSM state enum to human-readable string.
 */
const char *wifi_fsm_state_to_str(wifi_fsm_state_t state);

/**
 * @brief Convert Wi-Fi disconnect reason code to user-friendly Ukrainian message.
 */
const char *wifi_disconnect_reason_to_str(uint8_t reason);

/**
 * @brief Initialize dual Wi-Fi AP+STA mode, netifs, event handlers, mDNS, and Captive DNS.
 *
 * @param[in] initial_cfg Initial configuration loaded from NVS.
 * @return ESP_OK on success.
 */
esp_err_t wifi_manager_init(const wifi_stored_config_t *initial_cfg);

/**
 * @brief Get a thread-safe snapshot of current Wi-Fi status.
 *
 * @param[out] out_status Destination status structure.
 * @return ESP_OK on success.
 */
esp_err_t wifi_manager_get_status(wifi_runtime_status_t *out_status);

/**
 * @brief Perform a 2.4 GHz Wi-Fi scan and populate up to `max_items` results sorted by RSSI (FR-005).
 *
 * @param[out] out_items  Array of scan items.
 * @param[in]  max_items  Capacity of `out_items`.
 * @param[out] out_count  Number of networks written to `out_items`.
 * @return ESP_OK on success.
 */
esp_err_t wifi_manager_scan(wifi_scan_item_t *out_items, size_t max_items, size_t *out_count);

/**
 * @brief Save new STA credentials and immediately initiate connection without reboot (FR-008).
 *
 * @param[in] ssid     Target SSID.
 * @param[in] password Target password.
 * @return ESP_OK on success.
 */
esp_err_t wifi_manager_connect_sta(const char *ssid, const char *password);

/**
 * @brief Update SoftAP password on the fly and persist to NVS (FR-002).
 *
 * @param[in] new_ap_password New password (8..63 chars).
 * @return ESP_OK on success.
 */
esp_err_t wifi_manager_update_ap_password(const char *new_ap_password);

/**
 * @brief Apply factory reset to live Wi-Fi interfaces (restore "weather1" AP password, disconnect STA) (FR-012).
 *
 * @return ESP_OK on success.
 */
esp_err_t wifi_manager_apply_factory_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_MANAGER_H */
