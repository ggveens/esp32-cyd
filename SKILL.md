---
name: esp32-cyd-iot-controller
description: >
  Skill chuyên biệt cho dự án ESP32 CYD (Cheap Yellow Display) Hybrid IoT Controller — 
  bộ điều khiển IoT thông minh tích hợp màn hình cảm ứng 2.4" ILI9341, 3 relay, 
  RGB LED, Web UI, MQTT, đồng hồ âm lịch/dương lịch, và lịch hẹn tự động.
  
  Sử dụng skill này khi người dùng hỏi về BẤT KỲ vấn đề nào liên quan đến:
  - Thêm tính năng, sửa lỗi, hoặc tối ưu code cho dự án esp32-cyd
  - Viết code C++/Arduino cho ESP32 với TFT_eSPI, MQTT, WiFi, FreeRTOS
  - Kiến trúc ONLINE/OFFLINE, WebServer, relay control, schedule hẹn giờ
  - Thiết kế UI trên màn hình TFT ILI9341 320x240
  - Tích hợp cảm ứng XPT2046, RGB LED PWM, NVS Preferences
  - Mở rộng dự án: Alexa, HomeAssistant, OTA, BLE, cảm biến, dashboard
  - Bất kỳ câu hỏi về phần cứng CYD (Cheap Yellow Display), pinout, thư viện
compatibility: "PlatformIO + Arduino framework, ESP32, TFT_eSPI@^2.5.43, PubSubClient@^2.8, ArduinoJson, LVGL@^9.5.0"
---

# ESP32 CYD Hybrid IoT Controller — Skill Reference

## 1. Tổng quan kiến trúc

### Hardware
| Thành phần | Chip/Driver | Pin |
|---|---|---|
| Màn hình TFT | ILI9341_2 (HSPI) | SPI@55MHz, BL=GPIO21 |
| Cảm ứng | XPT2046 (built-in) | CS=33, IRQ=36, MOSI=32, MISO=39, CLK=25 |
| Relay 1 (TFT BL) | GPIO27 | RELAY_ON=HIGH |
| Relay 2 | GPIO22 | RELAY_ON=HIGH |
| Relay 3 | GPIO21 | RELAY_ON=HIGH |
| RGB LED | PWM | R=4, G=16, B=17 (active LOW, dim=245/255) |
| Nút reset | GPIO0 | INPUT_PULLUP (giữ 3s → factory reset) |
| DeviceID | eFuse MAC | `device_XXXX` (4 hex cuối MAC) |

### Phần mềm — Module map
```
src/
├── main.cpp              — Boot sequence v10, loop priority order
├── config.h              — Tất cả #define (pin, MQTT, NTP, API URL)
├── core/globals.h/.cpp   — Biến toàn cục (TFT, WebServer, relay states…)
├── wifi/wifi_manager     — State machine: IDLE→BOOT→CONNECTING→CONNECTED→AP_ONLY
├── mqtt/mqtt_manager     — PubSubClient, callback, enable/disable flag
├── clock/time_manager    — FreeRTOS task Core 0, Time API + lunar date
├── schedule/schedule     — 10 slot, hẹn giờ relay theo giờ:phút
├── relay/relay           — Flag-driven: handler chỉ set GPIO, loop() vẽ TFT
├── touch/touch           — tft.getTouch(), throttle 30ms, debounce
├── led/led               — RGB PWM: đỏ=mất WiFi, xanh=MQTT OK, lá=WiFi only
├── ui/ui.cpp             — drawHeader/drawTime/drawRelays/drawFooter/drawUI
├── ui/ui2.cpp            — Màn hình 2: animation bouncing ball + nút back
├── server/web_server     — HTTP routes, PROGMEM HTML, offline/online routes
├── online/online_manager — onlineStart/onlineLoop/onlineStop
└── offline/offline_manager — offlineStart/offlineLoop/offlineStop
```

---

## 2. Nguyên tắc thiết kế cốt lõi (PHẢI tuân thủ khi thêm code)

