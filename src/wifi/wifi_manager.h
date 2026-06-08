// wifi_manager.h — WiFi State Machine (chỉ hoạt động khi ONLINE)
#pragma once
#include <Arduino.h>

// ===== WIFI STATE =====
enum class WiFiState {
    IDLE,           // ← Trạng thái mới: không làm gì (khi OFFLINE)
    BOOT,
    CONNECTING,
    CONNECTED,
    AP_ONLY,        // Fallback STA thất bại — nhưng vẫn cho reconnect nền (ONLINE)
    RECONNECTING
};

extern WiFiState wifiState;
extern bool isAPOnlyMode;

// ── Credentials ──
void loadWiFi();
void saveWiFi(String s, String p);

// ── State machine (gọi trong loop — chỉ tác dụng khi ONLINE) ──
void wifiMaintain();
