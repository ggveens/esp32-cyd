// src/clock/time_manager.h
// Time Manager — chỉ tồn tại khi ONLINE
// ONLINE  → timeManagerResume()  : tạo FreeRTOS task trên Core 0
// OFFLINE → timeManagerSuspend() : xóa task, không còn HTTP nào
#pragma once

#include <Arduino.h>

// ---- Trạng thái đồng bộ ----
extern volatile bool timeSyncSuccess;     // Đã đồng bộ thành công ít nhất một lần
extern String lunarDate;                  // Ngày âm lịch từ API (vd: "21/04/2026")
extern String lastApiError;               // Chuỗi lỗi cuối cùng để hiển thị lên UI

// ---- Mutex bảo vệ dữ liệu dùng chung ----
extern SemaphoreHandle_t timeDataMutex;

// ---- Lifecycle ----
void timeManagerSetup();     // Gọi 1 lần trong setup() — chỉ tạo mutex, KHÔNG tạo task
void timeManagerResume();    // ONLINE: tạo FreeRTOS task (nếu chưa có)
void timeManagerSuspend();   // OFFLINE: xóa task, dừng mọi HTTP, giải phóng stack

// ---- Hàm tiện ích (thread-safe) ----
String getLastApiError();    // Trả về lỗi gần nhất
bool   isTimeSynced();       // true nếu đã đồng bộ ít nhất 1 lần
