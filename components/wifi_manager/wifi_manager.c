#include "wifi_manager.h"
#include "captive_dns.h"

#include <stdio.h>
#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lwip/inet.h"
#include "mdns.h"

static const char *TAG = "wifi_manager";

static SemaphoreHandle_t s_status_mutex = NULL;
static wifi_runtime_status_t s_status = {0};
static esp_netif_t *s_ap_netif = NULL;
static esp_netif_t *s_sta_netif = NULL;
static esp_timer_handle_t s_backoff_timer = NULL;
static bool s_mdns_started = false;
static bool s_scan_in_progress = false;

wifi_fsm_state_t wifi_fsm_compute_next_state(wifi_fsm_state_t current_state,
                                             wifi_fsm_event_t event,
                                             uint8_t ap_client_count,
                                             bool sta_configured)
{
    switch (event) {
        case WIFI_FSM_EV_BOOT_NO_STA_CFG:
        case WIFI_FSM_EV_FACTORY_RESET:
            return WIFI_FSM_AP_PROVISIONING_MODE;

        case WIFI_FSM_EV_BOOT_WITH_STA_CFG:
        case WIFI_FSM_EV_USER_CONNECT_REQ:
            return WIFI_FSM_CONNECTING;

        case WIFI_FSM_EV_STA_GOT_IP:
            return WIFI_FSM_CONNECTED_STA;

        case WIFI_FSM_EV_STA_DISCONNECTED:
            if (!sta_configured) {
                return WIFI_FSM_AP_PROVISIONING_MODE;
            }
            /* FR-010: Pause background reconnects while an AP client is connected */
            if (ap_client_count > 0U) {
                return WIFI_FSM_AP_PROVISIONING_MODE;
            }
            return WIFI_FSM_RECONNECT_BACKOFF;

        case WIFI_FSM_EV_BACKOFF_TIMER_EXPIRED:
            if (!sta_configured) {
                return WIFI_FSM_AP_PROVISIONING_MODE;
            }
            if (ap_client_count > 0U) {
                return WIFI_FSM_AP_PROVISIONING_MODE;
            }
            return WIFI_FSM_CONNECTING;

        case WIFI_FSM_EV_AP_CLIENT_CONNECTED:
            if (current_state == WIFI_FSM_RECONNECT_BACKOFF ||
                current_state == WIFI_FSM_DISCONNECTED) {
                return WIFI_FSM_AP_PROVISIONING_MODE;
            }
            return current_state;

        case WIFI_FSM_EV_AP_ALL_CLIENTS_LEFT:
            if (current_state == WIFI_FSM_AP_PROVISIONING_MODE && sta_configured) {
                return WIFI_FSM_RECONNECT_BACKOFF;
            }
            return current_state;

        default:
            return current_state;
    }
}

uint32_t wifi_fsm_compute_backoff_ms(uint16_t retry_count)
{
    uint16_t shift = (retry_count < 5U) ? (retry_count + 1U) : 6U;
    uint32_t delay_sec = 1UL << shift; /* 2, 4, 8, 16, 32, 64 */
    if (delay_sec > 60UL) {
        delay_sec = 60UL;
    }
    return delay_sec * 1000UL;
}

const char *wifi_fsm_state_to_str(wifi_fsm_state_t state)
{
    switch (state) {
        case WIFI_FSM_UNINITIALIZED:
            return "UNINITIALIZED";
        case WIFI_FSM_AP_PROVISIONING_MODE:
            return "AP_PROVISIONING_MODE";
        case WIFI_FSM_CONNECTING:
            return "CONNECTING";
        case WIFI_FSM_CONNECTED_STA:
            return "CONNECTED_STA";
        case WIFI_FSM_DISCONNECTED:
            return "DISCONNECTED";
        case WIFI_FSM_RECONNECT_BACKOFF:
            return "RECONNECT_BACKOFF";
        default:
            return "UNKNOWN";
    }
}

