#include "config_store.h"

#include <math.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "config_store";

#define NVS_KEY_SCHEMA_VER  "schema_ver"
#define NVS_KEY_STA_SET     "sta_set"
#define NVS_KEY_STA_SSID    "sta_ssid"
#define NVS_KEY_STA_PASS    "sta_pass"
#define NVS_KEY_AP_PASS     "ap_pass"
#define NVS_KEY_LOC_SET     "loc_set"
#define NVS_KEY_LOC_NAME    "loc_name"
#define NVS_KEY_LOC_LAT_U6  "loc_lat_u6"
#define NVS_KEY_LOC_LON_U6  "loc_lon_u6"

#define LOC_MICRODEG_SCALE  1000000.0

static portMUX_TYPE s_loc_mux = portMUX_INITIALIZER_UNLOCKED;
static location_config_t s_cached_location = {
    .configured = false,
    .name = "",
    .latitude = 0.0,
    .longitude = 0.0,
};

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

static void update_cached_location(bool configured, const char *name, double latitude, double longitude)
{
    portENTER_CRITICAL(&s_loc_mux);
    s_cached_location.configured = configured;
    if (configured && name != NULL) {
        strncpy(s_cached_location.name, name, LOC_NAME_MAX_LEN);
        s_cached_location.name[LOC_NAME_MAX_LEN] = '\0';
        s_cached_location.latitude = latitude;
        s_cached_location.longitude = longitude;
    } else {
        s_cached_location.name[0] = '\0';
        s_cached_location.latitude = 0.0;
        s_cached_location.longitude = 0.0;
    }
    portEXIT_CRITICAL(&s_loc_mux);
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
    out_cfg->loc_configured = false;
    out_cfg->loc_name[0] = '\0';
    out_cfg->latitude = 0.0;
    out_cfg->longitude = 0.0;
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

bool config_store_validate_location(const char *name, double latitude, double longitude)
{
    if (name == NULL) {
        return false;
    }
    size_t len = strnlen(name, LOC_NAME_MAX_LEN + 1U);
    if (len == 0U || len > LOC_NAME_MAX_LEN) {
        return false;
    }

    bool has_non_space = false;
    for (size_t i = 0U; i < len; ++i) {
        unsigned char c = (unsigned char)name[i];
        if (c < 0x20U || c == 0x7FU) {
            return false;
        }
        if (c > 0x20U) {
            has_non_space = true;
        }
    }
    if (!has_non_space) {
        return false;
    }

    if (!isfinite(latitude) || !isfinite(longitude)) {
        return false;
    }
    if (latitude < LOC_LAT_MIN || latitude > LOC_LAT_MAX) {
        return false;
    }
    if (longitude < LOC_LON_MIN || longitude > LOC_LON_MAX) {
        return false;
    }

    return true;
}

void config_store_get_location(location_config_t *out_loc)
{
    if (out_loc == NULL) {
        return;
    }
    portENTER_CRITICAL(&s_loc_mux);
    *out_loc = s_cached_location;
    portEXIT_CRITICAL(&s_loc_mux);
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
    update_cached_location(false, "", 0.0, 0.0);

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
        (schema_ver != CONFIG_STORE_SCHEMA_VERSION_V1 && schema_ver != CONFIG_STORE_SCHEMA_VERSION)) {
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

    if (schema_ver >= CONFIG_STORE_SCHEMA_VERSION) {
        uint8_t loc_set = 0U;
        if (nvs_get_u8(handle, NVS_KEY_LOC_SET, &loc_set) == ESP_OK && loc_set == 1U) {
            char loc_name[LOC_NAME_MAX_LEN + 1U] = {0};
            size_t loc_name_len = sizeof(loc_name);
            int32_t lat_u6 = 0;
            int32_t lon_u6 = 0;

            esp_err_t name_err = nvs_get_str(handle, NVS_KEY_LOC_NAME, loc_name, &loc_name_len);
            esp_err_t lat_err = nvs_get_i32(handle, NVS_KEY_LOC_LAT_U6, &lat_u6);
            esp_err_t lon_err = nvs_get_i32(handle, NVS_KEY_LOC_LON_U6, &lon_u6);

            double lat = (double)lat_u6 / LOC_MICRODEG_SCALE;
            double lon = (double)lon_u6 / LOC_MICRODEG_SCALE;

            if (name_err == ESP_OK && lat_err == ESP_OK && lon_err == ESP_OK &&
                config_store_validate_location(loc_name, lat, lon)) {
                out_cfg->loc_configured = true;
                strncpy(out_cfg->loc_name, loc_name, LOC_NAME_MAX_LEN);
                out_cfg->loc_name[LOC_NAME_MAX_LEN] = '\0';
                out_cfg->latitude = lat;
                out_cfg->longitude = lon;
                update_cached_location(true, out_cfg->loc_name, lat, lon);
            } else {
                ESP_LOGW(TAG, "Stored location failed validation or incomplete; falling back to unconfigured");
            }
        }
    }

    nvs_close(handle);

    /* Forward migration from schema v1 -> v2 preserving existing STA/AP config */
    if (schema_ver == CONFIG_STORE_SCHEMA_VERSION_V1) {
        nvs_handle_t rw_handle;
        if (nvs_open(CONFIG_STORE_NVS_NAMESPACE, NVS_READWRITE, &rw_handle) == ESP_OK) {
            if (nvs_set_u8(rw_handle, NVS_KEY_LOC_SET, 0U) == ESP_OK &&
                nvs_set_u8(rw_handle, NVS_KEY_SCHEMA_VER, CONFIG_STORE_SCHEMA_VERSION) == ESP_OK) {
                nvs_commit(rw_handle);
                ESP_LOGI(TAG, "Migrated NVS schema v1 -> v2 (STA/AP preserved, location initialized)");
            }
            nvs_close(rw_handle);
        }
    }

    ESP_LOGI(TAG, "Loaded config: sta_configured=%d, sta_ssid='%s', loc_configured=%d, loc_name='%s'",
             (int)out_cfg->sta_configured, out_cfg->sta_ssid,
             (int)out_cfg->loc_configured, out_cfg->loc_name);
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

esp_err_t config_store_save_location(const char *name, double latitude, double longitude)
{
    if (!config_store_validate_location(name, latitude, longitude)) {
        ESP_LOGW(TAG, "Rejected invalid location parameters prior to NVS write");
        return ESP_ERR_INVALID_ARG;
    }

    int32_t lat_u6 = (int32_t)llround(latitude * LOC_MICRODEG_SCALE);
    int32_t lon_u6 = (int32_t)llround(longitude * LOC_MICRODEG_SCALE);
    double normalized_lat = (double)lat_u6 / LOC_MICRODEG_SCALE;
    double normalized_lon = (double)lon_u6 / LOC_MICRODEG_SCALE;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(CONFIG_STORE_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for writing location config: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_u8(handle, NVS_KEY_SCHEMA_VER, CONFIG_STORE_SCHEMA_VERSION);
    if (err == ESP_OK) {
        err = nvs_set_str(handle, NVS_KEY_LOC_NAME, name);
    }
    if (err == ESP_OK) {
        err = nvs_set_i32(handle, NVS_KEY_LOC_LAT_U6, lat_u6);
    }
    if (err == ESP_OK) {
        err = nvs_set_i32(handle, NVS_KEY_LOC_LON_U6, lon_u6);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, NVS_KEY_LOC_SET, 1U);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit location config to NVS: %s", esp_err_to_name(err));
        return err;
    }

    update_cached_location(true, name, normalized_lat, normalized_lon);
    ESP_LOGI(TAG, "Saved location '%s' (lat=%.6f, lon=%.6f) to NVS",
             name, normalized_lat, normalized_lon);
    return ESP_OK;
}

esp_err_t config_store_factory_reset(void)
{
    update_cached_location(false, "", 0.0, 0.0);

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
        ESP_LOGW(TAG, "Factory reset completed: AP password restored to '%s', STA and location cleared",
                 WIFI_AP_DEFAULT_PASSWORD);
    }
    return err;
}
