// offline_manager.cpp — OFFLINE tuyệt đối v11
//
// ═══════════════════════════════════════════════════════════════
// OFFLINE V11 — CHỈ CÒN RELAY CONTROL, XÓA MỌI THỨ THỪA
//
//   ✅ WiFi.mode(WIFI_AP) + esp_wifi_set_ps(WIFI_PS_NONE)
//   ✅ WebServer 192.168.4.1 — relay ON/OFF là tất cả
//   ✅ TFT / Touch vẫn hoạt động trên màn hình
//   ❌ Schedule — BỎ HOÀN TOÀN (gây lag, cần time, không cần thiết)
//   ❌ drawHeader() trong offlineLoop — BỎ (gọi từ main loop)
//   ❌ MQTT, Time API, NTP, HTTP ngoài, WiFi STA, Scan, Reconnect
//
// offlineLoop() V11 CHỈ làm:
//   - handleTouch() — throttle 30ms
//   KHÔNG có gì khác — mọi thứ đã được main loop xử lý
// ═══════════════════════════════════════════════════════════════

#include "offline_manager.h"
#include "../core/globals.h"
#include "../config.h"
#include "../clock/time_manager.h"
#include "../mqtt/mqtt_manager.h"
#include "../relay/relay.h"
#include "../touch/touch.h"
#include "../ui/ui.h"
#include "../server/web_server.h"
#include "../wifi/wifi_manager.h"

#include <WiFi.h>
#include <esp_wifi.h>

static bool _offlineActive = false;

// ─────────────────────────────────────────────────────────────
void offlineStart() {
    if (_offlineActive) return;

    Serial.println(F("[OFFLINE v11] === KHỞI ĐỘNG OFFLINE AP-ONLY ==="));

    // 1. WiFi: chỉ AP, không STA, không scan, không reconnect
    WiFi.disconnect(true);
    delay(30);
    WiFi.mode(WIFI_AP);
    delay(30);
    // Tắt power save → AP respond nhanh hơn, không có latency spike
    esp_wifi_set_ps(WIFI_PS_NONE);
    WiFi.softAP(deviceId.c_str(), "minhkhongbiet");

    // Đánh dấu OFFLINE cho drawHeader() và wifiMaintain()
    isAPOnlyMode = true;
    wifiState    = WiFiState::IDLE;

    Serial.println("[OFFLINE] AP: " + WiFi.softAPIP().toString());

    // 2. Dừng Time task — không cần time trong offline
    timeManagerSuspend();

    // 3. Dừng MQTT
    mqttForceStop();

    // 4. Kích hoạt routes offline
    activateOfflineRoutes();

    _offlineActive = true;

    // 5. UI
    currentScreen = SCREEN_MAIN;
    drawUI();

    Serial.println(F("[OFFLINE v11] ✓ AP | ✓ WebServer | ✗ STA | ✗ MQTT | ✗ NTP | ✗ Schedule"));
}

// ─────────────────────────────────────────────────────────────
void offlineStop() {
    if (!_offlineActive) return;
    Serial.println(F("[OFFLINE] Thoát OFFLINE → chuẩn bị Online"));
    _offlineActive = false;
}

// ─────────────────────────────────────────────────────────────
// offlineLoop() V11 — Tối giản tuyệt đối: CHỈ Touch
//
// Lý do bỏ Schedule:
//   - Schedule cần time hợp lệ (getLocalTime) → offline không có time
//   - resetScheduleDaily() + handleSchedule() mỗi 500ms gây overhead
//   - Khi factory reset, schedules[] = rỗng → loop vô nghĩa nhưng vẫn tốn cycle
//   - Schedule chỉ có ý nghĩa khi ONLINE (có NTP sync)
//
// Lý do bỏ drawHeader() khỏi offlineLoop:
//   - main.cpp đã có PRIORITY 6 vẽ drawTime() mỗi 1s
//   - drawHeader() gọi từ offlineLoop gây double-draw, xung đột SPI
// ─────────────────────────────────────────────────────────────
void offlineLoop() {
    if (!_offlineActive) return;

    // Touch — throttle 30ms (tránh block SPI liên tục)
    handleTouch();
}

// ─────────────────────────────────────────────────────────────
bool isOfflineMode() {
    return _offlineActive;
}
