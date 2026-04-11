#include "unity.h"
#include <stdint.h>
#include <string>

void setUp(void) {}
void tearDown(void) {}

// ---------------------------------------------------------------------------
// OTA rollback eligibility predicate — native unit tests
//
// Mirrors the guard used in CWWebServer::getOtaStatus() and handleOtaRollback().
// The enum values below match esp_ota_img_states_t from ESP-IDF esp_ota_ops.h.
// A partition is eligible to be booted / rolled back into when its OTA state is
// NOT explicitly marked bad (INVALID or ABORTED). UNDEFINED covers factory
// partitions and slots that were never OTA'd — these have a valid image header
// and are bootable.
// ---------------------------------------------------------------------------
enum test_ota_state_t : uint32_t {
    OTA_STATE_NEW            = 0x0U,
    OTA_STATE_PENDING_VERIFY = 0x1U,
    OTA_STATE_VALID          = 0x2U,
    OTA_STATE_INVALID        = 0x3U,
    OTA_STATE_ABORTED        = 0x4U,
    OTA_STATE_UNDEFINED      = 0xFFFFFFFFU,
};

static bool ota_partition_is_rollback_eligible(test_ota_state_t state) {
    return state != OTA_STATE_INVALID && state != OTA_STATE_ABORTED;
}

void test_ota_rollback_eligible_new(void) {
    TEST_ASSERT_TRUE(ota_partition_is_rollback_eligible(OTA_STATE_NEW));
}

void test_ota_rollback_eligible_pending(void) {
    TEST_ASSERT_TRUE(ota_partition_is_rollback_eligible(OTA_STATE_PENDING_VERIFY));
}

void test_ota_rollback_eligible_valid(void) {
    TEST_ASSERT_TRUE(ota_partition_is_rollback_eligible(OTA_STATE_VALID));
}

void test_ota_rollback_eligible_undefined(void) {
    // UNDEFINED = factory slot or slot never OTA'd; bootable
    TEST_ASSERT_TRUE(ota_partition_is_rollback_eligible(OTA_STATE_UNDEFINED));
}

void test_ota_rollback_blocked_invalid(void) {
    TEST_ASSERT_FALSE(ota_partition_is_rollback_eligible(OTA_STATE_INVALID));
}

void test_ota_rollback_blocked_aborted(void) {
    TEST_ASSERT_FALSE(ota_partition_is_rollback_eligible(OTA_STATE_ABORTED));
}

// ---------------------------------------------------------------------------
// Night window predicate — mirrors main.cpp::isNightTime() behavior
// ---------------------------------------------------------------------------
static bool is_night_window(int now_h, int now_m, int start_h, int start_m, int end_h, int end_m) {
    const int now = now_h * 60 + now_m;
    const int start = start_h * 60 + start_m;
    const int end = end_h * 60 + end_m;
    if (start < end) {
        return now >= start && now < end;
    }
    return now >= start || now < end;
}

void test_night_window_same_day_inside(void) {
    TEST_ASSERT_TRUE(is_night_window(23, 0, 22, 0, 23, 59));
}

void test_night_window_same_day_outside(void) {
    TEST_ASSERT_FALSE(is_night_window(21, 30, 22, 0, 23, 59));
}

void test_night_window_wrap_inside_evening(void) {
    TEST_ASSERT_TRUE(is_night_window(23, 30, 22, 0, 7, 0));
}

void test_night_window_wrap_inside_morning(void) {
    TEST_ASSERT_TRUE(is_night_window(6, 30, 22, 0, 7, 0));
}

void test_night_window_wrap_outside(void) {
    TEST_ASSERT_FALSE(is_night_window(12, 0, 22, 0, 7, 0));
}

// ---------------------------------------------------------------------------
// URL decode helper behavior — mirrors CWWebServer::urlDecode()
// ---------------------------------------------------------------------------
static std::string web_url_decode(std::string v) {
    auto replace_all = [](std::string& s, const std::string& from, const std::string& to) {
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.length(), to);
            pos += to.length();
        }
    };

    replace_all(v, "%2F", "/");
    replace_all(v, "%2f", "/");
    replace_all(v, "%3A", ":");
    replace_all(v, "%3a", ":");
    replace_all(v, "%20", " ");
    replace_all(v, "%40", "@");
    replace_all(v, "%2B", "+");
    replace_all(v, "%2b", "+");
    replace_all(v, "%2C", ",");
    replace_all(v, "%2c", ",");
    return v;
}

void test_url_decode_timezone(void) {
    TEST_ASSERT_EQUAL_STRING("Europe/Stockholm", web_url_decode("Europe%2FStockholm").c_str());
}

void test_url_decode_symbols(void) {
    TEST_ASSERT_EQUAL_STRING("a+b,c:d @home", web_url_decode("a%2Bb%2Cc%3Ad%20%40home").c_str());
}

// ---------------------------------------------------------------------------
// OTA version normalization — mirrors CWOTA comparison behavior
// ---------------------------------------------------------------------------
static std::string normalize_version(std::string v) {
    if (!v.empty() && v[0] == 'v') {
        return v.substr(1);
    }
    return v;
}

static bool ota_update_available(const std::string& current, const std::string& remote) {
    return normalize_version(current) != normalize_version(remote);
}

void test_version_normalization_equivalent(void) {
    TEST_ASSERT_FALSE(ota_update_available("2.8.1", "v2.8.1"));
}

void test_version_normalization_update_available(void) {
    TEST_ASSERT_TRUE(ota_update_available("2.8.1", "v2.9.0"));
}

int runUnityTests(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ota_rollback_eligible_new);
    RUN_TEST(test_ota_rollback_eligible_pending);
    RUN_TEST(test_ota_rollback_eligible_valid);
    RUN_TEST(test_ota_rollback_eligible_undefined);
    RUN_TEST(test_ota_rollback_blocked_invalid);
    RUN_TEST(test_ota_rollback_blocked_aborted);
    RUN_TEST(test_night_window_same_day_inside);
    RUN_TEST(test_night_window_same_day_outside);
    RUN_TEST(test_night_window_wrap_inside_evening);
    RUN_TEST(test_night_window_wrap_inside_morning);
    RUN_TEST(test_night_window_wrap_outside);
    RUN_TEST(test_url_decode_timezone);
    RUN_TEST(test_url_decode_symbols);
    RUN_TEST(test_version_normalization_equivalent);
    RUN_TEST(test_version_normalization_update_available);
    return UNITY_END();
}

int main() {
    return runUnityTests();
}
