#include "web_server.h"
#include "../core/globals.h"
#include "../relay/relay.h"
#include "../wifi/wifi_manager.h"
#include "../schedule/schedule.h"
#include "../config.h"

#include <WiFi.h>

// ===== AUTH =====
// const char* adminUser = "admin";
// const char* adminPass = "0345030913";

// ===== WIFI =====
void handleScanWiFi() {

  int n = WiFi.scanNetworks();
  String json = "[";

  for (int i = 0; i < n; i++) {
    if (i) json += ",";
    json += "{";
    json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI(i));
    json += "}";
  }

  json += "]";
  server.send(200, "application/json", json);
}

void handleListSavedWiFi() {

  String json = "[";

  for (int i = 0; i < MAX_WIFI; i++) {
    if (ssids[i] == "") continue;

    if (json != "[") json += ",";
    json += "{";
    json += "\"ssid\":\"" + ssids[i] + "\"";
    json += "}";
  }

  json += "]";
  server.send(200, "application/json", json);
}

void handleSetWiFi() {

  if (!server.hasArg("ssid") || !server.hasArg("pass")) {
    server.send(400, "application/json", "{\"error\":\"missing\"}");
    return;
  }

  saveWiFi(server.arg("ssid"), server.arg("pass"));

  server.send(200, "application/json", "{\"status\":\"ok\"}");
  delay(1000);
  ESP.restart();
}

void handleResetDevice() {
  server.send(200, "text/plain", "Restarting...");
  delay(1000);
  ESP.restart();
}

