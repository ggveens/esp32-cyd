// relay.cpp — Điều khiển relay tức thời, KHÔNG block HTTP handler
//
// ═══════════════════════════════════════════════════════════════
// NGUYÊN NHÂN LAG: drawRelays() gọi SPI TFT ~30-60ms TRONG handler
//   → server.send() phải đợi SPI xong mới gửi → browser timeout
//
// GIẢI PHÁP: Tách hoàn toàn
//   1. Handler: chỉ set GPIO + state + cờ + gửi "OK" ngay lập tức
//   2. loop() (main): kiểm tra cờ → drawRelays() NGOÀI handler
//
// Kết quả: relay handler < 1ms, WebServer không bị block
// ═══════════════════════════════════════════════════════════════

#include "relay.h"
#include "../core/globals.h"
#include "../config.h"
#include "../ui/ui.h"

// Cờ pending — được set bởi handler, xử lý bởi loop()
volatile bool relayUpdatePending = false;

// ─── Inline helper: set GPIO + state + send OK + đánh dấu cần vẽ ───
static inline void _relaySet(uint8_t pin, bool on, bool& stateVar) {
    stateVar = on;
    digitalWrite(pin, on ? RELAY_ON : RELAY_OFF);
    server.send(200, "text/plain", "OK");   // Gửi ngay — không block
    relayUpdatePending = true;              // Loop sẽ vẽ TFT sau
}

void screendiplaytft_on()  { _relaySet(SCREENDIPLAYTFT, true,  state1); }
void screendiplaytft_off() { _relaySet(SCREENDIPLAYTFT, false, state1); }
void relay2_on()           { _relaySet(RELAY2,           true,  state2); }
void relay2_off()          { _relaySet(RELAY2,           false, state2); }
void relay3_on()           { _relaySet(RELAY3,           true,  state3); }
void relay3_off()          { _relaySet(RELAY3,           false, state3); }
