// remote_api.h — Module điều khiển từ xa qua Backend Node.js
// ═══════════════════════════════════════════════════════════════
//
//  MODULE ĐỘC LẬP — Không động vào bất kỳ code cũ nào
//
//  KIẾN TRÚC:
//  ┌─────────────────────────────────────────────────────────┐
//  │  ESP32 (Online mode)                                    │
//  │  ├── Gửi đăng ký: POST /api/esp32/register             │
//  │  │   { deviceId, ip, firmware, relayCount }            │
//  │  │                                                      │
//  │  └── Poll lệnh: GET  /api/esp32/poll/{deviceId}         │
//  │      → { command, relay, state, cmdId }                 │
//  │      → Thực thi relay → gửi ACK                        │
//  │                                                         │
//  │  ACK: POST /api/esp32/ack                              │
//  │       { deviceId, cmdId, success, relayStates }        │
//  └─────────────────────────────────────────────────────────┘
//
//  CHỈ ACTIVE KHI ONLINE — tự kiểm tra isOnlineMode()
//  KHÔNG block WebServer — dùng HTTPClient async
//  POLL INTERVAL: 3 giây (configurable)
// ═══════════════════════════════════════════════════════════════
#pragma once
#include <Arduino.h>

// ── Cấu hình Backend ──────────────────────────────────────────
// ❗ Đổi thành domain/IP backend Node.js của bạn
#define REMOTE_API_BASE_URL  "https://ggveens.org"
#define REMOTE_POLL_INTERVAL_MS  3000   // Poll mỗi 3 giây
#define REMOTE_REGISTER_RETRY_MS 15000  // Đăng ký lại khi mất kết nối

// ── Public API ────────────────────────────────────────────────
void remoteApiSetup();   // Gọi trong onlineStart() sau WiFi connected
void remoteApiLoop();    // Gọi trong onlineLoop() — non-blocking
void remoteApiStop();    // Gọi trong onlineStop()

// Trạng thái module
bool remoteApiIsRegistered();
String remoteApiGetStatus(); // "unregistered" | "registered" | "error"
