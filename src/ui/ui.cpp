#include "ui.h"
#include "../core/globals.h"
#include "../config.h"

#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <WiFi.h>
#include <time.h>

// ===== TOUCH =====
XPT2046_Touchscreen ts(TOUCH_CS);

// 🎨 Palette
#define BG_COLOR       0x0841
#define CARD_COLOR     0x1082
#define BORDER_COLOR   0x2104
#define TEXT_PRIMARY   0xFFFF
#define TEXT_SECONDARY 0x7BEF
#define ACCENT_COLOR   0x2D7F
#define TIME_COLOR     0xFD20

// ===== HEADER =====
void drawHeader() {

  tft.fillRect(0, 0, 320, 40, BG_COLOR);
  tft.drawFastHLine(0, 39, 320, BORDER_COLOR);

  tft.setTextSize(1);

  tft.setTextColor(ACCENT_COLOR, BG_COLOR);
  tft.setCursor(5, 5);
  tft.print("ID:");

  tft.setTextColor(TEXT_PRIMARY, BG_COLOR);
  tft.print(deviceId.substring(0, 10));

  String apStr = "AP:" + WiFi.softAPIP().toString();
  int16_t apX = 320 - tft.textWidth(apStr) - 5;

  tft.setCursor(apX, 5);
  tft.setTextColor(TEXT_SECONDARY, BG_COLOR);
  tft.print("AP:");

  tft.setTextColor(TEXT_PRIMARY, BG_COLOR);
  tft.print(WiFi.softAPIP());

  String wifiName = WiFi.status() == WL_CONNECTED ? WiFi.SSID() : "No WiFi";
  if (wifiName.length() > 16) wifiName = wifiName.substring(0, 16);

  tft.setCursor(5, 22);
  tft.setTextColor(ACCENT_COLOR, BG_COLOR);
  tft.print(wifiName);

  String ipStr = "IP:" + WiFi.localIP().toString();
  int16_t ipX = 320 - tft.textWidth(ipStr) - 5;

  tft.setCursor(ipX, 22);
  tft.setTextColor(TEXT_SECONDARY, BG_COLOR);
  tft.print("IP:");

  tft.setTextColor(TEXT_PRIMARY, BG_COLOR);
  tft.print(WiFi.localIP());
}

// ===== TIME =====
void drawTime() {

  static int lastSec = -1;

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  if (timeinfo.tm_sec == lastSec) return;
  lastSec = timeinfo.tm_sec;

  char timeStr[9];
  char dateStr[12];

  strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
  strftime(dateStr, sizeof(dateStr), "%d/%m/%Y", &timeinfo);

  tft.setTextSize(3);
  int16_t timeX = (320 - tft.textWidth(timeStr)) / 2;

  tft.setTextColor(TIME_COLOR, BG_COLOR);
  tft.setCursor(timeX, 55);
  tft.print(timeStr);

  static int lastDay = -1;

  if (timeinfo.tm_mday != lastDay) {
    lastDay = timeinfo.tm_mday;

    tft.setTextSize(1);
    int16_t dateX = (320 - tft.textWidth(dateStr)) / 2;

    tft.fillRect(0, 80, 320, 20, BG_COLOR);

    tft.setTextColor(TEXT_SECONDARY, BG_COLOR);
    tft.setCursor(dateX, 85);
    tft.print(dateStr);
  }
}

// ===== RELAY CARD =====
void drawRelayCompact(int y, const char* name, const char* pin, bool state) {
    
  int x = 10;
  int w = 300;
  int h = 38;

  uint16_t border = state ? 0x07E0 : 0xF800;

  tft.fillRoundRect(x, y, w, h, 6, CARD_COLOR);
  tft.drawRoundRect(x, y, w, h, 6, border);

  tft.fillCircle(x + 15, y + h/2, 5, border);

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
  drawRelayCompact(100, "DISPLAY33", "GPIO27", state1);
  drawRelayCompact(145, "RELAY 2", "GPIO22", state2);
  drawRelayCompact(190, "RELAY 3", "GPIO21", state3);
}

// ===== FOOTER =====
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
  tft.print(WiFi.RSSI());

  tft.setCursor(10, 265);
  tft.setTextColor(TEXT_SECONDARY, BG_COLOR);
  tft.print("UP:");

  unsigned long up = millis()/1000;
  tft.setTextColor(TEXT_PRIMARY, BG_COLOR);
  tft.print(up/3600);
  tft.print("h");
}

void drawUI() {

  
  tft.fillScreen(BG_COLOR);

  drawHeader();
  drawTime();
  drawRelays();
  drawFooter();
}