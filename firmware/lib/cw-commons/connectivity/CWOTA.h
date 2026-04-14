#pragma once

/**
 * CWOTA.h  -  Over-the-air firmware update for Clockwise Paradise
 *
 * Fetches the latest release from GitHub Releases API, compares version,
 * and flashes via ESP-IDF esp_https_ota if a newer version is available.
 *
 * Flow:
 *   1. GET https://api.github.com/repos/<owner>/<repo>/releases/latest
 *   2. Parse tag_name (e.g. "v2.1.0") and compare with CW_FW_VERSION
 *   3. Find asset named "clockwise-paradise.bin" in the release
 *   4. Flash via esp_https_ota
 *   5. Reboot on success
 *
 * Config (CWPreferences):
 *   otaEnabled     -  allow OTA updates (default: true)
 *   otaOwner       -  GitHub owner  (default: "Starlord-8bit")
 *   otaRepo        -  GitHub repo   (default: "clockwise-paradise")
 *   otaAssetName   -  .bin filename  (default: "clockwise-paradise.bin")
 *
 * Triggered by:
 *   - POST /ota/check   -  check for update, returns JSON with available version
 *   - POST /ota/update  -  check + flash if newer version available
 *   - Automatic check on boot (if otaEnabled)
 */

#include <Arduino.h>
#include <core/CWPreferences.h>
#include <display/StatusController.h>
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#ifdef __cplusplus
}
#endif

#define OTA_GITHUB_API_HOST  "api.github.com"
#define OTA_RELEASES_PATH    "/repos/%s/%s/releases/latest"
#define OTA_RESPONSE_BUFSIZE 4096

class CWOTA {
public:
    static CWOTA* getInstance() {
        static CWOTA instance;
        return &instance;
    }

    struct UpdateInfo {
        bool    available = false;
        String  version;
        String  download_url;
        String  body;        // release notes
        String  error;
    };

    /**
     * Check GitHub for a newer release. Non-blocking version info fetch.
     * Returns UpdateInfo struct; caller decides whether to flash.
     */
    UpdateInfo checkForUpdate() {
        UpdateInfo info;
        auto* p = ClockwiseParams::getInstance();

        // Build API URL
        char path[128];
        snprintf(path, sizeof(path), OTA_RELEASES_PATH,
                 p->otaOwner.c_str(), p->otaRepo.c_str());

        // Fetch release JSON from GitHub API
        String response = _httpGet(OTA_GITHUB_API_HOST, path);
        if (response.isEmpty()) {
            info.error = "Could not reach GitHub API";
            return info;
        }

        // Parse tag_name from JSON (minimal parsing  -  no full JSON library needed)
        info.version    = _extractJsonString(response, "tag_name");
        String asset_url = _extractAssetUrl(response, p->otaAssetName);

        if (info.version.isEmpty()) {
            info.error = "Could not parse release version";
            return info;
        }

        // Compare versions (strip leading 'v')
        String remote = info.version;
        if (remote.startsWith("v")) remote = remote.substring(1);
        String current = String(CW_FW_VERSION);
        if (current.startsWith("v")) current = current.substring(1);

        info.available   = (remote != current);
        info.download_url = asset_url;
        info.body        = _extractJsonString(response, "body");

        ESP_LOGI("OTA", "Current: %s, Latest: %s, Update available: %s",
                      current.c_str(), remote.c_str(), info.available ? "yes" : "no");
        return info;
    }

    /**
     * Flash firmware from a direct HTTPS URL.
     * Reboots on success; returns error string on failure.
     */
    String flashFromUrl(const String& url) {
        ESP_LOGI("OTA", "Flashing from: %s", url.c_str());
        StatusController::getInstance()->printCenter("Updating...", 32);

        esp_http_client_config_t http_cfg = {};
        char url_buf[512];
        url.toCharArray(url_buf, sizeof(url_buf));
        http_cfg.url                 = url_buf;
        http_cfg.crt_bundle_attach   = esp_crt_bundle_attach;
        http_cfg.keep_alive_enable   = true;
        http_cfg.buffer_size         = 4096;
        http_cfg.buffer_size_tx      = 1024;

        // esp_https_ota API differs between IDF v4.x and v5.x
        // v4.x: takes esp_http_client_config_t* directly
        // v5.x: takes esp_https_ota_config_t* with http_config member
        // Using compile-time detection for compatibility
#if ESP_IDF_VERSION_MAJOR >= 5
        esp_https_ota_config_t ota_cfg = {};
        ota_cfg.http_config = &http_cfg;
        esp_err_t err = esp_https_ota(&ota_cfg);
#else
        esp_err_t err = esp_https_ota(&http_cfg);
#endif
        if (err == ESP_OK) {
            ESP_LOGI("OTA", "Flash successful — rebooting");
            esp_restart();
            return "";  // never reached
        }

        String error = "Flash failed: " + String(esp_err_to_name(err));
        ESP_LOGE("OTA", "%s", error.c_str());
        return error;
    }

