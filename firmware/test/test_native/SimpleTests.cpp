#include "unity.h"
#include <deque>
#include <stdint.h>
#include <string>
#include <vector>

#include "CWLogic.h"
#include "NightModeLogic.h"

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
// Night window predicate — mirrors DisplayControl night-window logic
// ---------------------------------------------------------------------------
void test_night_window_same_day_inside(void) {
    TEST_ASSERT_TRUE(cw::logic::isNightWindow(23, 0, 22, 0, 23, 59));
}

void test_night_window_same_day_outside(void) {
    TEST_ASSERT_FALSE(cw::logic::isNightWindow(21, 30, 22, 0, 23, 59));
}

void test_night_window_wrap_inside_evening(void) {
    TEST_ASSERT_TRUE(cw::logic::isNightWindow(23, 30, 22, 0, 7, 0));
}

void test_night_window_wrap_inside_morning(void) {
    TEST_ASSERT_TRUE(cw::logic::isNightWindow(6, 30, 22, 0, 7, 0));
}

void test_night_window_wrap_outside(void) {
    TEST_ASSERT_FALSE(cw::logic::isNightWindow(12, 0, 22, 0, 7, 0));
}

void test_night_mode_transition_enter(void) {
    TEST_ASSERT_EQUAL(
        static_cast<int>(cw::logic::NightModeTransition::kEnterNight),
        static_cast<int>(cw::logic::resolveNightModeTransition(false, true))
    );
}

void test_night_mode_transition_hold(void) {
    TEST_ASSERT_EQUAL(
        static_cast<int>(cw::logic::NightModeTransition::kStayNight),
        static_cast<int>(cw::logic::resolveNightModeTransition(true, true))
    );
}

void test_night_mode_transition_exit(void) {
    TEST_ASSERT_EQUAL(
        static_cast<int>(cw::logic::NightModeTransition::kExitNight),
        static_cast<int>(cw::logic::resolveNightModeTransition(true, false))
    );
}

void test_normal_brightness_target_fixed_mode(void) {
    cw::logic::BrightnessTarget target = cw::logic::resolveNormalBrightnessTarget(
        cw::logic::kBrightnessMethodFixed, true, false, 32, 8, 0, 0, 0);
    TEST_ASSERT_TRUE(target.hasValue);
    TEST_ASSERT_EQUAL_UINT8(32, target.brightness);
    TEST_ASSERT_EQUAL(32, target.slot);
}

void test_normal_brightness_target_time_mode_uses_scheduled_night_value(void) {
    cw::logic::BrightnessTarget target = cw::logic::resolveNormalBrightnessTarget(
        cw::logic::kBrightnessMethodTime, true, true, 32, 8, 0, 0, 0);
    TEST_ASSERT_TRUE(target.hasValue);
    TEST_ASSERT_EQUAL_UINT8(8, target.brightness);
    TEST_ASSERT_EQUAL(8, target.slot);
}

void test_normal_brightness_target_time_mode_requires_wifi(void) {
    cw::logic::BrightnessTarget target = cw::logic::resolveNormalBrightnessTarget(
        cw::logic::kBrightnessMethodTime, false, false, 32, 8, 0, 0, 0);
    TEST_ASSERT_FALSE(target.hasValue);
    TEST_ASSERT_EQUAL(-1, target.slot);
}

void test_normal_brightness_target_ldr_mode_maps_brightness(void) {
    cw::logic::BrightnessTarget target = cw::logic::resolveNormalBrightnessTarget(
        cw::logic::kBrightnessMethodLdr, true, false, 32, 8, 300, 100, 500);
    TEST_ASSERT_TRUE(target.hasValue);
    TEST_ASSERT_EQUAL(5, target.slot);
    TEST_ASSERT_EQUAL_UINT8(16, target.brightness);
}

void test_automatic_brightness_skips_small_ldr_slot_change(void) {
    const cw::logic::BrightnessTarget target = {true, 12, 5};
    TEST_ASSERT_FALSE(cw::logic::shouldApplyAutomaticBrightness(
        cw::logic::kBrightnessMethodLdr, 4, target));
}

