// ui.cpp — Giao diện người dùng: hiển thị thông tin, trạng thái relay, đồng hồ, kết nối MQTT, RSSI, uptime
#include "ui.h"
#include "../core/globals.h"
#include "../config.h"
#include "../clock/time_manager.h"
#include "../wifi/wifi_manager.h"
#include <TFT_eSPI.h>
// XPT2046_Touchscreen removed — use tft.getTouch() built-in
#include <WiFi.h>
#include <time.h> 

// 🎨 Palette (Giữ nguyên cấu trúc màu sắc của bạn)
#define BG_COLOR       0x0841
#define CARD_COLOR     0x1082
#define BORDER_COLOR   0x2104
#define TEXT_PRIMARY   0xFFFF
#define TEXT_SECONDARY 0x7BEF
#define ACCENT_COLOR   0x2D7F
#define TIME_COLOR     0xFD20
#define TIME_WAIT_COLOR 0x7BEF  

// ===== HEADER ===== (Giữ nguyên)
void drawHeader() {
  tft.fillRect(0, 0, 320, 40, BG_COLOR);
  tft.drawFastHLine(0, 39, 320, BORDER_COLOR);
  tft.setTextSize(1);

  // Device ID
  tft.setTextColor(ACCENT_COLOR, BG_COLOR);
  tft.setCursor(5, 5);
  tft.print("ID:");
  tft.setTextColor(TEXT_PRIMARY, BG_COLOR);
  tft.print(deviceId.substring(0, 10));

  // AP IP
  String apStr = "AP:" + WiFi.softAPIP().toString();
  tft.setCursor(320 - tft.textWidth(apStr) - 5, 5);
  tft.setTextColor(TEXT_SECONDARY, BG_COLOR);
  tft.print("AP:");
  tft.setTextColor(TEXT_PRIMARY, BG_COLOR);
  tft.print(WiFi.softAPIP());

  // Dòng 2: Trạng thái WiFi rõ ràng
  if (isAPOnlyMode) {
    // *** Hiển thị nổi bật khi offline ***
    tft.setTextColor(0xFD20, BG_COLOR); // Màu vàng cảnh báo
    tft.setCursor(5, 22);
    tft.print("[OFFLINE] AP-Only");
  } else if (WiFi.status() == WL_CONNECTED) {
    String wifiName = WiFi.SSID();
    if (wifiName.length() > 16) wifiName = wifiName.substring(0, 16);
    tft.setTextColor(ACCENT_COLOR, BG_COLOR);
    tft.setCursor(5, 22);
    tft.print(wifiName);

    String ipStr = "IP:" + WiFi.localIP().toString();
    tft.setCursor(320 - tft.textWidth(ipStr) - 5, 22);
    tft.setTextColor(TEXT_SECONDARY, BG_COLOR);
    tft.print("IP:");
    tft.setTextColor(TEXT_PRIMARY, BG_COLOR);
    tft.print(WiFi.localIP());
  } else {
    tft.setTextColor(0x7BEF, BG_COLOR);
    tft.setCursor(5, 22);
    tft.print("Connecting...");
  }
}

