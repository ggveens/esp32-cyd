// web_server.cpp — WebServer ESP32 v11 OFFLINE ULTRA-SLIM
// ═══════════════════════════════════════════════════════════════
//
//  KIẾN TRÚC V11 — TÁCH BIỆT HOÀN TOÀN:
//
//  ┌─────────────────────────────────────────────────────────┐
//  │  OFFLINE Web (192.168.4.1) — CHỈ RELAY ON/OFF           │
//  │  ✅ Relay ON/OFF (3 relay)                              │
//  │  ✅ Mode status                                         │
//  │  ✅ Device Restart                                      │
//  │  ✅ Factory Reset                                       │
//  │  ✅ WiFi Save (để chuyển sang Online)                   │
//  │  ❌ Schedule — BỎ HOÀN TOÀN (chỉ Online)               │
//  │  ❌ WiFi Scan — BLOCKED                                 │
//  │  ❌ Learn All — BỎ (không cần trong slim offline)       │
//  │  ❌ WiFi Clear — BỎ (không cần trong slim offline)      │
//  └─────────────────────────────────────────────────────────┘
//  ┌─────────────────────────────────────────────────────────┐
//  │  ONLINE Web — Đầy đủ tính năng                         │
//  │  ✅ Schedule (add/list/delete/clear)                    │
//  │  ✅ WiFi Scan/Save/Delete/Clear                         │
//  │  ✅ Learn All                                           │
//  │  ✅ Mode switch                                         │
//  └─────────────────────────────────────────────────────────┘
//
//  HTML OFFLINE: KHÔNG có loadSchedules(), KHÔNG setTimeout poll nào
//  → Không có bất kỳ request nào tự phát sau khi trang load xong
//  → ESP32 AP hoàn toàn rảnh, relay response < 50ms
// ═══════════════════════════════════════════════════════════════

#include "web_server.h"
#include "../core/globals.h"
#include "../relay/relay.h"
#include "../wifi/wifi_manager.h"
#include "../schedule/schedule.h"
#include "../config.h"
#include "../offline/offline_manager.h"
#include "../online/online_manager.h"

#include <WiFi.h>

// ─────────────────────────────────────────────────────────────
// Screen
// ─────────────────────────────────────────────────────────────
void handleScreen1() { currentScreen = SCREEN_MAIN; server.send(200, "text/plain", "OK"); }
void handleScreen2() { currentScreen = SCREEN_2;    server.send(200, "text/plain", "OK"); }

// ─────────────────────────────────────────────────────────────
// Relay state JSON
// ─────────────────────────────────────────────────────────────
static String buildRelayStatus() {
    String j = "{";
    j += "\"r1\":" + String(state1 ? "true" : "false") + ",";
    j += "\"r2\":" + String(state2 ? "true" : "false") + ",";
    j += "\"r3\":" + String(state3 ? "true" : "false");
    j += "}";
    return j;
}

void handleRelayStatus() {
    server.sendHeader("Connection", "close");
    server.sendHeader("Cache-Control", "no-cache");
    server.send(200, "application/json", buildRelayStatus());
}

// ─────────────────────────────────────────────────────────────
// Device Reset — chỉ restart ESP32
// ─────────────────────────────────────────────────────────────
void handleResetDevice() {
    server.send(200, "application/json", "{\"status\":\"restarting\",\"msg\":\"ESP32 đang khởi động lại...\"}");
    server.client().flush();
    delay(800);
    ESP.restart();
}

// ─────────────────────────────────────────────────────────────
// Learn All — Reset schedule về mặc định + relay về OFF
// CHỈ dùng trong ONLINE mode
// ─────────────────────────────────────────────────────────────
void handleLearnAll() {
    if (isOfflineMode()) {
        server.send(403, "application/json", "{\"error\":\"Learn All chỉ khả dụng khi ONLINE\"}");
        return;
    }
    for (int i = 0; i < MAX_SCHEDULE; i++) {
        schedules[i].active = false;
    }
    digitalWrite(SCREENDIPLAYTFT, HIGH); state1 = true;
    digitalWrite(RELAY2, HIGH);          state2 = false;
    digitalWrite(RELAY3, HIGH);          state3 = false;
    Serial.println(F("[LEARN_ALL] Reset schedule và relay về mặc định"));
    server.send(200, "application/json",
        "{\"status\":\"ok\",\"msg\":\"Đã xóa tất cả lịch trình và reset relay về mặc định\"}");
}

// ─────────────────────────────────────────────────────────────
// Clear WiFi — Chỉ dùng trong ONLINE mode
// ─────────────────────────────────────────────────────────────
void handleClearWiFi() {
    if (isOfflineMode()) {
        server.send(403, "application/json", "{\"error\":\"Clear WiFi chỉ khả dụng khi ONLINE\"}");
        return;
    }
    prefs.begin("wifi", false);
    prefs.clear();
    prefs.end();
    for (int i = 0; i < MAX_WIFI; i++) {
        ssids[i]  = "";
        passes[i] = "";
    }
    Serial.println(F("[CLEAR_WIFI] Đã xóa tất cả WiFi credentials"));
    server.send(200, "application/json",
        "{\"status\":\"ok\",\"msg\":\"Đã xóa tất cả WiFi đã lưu. Thiết bị sẽ ở OFFLINE mode.\"}");
}

// ─────────────────────────────────────────────────────────────
// Factory Reset — Xóa TOÀN BỘ dữ liệu + restart
// Khả dụng cả OFFLINE lẫn ONLINE
// ─────────────────────────────────────────────────────────────
void handleFactoryReset() {
    Serial.println(F("[FACTORY_RESET] XÓA TOÀN BỘ DỮ LIỆU..."));

    prefs.begin("wifi", false);
    prefs.clear();
    prefs.end();

    for (int i = 0; i < MAX_SCHEDULE; i++) {
        schedules[i].active = false;
    }

    prefs.begin("sched", false);
    prefs.clear();
    prefs.end();

    prefs.begin("cfg", false);
    prefs.clear();
    prefs.end();

    Serial.println(F("[FACTORY_RESET] ✓ Xóa hoàn tất → Khởi động lại..."));

    server.send(200, "application/json",
        "{\"status\":\"ok\",\"msg\":\"Factory Reset hoàn tất. Thiết bị đang khởi động lại...\"}");
    server.client().flush();
    delay(1200);
    ESP.restart();
}

// ─────────────────────────────────────────────────────────────
// Mode status
// ─────────────────────────────────────────────────────────────
void handleModeStatus() {
    bool hasSsid = false;
    for (int i = 0; i < MAX_WIFI; i++) {
        if (ssids[i].length() > 0) { hasSsid = true; break; }
    }
    String mode = isOfflineMode() ? "offline" : "online";
    String json = "{\"mode\":\"" + mode + "\","
                  "\"hasWifi\":" + (hasSsid ? "true" : "false") + ","
                  "\"deviceId\":\"" + deviceId + "\","
                  "\"uptime\":" + String(millis() / 1000) + "}";
    server.sendHeader("Connection", "close");
    server.sendHeader("Cache-Control", "no-cache");
    server.send(200, "application/json", json);
}