void test_automatic_brightness_applies_large_ldr_slot_change(void) {
    const cw::logic::BrightnessTarget target = {true, 12, 6};
    TEST_ASSERT_TRUE(cw::logic::shouldApplyAutomaticBrightness(
        cw::logic::kBrightnessMethodLdr, 3, target));
}

void test_automatic_brightness_applies_time_mode_change(void) {
    const cw::logic::BrightnessTarget target = {true, 8, 8};
    TEST_ASSERT_TRUE(cw::logic::shouldApplyAutomaticBrightness(
        cw::logic::kBrightnessMethodTime, 32, target));
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

void test_sensitive_setting_detection(void) {
    TEST_ASSERT_TRUE(cw::logic::isSensitiveSetKey("wifiPwd"));
    TEST_ASSERT_TRUE(cw::logic::isSensitiveSetKey("mqttPass"));
    TEST_ASSERT_FALSE(cw::logic::isSensitiveSetKey("wifiSsid"));
}

void test_set_persistence_key_classifies_single_value_setting(void) {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(cw::logic::SetPersistenceKey::kDisplayBright),
        static_cast<int>(cw::logic::resolveSetPersistenceKey("displayBright")));
}

void test_set_persistence_key_classifies_compound_setting(void) {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(cw::logic::SetPersistenceKey::kAutoBright),
        static_cast<int>(cw::logic::resolveSetPersistenceKey("autoBright")));
}

void test_set_persistence_key_classifies_callback_driven_setting(void) {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(cw::logic::SetPersistenceKey::kClockFaceIndex),
        static_cast<int>(cw::logic::resolveSetPersistenceKey("clockFaceIndex")));
}

void test_set_persistence_key_rejects_unknown_setting(void) {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(cw::logic::SetPersistenceKey::kUnknown),
        static_cast<int>(cw::logic::resolveSetPersistenceKey("notARealSetting")));
}

void test_clockface_set_decision_accepts_successful_runtime_switch(void) {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(cw::logic::ClockfaceSetApplyDecision::kPersistMutation),
        static_cast<int>(cw::logic::resolveClockfaceSetApplyDecision(true, true)));
}

void test_clockface_set_decision_rejects_failed_runtime_switch(void) {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(cw::logic::ClockfaceSetApplyDecision::kReject),
        static_cast<int>(cw::logic::resolveClockfaceSetApplyDecision(true, false)));
}

void test_clockface_set_decision_persists_without_runtime_callback(void) {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(cw::logic::ClockfaceSetApplyDecision::kPersistMutation),
        static_cast<int>(cw::logic::resolveClockfaceSetApplyDecision(false, false)));
}

void test_sensitive_body_overrides_query_value(void) {
    cw::logic::SetRequestResolution resolved = cw::logic::resolveSetRequest("wifiPwd", "legacy", "new%20secret", true);
    TEST_ASSERT_EQUAL(cw::logic::SetRequestResolutionStatus::kResolvedFromBody, resolved.status);
    TEST_ASSERT_EQUAL_STRING("wifiPwd", resolved.key.c_str());
    TEST_ASSERT_EQUAL_STRING("new secret", resolved.value.c_str());
}

void test_sensitive_body_supports_form_value_with_query_key(void) {
    cw::logic::SetRequestResolution resolved = cw::logic::resolveSetRequest("mqttPass", "", "value=broker%2Bsecret", true);
    TEST_ASSERT_EQUAL(cw::logic::SetRequestResolutionStatus::kResolvedFromBody, resolved.status);
    TEST_ASSERT_EQUAL_STRING("mqttPass", resolved.key.c_str());
    TEST_ASSERT_EQUAL_STRING("broker+secret", resolved.value.c_str());
}

void test_sensitive_body_supports_key_value_form(void) {
    cw::logic::SetRequestResolution resolved = cw::logic::resolveSetRequest("", "", "key=wifiPwd&value=form+secret", true);
    TEST_ASSERT_EQUAL(cw::logic::SetRequestResolutionStatus::kResolvedFromBody, resolved.status);
    TEST_ASSERT_EQUAL_STRING("wifiPwd", resolved.key.c_str());
    TEST_ASSERT_EQUAL_STRING("form secret", resolved.value.c_str());
}

