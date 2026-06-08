// wifi_manager.cpp — WiFi State Machine
// Thêm trạng thái IDLE: khi OFFLINE, wifiMaintain() thoát ngay
// Không còn reconnect/scan nào xảy ra khi ở chế độ OFFLINE
// ─────────────────────────────────────────────────────────────

#include "wifi_manager.h"
#include "../core/globals.h"
#include "../config.h"
#include <WiFi.h>

WiFiState wifiState = WiFiState::IDLE;
bool isAPOnlyMode   = false;

static int           _staIdx       = 0;
static unsigned long _staStart     = 0;
static uint8_t       _retryCount   = 0;
static unsigned long _lastReconnect = 0;

static constexpr uint32_t STA_TIMEOUT_MS     = 7000;
static constexpr uint32_t RECONNECT_INTERVAL = 30000;

// ─────────────────────────────────────────────────────────────
void loadWiFi() {
    prefs.begin("wifi", true);
    for (int i = 0; i < MAX_WIFI; i++) {
        ssids[i]  = prefs.getString(("s" + String(i)).c_str(), "");
        passes[i] = prefs.getString(("p" + String(i)).c_str(), "");
    }
    prefs.end();
}

void saveWiFi(String s, String p) {
    prefs.begin("wifi", false);
    for (int i = MAX_WIFI - 1; i > 0; i--) {
        prefs.putString(("s" + String(i)).c_str(), ssids[i - 1]);
        prefs.putString(("p" + String(i)).c_str(), passes[i - 1]);
    }
    prefs.putString("s0", s);
    prefs.putString("p0", p);
    prefs.end();

    for (int i = MAX_WIFI - 1; i > 0; i--) {
        ssids[i] = ssids[i - 1];
        passes[i] = passes[i - 1];
    }
    ssids[0] = s;
    passes[0] = p;
}

// ─────────────────────────────────────────────────────────────
static int findNextValidSSID(int startIdx) {
    for (int i = startIdx; i < MAX_WIFI; i++) {
        if (ssids[i].length() > 0) return i;
    }
    return -1;
}

static void beginConnectSSID(int idx) {
    Serial.println("[WiFi] Đang kết nối: " + ssids[idx]);
    WiFi.disconnect(false);
    delay(50);
    WiFi.begin(ssids[idx].c_str(), passes[idx].c_str());
    _staStart = millis();
    _staIdx   = idx;
}

// AP_ONLY fallback trong ONLINE mode — vẫn cho phép reconnect nền
static void enterAPOnlyFallback() {
    WiFi.disconnect(false);
    WiFi.mode(WIFI_AP_STA);                          // Giữ AP+STA (không phải WIFI_AP pure)
    WiFi.softAP(deviceId.c_str(), "minhkhongbiet");
    isAPOnlyMode   = true;
    _lastReconnect = millis();
    wifiState      = WiFiState::AP_ONLY;
    Serial.println(F("[WiFi] STA thất bại → AP fallback (vẫn sẽ thử reconnect nền)"));
}

static void onConnected() {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(deviceId.c_str(), "minhkhongbiet");
    isAPOnlyMode = false;
    wifiState    = WiFiState::CONNECTED;
    _retryCount  = 0;
    Serial.println("[WiFi] Kết nối: " + WiFi.SSID() +
                   " | STA: " + WiFi.localIP().toString() +
                   " | AP: "  + WiFi.softAPIP().toString());
}

// ─────────────────────────────────────────────────────────────
void wifiMaintain() {
    // ← QUAN TRỌNG: khi IDLE (OFFLINE mode), không làm gì cả
    if (wifiState == WiFiState::IDLE) return;

    unsigned long now = millis();

    switch (wifiState) {

        case WiFiState::BOOT: {
            int idx = findNextValidSSID(0);
            if (idx < 0) {
                Serial.println(F("[WiFi] Không có SSID → AP fallback ngay"));
                enterAPOnlyFallback();
                return;
            }
            beginConnectSSID(idx);
            wifiState = WiFiState::CONNECTING;
            break;
        }

        case WiFiState::CONNECTING: {
            if (WiFi.status() == WL_CONNECTED) { onConnected(); return; }
            if (now - _staStart > STA_TIMEOUT_MS) {
                int next = findNextValidSSID(_staIdx + 1);
                if (next >= 0) {
                    beginConnectSSID(next);
                } else {
                    Serial.println(F("[WiFi] Tất cả SSID thất bại → AP fallback"));
                    enterAPOnlyFallback();
                }
            }
            break;
        }

        case WiFiState::CONNECTED: {
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println(F("[WiFi] Mất kết nối → Reconnecting"));
                wifiState   = WiFiState::RECONNECTING;
                _staIdx     = 0;
                _staStart   = 0;
                _retryCount = 0;
            }
            break;
        }

        case WiFiState::RECONNECTING: {
            if (WiFi.status() == WL_CONNECTED) { onConnected(); return; }
            if (_staStart == 0) {
                int idx = findNextValidSSID(0);
                if (idx < 0) { enterAPOnlyFallback(); return; }
                beginConnectSSID(idx);
                return;
            }
            if (now - _staStart > STA_TIMEOUT_MS) {
                int next = findNextValidSSID(_staIdx + 1);
                if (next >= 0) {
                    beginConnectSSID(next);
                } else {
                    _retryCount++;
                    Serial.printf("[WiFi] Reconnect thất bại #%d → AP fallback\n", _retryCount);
                    enterAPOnlyFallback();
                }
            }
            break;
        }

        case WiFiState::AP_ONLY: {
            // Trong ONLINE mode: thử kết nối lại ngầm mỗi 30 giây
            if (now - _lastReconnect > RECONNECT_INTERVAL) {
                int idx = findNextValidSSID(0);
                if (idx >= 0) {
                    Serial.println(F("[WiFi] Thử reconnect ngầm..."));
                    WiFi.mode(WIFI_AP_STA);
                    WiFi.softAP(deviceId.c_str(), "minhkhongbiet");
                    beginConnectSSID(idx);
                    wifiState    = WiFiState::RECONNECTING;
                    isAPOnlyMode = false;
                }
                _lastReconnect = now;
            }
            break;
        }

        case WiFiState::IDLE:
        default:
            break;
    }
}
