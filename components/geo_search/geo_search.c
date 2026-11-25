#include "geo_search.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"

static const char *TAG = "geo_search";

#define GEO_API_HOST                "geocoding-api.open-meteo.com"
#define GEO_API_PORT_STR            "443"
#define GEO_PROBE_TIMEOUT_MS        2500
#define GEO_PROBE_RETRY_MS          30000U
#define GEO_PROBE_TASK_STACK_BYTES  4096U
#define GEO_PROBE_TASK_PRIO         (tskIDLE_PRIORITY + 2U)
#define GEO_HTTP_TIMEOUT_MS         4500
#define GEO_RESP_BUF_SIZE           12288U

typedef struct {
    char *buf;
    size_t buf_size;
    size_t len;
    bool overflowed;
} geo_http_rx_ctx_t;

static SemaphoreHandle_t s_state_mutex = NULL;
static SemaphoreHandle_t s_search_mutex = NULL;
static TaskHandle_t s_probe_task = NULL;
static bool s_sta_connected = false;
static geo_internet_status_t s_internet = {0};

/* Static fixed response buffer guarded exclusively by s_search_mutex (Constitution Principle III) */
static char s_resp_buf[GEO_RESP_BUF_SIZE];

static bool tcp_probe_geocoding_host(void)
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *res = NULL;
    int gai_err = getaddrinfo(GEO_API_HOST, GEO_API_PORT_STR, &hints, &res);
    if (gai_err != 0 || res == NULL) {
        ESP_LOGD(TAG, "Internet probe DNS lookup failed (err=%d)", gai_err);
        return false;
    }

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        freeaddrinfo(res);
        return false;
    }

    bool reachable = false;
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags >= 0) {
        (void)fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    }

    int conn_res = connect(sock, res->ai_addr, res->ai_addrlen);
    if (conn_res == 0) {
        reachable = true;
    } else if (errno == EINPROGRESS) {
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(sock, &wfds);

        struct timeval tv;
        tv.tv_sec = GEO_PROBE_TIMEOUT_MS / 1000;
        tv.tv_usec = (GEO_PROBE_TIMEOUT_MS % 1000) * 1000;

        int sel = select(sock + 1, NULL, &wfds, NULL, &tv);
        if (sel > 0) {
            int so_err = 0;
            socklen_t so_len = sizeof(so_err);
            if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_err, &so_len) == 0 && so_err == 0) {
                reachable = true;
            }
        }
    }

    close(sock);
    freeaddrinfo(res);
    return reachable;
}

/*
 * Auxiliary background internet reachability probe task (T006).
 * Intentionally NOT subscribed to TWDT so 30s/portMAX_DELAY ulTaskNotifyTake waits
 * yield cleanly to Idle without triggering the 10s TWDT timeout.
 */
static void geo_probe_task(void *arg)
{
    (void)arg;

    for (;;) {
        bool should_probe = false;
        TickType_t wait_ticks = portMAX_DELAY;

        if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (s_sta_connected && !s_internet.available) {
                wait_ticks = pdMS_TO_TICKS(GEO_PROBE_RETRY_MS);
            }
            xSemaphoreGive(s_state_mutex);
        }

        uint32_t notified = ulTaskNotifyTake(pdTRUE, wait_ticks);

        if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (s_sta_connected && (notified > 0U || !s_internet.available)) {
                s_internet.checking = true;
                should_probe = true;
            }
            xSemaphoreGive(s_state_mutex);
        }

        if (!should_probe) {
            continue;
        }

        bool ok = tcp_probe_geocoding_host();
        int64_t now_sec = esp_timer_get_time() / 1000000LL;

        if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            s_internet.checking = false;
            if (s_sta_connected) {
                s_internet.available = ok;
                s_internet.checked = true;
                s_internet.last_check_uptime_sec = now_sec;
            } else {
                s_internet.available = false;
                s_internet.checked = false;
            }
            xSemaphoreGive(s_state_mutex);
        }

        ESP_LOGI(TAG, "Internet reachability probe (%s:%s): %s",
                 GEO_API_HOST, GEO_API_PORT_STR, ok ? "ONLINE" : "OFFLINE");
    }
}

