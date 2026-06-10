// src/clock/time_manager.cpp
#include "time_manager.h"
#include "../core/globals.h"
#include "../config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <sys/time.h>

// ---- Biến toàn cục ----
volatile bool     timeSyncSuccess   = false;
volatile bool     dateUpdatePending = false; 
String            lunarDate         = "";
String            solarDate         = ""; 
String            lastApiError      = "Chưa khởi động";
SemaphoreHandle_t timeDataMutex     = nullptr;

static TaskHandle_t _timeSyncTaskHandle = nullptr;

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

    String lunar        = doc["lunar"]["full_date"].as<String>();
    String solarTime    = doc["solar"]["time"].as<String>();      
    String solarDateApi = doc["solar"]["full_date"].as<String>(); 

    int h, m, s, day, month, year;
    if (sscanf(solarTime.c_str(), "%d:%d:%d", &h, &m, &s) != 3 ||
        sscanf(solarDateApi.c_str(), "%d/%d/%d", &day, &month, &year) != 3) {
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
        if (lunarDate != lunar || solarDate != solarDateApi) {
            dateUpdatePending = true; 
        }
        lunarDate       = lunar;
        solarDate       = solarDateApi; 
        timeSyncSuccess = true;
        lastApiError    = "";
        xSemaphoreGive(timeDataMutex);
    }

    Serial.println("[TimeTask] Đồng bộ thành công: " + solarTime + " — AL: " + lunar);
    return true;
}

// ---- FreeRTOS Task nâng cấp kiểm tra mốc 00:00:00 ----
static void timeSyncTask(void* parameter) {
    vTaskDelay(pdMS_TO_TICKS(2000));  // Chờ ổn định ban đầu
    
    unsigned long lastPeriodicSync = 0;
    bool forceMidnightSync = true; // Cho phép chạy ngay lần đầu tiên khởi động

    while (true) {
        unsigned long currentMillis = millis();
        bool needSync = false;

        if (WiFi.status() == WL_CONNECTED) {
            // Điều kiện 1: Đồng bộ định kỳ theo cấu hình (ví dụ: mỗi 1 tiếng)
            if (lastPeriodicSync == 0 || (currentMillis - lastPeriodicSync >= TIME_SYNC_INTERVAL)) {
                needSync = true;
            }

            // Điều kiện 2: Kiểm tra xem đồng hồ nội bộ có chạm mốc sang ngày mới (00 giờ 00 phút) hay không
            struct tm timeinfo;
            if (getLocalTime(&timeinfo, 0)) {
                if (timeinfo.tm_hour == 0 && timeinfo.tm_min == 0) {
                    if (forceMidnightSync) {
                        Serial.println(F("[TimeTask] Kích hoạt đồng bộ khẩn cấp lúc nửa đêm (00:00)!"));
                        needSync = true;
                        forceMidnightSync = false; // Tắt cờ tạm thời để không gọi liên tục trong phút đó
                    }
                } else {
                    // Khi thời gian thoát khỏi mốc 00:00, Reset lại cờ để chuẩn bị cho ngày kế tiếp
                    forceMidnightSync = true;
                }
            }

            // Thực thi xử lý gọi API
            if (needSync) {
                if (!fetchTimeFromAPI()) {
                    // Nếu lỗi mạng, hẹn thử lại sau 15 giây
                    vTaskDelay(pdMS_TO_TICKS(TIME_RETRY_INTERVAL));
                    continue;
                } else {
                    // Nếu thành công, cập nhật mốc thời gian định kỳ gần nhất
                    lastPeriodicSync = millis();
                }
            }
            
            // Quét kiểm tra trạng thái mốc thời gian mịn mỗi 1 giây
            vTaskDelay(pdMS_TO_TICKS(1000));
        } else {
            // Khi không kết nối mạng
            if (xSemaphoreTake(timeDataMutex, portMAX_DELAY)) {
                lastApiError = "Mất mạng";
                xSemaphoreGive(timeDataMutex);
            }
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }
}

void timeManagerSetup() {
    if (timeDataMutex == nullptr) {
        timeDataMutex = xSemaphoreCreateMutex();
    }
}

void timeManagerResume() {
    if (_timeSyncTaskHandle != nullptr) return;
    if (timeDataMutex == nullptr) timeManagerSetup();

    if (xSemaphoreTake(timeDataMutex, portMAX_DELAY)) {
        lastApiError = "Đang kết nối...";
        xSemaphoreGive(timeDataMutex);
    }

    xTaskCreatePinnedToCore(timeSyncTask, "TimeSync", 6144, nullptr, 1, &_timeSyncTaskHandle, 0);
}

void timeManagerSuspend() {
    if (_timeSyncTaskHandle != nullptr) {
        vTaskDelete(_timeSyncTaskHandle);
        _timeSyncTaskHandle = nullptr;
    }
    if (timeDataMutex != nullptr && xSemaphoreTake(timeDataMutex, pdMS_TO_TICKS(100))) {
        lastApiError    = "";
        timeSyncSuccess = false;
        xSemaphoreGive(timeDataMutex);
    }
}