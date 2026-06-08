// mqtt_manager.cpp — Quản lý MQTT
// mqttForceStop(): gọi khi chuyển OFFLINE — ngắt kết nối, vô hiệu hóa reconnect
// mqttLoop():      kiểm tra cờ _mqttEnabled trước khi làm bất kỳ thứ gì
// ─────────────────────────────────────────────────────────────────────────

#include "mqtt_manager.h"
#include "../core/globals.h"
#include "../relay/relay.h"
#include "../config.h"
#include <WiFi.h>

// Cờ bảo vệ — false = OFFLINE, mqttLoop() bị vô hiệu hóa hoàn toàn
static bool _mqttEnabled = false;

// ─────────────────────────────────────────────────────────────
// Callback xử lý lệnh từ MQTT broker
// ─────────────────────────────────────────────────────────────
void callback(char* topic, byte* payload, unsigned int length) {
    String msg;
    for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

    if (msg == "screendiplaytft_on")  screendiplaytft_on();
    if (msg == "screendiplaytft_off") screendiplaytft_off();
    if (msg == "relay2_on")           relay2_on();
    if (msg == "relay2_off")          relay2_off();
    if (msg == "relay3_on")           relay3_on();
    if (msg == "relay3_off")          relay3_off();
}

// ─────────────────────────────────────────────────────────────
void setupMQTT() {
    client.setServer(MQTT_SERVER, MQTT_PORT);
    client.setCallback(callback);
    _mqttEnabled = true;
    Serial.println(F("[MQTT] Setup xong, sẵn sàng kết nối."));
}

void reconnectMQTT() {
    if (!_mqttEnabled) return;
    if (WiFi.status() != WL_CONNECTED) return;

    static unsigned long _lastTry = 0;
    unsigned long now = millis();
    if (now - _lastTry < MQTT_RETRY_INTERVAL_MS) return;
    _lastTry = now;

    Serial.print(F("[MQTT] Kết nối... "));
    if (client.connect(deviceId.c_str())) {
        client.subscribe(("home/" + deviceId + "/relay").c_str());
        Serial.println(F("OK"));
    } else {
        Serial.println("[MQTT] Thất bại, code=" + String(client.state()));
    }
}

void sendStatusMQTT() {
    if (!_mqttEnabled || !client.connected()) return;

    String json = "{";
    json += "\"screendiplaytft\":" + String(state1 ? "true" : "false") + ",";
    json += "\"relay2\":"          + String(state2 ? "true" : "false") + ",";
    json += "\"relay3\":"          + String(state3 ? "true" : "false");
    json += "}";
    client.publish(("home/" + deviceId + "/status").c_str(), json.c_str(), true);
}

void mqttLoop() {
    if (!_mqttEnabled) return;           // OFFLINE → thoát ngay
    if (WiFi.status() != WL_CONNECTED) return;

    if (!client.connected()) reconnectMQTT();
    client.loop();
}

// ─────────────────────────────────────────────────────────────
// Gọi khi chuyển OFFLINE — ngắt kết nối, block hoàn toàn
// ─────────────────────────────────────────────────────────────
void mqttForceStop() {
    _mqttEnabled = false;
    if (client.connected()) {
        client.disconnect();
    }
    Serial.println(F("[MQTT] Đã dừng hoàn toàn (OFFLINE)."));
}
