#include <string.h>
#include "geo_search.h"
#include "unity.h"

TEST_CASE("geo_search validates Ukrainian UTF-8 queries and rejects invalid inputs", "[geo_search]")
{
    char trimmed[GEO_QUERY_MAX_BYTES + 1U] = {0};

    /* Valid 2-char Cyrillic and complex Ukrainian city names (apostrophe, hyphen, spaces) */
    TEST_ASSERT_TRUE(geo_search_validate_query("Ки", trimmed, sizeof(trimmed)));
    TEST_ASSERT_EQUAL_STRING("Ки", trimmed);

    TEST_ASSERT_TRUE(geo_search_validate_query("  Кам'янець-Подільський  ", trimmed, sizeof(trimmed)));
    TEST_ASSERT_EQUAL_STRING("Кам'янець-Подільський", trimmed);

    TEST_ASSERT_TRUE(geo_search_validate_query("Івано-Франківськ", trimmed, sizeof(trimmed)));
    TEST_ASSERT_EQUAL_STRING("Івано-Франківськ", trimmed);

    TEST_ASSERT_TRUE(geo_search_validate_query("Біла Церква", trimmed, sizeof(trimmed)));
    TEST_ASSERT_EQUAL_STRING("Біла Церква", trimmed);

    /* Reject empty, whitespace-only, 1 Cyrillic char (2 UTF-8 bytes!), control chars, and >64 bytes */
    TEST_ASSERT_FALSE(geo_search_validate_query("", trimmed, sizeof(trimmed)));
    TEST_ASSERT_FALSE(geo_search_validate_query("   ", trimmed, sizeof(trimmed)));
    TEST_ASSERT_FALSE(geo_search_validate_query("К", trimmed, sizeof(trimmed)));
    TEST_ASSERT_FALSE(geo_search_validate_query("  Ї  ", trimmed, sizeof(trimmed)));
    TEST_ASSERT_FALSE(geo_search_validate_query("Київ\n", trimmed, sizeof(trimmed)));
    TEST_ASSERT_FALSE(geo_search_validate_query("Київ\x01", trimmed, sizeof(trimmed)));

    /* 65 ASCII bytes exceeds GEO_QUERY_MAX_BYTES (64) */
    const char *too_long = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    TEST_ASSERT_FALSE(geo_search_validate_query(too_long, trimmed, sizeof(trimmed)));
}

TEST_CASE("geo_search percent-encodes and decodes UTF-8 Ukrainian queries safely", "[geo_search]")
{
    char encoded[192] = {0};
    char decoded[80] = {0};

    TEST_ASSERT_EQUAL(ESP_OK, geo_search_url_encode("Біла Церква", encoded, sizeof(encoded)));
    TEST_ASSERT_EQUAL(ESP_OK, geo_search_url_decode(encoded, decoded, sizeof(decoded)));
    TEST_ASSERT_EQUAL_STRING("Біла Церква", decoded);

    TEST_ASSERT_EQUAL(ESP_OK, geo_search_url_encode("Кам'янець-Подільський", encoded, sizeof(encoded)));
    TEST_ASSERT_EQUAL(ESP_OK, geo_search_url_decode(encoded, decoded, sizeof(decoded)));
    TEST_ASSERT_EQUAL_STRING("Кам'янець-Подільський", decoded);

    /* '+' decoded as space */
    TEST_ASSERT_EQUAL(ESP_OK, geo_search_url_decode("Bila+Tserkva", decoded, sizeof(decoded)));
    TEST_ASSERT_EQUAL_STRING("Bila Tserkva", decoded);

    /* Malformed percent escapes and embedded NUL (%00) rejected */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, geo_search_url_decode("Kyiv%2", decoded, sizeof(decoded)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, geo_search_url_decode("Kyiv%ZZ", decoded, sizeof(decoded)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, geo_search_url_decode("Kyiv%00x", decoded, sizeof(decoded)));

    /* Small destination buffer rejected */
    char tiny[4] = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, geo_search_url_encode("Київ", tiny, sizeof(tiny)));
}

