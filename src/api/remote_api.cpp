// remote_api.cpp — Điều khiển ESP32 từ xa qua Backend Node.js
// ═══════════════════════════════════════════════════════════════
//
//  FLOW:
//  1. remoteApiSetup()  → đăng ký thiết bị với backend
//  2. remoteApiLoop()   → poll lệnh mỗi REMOTE_POLL_INTERVAL_MS
//  3. Nhận lệnh        → thực thi relay (dùng hàm relay.cpp)
//  4. Gửi ACK          → backend biết đã thực thi xong
//
//  ĐỘC LẬP HOÀN TOÀN:
//  - Không sửa web_server.cpp, relay.cpp, mqtt_manager.cpp
//  - Dùng lại hàm relay (screendiplaytft_on/off, relay2_on/off...)
//    nhưng thực thi direct (không qua WebServer handler)
//  - Chỉ chạy khi isOnlineMode() == true
// ═══════════════════════════════════════════════════════════════

#include "remote_api.h"
#include "../core/globals.h"
#include "../config.h"
#include "../offline/offline_manager.h"
#include "../online/online_manager.h"
#include "../relay/relay.h"   // relayUpdatePending
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ─────────────────────────────────────────────────────────────
// State nội bộ
// ─────────────────────────────────────────────────────────────
static bool     _registered       = false;
static String   _lastError        = "";
static unsigned long _lastPollMs  = 0;
static unsigned long _lastRegMs   = 0;
static bool     _active           = false;

// ─────────────────────────────────────────────────────────────
// Helper: thực thi relay trực tiếp (không qua WebServer handler)
// Dùng digitalWrite + state trực tiếp để tránh phụ thuộc server
// ─────────────────────────────────────────────────────────────
static bool executeRelayCommand(int relayNum, bool turnOn) {
    switch (relayNum) {
        case 1:
            state1 = turnOn;
            digitalWrite(SCREENDIPLAYTFT, turnOn ? RELAY_ON : RELAY_OFF);
            relayUpdatePending = true;
            return true;
        case 2:
            state2 = turnOn;
            digitalWrite(RELAY2, turnOn ? RELAY_ON : RELAY_OFF);
            relayUpdatePending = true;
            return true;
        case 3:
            state3 = turnOn;
            digitalWrite(RELAY3, turnOn ? RELAY_ON : RELAY_OFF);
            relayUpdatePending = true;
            return true;
        default:
            return false;
    }
}

// ─────────────────────────────────────────────────────────────
// Helper: build relay states JSON
// ─────────────────────────────────────────────────────────────
static String buildRelayStatesJson() {
    String j = "{";
    j += "\"r1\":" + String(state1 ? "true" : "false") + ",";
    j += "\"r2\":" + String(state2 ? "true" : "false") + ",";
    j += "\"r3\":" + String(state3 ? "true" : "false");
    j += "}";
    return j;
}

// ─────────────────────────────────────────────────────────────
// Đăng ký thiết bị với backend
// POST /api/esp32/register
// Body: { deviceId, ip, firmware, relayCount, wifiRssi }
// ─────────────────────────────────────────────────────────────
static void doRegister() {
    if (WiFi.status() != WL_CONNECTED) return;

    HTTPClient http;
    String url = String(REMOTE_API_BASE_URL) + "/api/esp32/register";

    JsonDocument doc;
    doc["deviceId"]   = deviceId;
    doc["ip"]         = WiFi.localIP().toString();
    doc["firmware"]   = "v12";
    doc["relayCount"] = 3;
    doc["wifiRssi"]   = WiFi.RSSI();

    String body;
    serializeJson(doc, body);

    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(5000);

    int code = http.POST(body);
    if (code == 200 || code == 201) {
        _registered = true;
        _lastError  = "";
        Serial.println(F("[RemoteAPI] ✅ Đã đăng ký thiết bị với backend"));
    } else {
        _registered = false;
        _lastError  = "Register HTTP " + String(code);
        Serial.println("[RemoteAPI] ❌ Đăng ký thất bại: " + _lastError);
    }
    http.end();
}

