#include "relay.h"
#include "../core/globals.h"
#include "../config.h"
#include "../ui/ui.h"

void relay1_on(){ state1=true; digitalWrite(RELAY1,HIGH); drawRelays(); server.send(200,"text/plain","OK"); }
void relay1_off(){ state1=false; digitalWrite(RELAY1,LOW); drawRelays(); server.send(200,"text/plain","OK"); }

void relay2_on(){ state2=true; digitalWrite(RELAY2,LOW); drawRelays(); server.send(200,"text/plain","OK"); }
void relay2_off(){ state2=false; digitalWrite(RELAY2,HIGH); drawRelays(); server.send(200,"text/plain","OK"); }

void relay3_on(){ state3=true; digitalWrite(RELAY3,LOW); drawRelays(); server.send(200,"text/plain","OK"); }
void relay3_off(){ state3=false; digitalWrite(RELAY3,HIGH); drawRelays(); server.send(200,"text/plain","OK"); }