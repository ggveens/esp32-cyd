// src/clock/time_manager.cpp
// Time Manager — chỉ sống khi ONLINE
//   timeManagerSetup()   → gọi trong setup(), tạo mutex, KHÔNG tạo task
//   timeManagerResume()  → ONLINE: tạo FreeRTOS task trên Core 0
//   timeManagerSuspend() → OFFLINE: xóa task (vTaskDelete), ngừng mọi HTTP
// ─────────────────────────────────────────────────────────────────────────

#include "time_manager.h"
#include "../core/globals.h"
#include "../config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <sys/time.h>

// ---- Biến toàn cục ----
volatile bool     timeSyncSuccess = false;
String            lunarDate       = "";
String            lastApiError    = "Chưa khởi động";
SemaphoreHandle_t timeDataMutex   = nullptr;

// ---- Task handle — cần để xóa task khi Offline ----
static TaskHandle_t _timeSyncTaskHandle = nullptr;

// ─────────────────────────────────────────────────────────────
// Hàm thread-safe
// ─────────────────────────────────────────────────────────────
bool isTimeSynced() {
    bool ret = false;
    if (xSemaphoreTake(timeDataMutex, pdMS_TO_TICKS(10))) {
        ret = timeSyncSuccess;
        xSemaphoreGive(timeDataMutex);
    }
    return ret;
}

String getLastApiError() {
    String err;
    if (xSemaphoreTake(timeDataMutex, pdMS_TO_TICKS(10))) {
        err = lastApiError;
        xSemaphoreGive(timeDataMutex);
    } else {
        err = "Lỗi đọc dữ liệu";
    }
    return err;
}

// ─────────────────────────────────────────────────────────────
// Gọi API và set đồng hồ hệ thống
// ─────────────────────────────────────────────────────────────
static bool fetchTimeFromAPI() {
    if (WiFi.status() != WL_CONNECTED) return false;

    HTTPClient http;
    http.begin(TIME_API_URL);
    http.setTimeout(5000);
    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK) {
        if (xSemaphoreTake(timeDataMutex, portMAX_DELAY)) {
            lastApiError = "HTTP " + String(httpCode);
            xSemaphoreGive(timeDataMutex);
        }
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        if (xSemaphoreTake(timeDataMutex, portMAX_DELAY)) {
            lastApiError = "Lỗi JSON: " + String(error.c_str());
            xSemaphoreGive(timeDataMutex);
        }
        return false;
    }

    bool success = doc["success"];
    if (!success) {
        if (xSemaphoreTake(timeDataMutex, portMAX_DELAY)) {
            lastApiError = "API trả về success=false";
            xSemaphoreGive(timeDataMutex);
        }
        return false;
    }

    String lunar     = doc["lunar"]["full_date"].as<String>();
    String solarTime = doc["solar"]["time"].as<String>();      // "HH:MM:SS"
    String solarDate = doc["solar"]["full_date"].as<String>(); // "DD/MM/YYYY"

    int h, m, s, day, month, year;
    if (sscanf(solarTime.c_str(), "%d:%d:%d", &h, &m, &s) != 3 ||
        sscanf(solarDate.c_str(), "%d/%d/%d", &day, &month, &year) != 3) {
        if (xSemaphoreTake(timeDataMutex, portMAX_DELAY)) {
            lastApiError = "Sai định dạng ngày/giờ từ API";
            xSemaphoreGive(timeDataMutex);
        }
        return false;
    }

    struct tm tm_input = {};
    tm_input.tm_year = year - 1900;
    tm_input.tm_mon  = month - 1;
    tm_input.tm_mday = day;
    tm_input.tm_hour = h;
    tm_input.tm_min  = m;
    tm_input.tm_sec  = s;
    time_t t = mktime(&tm_input);

    struct timeval now_tv = { .tv_sec = t, .tv_usec = 0 };
    settimeofday(&now_tv, nullptr);

    if (xSemaphoreTake(timeDataMutex, portMAX_DELAY)) {
        lunarDate       = lunar;
        timeSyncSuccess = true;
        lastApiError    = "";
        xSemaphoreGive(timeDataMutex);
    }

    Serial.println("[TimeTask] Đồng bộ thành công: " + solarTime + " — AL: " + lunar);
    return true;
}

