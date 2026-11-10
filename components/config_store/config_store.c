#include "config_store.h"

#include <string.h>
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "config_store";

#define NVS_KEY_SCHEMA_VER  "schema_ver"
#define NVS_KEY_STA_SET     "sta_set"
#define NVS_KEY_STA_SSID    "sta_ssid"
#define NVS_KEY_STA_PASS    "sta_pass"
#define NVS_KEY_AP_PASS     "ap_pass"

static bool is_printable_ascii(const char *str, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)str[i];
        if (c < 0x20U || c > 0x7EU) {
            return false;
        }
    }
    return true;
}

void config_store_get_defaults(wifi_stored_config_t *out_cfg)
{
    if (out_cfg == NULL) {
        return;
    }
    memset(out_cfg, 0, sizeof(*out_cfg));
    out_cfg->schema_version = CONFIG_STORE_SCHEMA_VERSION;
    out_cfg->sta_configured = false;
    out_cfg->sta_ssid[0] = '\0';
    out_cfg->sta_password[0] = '\0';
    strncpy(out_cfg->ap_password, WIFI_AP_DEFAULT_PASSWORD, WIFI_PASS_MAX_LEN);
    out_cfg->ap_password[WIFI_PASS_MAX_LEN] = '\0';
}

bool config_store_validate_ssid(const char *ssid)
{
    if (ssid == NULL) {
        return false;
    }
    size_t len = strnlen(ssid, WIFI_SSID_MAX_LEN + 1U);
    if (len == 0U || len > WIFI_SSID_MAX_LEN) {
        return false;
    }
    return true;
}

bool config_store_validate_password(const char *password, bool allow_open)
{
    if (password == NULL) {
        return false;
    }
    size_t len = strnlen(password, WIFI_PASS_MAX_LEN + 1U);
    if (len == 0U) {
        return allow_open;
    }
    if (len < WIFI_PASS_MIN_LEN || len > WIFI_PASS_MAX_LEN) {
        return false;
    }
    return is_printable_ascii(password, len);
}

esp_err_t config_store_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition truncated or version changed, erasing NVS...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS flash: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "NVS storage initialized successfully");
    return ESP_OK;
}

esp_err_t config_store_load(wifi_stored_config_t *out_cfg)
{
    if (out_cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    config_store_get_defaults(out_cfg);

    nvs_handle_t handle;
    esp_err_t err = nvs_open(CONFIG_STORE_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No saved configuration found in NVS; using factory defaults");
        return ESP_OK;
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS namespace (%s); falling back to defaults", esp_err_to_name(err));
        return ESP_OK;
    }

    uint8_t schema_ver = 0U;
    if (nvs_get_u8(handle, NVS_KEY_SCHEMA_VER, &schema_ver) != ESP_OK ||
        schema_ver != CONFIG_STORE_SCHEMA_VERSION) {
        ESP_LOGW(TAG, "NVS schema mismatch or uninitialized (ver=%u); using defaults", (unsigned)schema_ver);
        nvs_close(handle);
        return ESP_OK;
    }

    char ap_pass[WIFI_PASS_MAX_LEN + 1U] = {0};
    size_t ap_pass_len = sizeof(ap_pass);
    if (nvs_get_str(handle, NVS_KEY_AP_PASS, ap_pass, &ap_pass_len) == ESP_OK &&
        config_store_validate_password(ap_pass, false)) {
        strncpy(out_cfg->ap_password, ap_pass, WIFI_PASS_MAX_LEN);
        out_cfg->ap_password[WIFI_PASS_MAX_LEN] = '\0';
    }

    uint8_t sta_set = 0U;
    if (nvs_get_u8(handle, NVS_KEY_STA_SET, &sta_set) == ESP_OK && sta_set == 1U) {
        char ssid[WIFI_SSID_MAX_LEN + 1U] = {0};
        char pass[WIFI_PASS_MAX_LEN + 1U] = {0};
        size_t ssid_len = sizeof(ssid);
        size_t pass_len = sizeof(pass);

        esp_err_t ssid_err = nvs_get_str(handle, NVS_KEY_STA_SSID, ssid, &ssid_len);
        esp_err_t pass_err = nvs_get_str(handle, NVS_KEY_STA_PASS, pass, &pass_len);

        if (ssid_err == ESP_OK && pass_err == ESP_OK &&
            config_store_validate_ssid(ssid) &&
            config_store_validate_password(pass, true)) {
            out_cfg->sta_configured = true;
            strncpy(out_cfg->sta_ssid, ssid, WIFI_SSID_MAX_LEN);
            out_cfg->sta_ssid[WIFI_SSID_MAX_LEN] = '\0';
            strncpy(out_cfg->sta_password, pass, WIFI_PASS_MAX_LEN);
            out_cfg->sta_password[WIFI_PASS_MAX_LEN] = '\0';
        } else {
            ESP_LOGW(TAG, "Stored STA credentials failed validation; ignoring STA config");
        }
    }

    nvs_close(handle);
    ESP_LOGI(TAG, "Loaded config: sta_configured=%d, sta_ssid='%s'",
             (int)out_cfg->sta_configured, out_cfg->sta_ssid);
    return ESP_OK;
}

esp_err_t config_store_save_sta(const char *ssid, const char *password)
{
    if (!config_store_validate_ssid(ssid) || !config_store_validate_password(password, true)) {
        ESP_LOGW(TAG, "Rejected invalid STA credentials prior to NVS write");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(CONFIG_STORE_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for writing STA config: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_u8(handle, NVS_KEY_SCHEMA_VER, CONFIG_STORE_SCHEMA_VERSION);
    if (err == ESP_OK) {
        err = nvs_set_str(handle, NVS_KEY_STA_SSID, ssid);
    }
    if (err == ESP_OK) {
        err = nvs_set_str(handle, NVS_KEY_STA_PASS, password);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, NVS_KEY_STA_SET, 1U);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit STA config to NVS: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Saved STA credentials for SSID '%s' to NVS", ssid);
    return ESP_OK;
}

esp_err_t config_store_save_ap_password(const char *ap_password)
{
    if (!config_store_validate_password(ap_password, false)) {
        ESP_LOGW(TAG, "Rejected invalid SoftAP password prior to NVS write");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(CONFIG_STORE_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for writing AP password: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_u8(handle, NVS_KEY_SCHEMA_VER, CONFIG_STORE_SCHEMA_VERSION);
    if (err == ESP_OK) {
        err = nvs_set_str(handle, NVS_KEY_AP_PASS, ap_password);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit AP password to NVS: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Saved updated SoftAP password to NVS");
    return ESP_OK;
}

esp_err_t config_store_factory_reset(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(CONFIG_STORE_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "Factory reset: NVS namespace already empty");
        return ESP_OK;
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for factory reset: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_erase_all(handle);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err == ESP_OK) {
        ESP_LOGW(TAG, "Factory reset completed: AP password restored to '%s', STA cleared",
                 WIFI_AP_DEFAULT_PASSWORD);
    }
    return err;
}
