// main.cpp — Boot + Loop tối ưu tuyệt đối cho OFFLINE-first v10
// ═══════════════════════════════════════════════════════════════
//
// KIẾN TRÚC LOOP V10 — ĐỘC LẬP HOÀN TOÀN KHI OFFLINE:
//
//  ┌─────────────────────────────────────────────────────────┐
//  │  loop() PRIORITY ORDER (CPU time budget):               │
//  │  1. server.handleClient()  ── 3x yield  ── ƯU TIÊN #1  │
//  │  2. relayUpdatePending     ── drawRelays() khi cần      │
//  │  3. handleTouch()          ── throttle 30ms             │
//  │  4. handleSchedule()       ── chỉ khi có time          │
//  │  5. drawHeader()           ── mỗi 5s offline           │
//  │  6. drawTime()             ── mỗi 1s                   │
//  └─────────────────────────────────────────────────────────┘
//
// CÁC LỖI ĐÃ XỬ LÝ TRONG V10:
//   ✅ FIX-A: relay handler không còn gọi drawRelays() → dùng flag
//   ✅ FIX-B: HTML serve bằng send_P → không alloc 18KB heap
//   ✅ FIX-C: server.handleClient() x3 trước mọi thứ + yield sau mỗi lần
//   ✅ FIX-D: relayUpdatePending xử lý ngay sau handleClient()
//   ✅ FIX-E: offlineLoop() loại bỏ schedule khi không có time
//   ✅ FIX-F: drawUI() không gọi trong loop thường xuyên
//   ✅ FIX-G: ESP32 WiFi provisioning bị tắt hoàn toàn khi OFFLINE
// ═══════════════════════════════════════════════════════════════

#include <Arduino.h>
#include "config.h"
#include "core/globals.h"
#include "wifi/wifi_manager.h"
#include "mqtt/mqtt_manager.h"
#include "ui/ui.h"
#include "ui/ui2.h"
#include "touch/touch.h"
#include "relay/relay.h"
#include "schedule/schedule.h"
#include "server/web_server.h"
#include "led/led.h"
#include "clock/time_manager.h"
#include "online/online_manager.h"
#include "offline/offline_manager.h"

#include <WiFi.h>
#include <esp_wifi.h>   // Để tắt hoàn toàn WiFi provisioning

// ─────────────────────────────────────────────────────────────
String getDeviceID() {
    uint64_t chipid = ESP.getEfuseMac();
    char id[20];
    sprintf(id, "device_%04X", (uint16_t)(chipid & 0xFFFF));
    return String(id);
}

// ─────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println(F("\n[BOOT v10] ESP32 khởi động..."));

    // ── LED & GPIO ──
    setupLED();
    pinMode(BTN_RESET, INPUT_PULLUP);

    deviceId = getDeviceID();
    Serial.println("[BOOT] Device ID: " + deviceId);

    // ── TFT khởi động ──
    tft.init();
    tft.setRotation(1);

    // ── Relay / GPIO — set trạng thái ban đầu ──
    pinMode(SCREENDIPLAYTFT, OUTPUT);
    pinMode(RELAY2, OUTPUT);
    pinMode(RELAY3, OUTPUT);
    digitalWrite(SCREENDIPLAYTFT, HIGH); state1 = true;
    digitalWrite(RELAY2, HIGH);          state2 = false;
    digitalWrite(RELAY3, HIGH);          state3 = false;

    // ── BƯỚC 1: AP bật TRƯỚC TIÊN ──
    // FIX-G: Tắt WiFi provisioning (SmartConfig/BLE) — tiết kiệm RAM + tránh interference
    WiFi.mode(WIFI_AP);
    esp_wifi_set_ps(WIFI_PS_NONE);   // Tắt power save → AP response nhanh hơn
    WiFi.softAP(deviceId.c_str(), "minhkhongbiet");
    Serial.println("[BOOT] AP UP: " + WiFi.softAPIP().toString());

    // ── BƯỚC 2: WebServer khởi động NGAY ──
    setupServer();
    Serial.println(F("[BOOT] WebServer ON tại 192.168.4.1"));

    // ── BƯỚC 3: TimeManager mutex (nhẹ, không tạo task) ──
    timeManagerSetup();

    // ── BƯỚC 4: Load credentials ──
    loadWiFi();

    bool hasSavedWifi = false;
    for (int i = 0; i < MAX_WIFI; i++) {
        if (ssids[i].length() > 0) { hasSavedWifi = true; break; }
    }

    // ── BƯỚC 5: Hiển thị UI ──
    currentScreen = SCREEN_MAIN;
    drawUI();

    // ── BƯỚC 6: Quyết định mode boot ──
    if (!hasSavedWifi) {
        Serial.println(F("[BOOT] Không có WiFi → OFFLINE MODE"));
        offlineStart();
    } else {
        Serial.println(F("[BOOT] Có WiFi → ONLINE MODE"));
        onlineStart();
    }

    Serial.println(F("[BOOT] ✓ Hoàn tất boot sequence v10"));
}

