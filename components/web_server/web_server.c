#include "web_server.h"

#include <stdio.h>
#include <string.h>
#include "cJSON.h"
#include "config_store.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "wifi_manager.h"

static const char *TAG = "web_server";

#define HTTP_MAX_POST_BODY_LEN  256U

/* Embedded static assets from components/web_server/assets/ (EMBED_TXTFILES) */
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");
extern const uint8_t style_css_start[]  asm("_binary_style_css_start");
extern const uint8_t style_css_end[]    asm("_binary_style_css_end");
extern const uint8_t app_js_start[]     asm("_binary_app_js_start");
extern const uint8_t app_js_end[]       asm("_binary_app_js_end");

static httpd_handle_t s_server = NULL;

esp_err_t web_server_parse_sta_connect_json(const char *json_str,
                                            char *out_ssid, size_t ssid_buf_len,
                                            char *out_pass, size_t pass_buf_len)
{
    if (json_str == NULL || out_ssid == NULL || out_pass == NULL ||
        ssid_buf_len <= WIFI_SSID_MAX_LEN || pass_buf_len <= WIFI_PASS_MAX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const cJSON *ssid_item = cJSON_GetObjectItemCaseSensitive(root, "ssid");
    const cJSON *pass_item = cJSON_GetObjectItemCaseSensitive(root, "password");

    if (!cJSON_IsString(ssid_item) || ssid_item->valuestring == NULL) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    const char *ssid_val = ssid_item->valuestring;
    const char *pass_val = "";
    if (pass_item != NULL && !cJSON_IsNull(pass_item)) {
        if (!cJSON_IsString(pass_item) || pass_item->valuestring == NULL) {
            cJSON_Delete(root);
            return ESP_ERR_INVALID_ARG;
        }
        pass_val = pass_item->valuestring;
    }

    if (!config_store_validate_ssid(ssid_val) ||
        !config_store_validate_password(pass_val, true)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(out_ssid, ssid_val, WIFI_SSID_MAX_LEN);
    out_ssid[WIFI_SSID_MAX_LEN] = '\0';
    strncpy(out_pass, pass_val, WIFI_PASS_MAX_LEN);
    out_pass[WIFI_PASS_MAX_LEN] = '\0';

    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t web_server_parse_ap_password_json(const char *json_str,
                                            char *out_pass, size_t pass_buf_len)
{
    if (json_str == NULL || out_pass == NULL || pass_buf_len <= WIFI_PASS_MAX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const cJSON *pass_item = cJSON_GetObjectItemCaseSensitive(root, "password");
    if (!cJSON_IsString(pass_item) || pass_item->valuestring == NULL ||
        !config_store_validate_password(pass_item->valuestring, false)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(out_pass, pass_item->valuestring, WIFI_PASS_MAX_LEN);
    out_pass[WIFI_PASS_MAX_LEN] = '\0';

    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t send_json_response(httpd_req_t *req, const char *status, cJSON *json_obj)
{
    char *rendered = cJSON_PrintUnformatted(json_obj);
    if (rendered == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON serialization failed");
        return ESP_ERR_NO_MEM;
    }

    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t err = httpd_resp_sendstr(req, rendered);
    cJSON_free(rendered);
    return err;
}

static esp_err_t read_request_body(httpd_req_t *req, char *buf, size_t buf_size)
{
    if (req->content_len == 0U || req->content_len >= buf_size) {
        return ESP_ERR_INVALID_SIZE;
    }

    size_t received = 0U;
    while (received < req->content_len) {
        int ret = httpd_req_recv(req, buf + received, req->content_len - received);
        if (ret <= 0) {
            return ESP_FAIL;
        }
        received += (size_t)ret;
    }
    buf[received] = '\0';
    return ESP_OK;
}

static esp_err_t handle_get_index(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    size_t len = (size_t)(index_html_end - index_html_start);
    if (len > 0U && index_html_start[len - 1U] == '\0') {
        len--;
    }
    return httpd_resp_send(req, (const char *)index_html_start, (ssize_t)len);
}

static esp_err_t handle_get_style(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/css; charset=utf-8");
    size_t len = (size_t)(style_css_end - style_css_start);
    if (len > 0U && style_css_start[len - 1U] == '\0') {
        len--;
    }
    return httpd_resp_send(req, (const char *)style_css_start, (ssize_t)len);
}

static esp_err_t handle_get_app_js(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/javascript; charset=utf-8");
    size_t len = (size_t)(app_js_end - app_js_start);
    if (len > 0U && app_js_start[len - 1U] == '\0') {
        len--;
    }
    return httpd_resp_send(req, (const char *)app_js_start, (ssize_t)len);
}

static esp_err_t handle_get_status(httpd_req_t *req)
{
    wifi_runtime_status_t st = {0};
    wifi_manager_get_status(&st);

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }

    cJSON_AddStringToObject(root, "fsm_state", wifi_fsm_state_to_str(st.fsm_state));

    cJSON *sta = cJSON_AddObjectToObject(root, "sta");
    if (sta != NULL) {
        cJSON_AddBoolToObject(sta, "configured", st.sta_configured);
        cJSON_AddBoolToObject(sta, "connected", st.sta_connected);
        cJSON_AddStringToObject(sta, "ssid", st.sta_ssid);
        cJSON_AddStringToObject(sta, "ip", st.sta_ip);
        cJSON_AddNumberToObject(sta, "rssi", st.sta_rssi);
        cJSON_AddStringToObject(sta, "last_error", wifi_disconnect_reason_to_str(st.last_error_reason));
    }

    cJSON *ap = cJSON_AddObjectToObject(root, "ap");
    if (ap != NULL) {
        cJSON_AddStringToObject(ap, "ssid", st.ap_ssid);
        cJSON_AddStringToObject(ap, "ip", st.ap_ip);
        cJSON_AddNumberToObject(ap, "client_count", st.ap_client_count);
        cJSON_AddNumberToObject(ap, "channel", st.ap_channel);
    }

    cJSON *sys = cJSON_AddObjectToObject(root, "system");
    if (sys != NULL) {
        uint64_t uptime_sec = (uint64_t)(esp_timer_get_time() / 1000000ULL);
        cJSON_AddNumberToObject(sys, "uptime_sec", (double)uptime_sec);
        cJSON_AddNumberToObject(sys, "free_heap_bytes", (double)esp_get_free_heap_size());
        cJSON_AddNumberToObject(sys, "min_free_heap_bytes", (double)esp_get_minimum_free_heap_size());
        cJSON_AddStringToObject(sys, "mdns_host", WIFI_MDNS_FQDN);
    }

    esp_err_t err = send_json_response(req, "200 OK", root);
    cJSON_Delete(root);
    return err;
}

static esp_err_t handle_get_wifi_scan(httpd_req_t *req)
{
    wifi_scan_item_t items[WIFI_MAX_SCAN_RESULTS];
    size_t count = 0U;
    wifi_manager_scan(items, WIFI_MAX_SCAN_RESULTS, &count);

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }

    cJSON *arr = cJSON_AddArrayToObject(root, "networks");
    if (arr != NULL) {
        for (size_t i = 0U; i < count; ++i) {
            cJSON *net = cJSON_CreateObject();
            if (net != NULL) {
                cJSON_AddStringToObject(net, "ssid", items[i].ssid);
                cJSON_AddNumberToObject(net, "rssi", items[i].rssi);
                cJSON_AddNumberToObject(net, "channel", items[i].channel);
                cJSON_AddBoolToObject(net, "secure", items[i].secure);
                cJSON_AddItemToArray(arr, net);
            }
        }
    }

    esp_err_t err = send_json_response(req, "200 OK", root);
    cJSON_Delete(root);
    return err;
}

static esp_err_t handle_post_wifi_connect(httpd_req_t *req)
{
    char body[HTTP_MAX_POST_BODY_LEN];
    if (read_request_body(req, body, sizeof(body)) != ESP_OK) {
        cJSON *err_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(err_obj, "error", "INVALID_PAYLOAD");
        cJSON_AddStringToObject(err_obj, "message", "Некоректний розмір або формат запиту.");
        esp_err_t res = send_json_response(req, "400 Bad Request", err_obj);
        cJSON_Delete(err_obj);
        return res;
    }

    char ssid[WIFI_SSID_MAX_LEN + 1U] = {0};
    char pass[WIFI_PASS_MAX_LEN + 1U] = {0};

    if (web_server_parse_sta_connect_json(body, ssid, sizeof(ssid), pass, sizeof(pass)) != ESP_OK) {
        cJSON *err_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(err_obj, "error", "INVALID_INPUT");
        cJSON_AddStringToObject(err_obj, "message",
            "SSID має містити від 1 до 32 символів, а пароль — від 8 до 63 символів (або бути порожнім для відкритої мережі).");
        esp_err_t res = send_json_response(req, "400 Bad Request", err_obj);
        cJSON_Delete(err_obj);
        return res;
    }

    esp_err_t conn_err = wifi_manager_connect_sta(ssid, pass);
    cJSON *resp = cJSON_CreateObject();
    if (conn_err != ESP_OK) {
        cJSON_AddStringToObject(resp, "error", "CONNECT_FAILED");
        cJSON_AddStringToObject(resp, "message", "Не вдалося застосувати конфігурацію Wi-Fi.");
        esp_err_t res = send_json_response(req, "500 Internal Server Error", resp);
        cJSON_Delete(resp);
        return res;
    }

    char msg[160];
    snprintf(msg, sizeof(msg), "Параметри збережено. Виконується підключення до %s...", ssid);
    cJSON_AddStringToObject(resp, "status", "connecting");
    cJSON_AddStringToObject(resp, "message", msg);

    esp_err_t res = send_json_response(req, "200 OK", resp);
    cJSON_Delete(resp);
    return res;
}

static esp_err_t handle_post_ap_password(httpd_req_t *req)
{
    char body[HTTP_MAX_POST_BODY_LEN];
    if (read_request_body(req, body, sizeof(body)) != ESP_OK) {
        cJSON *err_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(err_obj, "error", "INVALID_PAYLOAD");
        cJSON_AddStringToObject(err_obj, "message", "Некоректний розмір запиту.");
        esp_err_t res = send_json_response(req, "400 Bad Request", err_obj);
        cJSON_Delete(err_obj);
        return res;
    }

    char new_pass[WIFI_PASS_MAX_LEN + 1U] = {0};
    if (web_server_parse_ap_password_json(body, new_pass, sizeof(new_pass)) != ESP_OK) {
        cJSON *err_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(err_obj, "error", "INVALID_PASSWORD");
        cJSON_AddStringToObject(err_obj, "message",
            "Пароль точки доступу має містити від 8 до 63 друкованих ASCII символів.");
        esp_err_t res = send_json_response(req, "400 Bad Request", err_obj);
        cJSON_Delete(err_obj);
        return res;
    }

    esp_err_t err = wifi_manager_update_ap_password(new_pass);
    cJSON *resp = cJSON_CreateObject();
    if (err != ESP_OK) {
        cJSON_AddStringToObject(resp, "error", "UPDATE_FAILED");
        cJSON_AddStringToObject(resp, "message", "Не вдалося оновити пароль точки доступу.");
        esp_err_t res = send_json_response(req, "500 Internal Server Error", resp);
        cJSON_Delete(resp);
        return res;
    }

    cJSON_AddStringToObject(resp, "status", "updated");
    cJSON_AddStringToObject(resp, "message", "Пароль точки доступу 'ESP Weather Clock' успішно оновлено.");
    esp_err_t res = send_json_response(req, "200 OK", resp);
    cJSON_Delete(resp);
    return res;
}

static esp_err_t handle_post_wifi_reset(httpd_req_t *req)
{
    wifi_manager_apply_factory_reset();

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", "reset");
    cJSON_AddStringToObject(resp, "message",
        "Налаштування скинуто до заводських (пароль точки доступу: weather1).");
    esp_err_t res = send_json_response(req, "200 OK", resp);
    cJSON_Delete(resp);
    return res;
}

static esp_err_t handle_captive_portal_redirect(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t handle_http_404_or_captive(httpd_req_t *req, httpd_err_code_t err)
{
    (void)err;
    char host[64] = {0};
    if (httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host)) == ESP_OK) {
        wifi_runtime_status_t st = {0};
        wifi_manager_get_status(&st);
        /* T025: If request is targeting the LAN IP or weather-clock.local, return standard 404 */
        if ((st.sta_connected && strstr(host, st.sta_ip) != NULL) ||
            strstr(host, WIFI_MDNS_HOSTNAME) != NULL) {
            return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Route not found");
        }
    }
    /* For unknown routes/hosts arriving via SoftAP Captive Portal, redirect to root UI */
    return handle_captive_portal_redirect(req);
}

esp_err_t web_server_start(void)
{
    if (s_server != NULL) {
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 14;
    config.lru_purge_enable = true;
    config.stack_size = 6144;

    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
        return err;
    }

    static const httpd_uri_t uri_handlers[] = {
        { .uri = "/",                    .method = HTTP_GET,  .handler = handle_get_index,              .user_ctx = NULL },
        { .uri = "/style.css",           .method = HTTP_GET,  .handler = handle_get_style,              .user_ctx = NULL },
        { .uri = "/app.js",              .method = HTTP_GET,  .handler = handle_get_app_js,             .user_ctx = NULL },
        { .uri = "/api/status",          .method = HTTP_GET,  .handler = handle_get_status,             .user_ctx = NULL },
        { .uri = "/api/health",          .method = HTTP_GET,  .handler = handle_get_status,             .user_ctx = NULL },
        { .uri = "/api/wifi/scan",       .method = HTTP_GET,  .handler = handle_get_wifi_scan,          .user_ctx = NULL },
        { .uri = "/api/wifi/connect",    .method = HTTP_POST, .handler = handle_post_wifi_connect,      .user_ctx = NULL },
        { .uri = "/api/wifi/ap-password",.method = HTTP_POST, .handler = handle_post_ap_password,       .user_ctx = NULL },
        { .uri = "/api/wifi/reset",      .method = HTTP_POST, .handler = handle_post_wifi_reset,        .user_ctx = NULL },
        /* Captive Portal detection endpoints for Android, iOS/macOS, and Windows */
        { .uri = "/generate_204",        .method = HTTP_GET,  .handler = handle_captive_portal_redirect,.user_ctx = NULL },
        { .uri = "/hotspot-detect.html", .method = HTTP_GET,  .handler = handle_captive_portal_redirect,.user_ctx = NULL },
        { .uri = "/connecttest.txt",     .method = HTTP_GET,  .handler = handle_captive_portal_redirect,.user_ctx = NULL },
    };

    for (size_t i = 0U; i < sizeof(uri_handlers) / sizeof(uri_handlers[0]); ++i) {
        ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &uri_handlers[i]));
    }

    httpd_register_err_handler(s_server, HTTPD_404_NOT_FOUND, handle_http_404_or_captive);

    ESP_LOGI(TAG, "Built-in HTTP web server started on 0.0.0.0:80 (AP: http://%s/, LAN mDNS: http://%s/)",
             WIFI_AP_DEFAULT_IP, WIFI_MDNS_FQDN);
    return ESP_OK;
}

void web_server_stop(void)
{
    if (s_server != NULL) {
        httpd_stop(s_server);
        s_server = NULL;
    }
}