// ===== SetupServer AP SERVER =====
void setupServer(){

  server.on("/", []() {
    // if (!server.authenticate(adminUser, adminPass)) {
    //   return server.requestAuthentication();
    // }

    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>ESP32 Dashboard</title>
    <style>
        :root { --bg: #0f172a; --card: #1e293b; --primary: #3b82f6; --success: #22c55e; --danger: #ef4444; --text: #f8fafc; }
        * { box-sizing: border-box; -webkit-tap-highlight-color: transparent; }
        body { font-family: 'Segoe UI', Tahoma, sans-serif; background: var(--bg); color: var(--text); margin: 0; padding: 15px; display: flex; flex-direction: column; align-items: center; }
        
        .container { width: 100%; max-width: 450px; }
        .device-id { font-size: 11px; opacity: 0.5; margin: -10px 0 20px 0; letter-spacing: 1px; }

        /* Card & Relay Control */
        .card { background: var(--card); border-radius: 20px; padding: 20px; margin-bottom: 15px; border: 1px solid rgba(255,255,255,0.05); }
        .relay-row { display: grid; grid-template-columns: 1fr 140px; align-items: center; gap: 10px; }
        .relay-name { font-weight: 600; font-size: 15px; }
        .btn-group { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; } /* Tách biệt nút bằng grid gap */
        
        button { border: none; border-radius: 12px; padding: 12px; font-weight: 700; cursor: pointer; transition: 0.2s; font-size: 13px; text-transform: uppercase; }
        button:active { transform: scale(0.92); }
        .btn-on { background: var(--success); color: white; box-shadow: 0 4px 10px rgba(34, 197, 94, 0.2); }
        .btn-off { background: var(--danger); color: white; box-shadow: 0 4px 10px rgba(239, 68, 68, 0.2); }
        .btn-scan { background: var(--primary); color: white; width: 100%; margin: 10px 0; }
        
        /* Floating Tabs */
        .fab { position: fixed; bottom: 25px; width: 55px; height: 55px; border-radius: 50%; background: var(--primary); color: white; display: flex; align-items: center; justify-content: center; font-size: 22px; box-shadow: 0 8px 20px rgba(0,0,0,0.4); z-index: 100; border: 2px solid rgba(255,255,255,0.1); }
        .fab-left { left: 20px; }
        .fab-right { right: 20px; }
        
        /* Pane Settings */
        .pane { position: fixed; top: 0; left: 0; width: 100%; height: 100%; background: var(--bg); z-index: 101; display: none; padding: 25px; overflow-y: auto; }
        .pane-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 25px; }
        .close-btn { font-size: 30px; line-height: 1; padding: 10px; }

        /* Timer UI */
        .timer-row { background: rgba(255,255,255,0.03); padding: 15px; border-radius: 15px; margin-bottom: 12px; border: 1px solid rgba(255,255,255,0.05); }
        .flex-box { display: flex; align-items: center; gap: 10px; margin: 10px 0; }
        input[type="time"] { flex: 1; padding: 10px; border-radius: 8px; border: 1px solid #334155; background: #0f172a; color: white; font-size: 16px; }
        input[type="checkbox"] { width: 22px; height: 22px; accent-color: var(--success); }

        .list-item { background: var(--card); padding: 15px; border-radius: 12px; margin-top: 10px; display: flex; justify-content: space-between; border-left: 4px solid var(--primary); }
        .wifi-item { padding: 15px; border-bottom: 1px solid rgba(255,255,255,0.05); }
    </style>
</head>
<body>

    <h2 style="margin-top:10px">ESP32 REMOTE</h2>
    <div class="device-id">NODE: )rawliteral" + deviceId + R"rawliteral(</div>

    <div class="container">
        <div class="card">
            <div class="relay-row">
                <span class="relay-name">RELAY 1 <small style="display:block; font-weight:400; opacity:0.6">Màn hình LCD</small></span>
                <div class="btn-group">
                    <button class="btn-on" onclick="send('/relay/1/on')">ON</button>
                    <button class="btn-off" onclick="send('/relay/1/off')">OFF</button>
                </div>
            </div>
        </div>

        <div class="card">
            <div class="relay-row">
                <span class="relay-name">RELAY 2 <small style="display:block; font-weight:400; opacity:0.6">Cổng GPIO 22</small></span>
                <div class="btn-group">
                    <button class="btn-on" onclick="send('/relay/2/on')">ON</button>
                    <button class="btn-off" onclick="send('/relay/2/off')">OFF</button>
                </div>
            </div>
        </div>

        <div class="card">
            <div class="relay-row">
                <span class="relay-name">RELAY 3 <small style="display:block; font-weight:400; opacity:0.6">Cổng GPIO 21</small></span>
                <div class="btn-group">
                    <button class="btn-on" onclick="send('/relay/3/on')">ON</button>
                    <button class="btn-off" onclick="send('/relay/3/off')">OFF</button>
                </div>
            </div>
        </div>
    </div>

    <div class="fab fab-left" onclick="openPane('pane-timer')">⏱</div>
    <div class="fab fab-right" onclick="openPane('pane-wifi')">⚙</div>

    <div id="pane-timer" class="pane">
        <div class="pane-header">
            <h3>Hẹn giờ thông minh</h3>
            <span class="close-btn" onclick="closePanes()">&times;</span>
        </div>
        
        <label style="font-size:13px; opacity:0.7">Chọn thiết bị:</label>
        <select id="r_idx" style="width:100%; padding:12px; margin-bottom:15px; border-radius:10px; background:#1e293b; color:white; border:1px solid #334155;">
            <option value="1">Relay 1</option>
            <option value="2">Relay 2</option>
            <option value="3">Relay 3</option>
        </select>

        <div class="timer-row">
            <div class="flex-box">
                <input type="checkbox" id="en_on" checked>
                <label for="en_on">Tự động BẬT lúc:</label>
            </div>
            <input type="time" id="t_on" value="06:00">
        </div>

        <div class="timer-row">
            <div class="flex-box">
                <input type="checkbox" id="en_off" checked>
                <label for="en_off">Tự động TẮT lúc:</label>
            </div>
            <input type="time" id="t_off" value="18:00">
        </div>

        <button class="btn-scan" onclick="addSchedule()" style="background:var(--success); padding:15px; margin-top:10px">XÁC NHẬN LỊCH TRÌNH</button>
        
        <h4 style="margin-top:30px; border-bottom: 1px solid #334155; padding-bottom:10px">Lịch trình đang chạy</h4>
        <div id="sched_list"></div>
    </div>

    <div id="pane-wifi" class="pane">
        <div class="pane-header">
            <h3>Kết nối WiFi</h3>
            <span class="close-btn" onclick="closePanes()">&times;</span>
        </div>
        <button class="btn-scan" onclick="scanWifi()">TÌM KIẾM MẠNG</button>
        <div id="wifi_results" style="max-height:250px; overflow-y:auto; margin:15px 0; background:rgba(0,0,0,0.2); border-radius:10px;"></div>
        <input id="ssid" placeholder="SSID" style="width:100%; padding:12px; margin-bottom:10px; border-radius:10px; background:#1e293b; color:white; border:1px solid #334155;">
        <input id="pass" type="password" placeholder="Mật khẩu" style="width:100%; padding:12px; margin-bottom:20px; border-radius:10px; background:#1e293b; color:white; border:1px solid #334155;">
        <button class="btn-scan" onclick="saveWifi()" style="background:var(--success); padding:15px">LƯU THÔNG TIN</button>
    </div>

    <script>
        function openPane(id) {
            closePanes();
            document.getElementById(id).style.display = 'block';
            if(id === 'pane-timer') loadSchedules();
        }
        function closePanes() {
            document.querySelectorAll('.pane').forEach(p => p.style.display = 'none');
        }
        function send(url) { fetch(url); }

        function addSchedule() {
            const r = document.getElementById('r_idx').value;
            const enOn = document.getElementById('en_on').checked;
            const enOff = document.getElementById('en_off').checked;
            
            // Nếu không tích chọn, gửi giá trị "--:--" để Server bỏ qua
            const onVal = enOn ? document.getElementById('t_on').value : "null";
            const offVal = enOff ? document.getElementById('t_off').value : "null";

            if(!enOn && !enOff) return alert('Vui lòng chọn ít nhất Bật hoặc Tắt!');

            fetch(`/api/schedule/add?relay=${r}&on=${onVal}&off=${offVal}`).then(() => loadSchedules());
        }

        function loadSchedules() {
            fetch('/api/schedule/list').then(r=>r.json()).then(data => {
                let h = "";
                data.forEach((s, i) => {
                    let displayOn = s.on === "null" || s.on === "-1:-1" ? "OFF" : s.on;
                    let displayOff = s.off === "null" || s.off === "-1:-1" ? "OFF" : s.off;
                    h += `<div class="list-item">
                        <div>
                            <b style="color:var(--primary)">R${s.relay}</b> | 
                            <span style="color:var(--success)">ON: ${displayOn}</span> ➔ 
                            <span style="color:var(--danger)">OFF: ${displayOff}</span>
                        </div>
                        <button class="btn-off" style="padding:5px 12px; font-size:11px" onclick="delSched(${i})">XÓA</button>
                    </div>`;
                });
                document.getElementById('sched_list').innerHTML = h || "<p style='opacity:0.5; text-align:center'>Chưa có lịch trình nào</p>";
            });
        }
        function delSched(i) { fetch('/api/schedule/delete?id='+i).then(()=>loadSchedules()); }

        function scanWifi() {
            const res = document.getElementById('wifi_results');
            res.innerHTML = "<p style='padding:15px'>Đang quét...</p>";
            fetch('/api/wifi/scan').then(r=>r.json()).then(data => {
                res.innerHTML = "";
                data.forEach(w => {
                    let div = document.createElement('div');
                    div.className = 'wifi-item';
                    div.innerHTML = `<b>${w.ssid}</b> <small style="float:right">${w.rssi} dBm</small>`;
                    div.onclick = () => document.getElementById('ssid').value = w.ssid;
                    res.appendChild(div);
                });
            });
        }
        function saveWifi() {
            const s = document.getElementById('ssid').value;
            const p = document.getElementById('pass').value;
            if(!s) return alert('Chưa chọn WiFi');
            fetch(`/api/wifi/set?ssid=${s}&pass=${p}`).then(() => alert('Đang lưu và khởi động lại...'));
        }
    </script>
</body>
</html>
)rawliteral";

    server.send(200, "text/html", html);
  });

  // RELAY
  server.on("/relay/1/on", relay1_on);
  server.on("/relay/1/off", relay1_off);
  server.on("/relay/2/on", relay2_on);
  server.on("/relay/2/off", relay2_off);
  server.on("/relay/3/on", relay3_on);
  server.on("/relay/3/off", relay3_off);

  // WIFI
  server.on("/api/wifi/scan", handleScanWiFi);
  server.on("/api/wifi/list", handleListSavedWiFi);
  server.on("/api/wifi/set", handleSetWiFi);
  server.on("/api/reset", handleResetDevice);

  // SCHEDULE (giữ nguyên logic)
  server.on("/api/schedule/add", []() {

    if (!server.hasArg("relay") || !server.hasArg("on") || !server.hasArg("off")) {
      server.send(400, "text/plain", "missing");
      return;
    }

    int relay = server.arg("relay").toInt();

    String onT = server.arg("on");
    String offT = server.arg("off");

    int onH = onT.substring(0,2).toInt();
    int onM = onT.substring(3,5).toInt();

    int offH = offT.substring(0,2).toInt();
    int offM = offT.substring(3,5).toInt();

    for (int i = 0; i < MAX_SCHEDULE; i++) {

      if (!schedules[i].active) {

        schedules[i].active = true;
        schedules[i].relay = relay;

        schedules[i].onH = onH;
        schedules[i].onM = onM;

        schedules[i].offH = offH;
        schedules[i].offM = offM;

        schedules[i].firedOn = false;
        schedules[i].firedOff = false;

        server.send(200, "text/plain", "OK");
        return;
      }
    }

    server.send(500, "text/plain", "FULL");
  });

  server.on("/api/schedule/list", []() {

    String json = "[";

    for (int i = 0; i < MAX_SCHEDULE; i++) {
      if (!schedules[i].active) continue;

      if (json != "[") json += ",";

      json += "{";
      json += "\"relay\":" + String(schedules[i].relay) + ",";
      json += "\"on\":\"" + String(schedules[i].onH) + ":" + String(schedules[i].onM) + "\",";
      json += "\"off\":\"" + String(schedules[i].offH) + ":" + String(schedules[i].offM) + "\"";
      json += "}";
    }

    json += "]";
    server.send(200, "application/json", json);
  });

  server.on("/api/schedule/delete", []() {

    if (!server.hasArg("id")) {
      server.send(400, "text/plain", "missing");
      return;
    }

    int id = server.arg("id").toInt();

    if (id >= 0 && id < MAX_SCHEDULE) {
      schedules[id].active = false;
    }

    server.send(200, "text/plain", "OK");
  });

  server.begin();
}