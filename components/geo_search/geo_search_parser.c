#include "geo_search.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "cJSON.h"

static size_t utf8_seq_len(uint8_t lead)
{
    if ((lead & 0x80U) == 0x00U) {
        return 1U;
    }
    if ((lead & 0xE0U) == 0xC0U) {
        return 2U;
    }
    if ((lead & 0xF0U) == 0xE0U) {
        return 3U;
    }
    if ((lead & 0xF8U) == 0xF0U) {
        return 4U;
    }
    return 1U;
}

size_t geo_search_utf8_char_count(const char *str)
{
    if (str == NULL) {
        return 0U;
    }

    size_t count = 0U;
    const uint8_t *p = (const uint8_t *)str;
    while (*p != '\0') {
        if ((*p & 0xC0U) != 0x80U) {
            count++;
        }
        p++;
    }
    return count;
}

void geo_search_utf8_truncate(const char *src, char *dst, size_t dst_size, size_t max_bytes)
{
    if (dst == NULL || dst_size == 0U) {
        return;
    }
    dst[0] = '\0';
    if (src == NULL || max_bytes == 0U) {
        return;
    }

    size_t limit = dst_size - 1U;
    if (max_bytes < limit) {
        limit = max_bytes;
    }

    size_t src_len = strlen(src);
    size_t in_pos = 0U;
    size_t out_pos = 0U;

    while (in_pos < src_len) {
        uint8_t b = (uint8_t)src[in_pos];

        /* Skip ASCII control characters */
        if (b < 0x20U || b == 0x7FU) {
            in_pos++;
            continue;
        }

        size_t seq = utf8_seq_len(b);
        if (in_pos + seq > src_len) {
            /* Incomplete trailing UTF-8 sequence in input */
            break;
        }

        /* Verify continuation bytes for multi-byte sequences */
        bool valid_seq = true;
        for (size_t k = 1U; k < seq; ++k) {
            if (((uint8_t)src[in_pos + k] & 0xC0U) != 0x80U) {
                valid_seq = false;
                break;
            }
        }
        if (!valid_seq) {
            in_pos++;
            continue;
        }

        if (out_pos + seq > limit) {
            break;
        }

        memcpy(dst + out_pos, src + in_pos, seq);
        out_pos += seq;
        in_pos += seq;
    }

    dst[out_pos] = '\0';
}

bool geo_search_validate_query(const char *raw_query, char *out_trimmed, size_t trimmed_size)
{
    if (raw_query == NULL || out_trimmed == NULL || trimmed_size <= GEO_QUERY_MAX_BYTES) {
        return false;
    }

    /* Reject any ASCII control characters anywhere in the raw query */
    for (const uint8_t *p = (const uint8_t *)raw_query; *p != '\0'; ++p) {
        if (*p < 0x20U || *p == 0x7FU) {
            return false;
        }
    }

    /* Trim leading and trailing ASCII spaces */
    const char *start = raw_query;
    while (*start == ' ') {
        start++;
    }

    size_t len = strlen(start);
    while (len > 0U && start[len - 1U] == ' ') {
        len--;
    }

    if (len == 0U || len > GEO_QUERY_MAX_BYTES) {
        return false;
    }

    memcpy(out_trimmed, start, len);
    out_trimmed[len] = '\0';

    if (geo_search_utf8_char_count(out_trimmed) < GEO_QUERY_MIN_CHARS) {
        out_trimmed[0] = '\0';
        return false;
    }

    return true;
}

static bool is_rfc3986_unreserved(uint8_t c)
{
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') ||
           c == '-' || c == '_' || c == '.' || c == '~';
}

esp_err_t geo_search_url_encode(const char *src, char *dst, size_t dst_size)
{
    if (src == NULL || dst == NULL || dst_size == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    static const char HEX[] = "0123456789ABCDEF";
    size_t out_pos = 0U;

    for (const uint8_t *p = (const uint8_t *)src; *p != '\0'; ++p) {
        if (is_rfc3986_unreserved(*p)) {
            if (out_pos + 1U >= dst_size) {
                dst[0] = '\0';
                return ESP_ERR_INVALID_SIZE;
            }
            dst[out_pos++] = (char)(*p);
        } else {
            if (out_pos + 3U >= dst_size) {
                dst[0] = '\0';
                return ESP_ERR_INVALID_SIZE;
            }
            dst[out_pos++] = '%';
            dst[out_pos++] = HEX[(*p >> 4) & 0x0FU];
            dst[out_pos++] = HEX[*p & 0x0FU];
        }
    }

    dst[out_pos] = '\0';
    return ESP_OK;
}

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    return -1;
}