const char *wifi_disconnect_reason_to_str(uint8_t reason)
{
    if (reason == 0U) {
        return "";
    }
    switch (reason) {
        case WIFI_REASON_AUTH_EXPIRE:
        case WIFI_REASON_AUTH_FAIL:
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_CONNECTION_FAIL:
            return "Помилка авторизації (перевірте правильність пароля Wi-Fi)";
        case WIFI_REASON_NO_AP_FOUND:
            return "Точку доступу (SSID) не знайдено у зоні досяжності";
        case WIFI_REASON_ASSOC_LEAVE:
            return "Клієнт відключено за запитом переналаштування";
        default:
            return "Втрачено з'єднання з роутером";
    }
}

static void init_mdns_service(void)
{
    if (s_mdns_started) {
        return;
    }
    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "mDNS init failed: %s", esp_err_to_name(err));
        return;
    }
    mdns_hostname_set(WIFI_MDNS_HOSTNAME);
    mdns_instance_name_set(WIFI_AP_FIXED_SSID);
    mdns_service_add(WIFI_AP_FIXED_SSID, "_http", "_tcp", 80, NULL, 0);
    s_mdns_started = true;
    ESP_LOGI(TAG, "mDNS service started: http://%s/", WIFI_MDNS_FQDN);
}

static void schedule_backoff_locked(void)
{
    if (s_backoff_timer == NULL) {
        return;
    }
    esp_timer_stop(s_backoff_timer);
    uint32_t delay_ms = wifi_fsm_compute_backoff_ms(s_status.retry_count);
    s_status.retry_count++;
    ESP_LOGI(TAG, "Scheduling STA reconnect attempt #%u in %u ms",
             (unsigned)s_status.retry_count, (unsigned)delay_ms);
    esp_timer_start_once(s_backoff_timer, (uint64_t)delay_ms * 1000ULL);
}

static void backoff_timer_cb(void *arg)
{
    (void)arg;
    bool trigger_connect = false;

    if (xSemaphoreTake(s_status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        wifi_fsm_state_t next = wifi_fsm_compute_next_state(
            s_status.fsm_state,
            WIFI_FSM_EV_BACKOFF_TIMER_EXPIRED,
            s_status.ap_client_count,
            s_status.sta_configured);
        s_status.fsm_state = next;
        if (next == WIFI_FSM_CONNECTING && !s_scan_in_progress) {
            trigger_connect = true;
        }
        xSemaphoreGive(s_status_mutex);
    }

    if (trigger_connect) {
        ESP_LOGI(TAG, "Backoff timer expired -> attempting esp_wifi_connect()");
        esp_wifi_connect();
    }
}

static esp_err_t apply_softap_config(const char *ap_password)
{
    wifi_config_t ap_cfg = {0};
    strncpy((char *)ap_cfg.ap.ssid, WIFI_AP_FIXED_SSID, sizeof(ap_cfg.ap.ssid) - 1U);
    ap_cfg.ap.ssid_len = (uint8_t)strlen(WIFI_AP_FIXED_SSID);
    ap_cfg.ap.channel = 1;
    ap_cfg.ap.max_connection = WIFI_AP_MAX_CONN;
    ap_cfg.ap.pmf_cfg.required = false;

    const char *pwd = (ap_password != NULL && config_store_validate_password(ap_password, false))
                          ? ap_password
                          : WIFI_AP_DEFAULT_PASSWORD;
    strncpy((char *)ap_cfg.ap.password, pwd, sizeof(ap_cfg.ap.password) - 1U);
    ap_cfg.ap.authmode = WIFI_AUTH_WPA2_WPA3_PSK;

    esp_err_t err = esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure SoftAP: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "SoftAP configured: SSID='%s', IP=%s", WIFI_AP_FIXED_SSID, WIFI_AP_DEFAULT_IP);
    return ESP_OK;
}

