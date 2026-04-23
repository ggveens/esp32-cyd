#include "wifi_manager.h"
#include "../core/globals.h"
#include "../config.h"
#include <WiFi.h>

void loadWiFi() {
  prefs.begin("wifi", true);
  for (int i = 0; i < MAX_WIFI; i++) {
    ssids[i] = prefs.getString(("s" + String(i)).c_str(), "");
    passes[i] = prefs.getString(("p" + String(i)).c_str(), "");
  }
  prefs.end();
}

void saveWiFi(String s, String p) {
  prefs.begin("wifi", false);

  for (int i = MAX_WIFI - 1; i > 0; i--) {
    prefs.putString(("s" + String(i)).c_str(), ssids[i - 1]);
    prefs.putString(("p" + String(i)).c_str(), passes[i - 1]);
  }

  prefs.putString("s0", s);
  prefs.putString("p0", p);
  prefs.end();
}

bool connectWiFi() {
  for (int i = 0; i < MAX_WIFI; i++) {
    if (ssids[i] == "") continue;

    WiFi.begin(ssids[i].c_str(), passes[i].c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
      delay(300);
      if (millis() - start > 8000) break;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Connected: " + ssids[i]);
      return true;
    }
  }
  return false;
}