esp_err_t geo_search_url_decode(const char *src, char *dst, size_t dst_size)
{
    if (src == NULL || dst == NULL || dst_size == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t out_pos = 0U;
    for (size_t i = 0U; src[i] != '\0'; ++i) {
        char decoded;
        if (src[i] == '+') {
            decoded = ' ';
        } else if (src[i] == '%') {
            int hi = hex_nibble(src[i + 1U]);
            int lo = (hi >= 0) ? hex_nibble(src[i + 2U]) : -1;
            if (hi < 0 || lo < 0) {
                dst[0] = '\0';
                return ESP_ERR_INVALID_ARG;
            }
            uint8_t byte_val = (uint8_t)((hi << 4) | lo);
            if (byte_val == 0U) {
                dst[0] = '\0';
                return ESP_ERR_INVALID_ARG;
            }
            decoded = (char)byte_val;
            i += 2U;
        } else {
            decoded = src[i];
        }

        if (out_pos + 1U >= dst_size) {
            dst[0] = '\0';
            return ESP_ERR_INVALID_SIZE;
        }
        dst[out_pos++] = decoded;
    }

    dst[out_pos] = '\0';
    return ESP_OK;
}

static bool is_ukraine_country_code(const char *code)
{
    if (code == NULL) {
        return false;
    }
    return (code[0] == 'U' || code[0] == 'u') &&
           (code[1] == 'A' || code[1] == 'a') &&
           code[2] == '\0';
}

esp_err_t geo_search_parse_open_meteo_json(const char *json_str,
                                           geo_search_item_t *out_items,
                                           size_t max_items,
                                           size_t *out_count)
{
    if (json_str == NULL || out_items == NULL || out_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_count = 0U;

    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL || !cJSON_IsObject(root)) {
        if (root != NULL) {
            cJSON_Delete(root);
        }
        return ESP_ERR_INVALID_ARG;
    }

    const cJSON *results = cJSON_GetObjectItemCaseSensitive(root, "results");
    if (results == NULL || cJSON_IsNull(results)) {
        /* Open-Meteo omits "results" when no matching locations are found */
        cJSON_Delete(root);
        return ESP_OK;
    }

    if (!cJSON_IsArray(results)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    size_t cap = max_items;
    if (cap > GEO_SEARCH_MAX_RESULTS) {
        cap = GEO_SEARCH_MAX_RESULTS;
    }

    size_t written = 0U;
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, results) {
        if (written >= cap) {
            break;
        }
        if (!cJSON_IsObject(entry)) {
            continue;
        }

        const cJSON *cc_item = cJSON_GetObjectItemCaseSensitive(entry, "country_code");
        if (!cJSON_IsString(cc_item) || !is_ukraine_country_code(cc_item->valuestring)) {
            continue;
        }

        const cJSON *name_item = cJSON_GetObjectItemCaseSensitive(entry, "name");
        const cJSON *lat_item = cJSON_GetObjectItemCaseSensitive(entry, "latitude");
        const cJSON *lon_item = cJSON_GetObjectItemCaseSensitive(entry, "longitude");

        if (!cJSON_IsString(name_item) || name_item->valuestring == NULL ||
            !cJSON_IsNumber(lat_item) || !cJSON_IsNumber(lon_item)) {
            continue;
        }

        double lat = lat_item->valuedouble;
        double lon = lon_item->valuedouble;
        if (!isfinite(lat) || !isfinite(lon) ||
            lat < -90.0 || lat > 90.0 ||
            lon < -180.0 || lon > 180.0) {
            continue;
        }

        geo_search_item_t item;
        memset(&item, 0, sizeof(item));
        geo_search_utf8_truncate(name_item->valuestring, item.name, sizeof(item.name), LOC_NAME_MAX_LEN);

        if (!config_store_validate_location(item.name, lat, lon)) {
            continue;
        }

        const cJSON *admin1_item = cJSON_GetObjectItemCaseSensitive(entry, "admin1");
        if (cJSON_IsString(admin1_item) && admin1_item->valuestring != NULL) {
            geo_search_utf8_truncate(admin1_item->valuestring, item.admin1, sizeof(item.admin1), GEO_ADMIN_MAX_LEN);
        }

        const cJSON *admin2_item = cJSON_GetObjectItemCaseSensitive(entry, "admin2");
        if (cJSON_IsString(admin2_item) && admin2_item->valuestring != NULL) {
            geo_search_utf8_truncate(admin2_item->valuestring, item.admin2, sizeof(item.admin2), GEO_ADMIN_MAX_LEN);
        }

        item.latitude = lat;
        item.longitude = lon;
        out_items[written++] = item;
    }

    *out_count = written;
    cJSON_Delete(root);
    return ESP_OK;
}