TEST_CASE("geo_search truncates multi-byte UTF-8 names cleanly on character boundaries", "[geo_search]")
{
    /* 40 Cyrillic chars = 80 bytes UTF-8; truncating to 64 bytes must yield 32 full Cyrillic chars (64 bytes) */
    const char *long_ua = "АБВГҐДЕЄЖЗИІЇЙКЛМНОПРСТУФХЦЧШЩЬЮЯАБВГҐДЕ";
    char out[LOC_NAME_MAX_LEN + 1U] = {0};

    geo_search_utf8_truncate(long_ua, out, sizeof(out), LOC_NAME_MAX_LEN);
    TEST_ASSERT_EQUAL_UINT32(64U, (uint32_t)strlen(out));
    TEST_ASSERT_EQUAL_UINT32(32U, (uint32_t)geo_search_utf8_char_count(out));

    /* Odd byte cap (5 bytes) on 2-byte Cyrillic chars must stop at 4 bytes (2 chars), never splitting a char */
    char odd_out[16] = {0};
    geo_search_utf8_truncate("Київ", odd_out, sizeof(odd_out), 5U);
    TEST_ASSERT_EQUAL_STRING("Ки", odd_out);
}

TEST_CASE("geo_search parses Open-Meteo JSON, keeps admin1/admin2, and filters strictly to UA", "[geo_search]")
{
    const char *sample_json =
        "{"
        "\"results\":["
        "  {\"name\":\"Вишневе\",\"country_code\":\"UA\",\"admin1\":\"Київська область\",\"admin2\":\"Бучанський район\",\"latitude\":50.3891,\"longitude\":30.3705},"
        "  {\"name\":\"Warsaw\",\"country_code\":\"PL\",\"admin1\":\"Masovia\",\"latitude\":52.2297,\"longitude\":21.0122},"
        "  {\"name\":\"Вишневе\",\"country_code\":\"UA\",\"admin1\":\"Дніпропетровська область\",\"admin2\":\"Кам'янський район\",\"latitude\":48.4383,\"longitude\":33.9175},"
        "  {\"name\":\"BadCoord\",\"country_code\":\"UA\",\"admin1\":\"Тест\",\"latitude\":120.0,\"longitude\":30.0}"
        "]"
        "}";

    geo_search_item_t items[GEO_SEARCH_MAX_RESULTS];
    size_t count = 0U;

    esp_err_t err = geo_search_parse_open_meteo_json(sample_json, items, GEO_SEARCH_MAX_RESULTS, &count);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL_UINT32(2U, (uint32_t)count);

    TEST_ASSERT_EQUAL_STRING("Вишневе", items[0].name);
    TEST_ASSERT_EQUAL_STRING("Київська область", items[0].admin1);
    TEST_ASSERT_EQUAL_STRING("Бучанський район", items[0].admin2);
    TEST_ASSERT_DOUBLE_WITHIN(1e-4, 50.3891, items[0].latitude);
    TEST_ASSERT_DOUBLE_WITHIN(1e-4, 30.3705, items[0].longitude);

    TEST_ASSERT_EQUAL_STRING("Вишневе", items[1].name);
    TEST_ASSERT_EQUAL_STRING("Дніпропетровська область", items[1].admin1);
    TEST_ASSERT_EQUAL_STRING("Кам'янський район", items[1].admin2);
    TEST_ASSERT_DOUBLE_WITHIN(1e-4, 48.4383, items[1].latitude);
    TEST_ASSERT_DOUBLE_WITHIN(1e-4, 33.9175, items[1].longitude);
}

TEST_CASE("geo_search handles empty and malformed Open-Meteo JSON responses", "[geo_search]")
{
    geo_search_item_t items[GEO_SEARCH_MAX_RESULTS];
    size_t count = 99U;

    /* Open-Meteo omits "results" when nothing matches */
    TEST_ASSERT_EQUAL(ESP_OK,
        geo_search_parse_open_meteo_json("{\"generationtime_ms\":0.25}", items, GEO_SEARCH_MAX_RESULTS, &count));
    TEST_ASSERT_EQUAL_UINT32(0U, (uint32_t)count);

    /* Empty results array */
    TEST_ASSERT_EQUAL(ESP_OK,
        geo_search_parse_open_meteo_json("{\"results\":[]}", items, GEO_SEARCH_MAX_RESULTS, &count));
    TEST_ASSERT_EQUAL_UINT32(0U, (uint32_t)count);

    /* Malformed JSON */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
        geo_search_parse_open_meteo_json("{not valid json", items, GEO_SEARCH_MAX_RESULTS, &count));

    /* Non-array results field */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
        geo_search_parse_open_meteo_json("{\"results\":\"bad\"}", items, GEO_SEARCH_MAX_RESULTS, &count));
}
