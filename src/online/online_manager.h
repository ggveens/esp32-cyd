// online_manager.h — Quản lý chế độ ONLINE (WiFi STA + MQTT + Time API + Scheduler)
#pragma once
#include <Arduino.h>

// ── Khởi tạo / Chuyển sang ONLINE ──
void onlineStart();    // Gọi để kích hoạt chế độ online
void onlineStop();     // Gọi trước khi chuyển sang Offline
void onlineLoop();     // Gọi liên tục trong loop()

// ── Kiểm tra trạng thái ──
bool isOnlineMode();
