// online_manager.cpp — Chế độ ONLINE đầy đủ v9
//
// ONLINE bao gồm:
//   ✅ WiFi STA (WIFI_AP_STA)
//   ✅ WiFi reconnect / scan
//   ✅ MQTT
//   ✅ Time API (FreeRTOS task trên Core 0)
//   ✅ Scheduler
//   ✅ WebServer (HTML online với WiFi scan, mode switch)
//   ✅ activateOnlineRoutes() — kích hoạt đầy đủ routes
// ─────────────────────────────────────────────────────────────

#include "online_manager.h"
#include "../core/globals.h"
#include "../config.h"
#include "../wifi/wifi_manager.h"
#include "../mqtt/mqtt_manager.h"
#include "../clock/time_manager.h"
#include "../relay/relay.h"
#include "../schedule/schedule.h"
#include "../touch/touch.h"
#include "../ui/ui.h"
#include "../led/led.h"
#include "../server/web_server.h"

#include <WiFi.h>
#include <esp_wifi.h>

static bool _onlineActive = false;

// ─────────────────────────────────────────────────────────────
void onlineStart() {
    if (_onlineActive) return;

    Serial.println(F("[ONLINE] === Chuyển sang chế độ ONLINE ==="));

    // 1. Khôi phục WiFi AP+STA
    WiFi.mode(WIFI_AP_STA);
    esp_wifi_set_ps(WIFI_PS_NONE);   // Tắt power save — AP respond nhanh hơn
    WiFi.softAP(deviceId.c_str(), "minhkhongbiet");
    Serial.println("[ONLINE] AP IP: " + WiFi.softAPIP().toString());

    // 2. Load credentials & khởi động state machine WiFi
    loadWiFi();
    wifiState = WiFiState::BOOT; // Trigger kết nối STA ngay vòng loop tiếp theo

    // 3. Khởi động lại Time Manager task
    timeManagerResume();

    // 4. MQTT — setup lại, kết nối sẽ xảy ra trong mqttLoop
    setupMQTT();

    // 5. Kích hoạt online routes (WiFi scan, mode switch đầy đủ)
    activateOnlineRoutes();

    _onlineActive = true;

    // 6. Cập nhật UI
    currentScreen = SCREEN_MAIN;
    drawUI();

    Serial.println(F("[ONLINE] === Sẵn sàng ONLINE ==="));
}

// ─────────────────────────────────────────────────────────────
void onlineStop() {
    if (!_onlineActive) return;

    Serial.println(F("[ONLINE] Dừng chế độ ONLINE → chuẩn bị cho Offline"));

    // Đặt state machine WiFi về IDLE — ngừng mọi reconnect
    wifiState = WiFiState::IDLE;

    _onlineActive = false;
    // offlineStart() sẽ gọi mqttForceStop() và timeManagerSuspend()
}

// ─────────────────────────────────────────────────────────────
void onlineLoop() {
    if (!_onlineActive) return;

    // LED status
    updateLEDStatus();

    // WiFi state machine — reconnect, AP+STA
    wifiMaintain();

    // MQTT loop — kết nối lại nếu mất, gửi/nhận
    mqttLoop();

    // Touch
    handleTouch();

    // Schedule
    resetScheduleDaily();
    handleSchedule();

    // Gửi status MQTT mỗi 2 giây
    static unsigned long lastMQTT = 0;
    if (millis() - lastMQTT > 2000) {
        lastMQTT = millis();
        sendStatusMQTT();
    }

    // UI Header mỗi 2 giây
    static unsigned long lastHeader = 0;
    if (millis() - lastHeader > 2000) {
        lastHeader = millis();
        if (currentScreen == SCREEN_MAIN) drawHeader();
    }
}

// ─────────────────────────────────────────────────────────────
bool isOnlineMode() {
    return _onlineActive;
}