// ─────────────────────────────────────────────────────────────
// Gửi ACK sau khi thực thi lệnh
// POST /api/esp32/ack
// Body: { deviceId, cmdId, success, relayStates }
// ─────────────────────────────────────────────────────────────
static void sendAck(String cmdId, bool success) {
    if (WiFi.status() != WL_CONNECTED) return;

    HTTPClient http;
    String url = String(REMOTE_API_BASE_URL) + "/api/esp32/ack";

    // Build body thủ công để tránh alloc JsonDocument lớn
    String body = "{\"deviceId\":\"" + deviceId + "\","
                  "\"cmdId\":\"" + cmdId + "\","
                  "\"success\":" + (success ? "true" : "false") + ","
                  "\"relayStates\":" + buildRelayStatesJson() + "}";

    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(4000);
    int code = http.POST(body);
    if (code != 200) {
        Serial.println("[RemoteAPI] ⚠️ ACK thất bại: HTTP " + String(code));
    }
    http.end();
}

// ─────────────────────────────────────────────────────────────
// Poll lệnh từ backend
// GET /api/esp32/poll/{deviceId}
// Response: { hasCommand: false }
//        or { hasCommand: true, cmdId, command, relay, state }
//
// command = "relay_on" | "relay_off" | "relay_toggle" | "ping"
// ─────────────────────────────────────────────────────────────
static void doPoll() {
    if (WiFi.status() != WL_CONNECTED) return;

    HTTPClient http;
    String url = String(REMOTE_API_BASE_URL) + "/api/esp32/poll/" + deviceId;

    http.begin(url);
    http.setTimeout(4000);
    int code = http.GET();

    if (code != 200) {
        // Nếu 404 hoặc server error → đăng ký lại
        if (code == 404 || code < 0) {
            _registered = false;
            Serial.println("[RemoteAPI] ⚠️ Poll lỗi " + String(code) + " → cần đăng ký lại");
        }
        http.end();
        return;
    }

    String payload = http.getString();
    http.end();

    // Parse JSON
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.println(F("[RemoteAPI] ⚠️ Parse JSON lỗi"));
        return;
    }

    bool hasCommand = doc["hasCommand"] | false;
    if (!hasCommand) return; // Không có lệnh → bỏ qua

    String cmdId   = doc["cmdId"]   | String("");
    String command = doc["command"] | String("");
    int    relay   = doc["relay"]   | 0;
    bool   state   = doc["state"]   | false;

    Serial.println("[RemoteAPI] 📥 Lệnh: " + command +
                   " relay=" + String(relay) +
                   " state=" + String(state ? "ON" : "OFF") +
                   " cmdId=" + cmdId);

    // ── Thực thi lệnh ──
    bool success = false;

    if (command == "relay_on") {
        success = executeRelayCommand(relay, true);
    }
    else if (command == "relay_off") {
        success = executeRelayCommand(relay, false);
    }
    else if (command == "relay_toggle") {
        bool currentState = (relay == 1) ? state1 : (relay == 2) ? state2 : state3;
        success = executeRelayCommand(relay, !currentState);
    }
    else if (command == "relay_all_off") {
        executeRelayCommand(1, false);
        executeRelayCommand(2, false);
        executeRelayCommand(3, false);
        success = true;
    }
    else if (command == "ping") {
        success = true; // Chỉ cần gửi ACK với relay states
    }
    else {
        Serial.println("[RemoteAPI] ⚠️ Lệnh không xác định: " + command);
        success = false;
    }

    // ── Gửi ACK ──
    if (cmdId.length() > 0) {
        sendAck(cmdId, success);
    }
}

// ═══════════════════════════════════════════════════════════════
// Public API
// ═══════════════════════════════════════════════════════════════

void remoteApiSetup() {
    _active     = true;
    _registered = false;
    _lastPollMs = 0;
    _lastRegMs  = 0;
    Serial.println(F("[RemoteAPI] Module khởi động"));
}

void remoteApiLoop() {
    if (!_active) return;
    if (!isOnlineMode()) return;
    if (WiFi.status() != WL_CONNECTED) return;

    unsigned long now = millis();

    // ── Đăng ký / Đăng ký lại ──
    if (!_registered) {
        if (now - _lastRegMs >= REMOTE_REGISTER_RETRY_MS || _lastRegMs == 0) {
            _lastRegMs = now;
            doRegister();
        }
        return; // Không poll khi chưa đăng ký
    }

    // ── Poll lệnh ──
    if (now - _lastPollMs >= REMOTE_POLL_INTERVAL_MS) {
        _lastPollMs = now;
        doPoll();
    }
}

void remoteApiStop() {
    _active     = false;
    _registered = false;
    Serial.println(F("[RemoteAPI] Module dừng (offline mode)"));
}

bool remoteApiIsRegistered() {
    return _registered;
}

String remoteApiGetStatus() {
    if (!_active) return "inactive";
    if (!_registered) return "unregistered";
    return "registered";
}