void test_non_sensitive_query_behavior_unchanged(void) {
    cw::logic::SetRequestResolution resolved = cw::logic::resolveSetRequest("displayBright", "42", "99", true);
    TEST_ASSERT_EQUAL(cw::logic::SetRequestResolutionStatus::kUseQuery, resolved.status);
    TEST_ASSERT_EQUAL_STRING("displayBright", resolved.key.c_str());
    TEST_ASSERT_EQUAL_STRING("42", resolved.value.c_str());
}

void test_sensitive_query_fallback_still_works(void) {
    cw::logic::SetRequestResolution resolved = cw::logic::resolveSetRequest("wifiPwd", "legacy%20secret", "", true);
    TEST_ASSERT_EQUAL(cw::logic::SetRequestResolutionStatus::kUseQuery, resolved.status);
    TEST_ASSERT_EQUAL_STRING("wifiPwd", resolved.key.c_str());
    TEST_ASSERT_EQUAL_STRING("legacy%20secret", resolved.value.c_str());
}

void test_incomplete_sensitive_body_is_rejected(void) {
    cw::logic::SetRequestResolution resolved = cw::logic::resolveSetRequest("wifiPwd", "legacy", "value=new", false);
    TEST_ASSERT_EQUAL(cw::logic::SetRequestResolutionStatus::kRejectIncompleteBody, resolved.status);
    TEST_ASSERT_EQUAL_STRING("wifiPwd", resolved.key.c_str());
    TEST_ASSERT_EQUAL_STRING("legacy", resolved.value.c_str());
}

void test_invalid_sensitive_form_body_is_rejected(void) {
    cw::logic::SetRequestResolution resolved = cw::logic::resolveSetRequest("wifiPwd", "legacy", "value", true);
    TEST_ASSERT_EQUAL(cw::logic::SetRequestResolutionStatus::kRejectInvalidBody, resolved.status);
    TEST_ASSERT_EQUAL_STRING("wifiPwd", resolved.key.c_str());
    TEST_ASSERT_EQUAL_STRING("legacy", resolved.value.c_str());
}

struct ScheduledBodyChunk {
    unsigned long atMs;
    std::string data;
};

class FakeRequestBodyTransport {
  public:
    FakeRequestBodyTransport(std::initializer_list<ScheduledBodyChunk> chunks)
        : scheduled(chunks.begin(), chunks.end()) {
        pump();
    }

    int available() {
        pump();
        return static_cast<int>(buffer.size());
    }

    size_t readInto(std::string& target, size_t remaining) {
        pump();
        size_t appended = 0;
        while (appended < remaining && !buffer.empty()) {
            target.push_back(buffer.front());
            buffer.pop_front();
            ++appended;
        }
        return appended;
    }

    void waitTick() {
        ++nowMs;
        pump();
    }

    unsigned long now() const {
        return nowMs;
    }

  private:
    void pump() {
        while (nextChunk < scheduled.size() && scheduled[nextChunk].atMs <= nowMs) {
            for (char ch : scheduled[nextChunk].data) {
                buffer.push_back(ch);
            }
            ++nextChunk;
        }
    }

    std::vector<ScheduledBodyChunk> scheduled;
    std::deque<char> buffer;
    size_t nextChunk = 0;
    unsigned long nowMs = 0;
};

void test_split_request_body_is_collected_within_receive_window(void) {
    FakeRequestBodyTransport transport({
        {0, "value="},
        {4, "broker"},
        {7, "%2Bsecret"},
    });

    cw::logic::RequestBodyReadResult body = cw::logic::readRequestBodyWithinWindow(
        std::string("value=broker%2Bsecret").size(),
        cw::logic::kSetRequestBodyReceiveWindowMs,
        [&transport]() { return transport.available(); },
        [&transport](std::string& target, size_t remaining) { return transport.readInto(target, remaining); },
        [&transport]() { transport.waitTick(); },
        [&transport]() { return transport.now(); });

    TEST_ASSERT_TRUE(body.complete);
    TEST_ASSERT_EQUAL_STRING("value=broker%2Bsecret", body.body.c_str());

    cw::logic::SetRequestResolution resolved = cw::logic::resolveSetRequest("mqttPass", "legacy", body.body, body.complete);
    TEST_ASSERT_EQUAL(cw::logic::SetRequestResolutionStatus::kResolvedFromBody, resolved.status);
    TEST_ASSERT_EQUAL_STRING("mqttPass", resolved.key.c_str());
    TEST_ASSERT_EQUAL_STRING("broker+secret", resolved.value.c_str());
}

