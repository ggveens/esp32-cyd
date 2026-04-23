#include "mqtt_manager.h"
#include "../core/globals.h"
#include "../relay/relay.h"
#include "../config.h"

void callback(char* topic, byte* payload, unsigned int length){
  String msg;
  for(int i=0;i<length;i++) msg+=(char)payload[i];

  if(msg=="relay1_on") relay1_on();
  if(msg=="relay1_off") relay1_off();
  if(msg=="relay2_on") relay2_on();
  if(msg=="relay2_off") relay2_off();
  if(msg=="relay3_on") relay3_on();
  if(msg=="relay3_off") relay3_off();
}

void setupMQTT(){
  client.setServer(MQTT_SERVER,1883);
  client.setCallback(callback);
}

void reconnectMQTT(){
  while(!client.connected()){
    if(client.connect(deviceId.c_str())){
      client.subscribe(("home/"+deviceId+"/relay").c_str());
    } else delay(2000);
  }
}

void sendStatusMQTT(){
  String json = "{";
  json += "\"relay1\":" + String(state1?"true":"false") + ",";
  json += "\"relay2\":" + String(state2?"true":"false") + ",";
  json += "\"relay3\":" + String(state3?"true":"false");
  json += "}";
  client.publish(("home/"+deviceId+"/status").c_str(), json.c_str(), true);
}

void mqttLoop(){
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) reconnectMQTT();
    client.loop();
  }
}