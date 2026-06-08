// web_server.h — WebServer ESP32 v9
// ─────────────────────────────────────────────────────────────
// TÁCH BIỆT hoàn toàn: offline_web_server (AP local)
//                       online_web_server  (đầy đủ tính năng)
// OFFLINE: KHÔNG gọi bất kỳ API ngoài nào, KHÔNG MQTT, KHÔNG NTP
// ONLINE : Đầy đủ tính năng + quản lý WiFi
// ─────────────────────────────────────────────────────────────
#pragma once
#include <Arduino.h>

// ── Khởi động chung (gọi 1 lần trong setup) ──
void setupServer();

// ── Kích hoạt/tắt route theo mode ──
void activateOfflineRoutes();   // Đăng ký routes offline-only (gọi trong offlineStart)
void activateOnlineRoutes();    // Đăng ký routes online-only  (gọi trong onlineStart)

// ── Relay helpers (dùng trong relay.cpp) ──
void screendiplaytft_on();
void screendiplaytft_off();
void relay2_on();
void relay2_off();
void relay3_on();
void relay3_off();