static esp_err_t apply_sta_config(const char *ssid, const char *password)
{
    wifi_config_t sta_cfg = {0};
    if (ssid != NULL && ssid[0] != '\0') {
        strncpy((char *)sta_cfg.sta.ssid, ssid, sizeof(sta_cfg.sta.ssid) - 1U);
        if (password != NULL && password[0] != '\0') {
            strncpy((char *)sta_cfg.sta.password, password, sizeof(sta_cfg.sta.password) - 1U);
            sta_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        } else {
            sta_cfg.sta.threshold.authmode = WIFI_AUTH_OPEN;
        }
        sta_cfg.sta.pmf_cfg.capable = true;
        sta_cfg.sta.pmf_cfg.required = false;
    }

    return esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    (void)arg;

    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_AP_START:
                ESP_LOGI(TAG, "SoftAP '%s' started on %s", WIFI_AP_FIXED_SSID, WIFI_AP_DEFAULT_IP);
                break;

            case WIFI_EVENT_AP_STACONNECTED: {
                wifi_event_ap_staconnected_t *ev = (wifi_event_ap_staconnected_t *)event_data;
                if (xSemaphoreTake(s_status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    s_status.ap_client_count++;
                    esp_timer_stop(s_backoff_timer);
                    s_status.fsm_state = wifi_fsm_compute_next_state(
                        s_status.fsm_state,
                        WIFI_FSM_EV_AP_CLIENT_CONNECTED,
                        s_status.ap_client_count,
                        s_status.sta_configured);
                    ESP_LOGI(TAG, "Client connected to SoftAP (AID=%d, total=%u, fsm=%s)",
                             ev->aid, (unsigned)s_status.ap_client_count,
                             wifi_fsm_state_to_str(s_status.fsm_state));
                    xSemaphoreGive(s_status_mutex);
                }
                break;
            }

            case WIFI_EVENT_AP_STADISCONNECTED: {
                wifi_event_ap_stadisconnected_t *ev = (wifi_event_ap_stadisconnected_t *)event_data;
                if (xSemaphoreTake(s_status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    if (s_status.ap_client_count > 0U) {
                        s_status.ap_client_count--;
                    }
                    if (s_status.ap_client_count == 0U) {
                        wifi_fsm_state_t next = wifi_fsm_compute_next_state(
                            s_status.fsm_state,
                            WIFI_FSM_EV_AP_ALL_CLIENTS_LEFT,
                            s_status.ap_client_count,
                            s_status.sta_configured);
                        s_status.fsm_state = next;
                        if (next == WIFI_FSM_RECONNECT_BACKOFF) {
                            schedule_backoff_locked();
                        }
                    }
                    ESP_LOGI(TAG, "Client disconnected from SoftAP (AID=%d, remaining=%u, fsm=%s)",
                             ev->aid, (unsigned)s_status.ap_client_count,
                             wifi_fsm_state_to_str(s_status.fsm_state));
                    xSemaphoreGive(s_status_mutex);
                }
                break;
            }

            case WIFI_EVENT_STA_START: {
                bool connect_now = false;
                if (xSemaphoreTake(s_status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    if (s_status.sta_configured) {
                        s_status.fsm_state = wifi_fsm_compute_next_state(
                            s_status.fsm_state,
                            WIFI_FSM_EV_BOOT_WITH_STA_CFG,
                            s_status.ap_client_count,
                            true);
                        connect_now = true;
                    } else {
                        s_status.fsm_state = wifi_fsm_compute_next_state(
                            s_status.fsm_state,
                            WIFI_FSM_EV_BOOT_NO_STA_CFG,
                            s_status.ap_client_count,
                            false);
                    }
                    xSemaphoreGive(s_status_mutex);
                }
                if (connect_now) {
                    ESP_LOGI(TAG, "STA started with saved credentials -> connecting to '%s'",
                             s_status.sta_ssid);
                    esp_wifi_connect();
                }
                break;
            }

            case WIFI_EVENT_STA_DISCONNECTED: {
                wifi_event_sta_disconnected_t *dis = (wifi_event_sta_disconnected_t *)event_data;
                if (xSemaphoreTake(s_status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    s_status.sta_connected = false;
                    strncpy(s_status.sta_ip, "0.0.0.0", sizeof(s_status.sta_ip) - 1U);
                    s_status.sta_rssi = 0;
                    s_status.last_error_reason = dis->reason;

                    wifi_fsm_state_t next = wifi_fsm_compute_next_state(
                        s_status.fsm_state,
                        WIFI_FSM_EV_STA_DISCONNECTED,
                        s_status.ap_client_count,
                        s_status.sta_configured);
                    s_status.fsm_state = next;

                    ESP_LOGW(TAG, "STA disconnected (reason=%u: %s), next FSM state=%s (ap_clients=%u)",
                             (unsigned)dis->reason,
                             wifi_disconnect_reason_to_str(dis->reason),
                             wifi_fsm_state_to_str(next),
                             (unsigned)s_status.ap_client_count);

                    if (next == WIFI_FSM_RECONNECT_BACKOFF) {
                        schedule_backoff_locked();
                    } else {
                        esp_timer_stop(s_backoff_timer);
                    }
                    xSemaphoreGive(s_status_mutex);
                }
                break;
            }

            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        if (xSemaphoreTake(s_status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            esp_timer_stop(s_backoff_timer);
            s_status.sta_connected = true;
            s_status.retry_count = 0U;
            s_status.last_error_reason = 0U;
            snprintf(s_status.sta_ip, sizeof(s_status.sta_ip), IPSTR, IP2STR(&event->ip_info.ip));
            s_status.fsm_state = wifi_fsm_compute_next_state(
                s_status.fsm_state,
                WIFI_FSM_EV_STA_GOT_IP,
                s_status.ap_client_count,
                s_status.sta_configured);

            wifi_ap_record_t ap_info;
            if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
                s_status.sta_rssi = ap_info.rssi;
                s_status.ap_channel = ap_info.primary;
            }

            ESP_LOGI(TAG, "STA connected! Assigned LAN IP: %s (mDNS: http://%s/)",
                     s_status.sta_ip, WIFI_MDNS_FQDN);
            xSemaphoreGive(s_status_mutex);
        }
    }
}

esp_err_t wifi_manager_init(const wifi_stored_config_t *initial_cfg)
{
    if (initial_cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    s_status_mutex = xSemaphoreCreateMutex();
    if (s_status_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    memset(&s_status, 0, sizeof(s_status));
    s_status.fsm_state = WIFI_FSM_UNINITIALIZED;
    s_status.sta_configured = initial_cfg->sta_configured;
    s_status.sta_connected = false;
    strncpy(s_status.sta_ssid, initial_cfg->sta_ssid, WIFI_SSID_MAX_LEN);
    strncpy(s_status.sta_ip, "0.0.0.0", sizeof(s_status.sta_ip) - 1U);
    strncpy(s_status.ap_ssid, WIFI_AP_FIXED_SSID, WIFI_SSID_MAX_LEN);
    strncpy(s_status.ap_ip, WIFI_AP_DEFAULT_IP, sizeof(s_status.ap_ip) - 1U);
    s_status.ap_channel = 1U;

    const esp_timer_create_args_t backoff_args = {
        .callback = &backoff_timer_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "wifi_backoff",
    };
    ESP_ERROR_CHECK(esp_timer_create(&backoff_args, &s_backoff_timer));

    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t ev_err = esp_event_loop_create_default();
    if (ev_err != ESP_OK && ev_err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(ev_err);
    }

    s_ap_netif = esp_netif_create_default_wifi_ap();
    s_sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    /* FR-001: Always operate in combined WIFI_MODE_APSTA */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(apply_softap_config(initial_cfg->ap_password));

    if (initial_cfg->sta_configured) {
        ESP_ERROR_CHECK(apply_sta_config(initial_cfg->sta_ssid, initial_cfg->sta_password));
    }

    ESP_ERROR_CHECK(esp_wifi_start());

    /* Start mDNS (weather-clock.local) and Captive Portal DNS server (FR-003) */
    init_mdns_service();
    captive_dns_start();

    return ESP_OK;
}

esp_err_t wifi_manager_get_status(wifi_runtime_status_t *out_status)
{
    if (out_status == NULL || s_status_mutex == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_status_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    if (s_status.sta_connected) {
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            s_status.sta_rssi = ap_info.rssi;
            s_status.ap_channel = ap_info.primary;
        }
    }

    *out_status = s_status;
    xSemaphoreGive(s_status_mutex);
    return ESP_OK;
}

esp_err_t wifi_manager_scan(wifi_scan_item_t *out_items, size_t max_items, size_t *out_count)
{
    if (out_items == NULL || out_count == NULL || max_items == 0U) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_count = 0U;

    if (xSemaphoreTake(s_status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_scan_in_progress = true;
        esp_timer_stop(s_backoff_timer);
        xSemaphoreGive(s_status_mutex);
    }

    wifi_scan_config_t scan_cfg = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 50,
        .scan_time.active.max = 120,
    };

    esp_err_t err = esp_wifi_scan_start(&scan_cfg, true);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi scan start failed (%s)", esp_err_to_name(err));
        if (xSemaphoreTake(s_status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            s_scan_in_progress = false;
            xSemaphoreGive(s_status_mutex);
        }
        return err;
    }

    uint16_t ap_num = WIFI_MAX_SCAN_RESULTS;
    wifi_ap_record_t records[WIFI_MAX_SCAN_RESULTS];
    memset(records, 0, sizeof(records));

    err = esp_wifi_scan_get_ap_records(&ap_num, records);
    if (xSemaphoreTake(s_status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_scan_in_progress = false;
        xSemaphoreGive(s_status_mutex);
    }

    if (err != ESP_OK) {
        return err;
    }

    size_t written = 0U;
    for (uint16_t i = 0U; i < ap_num && written < max_items; ++i) {
        if (records[i].ssid[0] == '\0') {
            continue;
        }
        /* Deduplicate identical SSIDs, keeping the strongest RSSI (ESP-IDF sorts by RSSI desc) */
        bool duplicate = false;
        for (size_t j = 0U; j < written; ++j) {
            if (strncmp(out_items[j].ssid, (const char *)records[i].ssid, WIFI_SSID_MAX_LEN) == 0) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            continue;
        }

        strncpy(out_items[written].ssid, (const char *)records[i].ssid, WIFI_SSID_MAX_LEN);
        out_items[written].ssid[WIFI_SSID_MAX_LEN] = '\0';
        out_items[written].rssi = records[i].rssi;
        out_items[written].channel = records[i].primary;
        out_items[written].authmode = (uint8_t)records[i].authmode;
        out_items[written].secure = (records[i].authmode != WIFI_AUTH_OPEN);
        written++;
    }

    *out_count = written;
    ESP_LOGI(TAG, "Wi-Fi scan completed: %u unique networks found", (unsigned)written);
    return ESP_OK;
}

esp_err_t wifi_manager_connect_sta(const char *ssid, const char *password)
{
    esp_err_t err = config_store_save_sta(ssid, password);
    if (err != ESP_OK) {
        return err;
    }

    if (xSemaphoreTake(s_status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        esp_timer_stop(s_backoff_timer);
        s_status.sta_configured = true;
        s_status.retry_count = 0U;
        s_status.last_error_reason = 0U;
        strncpy(s_status.sta_ssid, ssid, WIFI_SSID_MAX_LEN);
        s_status.sta_ssid[WIFI_SSID_MAX_LEN] = '\0';
        s_status.fsm_state = wifi_fsm_compute_next_state(
            s_status.fsm_state,
            WIFI_FSM_EV_USER_CONNECT_REQ,
            s_status.ap_client_count,
            true);
        xSemaphoreGive(s_status_mutex);
    }

    esp_wifi_disconnect();
    err = apply_sta_config(ssid, password);
    if (err != ESP_OK) {
        return err;
    }

    ESP_LOGI(TAG, "Initiating STA connection to '%s' without reboot", ssid);
    return esp_wifi_connect();
}

esp_err_t wifi_manager_update_ap_password(const char *new_ap_password)
{
    esp_err_t err = config_store_save_ap_password(new_ap_password);
    if (err != ESP_OK) {
        return err;
    }
    return apply_softap_config(new_ap_password);
}

esp_err_t wifi_manager_apply_factory_reset(void)
{
    esp_err_t err = config_store_factory_reset();
    if (err != ESP_OK) {
        return err;
    }

    if (xSemaphoreTake(s_status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        esp_timer_stop(s_backoff_timer);
        s_status.sta_configured = false;
        s_status.sta_connected = false;
        s_status.sta_ssid[0] = '\0';
        strncpy(s_status.sta_ip, "0.0.0.0", sizeof(s_status.sta_ip) - 1U);
        s_status.sta_rssi = 0;
        s_status.retry_count = 0U;
        s_status.last_error_reason = 0U;
        s_status.fsm_state = wifi_fsm_compute_next_state(
            s_status.fsm_state,
            WIFI_FSM_EV_FACTORY_RESET,
            s_status.ap_client_count,
            false);
        xSemaphoreGive(s_status_mutex);
    }

    esp_wifi_disconnect();
    apply_sta_config("", "");
    return apply_softap_config(WIFI_AP_DEFAULT_PASSWORD);
}
