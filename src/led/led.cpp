#include "led.h"
#include "../config.h"
#include "../core/globals.h"
#include <WiFi.h>

// ===== SETUP PWM =====
void setupLED() {

  ledcSetup(CH_R, PWM_FREQ, PWM_RES);
  ledcSetup(CH_G, PWM_FREQ, PWM_RES);
  ledcSetup(CH_B, PWM_FREQ, PWM_RES);

  ledcAttachPin(LED_R, CH_R);
  ledcAttachPin(LED_G, CH_G);
  ledcAttachPin(LED_B, CH_B);

  // tắt hết (active LOW)
  ledcWrite(CH_R, 255);
  ledcWrite(CH_G, 255);
  ledcWrite(CH_B, 255);
}

// ===== SET RGB =====
void setRGB(bool r, bool g, bool b) {

  uint8_t dim = 10; // độ sáng dịu

  uint8_t rVal = r ? (255 - dim) : 255;
  uint8_t gVal = g ? (255 - dim) : 255;
  uint8_t bVal = b ? (255 - dim) : 255;

  ledcWrite(CH_R, rVal);
  ledcWrite(CH_G, gVal);
  ledcWrite(CH_B, bVal);
}

// ===== AUTO STATUS =====
void updateLEDStatus() {

  if (WiFi.status() != WL_CONNECTED) {
    // 🔴 mất WiFi
    setRGB(true, false, false);
  }
  else if (client.connected()) {
    // 🔵 MQTT OK
    setRGB(false, false, true);
  }
  else {
    // 🟢 WiFi OK, MQTT chưa
    setRGB(false, true, false);
  }
}