### A. Loop priority (main.cpp)
```
1. server.handleClient() × 3 + yield()   ← LUÔN đầu tiên
2. relayUpdatePending flag               ← drawRelays() NGOÀI handler
3. BTN_RESET (factory reset 3s)
4. onlineLoop() / offlineLoop()
5. Screen switching (flag-driven)
6. drawTime() mỗi 1s | updateUI2() mỗi 50ms
```

### B. Relay handler pattern (KHÔNG được vi phạm)
```cpp
// ✅ ĐÚNG — handler < 1ms
static inline void _relaySet(uint8_t pin, bool on, bool& stateVar) {
    stateVar = on;
    digitalWrite(pin, on ? RELAY_ON : RELAY_OFF);
    server.send(200, "text/plain", "OK");  // Gửi ngay
    relayUpdatePending = true;             // Loop sẽ vẽ sau
}

// ❌ SAI — gây timeout browser
void relay_on() {
    digitalWrite(pin, HIGH);
    drawRelays();          // SPI 30-60ms trong handler!
    server.send(200...);
}
```

### C. HTML phải dùng PROGMEM (send_P)
```cpp
static const char HTML[] PROGMEM = R"rawliteral(...)rawliteral";
server.send_P(200, "text/html", HTML);
// Lý do: tránh alloc 18KB heap trên stack
```

### D. Online/Offline tách biệt hoàn toàn
- **OFFLINE**: chỉ WIFI_AP, không STA, không scan, không MQTT, không NTP
- **ONLINE**: WIFI_AP_STA, wifiMaintain(), mqttLoop(), timeManagerResume()
- Chuyển mode: `offlineStop()` → `onlineStart()` hoặc ngược lại

### E. FreeRTOS Task cho Time (Core 0)
```cpp
xTaskCreatePinnedToCore(timeSyncTask, "TimeSync", 6144, nullptr, 1, &handle, 0);
// Core 0: Time/WiFi tasks
// Core 1 (mặc định Arduino): loop() — TFT, WebServer, relay
```

---

## 3. MQTT

**Broker**: `broker.hivemq.com:1883` (public, không auth)  
**Topic subscribe**: `home/{deviceId}/relay`  
**Topic publish**: `home/{deviceId}/status`  

**Commands nhận được:**
```
screendiplaytft_on / screendiplaytft_off
relay2_on / relay2_off
relay3_on / relay3_off
```

**Status JSON gửi mỗi 2s:**
```json
{"screendiplaytft": true, "relay2": false, "relay3": false}
```

---

## 4. Web API Endpoints

### OFFLINE routes (192.168.4.1)
| Method | Path | Mô tả |
|---|---|---|
| GET | `/` | HTML trang chính (ultra-slim, không auto-poll) |
| GET | `/relay/r1/on`, `/relay/r1/off` | Relay 1 |
| GET | `/relay/r2/on`, `/relay/r2/off` | Relay 2 |
| GET | `/relay/r3/on`, `/relay/r3/off` | Relay 3 |
| GET | `/api/relay/status` | JSON trạng thái 3 relay |
| GET | `/api/mode/status` | `{"mode":"offline"}` |
| POST | `/wifi/save` | Body: `ssid=X&pass=Y` → lưu vào NVS |
| GET | `/device/restart` | Restart ESP32 |
| GET | `/device/reset` | Factory reset (xóa NVS) |

### ONLINE routes (thêm vào trên)
| Method | Path | Mô tả |
|---|---|---|
| GET | `/wifi/scan` | JSON danh sách WiFi |
| GET | `/wifi/delete` | Xóa WiFi slot |
| GET | `/wifi/clear` | Xóa tất cả WiFi |
| GET/POST | `/schedule/add` | Thêm lịch |
| GET | `/schedule/list` | Danh sách 10 lịch |
| GET | `/schedule/delete?id=N` | Xóa lịch N |
| GET | `/schedule/clear` | Xóa tất cả lịch |
| GET | `/api/learn-all` | Reset về mặc định |
| GET | `/api/mode/switch` | Chuyển offline/online |

---

## 5. NVS Preferences — cấu trúc lưu trữ

