#include <math.h>
#include <string.h>
#include "unity.h"
#include "config_store.h"

TEST_CASE("config_store defaults initialize fixed AP password, empty STA, and unconfigured location", "[config_store]")
{
    wifi_stored_config_t cfg;
    config_store_get_defaults(&cfg);

    TEST_ASSERT_EQUAL_UINT8(CONFIG_STORE_SCHEMA_VERSION, cfg.schema_version);
    TEST_ASSERT_FALSE(cfg.sta_configured);
    TEST_ASSERT_EQUAL_STRING("", cfg.sta_ssid);
    TEST_ASSERT_EQUAL_STRING("", cfg.sta_password);
    TEST_ASSERT_EQUAL_STRING("weather1", cfg.ap_password);
    TEST_ASSERT_EQUAL_STRING("ESP Weather Clock", WIFI_AP_FIXED_SSID);
    TEST_ASSERT_FALSE(cfg.loc_configured);
    TEST_ASSERT_EQUAL_STRING("", cfg.loc_name);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 0.0, cfg.latitude);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 0.0, cfg.longitude);
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

TEST_CASE("config_store validates Ukrainian settlement names and coordinates (002-location-config FR-004)", "[config_store]")
{
    /* Valid Ukrainian settlements and coordinates */
    TEST_ASSERT_TRUE(config_store_validate_location("Київ", 50.4501, 30.5234));
    TEST_ASSERT_TRUE(config_store_validate_location("Івано-Франківськ", 48.9226, 24.7111));
    TEST_ASSERT_TRUE(config_store_validate_location("Кам'янець-Подільський", 48.6788, 26.5852));

    /* Exact boundary coordinates (-90, 90, -180, 180, 0.0) */
    TEST_ASSERT_TRUE(config_store_validate_location("Екватор", 0.0, 0.0));
    TEST_ASSERT_TRUE(config_store_validate_location("Південний полюс", -90.0, -180.0));
    TEST_ASSERT_TRUE(config_store_validate_location("Північний полюс", 90.0, 180.0));

    /* Out-of-range coordinates rejected */
    TEST_ASSERT_FALSE(config_store_validate_location("Київ", -90.0001, 30.5234));
    TEST_ASSERT_FALSE(config_store_validate_location("Київ", 90.0001, 30.5234));
    TEST_ASSERT_FALSE(config_store_validate_location("Київ", 50.4501, -180.0001));
    TEST_ASSERT_FALSE(config_store_validate_location("Київ", 50.4501, 180.0001));
    TEST_ASSERT_FALSE(config_store_validate_location("Київ", NAN, 30.5234));
    TEST_ASSERT_FALSE(config_store_validate_location("Київ", 50.4501, INFINITY));

    /* Invalid names: NULL, empty, whitespace-only, control chars, >64 bytes */
    TEST_ASSERT_FALSE(config_store_validate_location(NULL, 50.4501, 30.5234));
    TEST_ASSERT_FALSE(config_store_validate_location("", 50.4501, 30.5234));
    TEST_ASSERT_FALSE(config_store_validate_location("    ", 50.4501, 30.5234));
    TEST_ASSERT_FALSE(config_store_validate_location("Київ\n", 50.4501, 30.5234));

    const char *name_64 = "1234567890123456789012345678901234567890123456789012345678901234";
    const char *name_65 = "12345678901234567890123456789012345678901234567890123456789012345";
    TEST_ASSERT_TRUE(config_store_validate_location(name_64, 50.4501, 30.5234));
    TEST_ASSERT_FALSE(config_store_validate_location(name_65, 50.4501, 30.5234));
}