// ─────────────────────────────────────────────────────────────
// Schedule handlers — CHỈ dùng trong ONLINE mode
// ─────────────────────────────────────────────────────────────
void handleScheduleAdd() {
    if (isOfflineMode()) {
        server.send(403, "application/json", "{\"error\":\"Schedule chỉ khả dụng khi ONLINE\"}");
        return;
    }
    if (!server.hasArg("relay") || !server.hasArg("on") || !server.hasArg("off")) {
        server.send(400, "application/json", "{\"error\":\"missing params\"}"); return;
    }
    int relay = server.arg("relay").toInt();
    if (relay < 1 || relay > 3) {
        server.send(400, "application/json", "{\"error\":\"relay phải là 1-3\"}"); return;
    }
    String onT  = server.arg("on");
    String offT = server.arg("off");
    int onH  = -1, onM  = -1;
    int offH = -1, offM = -1;
    if (onT  != "null") sscanf(onT.c_str(),  "%d:%d", &onH,  &onM);
    if (offT != "null") sscanf(offT.c_str(), "%d:%d", &offH, &offM);

    for (int i = 0; i < MAX_SCHEDULE; i++) {
        if (!schedules[i].active) {
            schedules[i] = { true, relay, onH, onM, offH, offM, false, false };
            server.send(200, "application/json", "{\"status\":\"ok\",\"id\":" + String(i) + "}");
            return;
        }
    }
    server.send(503, "application/json", "{\"error\":\"Schedule đã đầy (max " + String(MAX_SCHEDULE) + ")\"}");
}

void handleScheduleList() {
    if (isOfflineMode()) {
        server.send(200, "application/json", "[]");
        return;
    }
    String json = "[";
    bool first = true;
    for (int i = 0; i < MAX_SCHEDULE; i++) {
        if (!schedules[i].active) continue;
        if (!first) json += ",";
        first = false;
        String onStr  = (schedules[i].onH  < 0) ? "-" : (String(schedules[i].onH)  + ":" + (schedules[i].onM  < 10 ? "0" : "") + String(schedules[i].onM));
        String offStr = (schedules[i].offH < 0) ? "-" : (String(schedules[i].offH) + ":" + (schedules[i].offM < 10 ? "0" : "") + String(schedules[i].offM));
        json += "{\"id\":" + String(i) + ",\"relay\":" + String(schedules[i].relay) +
                ",\"on\":\"" + onStr + "\",\"off\":\"" + offStr + "\"}";
    }
    json += "]";
    server.send(200, "application/json", json);
}

void handleScheduleDelete() {
    if (isOfflineMode()) {
        server.send(403, "application/json", "{\"error\":\"Schedule chỉ khả dụng khi ONLINE\"}");
        return;
    }
    if (!server.hasArg("id")) { server.send(400, "application/json", "{\"error\":\"missing id\"}"); return; }
    int id = server.arg("id").toInt();
    if (id >= 0 && id < MAX_SCHEDULE) {
        schedules[id].active   = false;
        schedules[id].firedOn  = false;
        schedules[id].firedOff = false;
        server.send(200, "application/json", "{\"status\":\"ok\"}");
    } else {
        server.send(400, "application/json", "{\"error\":\"id không hợp lệ\"}");
    }
}

void handleScheduleClear() {
    if (isOfflineMode()) {
        server.send(403, "application/json", "{\"error\":\"Schedule chỉ khả dụng khi ONLINE\"}");
        return;
    }
    for (int i = 0; i < MAX_SCHEDULE; i++) {
        schedules[i].active   = false;
        schedules[i].firedOn  = false;
        schedules[i].firedOff = false;
    }
    server.send(200, "application/json", "{\"status\":\"ok\",\"msg\":\"Đã xóa tất cả lịch trình\"}");
}

// ─────────────────────────────────────────────────────────────
// WiFi endpoints
// ─────────────────────────────────────────────────────────────
void handleScanWiFi() {
    if (isOfflineMode()) {
        server.send(403, "application/json", "{\"error\":\"OFFLINE mode — WiFi scan bị chặn\"}");
        return;
    }
    int n = WiFi.scanNetworks(false, false);
    if (n <= 0) {
        server.send(200, "application/json", "[]");
        return;
    }
    String json = "[";
    for (int i = 0; i < n; i++) {
        if (i) json += ",";
        json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) +
                ",\"auth\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false") + "}";
    }
    json += "]";
    server.send(200, "application/json", json);
}

void handleListSavedWiFi() {
    String json = "[";
    bool first = true;
    for (int i = 0; i < MAX_WIFI; i++) {
        if (ssids[i] == "") continue;
        if (!first) json += ",";
        first = false;
        json += "{\"ssid\":\"" + ssids[i] + "\",\"idx\":" + String(i) + "}";
    }
    json += "]";
    server.send(200, "application/json", json);
}

void handleSetWiFi() {
    if (!server.hasArg("ssid") || !server.hasArg("pass")) {
        server.send(400, "application/json", "{\"error\":\"missing ssid/pass\"}"); return;
    }
    String newSsid = server.arg("ssid");
    String newPass = server.arg("pass");
    if (newSsid.length() == 0) {
        server.send(400, "application/json", "{\"error\":\"SSID rỗng\"}"); return;
    }
    saveWiFi(newSsid, newPass);
    server.send(200, "application/json", "{\"status\":\"ok\",\"msg\":\"Đang khởi động lại để kết nối WiFi...\"}");
    server.client().flush();
    delay(1000);
    ESP.restart();
}

void handleDeleteSavedWiFi() {
    if (!server.hasArg("idx")) {
        server.send(400, "application/json", "{\"error\":\"missing idx\"}"); return;
    }
    int idx = server.arg("idx").toInt();
    if (idx < 0 || idx >= MAX_WIFI) {
        server.send(400, "application/json", "{\"error\":\"idx không hợp lệ\"}"); return;
    }
    prefs.begin("wifi", false);
    prefs.remove(("s" + String(idx)).c_str());
    prefs.remove(("p" + String(idx)).c_str());
    prefs.end();
    ssids[idx]  = "";
    passes[idx] = "";
    server.send(200, "application/json", "{\"status\":\"ok\",\"msg\":\"Đã xóa WiFi này\"}");
}

// ─────────────────────────────────────────────────────────────
// Mode Switch
// ─────────────────────────────────────────────────────────────
void handleModeSwitch() {
    String path = server.uri();
    if (path == "/api/mode/offline") {
        if (isOfflineMode()) {
            server.send(200, "application/json", "{\"mode\":\"offline\",\"changed\":false}");
            return;
        }
        server.send(200, "application/json", "{\"mode\":\"offline\",\"changed\":true}");
        server.client().flush();
        delay(100);
        onlineStop();
        offlineStart();
    } else if (path == "/api/mode/online") {
        if (isOnlineMode()) {
            server.send(200, "application/json", "{\"mode\":\"online\",\"changed\":false}");
            return;
        }
        bool hasSsid = false;
        for (int i = 0; i < MAX_WIFI; i++) {
            if (ssids[i].length() > 0) { hasSsid = true; break; }
        }
        if (!hasSsid) {
            server.send(400, "application/json",
                "{\"error\":\"Chưa có WiFi được lưu. Hãy cấu hình WiFi trước.\"}");
            return;
        }
        server.send(200, "application/json", "{\"mode\":\"online\",\"changed\":true}");
        server.client().flush();
        delay(100);
        offlineStop();
        onlineStart();
    } else {
        server.send(400, "application/json", "{\"error\":\"Unknown mode\"}");
    }
}