    /**
     * Full check-and-update flow. Call from web handler.
     * Returns JSON string describing result.
     */
    String checkAndUpdate() {
        UpdateInfo info = checkForUpdate();
        if (!info.error.isEmpty()) {
            return "{\"status\":\"error\",\"message\":\"" + info.error + "\"}";
        }
        if (!info.available) {
            return "{\"status\":\"up_to_date\",\"version\":\"" + info.version + "\"}";
        }
        if (info.download_url.isEmpty()) {
            return "{\"status\":\"error\",\"message\":\"Asset not found in release\"}";
        }
        // Flash  -  this call doesn't return on success
        String err = flashFromUrl(info.download_url);
        return "{\"status\":\"error\",\"message\":\"" + err + "\"}";
    }

    /**
     * Check-only (no flash). Returns JSON with version info.
     */
    String checkOnly() {
        UpdateInfo info = checkForUpdate();
        if (!info.error.isEmpty()) {
            return "{\"status\":\"error\",\"message\":\"" + info.error + "\"}";
        }
        String available = info.available ? "true" : "false";
        return "{\"status\":\"ok\",\"available\":" + available +
               ",\"latest\":\"" + info.version + "\""
               ",\"current\":\"" + String(CW_FW_VERSION) + "\"}";
    }

private:
    String _httpGet(const char* host, const char* path) {
        esp_http_client_config_t cfg = {};
        char url[256];
        snprintf(url, sizeof(url), "https://%s%s", host, path);
        cfg.url                = url;
        cfg.crt_bundle_attach  = esp_crt_bundle_attach;

        esp_http_client_handle_t client = esp_http_client_init(&cfg);
        // Set User-Agent (GitHub API requires it)
        esp_http_client_set_header(client, "User-Agent", "clockwise-paradise-ota");
        esp_http_client_set_header(client, "Accept", "application/vnd.github.v3+json");

        esp_err_t err = esp_http_client_open(client, 0);
        if (err != ESP_OK) {
            esp_http_client_cleanup(client);
            return "";
        }
        esp_http_client_fetch_headers(client);

        static char buf[OTA_RESPONSE_BUFSIZE];
        int len = esp_http_client_read(client, buf, OTA_RESPONSE_BUFSIZE - 1);
        esp_http_client_cleanup(client);

        if (len <= 0) return "";
        buf[len] = '\0';
        return String(buf);
    }

    // Minimal JSON string extractor (no full parser  -  saves ~10KB flash)
    String _extractJsonString(const String& json, const String& key) {
        String search = "\"" + key + "\":\"";
        int start = json.indexOf(search);
        if (start < 0) return "";
        start += search.length();
        int end = json.indexOf("\"", start);
        if (end < 0) return "";
        return json.substring(start, end);
    }

    // Find download URL for a specific asset filename in releases JSON
    String _extractAssetUrl(const String& json, const String& assetName) {
        // Find the asset block containing our filename
        int name_pos = json.indexOf("\"" + assetName + "\"");
        if (name_pos < 0) return "";
        // Search backwards for "browser_download_url" in the same asset block
        String search = "\"browser_download_url\":\"";
        int url_start = json.lastIndexOf(search, name_pos + 200);
        if (url_start < 0) {
            // Try forward search too
            url_start = json.indexOf(search, name_pos);
        }
        if (url_start < 0) return "";
        url_start += search.length();
        int url_end = json.indexOf("\"", url_start);
        if (url_end < 0) return "";
        String url = json.substring(url_start, url_end);
        // Unescape \/ in JSON
        url.replace("\\/", "/");
        return url;
    }
};
