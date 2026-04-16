#include "unity.h"
#include "core/CWLogic.h"

void setUp(void) {}
void tearDown(void) {}

// ── OTA rollback eligibility (cw::ota::isRollbackEligible) ──────────────

void test_ota_rollback_eligible_new(void) {
    TEST_ASSERT_TRUE(cw::ota::isRollbackEligible(cw::ota::STATE_NEW));
}

void test_ota_rollback_eligible_pending(void) {
    TEST_ASSERT_TRUE(cw::ota::isRollbackEligible(cw::ota::STATE_PENDING_VERIFY));
}

void test_ota_rollback_eligible_valid(void) {
    TEST_ASSERT_TRUE(cw::ota::isRollbackEligible(cw::ota::STATE_VALID));
}

void test_ota_rollback_eligible_undefined(void) {
    TEST_ASSERT_TRUE(cw::ota::isRollbackEligible(cw::ota::STATE_UNDEFINED));
}

void test_ota_rollback_blocked_invalid(void) {
    TEST_ASSERT_FALSE(cw::ota::isRollbackEligible(cw::ota::STATE_INVALID));
}

void test_ota_rollback_blocked_aborted(void) {
    TEST_ASSERT_FALSE(cw::ota::isRollbackEligible(cw::ota::STATE_ABORTED));
}

// ── OTA mark-valid predicate (cw::ota::shouldMarkValid) ─────────────────

void test_ota_mark_valid_only_when_pending(void) {
    TEST_ASSERT_TRUE(cw::ota::shouldMarkValid(cw::ota::STATE_PENDING_VERIFY));
    TEST_ASSERT_FALSE(cw::ota::shouldMarkValid(cw::ota::STATE_VALID));
    TEST_ASSERT_FALSE(cw::ota::shouldMarkValid(cw::ota::STATE_NEW));
    TEST_ASSERT_FALSE(cw::ota::shouldMarkValid(cw::ota::STATE_UNDEFINED));
}

// ── Night window predicate (cw::isNightWindow) ─────────────────────────

void test_night_window_same_day_inside(void) {
    TEST_ASSERT_TRUE(cw::isNightWindow(23, 0, 22, 0, 23, 59));
}

void test_night_window_same_day_outside(void) {
    TEST_ASSERT_FALSE(cw::isNightWindow(21, 30, 22, 0, 23, 59));
}

void test_night_window_wrap_inside_evening(void) {
    TEST_ASSERT_TRUE(cw::isNightWindow(23, 30, 22, 0, 7, 0));
}

void test_night_window_wrap_inside_morning(void) {
    TEST_ASSERT_TRUE(cw::isNightWindow(6, 30, 22, 0, 7, 0));
}

void test_night_window_wrap_outside(void) {
    TEST_ASSERT_FALSE(cw::isNightWindow(12, 0, 22, 0, 7, 0));
}

void test_night_window_exact_start_is_inside(void) {
    TEST_ASSERT_TRUE(cw::isNightWindow(22, 0, 22, 0, 7, 0));
}

void test_night_window_exact_end_is_outside(void) {
    TEST_ASSERT_FALSE(cw::isNightWindow(7, 0, 22, 0, 7, 0));
}

// ── URL decode (cw::urlDecoded) ─────────────────────────────────────────

void test_url_decode_timezone(void) {
    TEST_ASSERT_EQUAL_STRING("Europe/Stockholm", cw::urlDecoded("Europe%2FStockholm").c_str());
}

void test_url_decode_symbols(void) {
    TEST_ASSERT_EQUAL_STRING("a+b,c:d @home", cw::urlDecoded("a%2Bb%2Cc%3Ad%20%40home").c_str());
}

void test_url_decode_lowercase_percent(void) {
    TEST_ASSERT_EQUAL_STRING("Europe/Stockholm", cw::urlDecoded("Europe%2fStockholm").c_str());
}

void test_url_decode_no_encoding(void) {
    TEST_ASSERT_EQUAL_STRING("plain text", cw::urlDecoded("plain text").c_str());
}

// ── Sensitive key check (cw::isSensitiveKey) ────────────────────────────

void test_sensitive_key_wifiPwd(void) {
    TEST_ASSERT_TRUE(cw::isSensitiveKey("wifiPwd"));
}

void test_sensitive_key_mqttPass(void) {
    TEST_ASSERT_TRUE(cw::isSensitiveKey("mqttPass"));
}

void test_sensitive_key_normal(void) {
    TEST_ASSERT_FALSE(cw::isSensitiveKey("displayBright"));
}

// ── Version normalization (cw::normalizeVersion, cw::otaUpdateAvailable) ─

void test_version_normalization_strips_v(void) {
    TEST_ASSERT_EQUAL_STRING("2.8.1", cw::normalizeVersion("v2.8.1").c_str());
}

void test_version_normalization_no_v(void) {
    TEST_ASSERT_EQUAL_STRING("2.8.1", cw::normalizeVersion("2.8.1").c_str());
}

