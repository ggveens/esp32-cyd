// touch.cpp — Cảm ứng TFT thuần túy (không dùng XPT2046_Touchscreen)
// Dùng GPIO capacitive hoặc resistive đơn giản qua TFT_eSPI built-in
// XPT2046_Touchscreen đã bị XÓA — tránh block SPI khi offline
#include "touch.h"
#include "../core/globals.h"
#include "../config.h"

// ✅ FIX #4: tft.getTouch() là blocking SPI call (~5–20ms)
//    Nếu gọi mỗi vòng loop (không throttle) → WebServer bị đói CPU
//    → Relay call timeout → "Không phản hồi — thử lại"
//    Giải pháp: poll touch tối đa 30ms/lần (33 lần/giây là đủ mượt)

static bool          _wasTouching = false;
static unsigned long _lastTouchPoll = 0;

void handleTouch() {
    // Throttle: chỉ đọc SPI touch mỗi 30ms
    unsigned long now = millis();
    if (now - _lastTouchPoll < 30) return;
    _lastTouchPoll = now;

    uint16_t tx, ty;
    bool touched = tft.getTouch(&tx, &ty, 300); // threshold 300

    if (!touched) {
        _wasTouching = false;
        return;
    }

    if (_wasTouching) return; // chỉ xử lý khi nhấn mới (debounce)
    _wasTouching = true;

    // Chuyển màn hình khi chạm vào vùng nút back (màn hình 2)
    if (currentScreen == SCREEN_2) {
        // Vùng nút quay lại: góc dưới giữa
        if (tx > 80 && tx < 240 && ty > 180 && ty < 240) {
            currentScreen = SCREEN_MAIN;
        }
    }
}
