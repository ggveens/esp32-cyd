#include "touch.h"
#include "../core/globals.h"
#include "../config.h"
#include <XPT2046_Touchscreen.h>

extern XPT2046_Touchscreen ts;

#define BTN_X 80
#define BTN_Y 180
#define BTN_W 160
#define BTN_H 40

static bool touching = false;

void handleTouch() {

  if (!ts.touched()) {
    touching = false;
    return;
  }

  TS_Point p = ts.getPoint();

  int x = map(p.x, 200, 3800, 0, 320);
  int y = map(p.y, 200, 3800, 0, 240);

  // ===== TAP =====
  if (!touching) {
    touching = true;

    // 👉 CLICK BUTTON UI2
    if (currentScreen == SCREEN_2) {
      if (x > BTN_X && x < BTN_X + BTN_W &&
          y > BTN_Y && y < BTN_Y + BTN_H) {

        currentScreen = SCREEN_MAIN;
        return;
      }
    }
  }
}