// ===== TIME =====
void drawTime() {
  static int lastSec = -1;

  if (!isTimeSynced()) {
    // ✅ FIX #3a: Khi OFFLINE, lastApiError = "" (sau fix #2)
    //    → Xóa vùng đồng hồ và KHÔNG in bất kỳ chữ nào
    //    Khi ONLINE chưa sync, lastApiError có nội dung → hiển thị trạng thái
    String msg = getLastApiError();
    if (msg.isEmpty()) {
      // OFFLINE hoàn toàn — chỉ xóa vùng, không in gì
      static bool _areaCleaned = false;
      if (!_areaCleaned) {
        tft.fillRect(0, 50, 320, 45, BG_COLOR);
        _areaCleaned = true;
      }
      return;
    }
    // ONLINE đang chờ sync — hiển thị trạng thái
    static bool _areaCleaned = false;
    _areaCleaned = false;  // Reset để lần sau khi quay OFFLINE sẽ xóa lại
    static unsigned long lastWaitMsg = 0;
    if (millis() - lastWaitMsg > 1000) {
      lastWaitMsg = millis();
      tft.setTextSize(2);
      int16_t x = (320 - tft.textWidth(msg)) / 2;
      tft.setTextColor(TIME_WAIT_COLOR, BG_COLOR);
      tft.fillRect(0, 50, 320, 45, BG_COLOR);
      tft.setCursor(x, 58);
      tft.print(msg);
    }
    return;
  }

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 0)) return;

  if (timeinfo.tm_sec == lastSec) return;
  lastSec = timeinfo.tm_sec;

  // Giờ
  char timeStr[9];
  strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
  tft.setTextSize(3);
  int16_t timeX = (320 - tft.textWidth(timeStr)) / 2;
  tft.setTextColor(TIME_COLOR, BG_COLOR);
  tft.fillRect(0, 50, 320, 30, BG_COLOR);
  tft.setCursor(timeX, 52);
  tft.print(timeStr);

  // Ngày âm lịch (cập nhật khi sang ngày mới)
  static int lastDay = -1;
  if (timeinfo.tm_mday != lastDay) {
    lastDay = timeinfo.tm_mday;
    tft.setTextSize(1);
    tft.fillRect(0, 82, 320, 18, BG_COLOR);
    
    String lunar;
    if (xSemaphoreTake(timeDataMutex, pdMS_TO_TICKS(10))) {
      lunar = lunarDate;
      xSemaphoreGive(timeDataMutex);
    } else {
      lunar = "???";
    }
    String displayDate = "AL: " + lunar;
    int16_t dateX = (320 - tft.textWidth(displayDate)) / 2;
    tft.setTextColor(TEXT_SECONDARY, BG_COLOR);
    tft.setCursor(dateX, 85);
    tft.print(displayDate);
  }
}

// ===== RELAY CARD ===== (Giữ nguyên)
void drawRelayCompact(int y, const char* name, const char* pin, bool state) {
  int x = 10;
  int w = 300;
  int h = 38;
  uint16_t border = state ? 0x07E0 : 0xF800;

  tft.fillRoundRect(x, y, w, h, 6, CARD_COLOR);
  tft.drawRoundRect(x, y, w, h, 6, border);
  tft.fillCircle(x + 15, y + h / 2, 5, border);

  tft.setTextColor(TEXT_PRIMARY, CARD_COLOR);
  tft.setCursor(x + 28, y + 10);
  tft.print(name);

  tft.setTextColor(TEXT_SECONDARY, CARD_COLOR);
  tft.setCursor(x + 28, y + 24);
  tft.print(pin);

  uint16_t btnColor  = state ? 0x07E0 : 0xF800;
  uint16_t textColor = state ? TFT_BLACK : TFT_WHITE;

  tft.fillRoundRect(x + w - 50, y + 8, 40, 22, 4, btnColor);
  tft.setTextColor(textColor);

  String label = state ? "ON" : "OFF";
  int16_t tx = x + w - 50 + (40 - tft.textWidth(label)) / 2;
  int16_t ty = y + 8 + (22 / 2) - 4;
  tft.setCursor(tx, ty);
  tft.print(label);
}

void drawRelays() {
  tft.setTextSize(1);
  drawRelayCompact(100, "DISPLAY", "GPIO27", state1);
  drawRelayCompact(145, "RELAY 2", "GPIO22", state2);
  drawRelayCompact(190, "RELAY 3", "GPIO21", state3);
}

// ===== FOOTER ===== (Giữ nguyên)
void drawFooter() {
  tft.setTextSize(1);
  tft.fillRect(0, 240, 320, 40, BG_COLOR);
  tft.drawFastHLine(0, 240, 320, BORDER_COLOR);

  tft.setTextColor(TEXT_SECONDARY, BG_COLOR);
  tft.setCursor(10, 250);
  tft.print("MQTT:");
  tft.setTextColor(client.connected() ? 0x07E0 : 0xF800, BG_COLOR);
  tft.print(client.connected() ? " ON" : " OFF");

  tft.setCursor(150, 250);
  tft.setTextColor(TEXT_SECONDARY, BG_COLOR);
  tft.print("RSSI:");
  tft.setTextColor(TEXT_PRIMARY, BG_COLOR);
  if (WiFi.status() == WL_CONNECTED) {
    tft.print(WiFi.RSSI());
  } else {
    tft.print("--");
  }

  tft.setCursor(10, 265);
  tft.setTextColor(TEXT_SECONDARY, BG_COLOR);
  tft.print("UP:");
  unsigned long up = millis() / 1000;
  tft.setTextColor(TEXT_PRIMARY, BG_COLOR);
  tft.print(up / 3600);
  tft.print("h");
}

void drawUI() {
  tft.fillScreen(BG_COLOR);
  drawHeader();
  drawTime();
  drawRelays();
  drawFooter();
}