esp_err_t geo_search_init(void)
{
    if (s_state_mutex != NULL) {
        return ESP_OK;
    }

    s_state_mutex = xSemaphoreCreateMutex();
    s_search_mutex = xSemaphoreCreateMutex();
    if (s_state_mutex == NULL || s_search_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    memset(&s_internet, 0, sizeof(s_internet));
    s_sta_connected = false;

    BaseType_t ok = xTaskCreate(&geo_probe_task,
                                "geo_probe",
                                GEO_PROBE_TASK_STACK_BYTES,
                                NULL,
                                GEO_PROBE_TASK_PRIO,
                                &s_probe_task);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create geo_probe background task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "geo_search subsystem initialized");
    return ESP_OK;
}

void geo_search_notify_sta_connected(void)
{
    if (s_state_mutex == NULL) {
        return;
    }

    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        s_sta_connected = true;
        s_internet.available = false;
        s_internet.checked = false;
        s_internet.checking = true;
        xSemaphoreGive(s_state_mutex);
    }

    if (s_probe_task != NULL) {
        xTaskNotifyGive(s_probe_task);
    }
}

void geo_search_notify_sta_disconnected(void)
{
    if (s_state_mutex == NULL) {
        return;
    }

    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        s_sta_connected = false;
        s_internet.available = false;
        s_internet.checked = false;
        s_internet.checking = false;
        xSemaphoreGive(s_state_mutex);
    }
}

void geo_search_get_internet_status(geo_internet_status_t *out_status)
{
    if (out_status == NULL) {
        return;
    }
    memset(out_status, 0, sizeof(*out_status));

    if (s_state_mutex == NULL) {
        return;
    }

    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        *out_status = s_internet;
        xSemaphoreGive(s_state_mutex);
    }
}

uint32_t geo_search_get_probe_stack_hwm_bytes(void)
{
    if (s_probe_task == NULL) {
        return 0U;
    }
    return (uint32_t)(uxTaskGetStackHighWaterMark(s_probe_task) * sizeof(StackType_t));
}

static esp_err_t geo_http_event_handler(esp_http_client_event_t *evt)
{
    geo_http_rx_ctx_t *ctx = (geo_http_rx_ctx_t *)evt->user_data;
    if (ctx == NULL) {
        return ESP_OK;
    }

    if (evt->event_id == HTTP_EVENT_ON_DATA && evt->data != NULL && evt->data_len > 0) {
        size_t chunk = (size_t)evt->data_len;
        if (ctx->len + chunk >= ctx->buf_size) {
            ctx->overflowed = true;
            return ESP_FAIL;
        }
        memcpy(ctx->buf + ctx->len, evt->data, chunk);
        ctx->len += chunk;
        ctx->buf[ctx->len] = '\0';
    }
    return ESP_OK;
}

static void update_internet_after_search(bool online_ok, bool trigger_reprobe)
{
    if (s_state_mutex == NULL) {
        return;
    }

    int64_t now_sec = esp_timer_get_time() / 1000000LL;
    bool sta_active = false;

    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        sta_active = s_sta_connected;
        if (sta_active) {
            s_internet.available = online_ok;
            s_internet.checked = true;
            s_internet.checking = false;
            s_internet.last_check_uptime_sec = now_sec;
        } else {
            s_internet.available = false;
            s_internet.checked = false;
            s_internet.checking = false;
        }
        xSemaphoreGive(s_state_mutex);
    }

    if (trigger_reprobe && sta_active && !online_ok && s_probe_task != NULL) {
        xTaskNotifyGive(s_probe_task);
    }
}

