// globals.h — Biến toàn cục dự án ESP32
#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
// XPT2046_Touchscreen ĐÃ BỊ XÓA — dùng tft.getTouch() built-in thay thế

// ===== GLOBAL OBJECTS =====
extern WebServer    server;
extern WiFiClient   espClient;
extern PubSubClient client;
extern Preferences  prefs;
extern TFT_eSPI     tft;

// ===== DEVICE =====
extern String deviceId;

// ===== WIFI =====
extern String ssids[];
extern String passes[];

// ===== RELAY STATE =====
extern bool state1;
extern bool state2;
extern bool state3;

// ===== SCHEDULE =====
struct Schedule {
    bool active;
    int  relay;
    int  onH,  onM;
    int  offH, offM;
    bool firedOn;
    bool firedOff;
};
extern Schedule schedules[];

// ===== SCREEN =====
enum Screen {
    SCREEN_MAIN,
    SCREEN_2,
    SCREEN_3
};
extern Screen currentScreen;
