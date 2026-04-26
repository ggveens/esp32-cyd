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

#include <WiFi.h>
#include <time.h>

// ===== DEVICE ID =====
String getDeviceID() {
  uint64_t chipid = ESP.getEfuseMac();
  char id[20];
  sprintf(id, "device_%04X", (uint16_t)(chipid & 0xFFFF));
  return String(id);
}

void setup(){

  Serial.begin(115200);
  setupLED();
  pinMode(BTN_RESET, INPUT_PULLUP);

  deviceId = getDeviceID();

  tft.init();          // 🔥 init TFT 1 lần duy nhất
  tft.setRotation(1);  // 🔥 xoay màn hình

  ts.begin();
  ts.setRotation(1);

  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(RELAY3, OUTPUT);

  digitalWrite(RELAY1,HIGH);
  state1 = true;

  digitalWrite(RELAY2,HIGH);
  digitalWrite(RELAY3,HIGH);

  loadWiFi();

  WiFi.mode(WIFI_AP_STA);
  connectWiFi();
  WiFi.softAP(deviceId.c_str(), "minhkhongbiet");

  setupMQTT();

  configTime(GMT_OFFSET,0,NTP_SERVER);
  
  currentScreen = SCREEN_MAIN;
  drawUI();

  setupServer();
}

void loop(){

  updateLEDStatus();
  
  if (digitalRead(BTN_RESET) == LOW) {
    prefs.begin("wifi", false);
    prefs.clear();
    prefs.end();
    ESP.restart();
  }

  server.handleClient();
  handleTouch();
  mqttLoop();

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
  }

  // =========================
  // 🔥 QUẢN LÝ SCREEN
  // =========================
  static uint8_t lastScreen = 255;

  if (currentScreen != lastScreen) {
    lastScreen = currentScreen;

    if (currentScreen == SCREEN_MAIN) {
      drawUI();
    } 
    else if (currentScreen == SCREEN_2) {
      drawUI2();
    }
  }

  // =========================
  // 🔥 UPDATE UI THEO SCREEN
  // =========================
if (currentScreen == SCREEN_MAIN) {

  drawTime();

  static unsigned long lastHeader = 0;
  if (millis() - lastHeader > 2000) {
    lastHeader = millis();
    drawHeader();
  }
}
else if (currentScreen == SCREEN_2) {

  static unsigned long lastAnim = 0;
  if (millis() - lastAnim > 50) {
    lastAnim = millis();
    updateUI2();
  }
}

  // =========================
  // 🔥 LOGIC KHÁC (GIỮ NGUYÊN)
  // =========================
  static unsigned long lastMQTT = 0;
  if (millis() - lastMQTT > 2000) {
    lastMQTT = millis();
    sendStatusMQTT();
  }

  resetScheduleDaily();
  handleSchedule();
}