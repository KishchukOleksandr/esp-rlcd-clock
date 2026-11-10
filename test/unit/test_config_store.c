#include <string.h>
#include "unity.h"
#include "config_store.h"

TEST_CASE("config_store defaults initialize fixed AP password and empty STA", "[config_store]")
{
    wifi_stored_config_t cfg;
    config_store_get_defaults(&cfg);

    TEST_ASSERT_EQUAL_UINT8(CONFIG_STORE_SCHEMA_VERSION, cfg.schema_version);
    TEST_ASSERT_FALSE(cfg.sta_configured);
    TEST_ASSERT_EQUAL_STRING("", cfg.sta_ssid);
    TEST_ASSERT_EQUAL_STRING("", cfg.sta_password);
    TEST_ASSERT_EQUAL_STRING("weather1", cfg.ap_password);
    TEST_ASSERT_EQUAL_STRING("ESP Weather Clock", WIFI_AP_FIXED_SSID);
}

TEST_CASE("config_store validates SSID bounds 1..32 bytes (FR-006)", "[config_store]")
{
    TEST_ASSERT_FALSE(config_store_validate_ssid(NULL));
    TEST_ASSERT_FALSE(config_store_validate_ssid(""));
    TEST_ASSERT_TRUE(config_store_validate_ssid("A"));
    TEST_ASSERT_TRUE(config_store_validate_ssid("Home_WiFi_2.4G"));
    TEST_ASSERT_TRUE(config_store_validate_ssid("12345678901234567890123456789012")); /* 32 chars */
    TEST_ASSERT_FALSE(config_store_validate_ssid("123456789012345678901234567890123")); /* 33 chars */
}

TEST_CASE("config_store validates STA and AP passwords (FR-002, FR-006)", "[config_store]")
{
    /* Open network allowed for STA, forbidden for SoftAP */
    TEST_ASSERT_TRUE(config_store_validate_password("", true));
    TEST_ASSERT_FALSE(config_store_validate_password("", false));

    /* Too short (<8 chars) rejected for both */
    TEST_ASSERT_FALSE(config_store_validate_password("short77", true));
    TEST_ASSERT_FALSE(config_store_validate_password("short77", false));

    /* Default "weather1" (8 chars) valid for both */
    TEST_ASSERT_TRUE(config_store_validate_password("weather1", true));
    TEST_ASSERT_TRUE(config_store_validate_password("weather1", false));

    /* 63 chars valid, 64 chars rejected */
    const char *pass_63 = "123456789012345678901234567890123456789012345678901234567890123";
    const char *pass_64 = "1234567890123456789012345678901234567890123456789012345678901234";
    TEST_ASSERT_TRUE(config_store_validate_password(pass_63, false));
    TEST_ASSERT_FALSE(config_store_validate_password(pass_64, false));
}
