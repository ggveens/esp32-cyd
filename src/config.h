// config.h — Cấu hình phần cứng, MQTT, Time, Schedule
#pragma once

// ===== WIFI =====
#define MAX_WIFI 5

// ===== RELAY =====
#define SCREENDIPLAYTFT 27
#define RELAY2          22
#define RELAY3          21
#define RELAY_ON        HIGH
#define RELAY_OFF       LOW

// ===== RGB LED =====
#define LED_R 4
#define LED_G 16
#define LED_B 17
#define CH_R  0
#define CH_G  1
#define CH_B  2
#define PWM_FREQ 5000
#define PWM_RES  8

// ===== BUTTON =====
#define BTN_RESET 0

// ===== TOUCH — XPT2046 ĐÃ XÓA =====
// Dùng tft.getTouch() built-in thay thế
// Nếu màn hình không có touch chip → handleTouch() tự động skip

// ===== MQTT =====
#define MQTT_SERVER              "broker.hivemq.com"
#define MQTT_PORT                1883
#define MQTT_CONNECT_TIMEOUT_MS  5000
#define MQTT_RETRY_INTERVAL_MS   10000

// ===== TIME =====
#define NTP_SERVER  "pool.ntp.org"
#define GMT_OFFSET  (7 * 3600)

// ===== SCHEDULE =====
#define MAX_SCHEDULE 10

// ===== TIME API =====
#define TIME_API_URL         "https://ggveens.org/api/today/datetime"
#define TIME_SYNC_INTERVAL   3600000   // Đồng bộ lại sau 1 tiếng (ms)
#define TIME_RETRY_INTERVAL  15000     // Thử lại khi lỗi (ms)

// ===== OTA =====
// Mật khẩu cho ArduinoOTA (upload từ PlatformIO/IDE)
// Web OTA (/ota) không yêu cầu mật khẩu — chỉ cần kết nối AP
#define REMOTE_API_BASE_URL  "https://ggveens.org"   