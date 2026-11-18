#include "unity.h"
#include "web_server.h"

TEST_CASE("web_server parses valid STA connect JSON payloads", "[web_server]")
{
    char ssid[WIFI_SSID_MAX_LEN + 1U] = {0};
    char pass[WIFI_PASS_MAX_LEN + 1U] = {0};

    /* WPA2 network */
    esp_err_t err = web_server_parse_sta_connect_json(
        "{\"ssid\":\"Home_WiFi\",\"password\":\"secret1234\"}",
        ssid, sizeof(ssid), pass, sizeof(pass));
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL_STRING("Home_WiFi", ssid);
    TEST_ASSERT_EQUAL_STRING("secret1234", pass);

    /* Open network (empty password) */
    err = web_server_parse_sta_connect_json(
        "{\"ssid\":\"OpenCafe\",\"password\":\"\"}",
        ssid, sizeof(ssid), pass, sizeof(pass));
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL_STRING("OpenCafe", ssid);
    TEST_ASSERT_EQUAL_STRING("", pass);
}

TEST_CASE("web_server rejects malformed or out-of-bounds JSON payloads", "[web_server]")
{
    char ssid[WIFI_SSID_MAX_LEN + 1U] = {0};
    char pass[WIFI_PASS_MAX_LEN + 1U] = {0};

    /* Empty SSID */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
        web_server_parse_sta_connect_json("{\"ssid\":\"\",\"password\":\"weather1\"}",
                                          ssid, sizeof(ssid), pass, sizeof(pass)));

    /* Short STA password (1..7 chars) */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
        web_server_parse_sta_connect_json("{\"ssid\":\"Home\",\"password\":\"12345\"}",
                                          ssid, sizeof(ssid), pass, sizeof(pass)));

    /* Malformed JSON */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
        web_server_parse_sta_connect_json("{invalid json",
                                          ssid, sizeof(ssid), pass, sizeof(pass)));

    /* SoftAP password validation */
    TEST_ASSERT_EQUAL(ESP_OK,
        web_server_parse_ap_password_json("{\"password\":\"weather1\"}", pass, sizeof(pass)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
        web_server_parse_ap_password_json("{\"password\":\"\"}", pass, sizeof(pass)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
        web_server_parse_ap_password_json("{\"password\":\"short\"}", pass, sizeof(pass)));
}

TEST_CASE("web_server parses and validates POST /api/location JSON payloads (002-location-config)", "[web_server]")
{
    char loc_name[LOC_NAME_MAX_LEN + 1U] = {0};
    double lat = 0.0;
    double lon = 0.0;

    /* Valid Ukrainian settlement payload */
    esp_err_t err = web_server_parse_location_json(
        "{\"name\":\"Львів\",\"latitude\":49.8397,\"longitude\":24.0297}",
        loc_name, sizeof(loc_name), &lat, &lon);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL_STRING("Львів", loc_name);
    TEST_ASSERT_DOUBLE_WITHIN(1e-4, 49.8397, lat);
    TEST_ASSERT_DOUBLE_WITHIN(1e-4, 24.0297, lon);

    /* Boundary values (-90, 180) */
    err = web_server_parse_location_json(
        "{\"name\":\"Межа\",\"latitude\":-90.0,\"longitude\":180.0}",
        loc_name, sizeof(loc_name), &lat, &lon);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, -90.0, lat);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 180.0, lon);

    /* Reject empty name, string coordinates, out-of-range coordinates, or malformed JSON */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
        web_server_parse_location_json("{\"name\":\"\",\"latitude\":50.45,\"longitude\":30.52}",
                                       loc_name, sizeof(loc_name), &lat, &lon));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
        web_server_parse_location_json("{\"name\":\"Київ\",\"latitude\":\"50.45\",\"longitude\":30.52}",
                                       loc_name, sizeof(loc_name), &lat, &lon));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
        web_server_parse_location_json("{\"name\":\"Київ\",\"latitude\":95.0,\"longitude\":30.52}",
                                       loc_name, sizeof(loc_name), &lat, &lon));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
        web_server_parse_location_json("{\"name\":\"Київ\",\"latitude\":50.45,\"longitude\":-190.0}",
                                       loc_name, sizeof(loc_name), &lat, &lon));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
        web_server_parse_location_json("{bad json",
                                       loc_name, sizeof(loc_name), &lat, &lon));
}