```cpp
// WiFi (namespace "wifi")
prefs.getString("s0") ... prefs.getString("s4")  // SSIDs (max 5)
prefs.getString("p0") ... prefs.getString("p4")  // Passwords

// Schedule (namespace "sched") — serialize mảng 10 struct Schedule
// Config (namespace "cfg") — cài đặt khác (chưa triển khai)
```

---

## 6. Màn hình TFT — Layout SCREEN_MAIN (320×240)

```
Y=0   ┌──────────────────────────────┐
      │ [HEADER] ID: xxxx  AP:192.168.4.1  ← drawHeader()
Y=40  ├──────────────────────────────┤
      │ [TIME] HH:MM:SS centered      ← drawTime()
      │ [DATE] AL: dd/mm/yyyy         ← lunar date
Y=100 ├──────────────────────────────┤
      │ [RELAY1] DISPLAY GPIO27 [ON]  ← drawRelayCompact(100)
Y=145 │ [RELAY2] RELAY 2  GPIO22 [OFF]← drawRelayCompact(145)
Y=190 │ [RELAY3] RELAY 3  GPIO21 [OFF]← drawRelayCompact(190)
Y=240 ├──────────────────────────────┤
      │ MQTT: ON   RSSI: -65  UP: 2h  ← drawFooter()
Y=280 └──────────────────────────────┘
```

**Màu palette:**
```cpp
#define BG_COLOR     0x0841  // Navy dark
#define CARD_COLOR   0x1082  // Dark card
#define ACCENT_COLOR 0x2D7F  // Cyan accent
#define TIME_COLOR   0xFD20  // Orange
#define TEXT_PRIMARY 0xFFFF  // White
```

---

## 7. Dependency versions (platformio.ini)

```ini
platform = espressif32@6.7.0
board    = esp32dev
framework = arduino

lib_deps =
    bodmer/TFT_eSPI@^2.5.43
    knolleary/PubSubClient@^2.8
    https://github.com/PaulStoffregen/XPT2046_Touchscreen.git
    lvgl/lvgl@^9.5.0
    bblanchon/ArduinoJson
```

**LVGL hiện tại**: Được import nhưng **chưa dùng** (dự phòng cho UI nâng cao).  
**XPT2046_Touchscreen**: Được import nhưng đã xóa khỏi code — dùng `tft.getTouch()` built-in.

---

## 8. Patterns thường gặp khi mở rộng

### Thêm Relay mới
1. Thêm `#define RELAY4 XX` vào `config.h`
2. Thêm `extern bool state4;` vào `globals.h`, khởi tạo trong `globals.cpp`
3. Thêm helper trong `relay.cpp`: `relay4_on/off()`
4. Thêm route `/relay/r4/on` và `/relay/r4/off` vào `web_server.cpp`
5. Cập nhật `buildRelayStatus()` JSON, `drawRelays()` UI, MQTT callback

### Thêm màn hình mới
1. Thêm `SCREEN_3` vào enum `Screen` trong `globals.h`
2. Tạo file `src/ui/ui3.cpp` và `ui3.h`
3. Thêm `else if (currentScreen == SCREEN_3) drawUI3();` vào loop() main
4. Thêm touch handler trong `touch.cpp`

### Thêm cảm biến
1. Đọc cảm biến trong FreeRTOS task riêng (Core 0) hoặc trong `onlineLoop()`
2. Publish lên MQTT topic `home/{deviceId}/sensor`
3. Hiển thị trên TFT với `drawSensorData()` tương tự `drawRelayCompact()`

---

## 9. Lưu ý quan trọng khi code

- **SPI conflict**: TFT và XPT2046 dùng cùng SPI bus → tft.getTouch() tự manage CS
- **Heap**: HTML page ~18KB → luôn dùng PROGMEM + send_P, không String concatenation
- **Thread safety**: Chỉ `timeDataMutex` protect `lunarDate`, `timeSyncSuccess`, `lastApiError`
- **WiFi AP password**: hardcode `"minhkhongbiet"` — đổi trong `onlineStart()` và `offlineStart()`
- **Factory reset**: Giữ GPIO0 3 giây → xóa NVS "wifi", "sched", "cfg"
- **DeviceID**: `device_XXXX` từ 2 byte cuối MAC — unique per board, dùng làm MQTT client ID