void test_split_request_body_times_out_after_receive_window(void) {
    FakeRequestBodyTransport transport({
        {0, "value="},
        {cw::logic::kSetRequestBodyReceiveWindowMs + 5, "late"},
    });

    cw::logic::RequestBodyReadResult body = cw::logic::readRequestBodyWithinWindow(
        std::string("value=late").size(),
        cw::logic::kSetRequestBodyReceiveWindowMs,
        [&transport]() { return transport.available(); },
        [&transport](std::string& target, size_t remaining) { return transport.readInto(target, remaining); },
        [&transport]() { transport.waitTick(); },
        [&transport]() { return transport.now(); });

    TEST_ASSERT_FALSE(body.complete);
    TEST_ASSERT_EQUAL_STRING("value=", body.body.c_str());

    cw::logic::SetRequestResolution resolved = cw::logic::resolveSetRequest("wifiPwd", "legacy", body.body, body.complete);
    TEST_ASSERT_EQUAL(cw::logic::SetRequestResolutionStatus::kRejectIncompleteBody, resolved.status);
    TEST_ASSERT_EQUAL_STRING("wifiPwd", resolved.key.c_str());
    TEST_ASSERT_EQUAL_STRING("legacy", resolved.value.c_str());
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
    RUN_TEST(test_night_mode_transition_enter);
    RUN_TEST(test_night_mode_transition_hold);
    RUN_TEST(test_night_mode_transition_exit);
    RUN_TEST(test_normal_brightness_target_fixed_mode);
    RUN_TEST(test_normal_brightness_target_time_mode_uses_scheduled_night_value);
    RUN_TEST(test_normal_brightness_target_time_mode_requires_wifi);
    RUN_TEST(test_normal_brightness_target_ldr_mode_maps_brightness);
    RUN_TEST(test_automatic_brightness_skips_small_ldr_slot_change);
    RUN_TEST(test_automatic_brightness_applies_large_ldr_slot_change);
    RUN_TEST(test_automatic_brightness_applies_time_mode_change);
    RUN_TEST(test_url_decode_timezone);
    RUN_TEST(test_url_decode_symbols);
    RUN_TEST(test_version_normalization_equivalent);
    RUN_TEST(test_version_normalization_update_available);
    RUN_TEST(test_sensitive_setting_detection);
    RUN_TEST(test_set_persistence_key_classifies_single_value_setting);
    RUN_TEST(test_set_persistence_key_classifies_compound_setting);
    RUN_TEST(test_set_persistence_key_classifies_callback_driven_setting);
    RUN_TEST(test_set_persistence_key_rejects_unknown_setting);
    RUN_TEST(test_clockface_set_decision_accepts_successful_runtime_switch);
    RUN_TEST(test_clockface_set_decision_rejects_failed_runtime_switch);
    RUN_TEST(test_clockface_set_decision_persists_without_runtime_callback);
    RUN_TEST(test_sensitive_body_overrides_query_value);
    RUN_TEST(test_sensitive_body_supports_form_value_with_query_key);
    RUN_TEST(test_sensitive_body_supports_key_value_form);
    RUN_TEST(test_non_sensitive_query_behavior_unchanged);
    RUN_TEST(test_sensitive_query_fallback_still_works);
    RUN_TEST(test_incomplete_sensitive_body_is_rejected);
    RUN_TEST(test_invalid_sensitive_form_body_is_rejected);
    RUN_TEST(test_split_request_body_is_collected_within_receive_window);
    RUN_TEST(test_split_request_body_times_out_after_receive_window);
    return UNITY_END();
}

int main() {
    return runUnityTests();
}