void test_version_update_not_available_same(void) {
    TEST_ASSERT_FALSE(cw::otaUpdateAvailable("2.8.1", "v2.8.1"));
}

void test_version_update_available_different(void) {
    TEST_ASSERT_TRUE(cw::otaUpdateAvailable("2.8.1", "v2.9.0"));
}

// ── MQTT node ID sanitization (cw::sanitizeMqttNodeId) ──────────────────

void test_mqtt_node_id_simple(void) {
    TEST_ASSERT_EQUAL_STRING("mydevice", cw::sanitizeMqttNodeId("MyDevice").c_str());
}

void test_mqtt_node_id_spaces(void) {
    TEST_ASSERT_EQUAL_STRING("my_clock", cw::sanitizeMqttNodeId("My Clock").c_str());
}

void test_mqtt_node_id_special_chars(void) {
    TEST_ASSERT_EQUAL_STRING("abc123", cw::sanitizeMqttNodeId("abc!@#123").c_str());
}

void test_mqtt_node_id_hyphen_underscore_kept(void) {
    TEST_ASSERT_EQUAL_STRING("my-clock_1", cw::sanitizeMqttNodeId("my-clock_1").c_str());
}

void test_mqtt_node_id_empty(void) {
    TEST_ASSERT_EQUAL_STRING("", cw::sanitizeMqttNodeId("").c_str());
}

// ── Auto-change clockface (cw::nextAutoChangeIndex) ─────────────────────

void test_auto_change_sequence_wraps(void) {
    TEST_ASSERT_EQUAL_UINT8(0, cw::nextAutoChangeIndex(6, 7, 1, 0));
}

void test_auto_change_sequence_increments(void) {
    TEST_ASSERT_EQUAL_UINT8(3, cw::nextAutoChangeIndex(2, 7, 1, 0));
}

void test_auto_change_random_avoids_same(void) {
    TEST_ASSERT_EQUAL_UINT8(4, cw::nextAutoChangeIndex(3, 7, 2, 3));
}

void test_auto_change_random_different(void) {
    TEST_ASSERT_EQUAL_UINT8(5, cw::nextAutoChangeIndex(3, 7, 2, 5));
}

void test_auto_change_zero_count(void) {
    TEST_ASSERT_EQUAL_UINT8(0, cw::nextAutoChangeIndex(0, 0, 1, 0));
}

// ── Test runner ─────────────────────────────────────────────────────────

int runUnityTests(void) {
    UNITY_BEGIN();

    // OTA rollback
    RUN_TEST(test_ota_rollback_eligible_new);
    RUN_TEST(test_ota_rollback_eligible_pending);
    RUN_TEST(test_ota_rollback_eligible_valid);
    RUN_TEST(test_ota_rollback_eligible_undefined);
    RUN_TEST(test_ota_rollback_blocked_invalid);
    RUN_TEST(test_ota_rollback_blocked_aborted);
    RUN_TEST(test_ota_mark_valid_only_when_pending);

    // Night window
    RUN_TEST(test_night_window_same_day_inside);
    RUN_TEST(test_night_window_same_day_outside);
    RUN_TEST(test_night_window_wrap_inside_evening);
    RUN_TEST(test_night_window_wrap_inside_morning);
    RUN_TEST(test_night_window_wrap_outside);
    RUN_TEST(test_night_window_exact_start_is_inside);
    RUN_TEST(test_night_window_exact_end_is_outside);

    // URL decode
    RUN_TEST(test_url_decode_timezone);
    RUN_TEST(test_url_decode_symbols);
    RUN_TEST(test_url_decode_lowercase_percent);
    RUN_TEST(test_url_decode_no_encoding);

    // Sensitive key
    RUN_TEST(test_sensitive_key_wifiPwd);
    RUN_TEST(test_sensitive_key_mqttPass);
    RUN_TEST(test_sensitive_key_normal);

    // Version
    RUN_TEST(test_version_normalization_strips_v);
    RUN_TEST(test_version_normalization_no_v);
    RUN_TEST(test_version_update_not_available_same);
    RUN_TEST(test_version_update_available_different);

    // MQTT node ID
    RUN_TEST(test_mqtt_node_id_simple);
    RUN_TEST(test_mqtt_node_id_spaces);
    RUN_TEST(test_mqtt_node_id_special_chars);
    RUN_TEST(test_mqtt_node_id_hyphen_underscore_kept);
    RUN_TEST(test_mqtt_node_id_empty);

    // Auto-change
    RUN_TEST(test_auto_change_sequence_wraps);
    RUN_TEST(test_auto_change_sequence_increments);
    RUN_TEST(test_auto_change_random_avoids_same);
    RUN_TEST(test_auto_change_random_different);
    RUN_TEST(test_auto_change_zero_count);

    return UNITY_END();
}

int main() {
    return runUnityTests();
}