// ═══════════════════════════════════════════════════════════════
//  HTML OFFLINE V11 — ULTRA SLIM
//
//  NGUYÊN TẮC THIẾT KẾ:
//  1. window.onload CHỈ gọi 1 request duy nhất: /api/mode/status
//  2. KHÔNG có loadSchedules() — schedule bị bỏ hoàn toàn
//  3. KHÔNG có setInterval, KHÔNG có setTimeout poll lặp lại
//  4. Relay button: 1 click = 1 fetch = 1 response → done
//  5. KHÔNG có phần "Hẹn giờ" (schedule pane đã bị xóa)
//
//  Tại sao điều này fix lag:
//  - Trước: window.onload gọi loadDeviceInfo() + loadSchedules()
//    → 2 request đồng thời ngay khi trang load
//    → ESP32 AP phải xử lý 2 connections cùng lúc + vẽ TFT
//    → timeout, queue, lag
//  - Sau: window.onload chỉ gọi 1 request duy nhất
//    → ESP32 xử lý tuần tự, không bao giờ bị quá tải
// ═══════════════════════════════════════════════════════════════
static const char HTML_OFFLINE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0,maximum-scale=1.0,user-scalable=no">
<title>ESP32 OFFLINE</title>
<style>
:root{--bg:#0f172a;--card:#1e293b;--border:#334155;--primary:#3b82f6;--success:#22c55e;--danger:#ef4444;--warn:#f59e0b;--text:#f8fafc;--dim:#94a3b8}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent;margin:0;padding:0}
body{font-family:'Segoe UI',sans-serif;background:var(--bg);color:var(--text);min-height:100vh;padding:14px}
h1{font-size:20px;font-weight:800;margin:8px 0 2px;letter-spacing:.5px}
.device-id{font-size:11px;color:var(--dim);letter-spacing:1px;margin-bottom:14px}
.badge-offline{display:inline-flex;align-items:center;gap:6px;background:rgba(245,158,11,.15);color:var(--warn);border:1px solid var(--warn);border-radius:20px;padding:4px 14px;font-size:12px;font-weight:700;margin-bottom:14px}
.dot{width:8px;height:8px;border-radius:50%;background:var(--warn);animation:pulse 1.5s infinite}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.3}}
.card{background:var(--card);border-radius:16px;padding:16px;margin-bottom:12px;border:1px solid rgba(255,255,255,.05)}
.card-title{font-size:11px;text-transform:uppercase;letter-spacing:1px;color:var(--dim);margin-bottom:12px;font-weight:600}
.relay-row{display:flex;align-items:center;justify-content:space-between;padding:10px 0;border-bottom:1px solid var(--border)}
.relay-row:last-child{border-bottom:none;padding-bottom:0}
.relay-info{flex:1}
.relay-name{font-weight:700;font-size:14px}
.relay-sub{font-size:11px;color:var(--dim);margin-top:2px}
.relay-state{font-size:11px;font-weight:700;margin-top:3px}
.state-on{color:var(--success)}
.state-off{color:var(--danger)}
.btn-pair{display:flex;gap:8px;flex-shrink:0}
.btn{border:none;border-radius:10px;padding:10px 16px;font-weight:700;cursor:pointer;transition:transform .15s,opacity .15s;font-size:13px;letter-spacing:.5px;min-width:56px}
.btn:active{transform:scale(.91);opacity:.8}
.btn-on{background:var(--success);color:#fff}
.btn-off{background:var(--danger);color:#fff}
.btn-warn{background:var(--warn);color:#000}
.btn-primary{background:var(--primary);color:#fff}
.btn-gray{background:#334155;color:var(--text)}
.btn-full{width:100%;padding:13px;border-radius:12px;font-size:14px;margin-bottom:10px}
#toast{position:fixed;bottom:20px;left:50%;transform:translateX(-50%);background:#1e293b;border:1px solid var(--border);color:var(--text);padding:12px 22px;border-radius:12px;font-size:13px;z-index:999;opacity:0;transition:opacity .3s;pointer-events:none;max-width:320px;text-align:center}
#toast.show{opacity:1}
.pane{position:fixed;top:0;left:0;width:100%;height:100%;background:var(--bg);z-index:100;display:none;padding:20px;overflow-y:auto}
.pane-open{display:block!important}
.pane-header{display:flex;justify-content:space-between;align-items:center;margin-bottom:22px}
.pane-header h2{font-size:18px;font-weight:700}
.close-btn{font-size:28px;cursor:pointer;color:var(--dim);line-height:1;padding:4px 8px}
.section-title{font-size:12px;text-transform:uppercase;letter-spacing:1px;color:var(--dim);font-weight:600;margin:18px 0 10px;border-bottom:1px solid var(--border);padding-bottom:6px}
.info-row{display:flex;justify-content:space-between;padding:8px 0;border-bottom:1px solid rgba(255,255,255,.04);font-size:13px}
.info-row:last-child{border-bottom:none}
.info-val{color:var(--dim);font-size:12px}
.danger-zone{border:1px solid var(--danger);border-radius:12px;padding:14px;margin-top:8px}
.danger-title{color:var(--danger);font-size:12px;font-weight:700;text-transform:uppercase;letter-spacing:1px;margin-bottom:8px}
.danger-desc{font-size:12px;color:var(--dim);margin-bottom:12px;line-height:1.6}
input[type=text],input[type=password]{width:100%;padding:12px;margin-bottom:10px;border-radius:10px;background:#0f172a;color:#fff;border:1px solid var(--border);font-size:15px}
.notice-offline{border-color:rgba(245,158,11,.3);background:rgba(245,158,11,.05)}
</style>
</head>
<body>

<h1>ESP32 REMOTE</h1>
<div class="device-id" id="did">NODE: —</div>
<div class="badge-offline"><span class="dot"></span>OFFLINE MODE — 192.168.4.1</div>

<!-- RELAY CONTROLS -->
<div class="card">
  <div class="card-title">⚡ Điều khiển thiết bị</div>
  <div class="relay-row">
    <div class="relay-info">
      <div class="relay-name">MÀN HÌNH TFT</div>
      <div class="relay-sub">Relay 1 · GPIO 27</div>
    </div>
    <div class="btn-pair">
      <button class="btn btn-on" onclick="relay('/relay/1/on','R1 BẬT')">BẬT</button>
      <button class="btn btn-off" onclick="relay('/relay/1/off','R1 TẮT')">TẮT</button>
    </div>
  </div>
  <div class="relay-row">
    <div class="relay-info">
      <div class="relay-name">RELAY 2</div>
      <div class="relay-sub">GPIO 22</div>
    </div>
    <div class="btn-pair">
      <button class="btn btn-on" onclick="relay('/relay/2/on','R2 BẬT')">BẬT</button>
      <button class="btn btn-off" onclick="relay('/relay/2/off','R2 TẮT')">TẮT</button>
    </div>
  </div>
  <div class="relay-row">
    <div class="relay-info">
      <div class="relay-name">RELAY 3</div>
      <div class="relay-sub">GPIO 21</div>
    </div>
    <div class="btn-pair">
      <button class="btn btn-on" onclick="relay('/relay/3/on','R3 BẬT')">BẬT</button>
      <button class="btn btn-off" onclick="relay('/relay/3/off','R3 TẮT')">TẮT</button>
    </div>
  </div>
</div>

<!-- SCREEN SWITCH -->
<div class="card">
  <div class="card-title">🖥 Màn hình ESP32</div>
  <div class="btn-pair" style="gap:10px">
    <button class="btn btn-primary" style="flex:1;padding:12px" onclick="relay('/screen/1','UI 1')">UI 1</button>
    <button class="btn btn-gray" style="flex:1;padding:12px" onclick="relay('/screen/2','UI 2')">UI 2</button>
  </div>
</div>

<!-- SETTINGS FAB -->
<div style="display:flex;justify-content:center;margin-bottom:16px">
  <button style="width:52px;height:52px;border-radius:50%;border:none;font-size:20px;cursor:pointer;background:#334155;color:#fff;display:flex;align-items:center;justify-content:center;box-shadow:0 4px 12px rgba(0,0,0,.3)" onclick="openPane('pane-manage')">⚙</button>
</div>

<!-- NOTICE -->
<div class="card notice-offline">
  <div style="font-size:12px;color:var(--warn);line-height:1.7">
    📡 <b>Chế độ OFFLINE</b> — Chỉ điều khiển relay qua AP local 192.168.4.1<br>
    Schedule &amp; tính năng nâng cao chỉ hoạt động khi <b>ONLINE</b>
  </div>
</div>

<!-- MANAGE PANE -->
<div id="pane-manage" class="pane">
  <div class="pane-header">
    <h2>⚙ Cài đặt thiết bị</h2>
    <span class="close-btn" onclick="closePane()">✕</span>
  </div>

  <div class="section-title">📋 Thông tin thiết bị</div>
  <div class="card" style="padding:12px">
    <div class="info-row"><span>Device ID</span><span class="info-val" id="info-id">—</span></div>
    <div class="info-row"><span>Chế độ</span><span class="info-val" style="color:var(--warn)">📡 OFFLINE</span></div>
    <div class="info-row"><span>AP IP</span><span class="info-val">192.168.4.1</span></div>
    <div class="info-row"><span>Uptime</span><span class="info-val" id="info-uptime">—</span></div>
  </div>

  <div class="section-title">📶 Kết nối Internet (chuyển sang Online)</div>
  <div class="card">
    <p style="font-size:12px;color:var(--dim);margin-bottom:12px;line-height:1.6">
      Nhập WiFi để sử dụng đầy đủ tính năng (Schedule, MQTT, ...). Thiết bị sẽ khởi động lại.
    </p>
    <input type="text" id="wz-ssid" placeholder="Tên WiFi (SSID)" autocomplete="off" autocorrect="off" spellcheck="false">
    <input type="password" id="wz-pass" placeholder="Mật khẩu WiFi">
    <button class="btn btn-primary btn-full" onclick="wizardSave()">💾 LƯU WIFI & KẾT NỐI</button>
  </div>

  <div class="section-title">🔧 Hành động thiết bị</div>
  <button class="btn btn-gray btn-full" onclick="doReset()">🔄 KHỞI ĐỘNG LẠI (Restart)</button>

  <div class="section-title">⚠️ Vùng nguy hiểm</div>
  <div class="danger-zone">
    <div class="danger-title">⚠️ Factory Reset — Bàn giao khách hàng mới</div>
    <div class="danger-desc">
      Xóa TOÀN BỘ: WiFi đã lưu, lịch trình, cấu hình.<br>
      Thiết bị khởi động như máy mới.<br>
      <b style="color:var(--danger)">Không thể hoàn tác!</b>
    </div>
    <button class="btn btn-off btn-full" style="margin-bottom:0" onclick="doFactoryReset()">
      💥 FACTORY RESET — LÀM MỚI HOÀN TOÀN
    </button>
  </div>
</div>

<div id="toast"></div>

<script>
// ── Khởi tạo: CHỈ 1 request duy nhất ──
// Không load schedule, không poll lặp lại
window.addEventListener('load', function() {
  loadDeviceInfo();
});

function toast(msg, dur) {
  var t = document.getElementById('toast');
  t.textContent = msg;
  t.classList.add('show');
  clearTimeout(window._tt);
  window._tt = setTimeout(function(){ t.classList.remove('show'); }, dur || 2000);
}

function openPane(id) {
  document.querySelectorAll('.pane').forEach(function(p){ p.classList.remove('pane-open'); });
  var el = document.getElementById(id);
  if (el) {
    el.classList.add('pane-open');
    if (id === 'pane-manage') loadDeviceInfo();
  }
}
function closePane() {
  document.querySelectorAll('.pane').forEach(function(p){ p.classList.remove('pane-open'); });
}

// Relay: keepalive false + timeout 3500ms
// Không gọi thêm bất kỳ request nào sau relay
function relay(url, msg) {
  fetch(url, {method:'GET', keepalive:false, signal: AbortSignal.timeout(3500)})
    .then(function(r){ if(r.ok) toast('✅ ' + msg); else toast('⚠️ Lỗi server'); })
    .catch(function(e){ toast(e.name==='TimeoutError' ? '⏱ Timeout — thử lại' : '⚠️ Không phản hồi'); });
}

// Chỉ gọi 1 lần khi cần (mở pane, hoặc window.onload)
function loadDeviceInfo() {
  fetch('/api/mode/status', {keepalive:false, signal: AbortSignal.timeout(3000)})
    .then(function(r){ return r.json(); })
    .then(function(d){
      var did = d.deviceId || '—';
      var el = document.getElementById('did');
      if (el) el.textContent = 'NODE: ' + did;
      var ii = document.getElementById('info-id');
      if (ii) ii.textContent = did;
      var up = document.getElementById('info-uptime');
      if (up) {
        var s = d.uptime || 0;
        var h = Math.floor(s/3600), m = Math.floor((s%3600)/60), ss = s%60;
        up.textContent = h+'h '+m+'m '+ss+'s';
      }
    })
    .catch(function(){});
}

function wizardSave() {
  var s = document.getElementById('wz-ssid').value.trim();
  var p = document.getElementById('wz-pass').value;
  if (!s) { toast('⚠️ Chưa nhập tên WiFi'); return; }
  if (!confirm('Lưu WiFi "' + s + '" và khởi động lại để kết nối Internet?')) return;
  toast('⏳ Đang lưu...');
  fetch('/api/wifi/set?ssid='+encodeURIComponent(s)+'&pass='+encodeURIComponent(p),
        {keepalive:false, signal: AbortSignal.timeout(5000)})
    .then(function(){ toast('✅ Đang khởi động lại...'); })
    .catch(function(){ toast('⏳ ESP32 đang restart...'); });
}

function doReset() {
  if (!confirm('Khởi động lại ESP32?')) return;
  toast('⏳ Đang restart...');
  fetch('/api/reset', {keepalive:false, signal: AbortSignal.timeout(3000)})
    .catch(function(){ toast('⏳ ESP32 đang khởi động lại...'); });
}

function doFactoryReset() {
  if (!confirm('⚠️ FACTORY RESET\n\nXóa TOÀN BỘ dữ liệu: WiFi, lịch trình, cấu hình.\nThiết bị về trạng thái như mới.\n\nXác nhận?')) return;
  if (!confirm('Xác nhận lần 2: LÀM MỚI HOÀN TOÀN thiết bị?')) return;
  toast('⏳ Đang Factory Reset...');
  fetch('/api/factory-reset', {keepalive:false, signal: AbortSignal.timeout(5000)})
    .then(function(r){ return r.json(); })
    .then(function(d){ toast('✅ ' + (d.msg || 'Factory Reset hoàn tất')); })
    .catch(function(){ toast('⏳ ESP32 đang khởi động lại như mới...'); });
}
</script>
</body>
</html>
)rawliteral";

// ═══════════════════════════════════════════════════════════════
//  HTML ONLINE — Đầy đủ tính năng (giữ nguyên từ v10)
// ═══════════════════════════════════════════════════════════════
static const char HTML_ONLINE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0,maximum-scale=1.0,user-scalable=no">
<title>ESP32 ONLINE</title>
<style>
:root{--bg:#0f172a;--card:#1e293b;--border:#334155;--primary:#3b82f6;--success:#22c55e;--danger:#ef4444;--warn:#f59e0b;--text:#f8fafc;--dim:#94a3b8}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent;margin:0;padding:0}
body{font-family:'Segoe UI',sans-serif;background:var(--bg);color:var(--text);min-height:100vh;padding:14px}
h1{font-size:20px;font-weight:800;margin:8px 0 2px;letter-spacing:.5px}
.device-id{font-size:11px;color:var(--dim);letter-spacing:1px;margin-bottom:14px}
.badge-online{display:inline-flex;align-items:center;gap:6px;background:rgba(34,197,94,.15);color:var(--success);border:1px solid var(--success);border-radius:20px;padding:4px 14px;font-size:12px;font-weight:700;margin-bottom:14px}
.dot{width:8px;height:8px;border-radius:50%;background:var(--success);animation:pulse 1.5s infinite}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.3}}
.card{background:var(--card);border-radius:16px;padding:16px;margin-bottom:12px;border:1px solid rgba(255,255,255,.05)}
.card-title{font-size:11px;text-transform:uppercase;letter-spacing:1px;color:var(--dim);margin-bottom:12px;font-weight:600}
.relay-row{display:flex;align-items:center;justify-content:space-between;padding:10px 0;border-bottom:1px solid var(--border)}
.relay-row:last-child{border-bottom:none;padding-bottom:0}
.relay-info{flex:1}
.relay-name{font-weight:700;font-size:14px}
.relay-sub{font-size:11px;color:var(--dim);margin-top:2px}
.btn-pair{display:flex;gap:8px;flex-shrink:0}
.btn{border:none;border-radius:10px;padding:10px 16px;font-weight:700;cursor:pointer;transition:transform .15s,opacity .15s;font-size:13px;letter-spacing:.5px;min-width:56px}
.btn:active{transform:scale(.91);opacity:.8}
.btn-on{background:var(--success);color:#fff}
.btn-off{background:var(--danger);color:#fff}
.btn-warn{background:var(--warn);color:#000}
.btn-primary{background:var(--primary);color:#fff}
.btn-gray{background:#334155;color:var(--text)}
.btn-full{width:100%;padding:13px;border-radius:12px;font-size:14px;margin-bottom:10px}
#toast{position:fixed;bottom:20px;left:50%;transform:translateX(-50%);background:#1e293b;border:1px solid var(--border);color:var(--text);padding:12px 22px;border-radius:12px;font-size:13px;z-index:999;opacity:0;transition:opacity .3s;pointer-events:none;max-width:320px;text-align:center}
#toast.show{opacity:1}
.fab-row{display:flex;gap:12px;justify-content:center;margin-bottom:16px}
.fab{width:52px;height:52px;border-radius:50%;border:none;font-size:20px;cursor:pointer;display:flex;align-items:center;justify-content:center;box-shadow:0 4px 12px rgba(0,0,0,.3);transition:transform .15s}
.fab:active{transform:scale(.9)}
.pane{position:fixed;top:0;left:0;width:100%;height:100%;background:var(--bg);z-index:100;display:none;padding:20px;overflow-y:auto}
.pane-open{display:block!important}
.pane-header{display:flex;justify-content:space-between;align-items:center;margin-bottom:22px}
.pane-header h2{font-size:18px;font-weight:700}
.close-btn{font-size:28px;cursor:pointer;color:var(--dim);line-height:1;padding:4px 8px}
.mode-btn-row{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-bottom:10px}
.btn-mode-offline{background:rgba(245,158,11,.15);color:var(--warn);border:2px solid var(--warn);border-radius:12px;padding:12px;font-weight:700;font-size:13px;cursor:pointer;transition:.2s}
.btn-mode-online{background:rgba(34,197,94,.15);color:var(--success);border:2px solid var(--success);border-radius:12px;padding:12px;font-weight:700;font-size:13px;cursor:pointer;transition:.2s}
.btn-mode-offline.active{background:var(--warn);color:#000}
.btn-mode-online.active{background:var(--success);color:#000}
.sched-item{background:rgba(255,255,255,.03);border-radius:12px;padding:12px 14px;margin-bottom:8px;display:flex;justify-content:space-between;align-items:center;border-left:3px solid var(--primary)}
.sched-del{background:var(--danger);border:none;color:#fff;border-radius:8px;padding:6px 12px;font-size:12px;font-weight:700;cursor:pointer}
.timer-row{background:rgba(255,255,255,.03);padding:14px;border-radius:12px;margin-bottom:10px;border:1px solid var(--border)}
.flex-check{display:flex;align-items:center;gap:8px;margin-bottom:8px}
input[type=time]{width:100%;padding:10px;border-radius:8px;border:1px solid var(--border);background:#0f172a;color:#fff;font-size:16px}
input[type=checkbox]{width:20px;height:20px;accent-color:var(--success);cursor:pointer}
select{width:100%;padding:11px;margin-bottom:12px;border-radius:10px;background:#1e293b;color:#fff;border:1px solid var(--border);font-size:14px}
input[type=text],input[type=password]{width:100%;padding:12px;margin-bottom:10px;border-radius:10px;background:#0f172a;color:#fff;border:1px solid var(--border);font-size:15px}
.wifi-item{padding:12px 14px;border-bottom:1px solid rgba(255,255,255,.05);cursor:pointer;display:flex;justify-content:space-between;align-items:center}
.wifi-item:active{background:rgba(255,255,255,.05)}
.wifi-item:last-child{border-bottom:none}
.section-title{font-size:12px;text-transform:uppercase;letter-spacing:1px;color:var(--dim);font-weight:600;margin:18px 0 10px;border-bottom:1px solid var(--border);padding-bottom:6px}
.danger-zone{border:1px solid var(--danger);border-radius:12px;padding:14px;margin-top:8px}
.danger-title{color:var(--danger);font-size:12px;font-weight:700;text-transform:uppercase;letter-spacing:1px;margin-bottom:8px}
.danger-desc{font-size:12px;color:var(--dim);margin-bottom:12px;line-height:1.6}
.info-row{display:flex;justify-content:space-between;padding:8px 0;border-bottom:1px solid rgba(255,255,255,.04);font-size:13px}
.info-row:last-child{border-bottom:none}
.info-val{color:var(--dim);font-size:12px}
</style>
</head>
<body>

<h1>ESP32 REMOTE</h1>
<div class="device-id" id="did">NODE: —</div>
<div class="badge-online"><span class="dot"></span>ONLINE MODE</div>

<div class="card">
  <div class="card-title">⚡ Chế độ hoạt động</div>
  <div class="mode-btn-row">
    <button class="btn-mode-offline" id="btn-offline" onclick="switchMode('offline')">📡 OFFLINE</button>
    <button class="btn-mode-online active" id="btn-online" onclick="switchMode('online')">🌐 ONLINE</button>
  </div>
  <div style="font-size:11px;color:var(--dim)" id="mode-desc">WiFi STA+AP · MQTT · Scheduler · WebServer</div>
</div>

<div class="card">
  <div class="card-title">⚡ Điều khiển thiết bị</div>
  <div class="relay-row">
    <div class="relay-info">
      <div class="relay-name">MÀN HÌNH TFT</div>
      <div class="relay-sub">Relay 1 · GPIO 27</div>
    </div>
    <div class="btn-pair">
      <button class="btn btn-on" onclick="relay('/relay/1/on','R1 BẬT')">BẬT</button>
      <button class="btn btn-off" onclick="relay('/relay/1/off','R1 TẮT')">TẮT</button>
    </div>
  </div>
  <div class="relay-row">
    <div class="relay-info">
      <div class="relay-name">RELAY 2</div>
      <div class="relay-sub">GPIO 22</div>
    </div>
    <div class="btn-pair">
      <button class="btn btn-on" onclick="relay('/relay/2/on','R2 BẬT')">BẬT</button>
      <button class="btn btn-off" onclick="relay('/relay/2/off','R2 TẮT')">TẮT</button>
    </div>
  </div>
  <div class="relay-row">
    <div class="relay-info">
      <div class="relay-name">RELAY 3</div>
      <div class="relay-sub">GPIO 21</div>
    </div>
    <div class="btn-pair">
      <button class="btn btn-on" onclick="relay('/relay/3/on','R3 BẬT')">BẬT</button>
      <button class="btn btn-off" onclick="relay('/relay/3/off','R3 TẮT')">TẮT</button>
    </div>
  </div>
</div>

<div class="card">
  <div class="card-title">🖥 Màn hình ESP32</div>
  <div class="btn-pair" style="gap:10px">
    <button class="btn btn-primary" style="flex:1;padding:12px" onclick="relay('/screen/1','UI 1')">UI 1</button>
    <button class="btn btn-gray" style="flex:1;padding:12px" onclick="relay('/screen/2','UI 2')">UI 2</button>
  </div>
</div>

<div class="fab-row">
  <button class="fab" style="background:var(--primary)" onclick="openPane('pane-timer')">⏱</button>
  <button class="fab" style="background:#334155" onclick="openPane('pane-wifi')">📶</button>
  <button class="fab" style="background:#1e3a5f" onclick="openPane('pane-manage')">⚙</button>
</div>

<!-- TIMER PANE -->
<div id="pane-timer" class="pane">
  <div class="pane-header">
    <h2>⏱ Hẹn giờ tự động</h2>
    <span class="close-btn" onclick="closePane()">✕</span>
  </div>
  <select id="r_idx">
    <option value="1">Relay 1 — Màn hình TFT</option>
    <option value="2">Relay 2 — GPIO 22</option>
    <option value="3">Relay 3 — GPIO 21</option>
  </select>
  <div class="timer-row">
    <div class="flex-check">
      <input type="checkbox" id="en_on" checked>
      <label for="en_on" style="font-size:13px">Tự động <b style="color:var(--success)">BẬT</b> lúc:</label>
    </div>
    <input type="time" id="t_on" value="06:00">
  </div>
  <div class="timer-row">
    <div class="flex-check">
      <input type="checkbox" id="en_off" checked>
      <label for="en_off" style="font-size:13px">Tự động <b style="color:var(--danger)">TẮT</b> lúc:</label>
    </div>
    <input type="time" id="t_off" value="18:00">
  </div>
  <button class="btn btn-primary btn-full" style="margin-top:14px" onclick="addSchedule()">✅ XÁC NHẬN</button>
  <div class="section-title">Lịch trình đang chạy</div>
  <div id="sched_list"><p style="color:var(--dim);font-size:13px">Đang tải...</p></div>
  <button class="btn btn-warn btn-full" style="margin-top:14px" onclick="clearSchedules()">🗑 XÓA TẤT CẢ</button>
</div>

<!-- WIFI PANE -->
<div id="pane-wifi" class="pane">
  <div class="pane-header">
    <h2>📶 Cài đặt WiFi</h2>
    <span class="close-btn" onclick="closePane()">✕</span>
  </div>
  <button class="btn btn-primary btn-full" id="btn-scan" onclick="scanWifi()">🔍 QUÉT MẠNG WiFi</button>
  <div id="wifi_results" style="background:rgba(0,0,0,.2);border-radius:12px;margin-bottom:12px;max-height:220px;overflow-y:auto"></div>
  <input type="text" id="ssid" placeholder="Tên WiFi (SSID)" autocomplete="off" autocorrect="off" spellcheck="false">
  <input type="password" id="pass" placeholder="Mật khẩu WiFi">
  <button class="btn btn-on btn-full" onclick="saveWifi()">💾 LƯU & KẾT NỐI</button>
  <div class="section-title">WiFi đã lưu</div>
  <div id="saved_wifi"></div>
</div>

<!-- MANAGE PANE -->
<div id="pane-manage" class="pane">
  <div class="pane-header">
    <h2>⚙ Quản lý thiết bị</h2>
    <span class="close-btn" onclick="closePane()">✕</span>
  </div>
  <div class="section-title">📋 Thông tin thiết bị</div>
  <div class="card" style="padding:12px">
    <div class="info-row"><span>Device ID</span><span class="info-val" id="info-id">—</span></div>
    <div class="info-row"><span>Chế độ</span><span class="info-val" id="info-mode">—</span></div>
    <div class="info-row"><span>Uptime</span><span class="info-val" id="info-uptime">—</span></div>
  </div>
  <div class="section-title">🔧 Hành động</div>
  <button class="btn btn-warn btn-full" onclick="doLearnAll()">📚 LEARN ALL — Reset relay & lịch trình</button>
  <button class="btn btn-gray btn-full" onclick="doReset()">🔄 KHỞI ĐỘNG LẠI (Restart)</button>
  <button class="btn btn-gray btn-full" onclick="doClearWifi()">🗑 XÓA WiFi ĐÃ LƯU</button>
  <div class="section-title">⚠️ Vùng nguy hiểm</div>
  <div class="danger-zone">
    <div class="danger-title">⚠️ Factory Reset</div>
    <div class="danger-desc">Xóa TOÀN BỘ: WiFi, lịch trình, cấu hình. Không thể hoàn tác!</div>
    <button class="btn btn-off btn-full" style="margin-bottom:0" onclick="doFactoryReset()">💥 FACTORY RESET</button>
  </div>
</div>

<div id="toast"></div>

<script>
var currentMode = 'online';
window.addEventListener('load', function() { loadDeviceInfo(); });

function toast(msg, dur) {
  var t = document.getElementById('toast');
  t.textContent = msg;
  t.classList.add('show');
  clearTimeout(window._tt);
  window._tt = setTimeout(function(){ t.classList.remove('show'); }, dur || 2200);
}

function openPane(id) {
  document.querySelectorAll('.pane').forEach(function(p){ p.classList.remove('pane-open'); });
  var el = document.getElementById(id);
  if (el) {
    el.classList.add('pane-open');
    if (id === 'pane-timer')  loadSchedules();
    if (id === 'pane-wifi')   loadSavedWifi();
    if (id === 'pane-manage') loadDeviceInfo();
  }
}
function closePane() {
  document.querySelectorAll('.pane').forEach(function(p){ p.classList.remove('pane-open'); });
}

function relay(url, msg) {
  fetch(url, {method:'GET', keepalive:false, signal: AbortSignal.timeout(3500)})
    .then(function(r){ if(r.ok) toast('✅ ' + msg); else toast('⚠️ Lỗi server'); })
    .catch(function(e){ toast(e.name==='TimeoutError' ? '⏱ Timeout — thử lại' : '⚠️ Không phản hồi'); });
}

function loadDeviceInfo() {
  fetch('/api/mode/status', {signal: AbortSignal.timeout(3000)})
    .then(function(r){ return r.json(); })
    .then(function(d){
      var did = d.deviceId || '—';
      var el = document.getElementById('did');
      if (el) el.textContent = 'NODE: ' + did;
      var ii = document.getElementById('info-id');
      if (ii) ii.textContent = did;
      var im = document.getElementById('info-mode');
      if (im) im.textContent = d.mode === 'online' ? '🌐 ONLINE' : '📡 OFFLINE';
      var up = document.getElementById('info-uptime');
      if (up) {
        var s = d.uptime || 0;
        up.textContent = Math.floor(s/3600)+'h '+Math.floor((s%3600)/60)+'m '+s%60+'s';
      }
      currentMode = d.mode;
      updateModeButtons(d.mode);
    })
    .catch(function(){});
}

function updateModeButtons(mode) {
  var btnOff = document.getElementById('btn-offline');
  var btnOn  = document.getElementById('btn-online');
  var desc   = document.getElementById('mode-desc');
  if (!btnOff || !btnOn) return;
  if (mode === 'offline') {
    btnOff.classList.add('active'); btnOn.classList.remove('active');
    if (desc) desc.textContent = 'AP-Only · 192.168.4.1 · Relay · Không MQTT';
  } else {
    btnOn.classList.add('active'); btnOff.classList.remove('active');
    if (desc) desc.textContent = 'WiFi STA+AP · MQTT · Scheduler · WebServer';
  }
}

function switchMode(target) {
  if (currentMode === target) return;
  toast('⏳ Đang chuyển sang ' + (target === 'offline' ? 'OFFLINE' : 'ONLINE') + '...');
  fetch('/api/mode/' + target, {signal: AbortSignal.timeout(5000)})
    .then(function(r){ return r.json(); })
    .then(function(d){
      if (d.error) { toast('❌ ' + d.error); return; }
      setTimeout(loadDeviceInfo, 1500);
      if (target === 'offline') {
        toast('✅ Chuyển OFFLINE — tải lại trang...');
        setTimeout(function(){ window.location.reload(); }, 2000);
      }
    })
    .catch(function(){ setTimeout(loadDeviceInfo, 2500); });
}

function addSchedule() {
  var r = document.getElementById('r_idx').value;
  var enOn  = document.getElementById('en_on').checked;
  var enOff = document.getElementById('en_off').checked;
  var onVal = enOn  ? document.getElementById('t_on').value  : 'null';
  var offVal= enOff ? document.getElementById('t_off').value : 'null';
  if (!enOn && !enOff) { toast('⚠️ Chọn ít nhất Bật hoặc Tắt!'); return; }
  fetch('/api/schedule/add?relay='+r+'&on='+onVal+'&off='+offVal, {signal: AbortSignal.timeout(3000)})
    .then(function(r){ return r.json(); })
    .then(function(d){
      if (d.status === 'ok') { toast('✅ Đã thêm lịch trình'); loadSchedules(); }
      else toast('❌ ' + (d.error || 'Lỗi'));
    })
    .catch(function(){ toast('❌ Lỗi kết nối'); });
}

function loadSchedules() {
  fetch('/api/schedule/list', {signal: AbortSignal.timeout(3000)})
    .then(function(r){ return r.json(); })
    .then(function(data){
      var el = document.getElementById('sched_list');
      if (!el) return;
      if (!data.length) {
        el.innerHTML = '<p style="color:var(--dim);font-size:13px;text-align:center;padding:10px 0">Chưa có lịch trình</p>';
        return;
      }
      var h = '';
      data.forEach(function(s){
        h += '<div class="sched-item">' +
          '<div><b style="color:var(--primary)">Relay '+s.relay+'</b><br>' +
          '<small style="color:var(--success)">Bật: '+s.on+'</small> &nbsp;' +
          '<small style="color:var(--danger)">Tắt: '+s.off+'</small></div>' +
          '<button class="sched-del" onclick="delSched('+s.id+')">XÓA</button>' +
        '</div>';
      });
      el.innerHTML = h;
    })
    .catch(function(){});
}
function delSched(id) {
  fetch('/api/schedule/delete?id='+id, {signal: AbortSignal.timeout(3000)})
    .then(function(){ toast('🗑 Đã xóa'); loadSchedules(); })
    .catch(function(){ toast('❌ Lỗi'); });
}
function clearSchedules() {
  if (!confirm('Xóa tất cả lịch trình?')) return;
  fetch('/api/schedule/clear', {signal: AbortSignal.timeout(3000)})
    .then(function(r){ return r.json(); })
    .then(function(){ toast('🗑 Đã xóa tất cả'); loadSchedules(); })
    .catch(function(){ toast('❌ Lỗi'); });
}

function scanWifi() {
  var btn = document.getElementById('btn-scan');
  var res = document.getElementById('wifi_results');
  if (btn) { btn.disabled = true; btn.textContent = '⏳ Đang quét...'; }
  if (res) res.innerHTML = '<p style="padding:12px;color:var(--dim);font-size:13px">Đang quét mạng WiFi...</p>';
  fetch('/api/wifi/scan', {signal: AbortSignal.timeout(15000)})
    .then(function(r){ return r.json(); })
    .then(function(data){
      if (btn) { btn.disabled = false; btn.textContent = '🔍 QUÉT MẠNG WiFi'; }
      if (!res) return;
      if (!data.length) { res.innerHTML = '<p style="padding:12px;color:var(--dim)">Không tìm thấy mạng nào</p>'; return; }
      var h = '';
      data.sort(function(a,b){ return b.rssi - a.rssi; }).forEach(function(w){
        var bars = w.rssi > -60 ? '▂▄▆█' : w.rssi > -75 ? '▂▄▆' : w.rssi > -85 ? '▂▄' : '▂';
        h += '<div class="wifi-item" onclick="document.getElementById(\'ssid\').value=\''+w.ssid.replace(/'/g,"\\'")+'\'">'+
          '<span style="font-weight:600">'+w.ssid+'</span>'+
          '<span style="color:var(--dim);font-size:11px">'+bars+' '+w.rssi+'dBm '+(w.auth?'🔒':'🔓')+'</span>'+
        '</div>';
      });
      res.innerHTML = h;
    })
    .catch(function(){
      if (btn) { btn.disabled = false; btn.textContent = '🔍 QUÉT MẠNG WiFi'; }
      toast('❌ Quét WiFi thất bại');
    });
}

function loadSavedWifi() {
  fetch('/api/wifi/list', {signal: AbortSignal.timeout(3000)})
    .then(function(r){ return r.json(); })
    .then(function(data){
      var el = document.getElementById('saved_wifi');
      if (!el) return;
      if (!data.length) { el.innerHTML = '<p style="color:var(--dim);font-size:13px">Chưa có WiFi nào được lưu</p>'; return; }
      var h = '';
      data.forEach(function(w){
        h += '<div class="sched-item">'+
          '<span style="font-weight:600">'+w.ssid+'</span>'+
          '<button class="sched-del" onclick="deleteWifi('+w.idx+')">XÓA</button>'+
        '</div>';
      });
      el.innerHTML = h;
    })
    .catch(function(){});
}
function saveWifi() {
  var s = document.getElementById('ssid').value.trim();
  var p = document.getElementById('pass').value;
  if (!s) { toast('⚠️ Chưa nhập tên WiFi'); return; }
  toast('⏳ Đang lưu...');
  fetch('/api/wifi/set?ssid='+encodeURIComponent(s)+'&pass='+encodeURIComponent(p), {signal: AbortSignal.timeout(5000)})
    .then(function(){ toast('✅ Đã lưu — đang khởi động lại...'); })
    .catch(function(){ toast('⏳ ESP32 đang restart...'); });
}
function deleteWifi(idx) {
  fetch('/api/wifi/delete?idx='+idx, {signal: AbortSignal.timeout(3000)})
    .then(function(r){ return r.json(); })
    .then(function(d){ toast('✅ ' + (d.msg || 'Đã xóa')); loadSavedWifi(); })
    .catch(function(){ toast('❌ Lỗi'); });
}

function doLearnAll() {
  if (!confirm('Xóa tất cả lịch trình và reset relay về mặc định?')) return;
  fetch('/api/learn-all', {signal: AbortSignal.timeout(3000)})
    .then(function(r){ return r.json(); })
    .then(function(d){ toast('✅ ' + (d.msg || 'Learn All hoàn tất')); })
    .catch(function(){ toast('❌ Lỗi'); });
}
function doReset() {
  if (!confirm('Khởi động lại ESP32?')) return;
  toast('⏳ Đang restart...');
  fetch('/api/reset', {signal: AbortSignal.timeout(3000)}).catch(function(){});
}
function doClearWifi() {
  if (!confirm('Xóa toàn bộ WiFi đã lưu?\nThiết bị sẽ ở OFFLINE mode sau khi restart.')) return;
  fetch('/api/wifi/clear', {signal: AbortSignal.timeout(3000)})
    .then(function(r){ return r.json(); })
    .then(function(d){ toast('✅ ' + (d.msg || 'Đã xóa WiFi')); })
    .catch(function(){ toast('❌ Lỗi'); });
}
function doFactoryReset() {
  if (!confirm('⚠️ FACTORY RESET\n\nXóa TOÀN BỘ dữ liệu: WiFi, lịch trình, cấu hình.\nThiết bị về trạng thái như mới. Xác nhận?')) return;
  if (!confirm('Xác nhận lần 2: LÀM MỚI HOÀN TOÀN thiết bị?')) return;
  toast('⏳ Đang Factory Reset...');
  fetch('/api/factory-reset', {signal: AbortSignal.timeout(5000)})
    .then(function(r){ return r.json(); })
    .then(function(d){ toast('✅ ' + (d.msg || 'Factory Reset hoàn tất')); })
    .catch(function(){ toast('⏳ ESP32 đang khởi động lại như mới...'); });
}
</script>
</body>
</html>
)rawliteral";

// ─────────────────────────────────────────────────────────────
// Route registrations
// ─────────────────────────────────────────────────────────────
static void registerCommonRoutes() {
    server.on("/relay/1/on",  screendiplaytft_on);
    server.on("/relay/1/off", screendiplaytft_off);
    server.on("/relay/2/on",  relay2_on);
    server.on("/relay/2/off", relay2_off);
    server.on("/relay/3/on",  relay3_on);
    server.on("/relay/3/off", relay3_off);

    server.on("/screen/1", handleScreen1);
    server.on("/screen/2", handleScreen2);

    server.on("/api/relay/status", handleRelayStatus);
    server.on("/api/mode/status",  handleModeStatus);

    // Schedule — handler tự block nếu offline
    server.on("/api/schedule/add",    handleScheduleAdd);
    server.on("/api/schedule/list",   handleScheduleList);
    server.on("/api/schedule/delete", handleScheduleDelete);
    server.on("/api/schedule/clear",  handleScheduleClear);

    // Device management
    server.on("/api/reset",         handleResetDevice);
    server.on("/api/learn-all",     handleLearnAll);     // block khi offline
    server.on("/api/wifi/clear",    handleClearWiFi);    // block khi offline
    server.on("/api/factory-reset", handleFactoryReset); // khả dụng mọi lúc
}

void setupServer() {
    server.on("/", []() {
        server.sendHeader("Cache-Control", "no-store");
        if (isOfflineMode()) {
            server.send_P(200, "text/html", HTML_OFFLINE);
        } else {
            server.send_P(200, "text/html", HTML_ONLINE);
        }
    });

    registerCommonRoutes();

    server.on("/api/wifi/scan",   handleScanWiFi);
    server.on("/api/wifi/list",   handleListSavedWiFi);
    server.on("/api/wifi/set",    handleSetWiFi);
    server.on("/api/wifi/delete", handleDeleteSavedWiFi);

    server.on("/api/mode/offline", handleModeSwitch);
    server.on("/api/mode/online",  handleModeSwitch);

    server.onNotFound([]() {
        server.send(404, "application/json", "{\"error\":\"Not found\"}");
    });

    server.begin();
    Serial.println(F("[Server] WebServer v11 khởi động tại 192.168.4.1"));
}

void activateOfflineRoutes() {
    Serial.println(F("[Server] OFFLINE routes active — slim mode, chỉ relay"));
}

void activateOnlineRoutes() {
    Serial.println(F("[Server] ONLINE routes active — full features"));
}
