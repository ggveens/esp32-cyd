// mqtt_manager.h — Quản lý MQTT
#pragma once

void setupMQTT();
void reconnectMQTT();
void sendStatusMQTT();
void mqttLoop();
void mqttForceStop();   // OFFLINE: ngắt kết nối, vô hiệu hóa reconnect
