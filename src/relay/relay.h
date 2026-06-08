#pragma once
#include <Arduino.h>

// Cờ pending: loop() kiểm tra để drawRelays() sau khi handler trả response
extern volatile bool relayUpdatePending;

void screendiplaytft_on();
void screendiplaytft_off();
void relay2_on();
void relay2_off();
void relay3_on();
void relay3_off();
