#include "unity.h"
#include "wifi_manager.h"

TEST_CASE("wifi_fsm transitions from boot and user connect requests", "[wifi_fsm]")
{
    TEST_ASSERT_EQUAL(WIFI_FSM_AP_PROVISIONING_MODE,
        wifi_fsm_compute_next_state(WIFI_FSM_UNINITIALIZED, WIFI_FSM_EV_BOOT_NO_STA_CFG, 0, false));

    TEST_ASSERT_EQUAL(WIFI_FSM_CONNECTING,
        wifi_fsm_compute_next_state(WIFI_FSM_UNINITIALIZED, WIFI_FSM_EV_BOOT_WITH_STA_CFG, 0, true));

    TEST_ASSERT_EQUAL(WIFI_FSM_CONNECTED_STA,
        wifi_fsm_compute_next_state(WIFI_FSM_CONNECTING, WIFI_FSM_EV_STA_GOT_IP, 1, true));
}

TEST_CASE("wifi_fsm pauses background reconnects when AP client is connected (FR-010)", "[wifi_fsm]")
{
    /* When 0 AP clients are connected, disconnect transitions to RECONNECT_BACKOFF */
    TEST_ASSERT_EQUAL(WIFI_FSM_RECONNECT_BACKOFF,
        wifi_fsm_compute_next_state(WIFI_FSM_CONNECTING, WIFI_FSM_EV_STA_DISCONNECTED, 0, true));

    /* When >=1 AP clients are connected, disconnect transitions to AP_PROVISIONING_MODE to prevent channel hopping */
    TEST_ASSERT_EQUAL(WIFI_FSM_AP_PROVISIONING_MODE,
        wifi_fsm_compute_next_state(WIFI_FSM_CONNECTING, WIFI_FSM_EV_STA_DISCONNECTED, 1, true));

    /* If an AP client joins during RECONNECT_BACKOFF, pause and enter AP_PROVISIONING_MODE */
    TEST_ASSERT_EQUAL(WIFI_FSM_AP_PROVISIONING_MODE,
        wifi_fsm_compute_next_state(WIFI_FSM_RECONNECT_BACKOFF, WIFI_FSM_EV_AP_CLIENT_CONNECTED, 1, true));

    /* Once all AP clients disconnect and STA is configured, resume RECONNECT_BACKOFF */
    TEST_ASSERT_EQUAL(WIFI_FSM_RECONNECT_BACKOFF,
        wifi_fsm_compute_next_state(WIFI_FSM_AP_PROVISIONING_MODE, WIFI_FSM_EV_AP_ALL_CLIENTS_LEFT, 0, true));

    /* Explicit user connect request always transitions to CONNECTING even with AP clients connected */
    TEST_ASSERT_EQUAL(WIFI_FSM_CONNECTING,
        wifi_fsm_compute_next_state(WIFI_FSM_AP_PROVISIONING_MODE, WIFI_FSM_EV_USER_CONNECT_REQ, 1, true));
}

TEST_CASE("wifi_fsm exponential backoff bounded at 60 seconds", "[wifi_fsm]")
{
    TEST_ASSERT_EQUAL_UINT32(2000UL,  wifi_fsm_compute_backoff_ms(0));
    TEST_ASSERT_EQUAL_UINT32(4000UL,  wifi_fsm_compute_backoff_ms(1));
    TEST_ASSERT_EQUAL_UINT32(8000UL,  wifi_fsm_compute_backoff_ms(2));
    TEST_ASSERT_EQUAL_UINT32(16000UL, wifi_fsm_compute_backoff_ms(3));
    TEST_ASSERT_EQUAL_UINT32(32000UL, wifi_fsm_compute_backoff_ms(4));
    TEST_ASSERT_EQUAL_UINT32(60000UL, wifi_fsm_compute_backoff_ms(5));
    TEST_ASSERT_EQUAL_UINT32(60000UL, wifi_fsm_compute_backoff_ms(20));
}
