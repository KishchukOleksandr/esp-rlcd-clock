#ifndef GEO_SEARCH_H
#define GEO_SEARCH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "config_store.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GEO_SEARCH_MAX_RESULTS  10U
#define GEO_QUERY_MIN_CHARS     2U
#define GEO_QUERY_MAX_BYTES     64U
#define GEO_ADMIN_MAX_LEN       64U

/**
 * @brief Single Ukrainian settlement candidate returned by online geocoding search.
 */
typedef struct {
    char name[LOC_NAME_MAX_LEN + 1U];
    char admin1[GEO_ADMIN_MAX_LEN + 1U];
    char admin2[GEO_ADMIN_MAX_LEN + 1U];
    double latitude;
    double longitude;
} geo_search_item_t;

/**
 * @brief Thread-safe snapshot of ESP32 internet reachability state (FR-002).
 */
typedef struct {
    bool available;
    bool checked;
    bool checking;
    int64_t last_check_uptime_sec;
} geo_internet_status_t;

/**
 * @brief Outcome codes for online settlement search (used to map HTTP status codes cleanly).
 */
typedef enum {
    GEO_SEARCH_OK = 0,
    GEO_SEARCH_ERR_INVALID_QUERY,
    GEO_SEARCH_ERR_OFFLINE,
    GEO_SEARCH_ERR_BUSY,
    GEO_SEARCH_ERR_TIMEOUT,
    GEO_SEARCH_ERR_UPSTREAM
} geo_search_result_t;

/**
 * @brief Initialize geo_search mutexes and background internet probe FreeRTOS task.
 *
 * @return ESP_OK on success.
 */
esp_err_t geo_search_init(void);

/**
 * @brief Notify geo_search that STA obtained an IP address and trigger an async internet probe.
 */
void geo_search_notify_sta_connected(void);

/**
 * @brief Notify geo_search that STA disconnected or factory reset occurred (resets status to offline).
 */
void geo_search_notify_sta_disconnected(void);

/**
 * @brief Get a thread-safe snapshot of current internet availability status.
 *
 * @param[out] out_status Destination status structure.
 */
void geo_search_get_internet_status(geo_internet_status_t *out_status);

/**
 * @brief Get stack high-water mark (in bytes) of the background internet probe task for diagnostics.
 *
 * @return High-water mark in bytes, or 0 if task is not initialized.
 */
uint32_t geo_search_get_probe_stack_hwm_bytes(void);

/**
 * @brief Perform a mutex-serialized HTTPS search for Ukrainian settlements via Open-Meteo Geocoding API.
 *
 * @param[in]  query      Raw UTF-8 search query (validated to >= 2 UTF-8 chars, <= 64 bytes).
 * @param[out] out_items  Caller-provided array for results.
 * @param[in]  max_items  Capacity of out_items (clamped to GEO_SEARCH_MAX_RESULTS).
 * @param[out] out_count  Number of Ukrainian settlements written to out_items.
 * @return GEO_SEARCH_OK on success (including 0 results), or specific error code.
 */
geo_search_result_t geo_search_query_ukraine(const char *query,
                                             geo_search_item_t *out_items,
                                             size_t max_items,
                                             size_t *out_count);

/* --- Pure unit-testable functions (geo_search_parser.c) --- */

/**
 * @brief Count UTF-8 code points (characters, not bytes) in a null-terminated string.
 */
size_t geo_search_utf8_char_count(const char *str);

/**
 * @brief Copy src into dst safely truncating on a UTF-8 character boundary to at most max_bytes bytes.
 *
 * Also strips ASCII control characters (< 0x20 or 0x7F).
 */
void geo_search_utf8_truncate(const char *src, char *dst, size_t dst_size, size_t max_bytes);

/**
 * @brief Validate and trim a raw UTF-8 search query (FR-009, FR-011).
 *
 * @param[in]  raw_query     Raw input string.
 * @param[out] out_trimmed   Destination buffer of size >= GEO_QUERY_MAX_BYTES + 1.
 * @param[in]  trimmed_size  Size of out_trimmed.
 * @return true if valid (>= 2 UTF-8 chars, <= 64 UTF-8 bytes, no control chars), false otherwise.
 */
bool geo_search_validate_query(const char *raw_query, char *out_trimmed, size_t trimmed_size);

/**
 * @brief RFC 3986 percent-encode a UTF-8 string into a fixed destination buffer.
 *
 * @param[in]  src      Null-terminated UTF-8 string.
 * @param[out] dst      Destination buffer.
 * @param[in]  dst_size Size of dst in bytes.
 * @return ESP_OK on success, ESP_ERR_INVALID_SIZE if dst is too small, ESP_ERR_INVALID_ARG on NULL.
 */
esp_err_t geo_search_url_encode(const char *src, char *dst, size_t dst_size);

/**
 * @brief Percent-decode a URL query parameter value into a fixed destination buffer.
 *
 * Treats '+' as space and decodes '%XX' hex sequences safely.
 *
 * @param[in]  src      Null-terminated URL-encoded string.
 * @param[out] dst      Destination buffer.
 * @param[in]  dst_size Size of dst in bytes.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on malformed escape or NULL, ESP_ERR_INVALID_SIZE if too small.
 */
esp_err_t geo_search_url_decode(const char *src, char *dst, size_t dst_size);

/**
 * @brief Parse Open-Meteo Geocoding JSON response, filtering strictly to country_code == "UA" (FR-003, FR-004).
 *
 * @param[in]  json_str   Null-terminated JSON body from Open-Meteo.
 * @param[out] out_items  Destination array of geo_search_item_t.
 * @param[in]  max_items  Capacity of out_items.
 * @param[out] out_count  Number of valid Ukrainian items parsed.
 * @return ESP_OK if JSON is valid (even when results is absent/empty -> *out_count == 0), ESP_ERR_INVALID_ARG on malformed JSON.
 */
esp_err_t geo_search_parse_open_meteo_json(const char *json_str,
                                           geo_search_item_t *out_items,
                                           size_t max_items,
                                           size_t *out_count);

#ifdef __cplusplus
}
#endif

#endif /* GEO_SEARCH_H */
