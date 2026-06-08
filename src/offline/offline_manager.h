// offline_manager.h — Chế độ OFFLINE tuyệt đối (AP-Only) v9
// ─────────────────────────────────────────────────────────────
// OFFLINE: 100% local, không gọi bất kỳ API Internet nào
// WebServer phục vụ HTML offline riêng — không có fetch() ra ngoài
// ─────────────────────────────────────────────────────────────
#pragma once
#include <Arduino.h>

void offlineStart();   // Kích hoạt OFFLINE mode
void offlineStop();    // Tắt OFFLINE (chuẩn bị chuyển Online)
void offlineLoop();    // Gọi liên tục trong loop()

bool isOfflineMode();