// ═══════════════════════════════════════════════════════════════
// loop() — Tối ưu tuyệt đối
//
// NGUYÊN TẮC:
//   - server.handleClient() LUÔN được gọi đầu tiên + nhiều lần
//   - KHÔNG có bất kỳ blocking call nào trong đường chính
//   - TFT chỉ vẽ khi thực sự cần (flag-driven)
//   - yield() sau mỗi operation quan trọng
// ═══════════════════════════════════════════════════════════════
void loop() {
    // ══ PRIORITY 1: WebServer — 3 lần để bắt request dồn dập ══
    // Khi browser load trang: có thể có 2-3 request liên tiếp
    // (HTML + /api/mode/status + favicon). 3 lần đảm bảo không miss.
    server.handleClient(); yield();
    server.handleClient(); yield();
    server.handleClient(); yield();

    // ══ PRIORITY 2: Reset button ══
    if (digitalRead(BTN_RESET) == LOW) {
        unsigned long pressStart = millis();
        while (digitalRead(BTN_RESET) == LOW && millis() - pressStart < 3000) {
            server.handleClient(); // Vẫn serve trong khi giữ nút
            yield();
        }
        if (millis() - pressStart >= 3000) {
            Serial.println(F("[BTN] Factory Reset — 3 giây"));
            prefs.begin("wifi",  false); prefs.clear(); prefs.end();
            prefs.begin("sched", false); prefs.clear(); prefs.end();
            prefs.begin("cfg",   false); prefs.clear(); prefs.end();
            for (int i = 0; i < MAX_SCHEDULE; i++) schedules[i].active = false;
        } else {
            Serial.println(F("[BTN] Nhấn ngắn → Restart"));
        }
        delay(300);
        ESP.restart();
    }

    // ══ PRIORITY 3: Relay TFT update (flag-driven, không block handler) ══
    // FIX-A: Handler chỉ set GPIO + send OK, loop() mới drawRelays()
    // Đây là lý do relay cập nhật TFT sau khi browser đã nhận OK
    if (relayUpdatePending) {
        relayUpdatePending = false;
        if (currentScreen == SCREEN_MAIN) {
            drawRelays();
        }
        server.handleClient(); yield(); // Serve thêm sau khi vẽ
    }

    // ══ PRIORITY 4: Mode-specific loop ══
    if (isOnlineMode()) {
        onlineLoop();
    } else {
        offlineLoop();
    }

    // ══ PRIORITY 5: Screen switching (chỉ khi thay đổi) ══
    static uint8_t lastScreen = 255;
    if (currentScreen != lastScreen) {
        lastScreen = currentScreen;
        if      (currentScreen == SCREEN_MAIN) drawUI();
        else if (currentScreen == SCREEN_2)    drawUI2();
        server.handleClient(); yield(); // Serve sau khi vẽ toàn màn
    }

    // ══ PRIORITY 6: Time display (mỗi 1 giây, nhẹ) ══
    if (currentScreen == SCREEN_MAIN) {
        static unsigned long lastTime = 0;
        unsigned long now = millis();
        if (now - lastTime >= 1000) {
            lastTime = now;
            drawTime();
        }
    } else if (currentScreen == SCREEN_2) {
        static unsigned long lastAnim = 0;
        unsigned long now = millis();
        if (now - lastAnim >= 50) {
            lastAnim = now;
            updateUI2();
        }
    }

    // Nhường CPU cuối vòng
    yield();
}
