#include "unity.h"
#include <stdint.h>

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

int runUnityTests(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ota_rollback_eligible_new);
    RUN_TEST(test_ota_rollback_eligible_pending);
    RUN_TEST(test_ota_rollback_eligible_valid);
    RUN_TEST(test_ota_rollback_eligible_undefined);
    RUN_TEST(test_ota_rollback_blocked_invalid);
    RUN_TEST(test_ota_rollback_blocked_aborted);
    return UNITY_END();
}

int main() {
    return runUnityTests();
}