geo_search_result_t geo_search_query_ukraine(const char *query,
                                             geo_search_item_t *out_items,
                                             size_t max_items,
                                             size_t *out_count)
{
    if (out_count != NULL) {
        *out_count = 0U;
    }
    if (query == NULL || out_items == NULL || out_count == NULL || max_items == 0U) {
        return GEO_SEARCH_ERR_INVALID_QUERY;
    }

    char trimmed[GEO_QUERY_MAX_BYTES + 1U];
    if (!geo_search_validate_query(query, trimmed, sizeof(trimmed))) {
        return GEO_SEARCH_ERR_INVALID_QUERY;
    }

    bool sta_active = false;
    if (s_state_mutex != NULL && xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        sta_active = s_sta_connected;
        xSemaphoreGive(s_state_mutex);
    }
    if (!sta_active) {
        return GEO_SEARCH_ERR_OFFLINE;
    }

    if (s_search_mutex == NULL || xSemaphoreTake(s_search_mutex, 0) != pdTRUE) {
        return GEO_SEARCH_ERR_BUSY;
    }

    char encoded[GEO_QUERY_MAX_BYTES * 3U + 1U];
    if (geo_search_url_encode(trimmed, encoded, sizeof(encoded)) != ESP_OK) {
        xSemaphoreGive(s_search_mutex);
        return GEO_SEARCH_ERR_INVALID_QUERY;
    }

    char url[320];
    int written = snprintf(url, sizeof(url),
                           "https://%s/v1/search?name=%s&countryCode=UA&language=uk&count=%u",
                           GEO_API_HOST, encoded, (unsigned)GEO_SEARCH_MAX_RESULTS);
    if (written <= 0 || (size_t)written >= sizeof(url)) {
        xSemaphoreGive(s_search_mutex);
        return GEO_SEARCH_ERR_INVALID_QUERY;
    }

    s_resp_buf[0] = '\0';
    geo_http_rx_ctx_t rx_ctx = {
        .buf = s_resp_buf,
        .buf_size = sizeof(s_resp_buf),
        .len = 0U,
        .overflowed = false,
    };

    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = GEO_HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = geo_http_event_handler,
        .user_data = &rx_ctx,
        .buffer_size = 2048,
        .buffer_size_tx = 1024,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        xSemaphoreGive(s_search_mutex);
        return GEO_SEARCH_ERR_UPSTREAM;
    }

    /* Request uncompressed JSON to keep RAM overhead deterministic */
    esp_http_client_set_header(client, "Accept-Encoding", "identity");
    esp_http_client_set_header(client, "Accept", "application/json");

    int64_t t_start_us = esp_timer_get_time();
    esp_err_t http_err = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);
    int64_t elapsed_ms = (esp_timer_get_time() - t_start_us) / 1000LL;

    esp_http_client_cleanup(client);

    geo_search_result_t result = GEO_SEARCH_ERR_UPSTREAM;

    if (http_err != ESP_OK) {
        ESP_LOGW(TAG, "Open-Meteo request failed (%s) in %lld ms", esp_err_to_name(http_err), (long long)elapsed_ms);
        if (http_err == ESP_ERR_HTTP_EAGAIN || http_err == ESP_ERR_TIMEOUT ||
            elapsed_ms >= (int64_t)(GEO_HTTP_TIMEOUT_MS - 150)) {
            result = GEO_SEARCH_ERR_TIMEOUT;
        } else if (http_err == ESP_ERR_HTTP_CONNECT) {
            update_internet_after_search(false, true);
            result = GEO_SEARCH_ERR_OFFLINE;
        } else {
            result = GEO_SEARCH_ERR_UPSTREAM;
        }
    } else if (rx_ctx.overflowed || status_code != 200) {
        ESP_LOGW(TAG, "Open-Meteo HTTP status=%d overflow=%d (%lld ms)",
                 status_code, (int)rx_ctx.overflowed, (long long)elapsed_ms);
        result = GEO_SEARCH_ERR_UPSTREAM;
    } else {
        esp_err_t parse_err = geo_search_parse_open_meteo_json(s_resp_buf, out_items, max_items, out_count);
        if (parse_err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to parse Open-Meteo JSON payload (%u bytes)", (unsigned)rx_ctx.len);
            result = GEO_SEARCH_ERR_UPSTREAM;
        } else {
            update_internet_after_search(true, false);
            ESP_LOGI(TAG, "Search '%s' -> %u UA result(s) in %lld ms (%u B)",
                     trimmed, (unsigned)(*out_count), (long long)elapsed_ms, (unsigned)rx_ctx.len);
            result = GEO_SEARCH_OK;
        }
    }

    xSemaphoreGive(s_search_mutex);
    return result;
}
