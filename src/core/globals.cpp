#include "globals.h"
#include "../config.h"
#include <TFT_eSPI.h>

WebServer server(80);
WiFiClient espClient;
PubSubClient client(espClient);
Preferences prefs;
TFT_eSPI tft = TFT_eSPI();
String deviceId;

String ssids[MAX_WIFI];
String passes[MAX_WIFI];

bool state1 = false;
bool state2 = false;
bool state3 = false;

Schedule schedules[MAX_SCHEDULE];