// ─────────────────────────────────────────────────────────────
// FreeRTOS Task — chỉ tồn tại khi ONLINE
// ─────────────────────────────────────────────────────────────
static void timeSyncTask(void* parameter) {
    vTaskDelay(pdMS_TO_TICKS(2000));  // Chờ WiFi ổn định

    while (true) {
        if (WiFi.status() == WL_CONNECTED) {
            if (!fetchTimeFromAPI()) {
                vTaskDelay(pdMS_TO_TICKS(TIME_RETRY_INTERVAL));
            } else {
                vTaskDelay(pdMS_TO_TICKS(TIME_SYNC_INTERVAL));
            }
        } else {
            if (xSemaphoreTake(timeDataMutex, portMAX_DELAY)) {
                lastApiError = "Mất mạng";
                xSemaphoreGive(timeDataMutex);
            }
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }
}

// ─────────────────────────────────────────────────────────────
// API công khai
// ─────────────────────────────────────────────────────────────

// Gọi 1 lần trong setup() — chỉ tạo mutex, KHÔNG tạo task
void timeManagerSetup() {
    if (timeDataMutex == nullptr) {
        timeDataMutex = xSemaphoreCreateMutex();
        if (timeDataMutex == nullptr) {
            Serial.println(F("[TimeManager] Lỗi tạo mutex!"));
            return;
        }
    }
    Serial.println(F("[TimeManager] Mutex sẵn sàng. Task chưa chạy (chờ ONLINE)."));
}

// ONLINE: tạo task nếu chưa có
void timeManagerResume() {
    if (_timeSyncTaskHandle != nullptr) {
        Serial.println(F("[TimeManager] Task đã tồn tại, bỏ qua."));
        return;
    }

    if (timeDataMutex == nullptr) timeManagerSetup();

    // Reset trạng thái lỗi
    if (xSemaphoreTake(timeDataMutex, portMAX_DELAY)) {
        lastApiError = "Đang kết nối...";
        xSemaphoreGive(timeDataMutex);
    }

    xTaskCreatePinnedToCore(
        timeSyncTask,          // Hàm task
        "TimeSync",            // Tên
        6144,                  // Stack
        nullptr,               // Tham số
        1,                     // Ưu tiên
        &_timeSyncTaskHandle,  // ← Lưu handle để xóa được
        0                      // Core 0
    );

    Serial.println(F("[TimeManager] Task TimeSync đã tạo trên Core 0."));
}

// OFFLINE: xóa task, không còn HTTP nào
// ✅ FIX #2: Luôn reset lastApiError dù task có hay không
//    Nếu không làm vậy, sau power-cycle khi boot thẳng vào OFFLINE,
//    lastApiError vẫn là "Chưa khởi động" (giá trị khởi tạo) → TFT hiển thị sai
void timeManagerSuspend() {
    if (_timeSyncTaskHandle != nullptr) {
        vTaskDelete(_timeSyncTaskHandle);   // Giải phóng stack & xóa task khỏi scheduler
        _timeSyncTaskHandle = nullptr;
        Serial.println(F("[TimeManager] Task TimeSync đã xóa (OFFLINE)."));
    } else {
        Serial.println(F("[TimeManager] Không có task — chỉ reset trạng thái."));
    }

    // ✅ LUÔN reset — kể cả khi không có task (boot lần đầu vào OFFLINE)
    if (timeDataMutex != nullptr &&
        xSemaphoreTake(timeDataMutex, pdMS_TO_TICKS(100))) {
        lastApiError    = "";          // Xóa thông báo lỗi — OFFLINE không cần hiện gì
        timeSyncSuccess = false;       // Không có đồng hồ thật → drawTime() ẩn đồng hồ
        xSemaphoreGive(timeDataMutex);
    }
}
