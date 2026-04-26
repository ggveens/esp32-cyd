#include "ui2.h"
#include "../core/globals.h"
#include <TFT_eSPI.h>

#define BG2 0x001F

// ===== BUTTON AREA =====
#define BTN_X 80
#define BTN_Y 180
#define BTN_W 160
#define BTN_H 40

void drawUI2() {

  tft.fillScreen(BG2);

  tft.setTextColor(TFT_WHITE, BG2);
  tft.setTextSize(2);

  tft.setCursor(40, 30);
  tft.println("SCREEN 2");

  // ===== DRAW BUTTON =====
  tft.fillRoundRect(BTN_X, BTN_Y, BTN_W, BTN_H, 8, TFT_RED);
  tft.setTextColor(TFT_WHITE, TFT_RED);
  tft.setTextSize(2);

  tft.setCursor(BTN_X + 20, BTN_Y + 10);
  tft.print("GO MAIN");
}

void updateUI2() {

  static int x = 0;
  static int dir = 1;

  // animation
  tft.fillRect(0, 100, 320, 40, BG2);
  tft.fillCircle(x, 120, 10, TFT_YELLOW);

  x += dir * 5;
  if (x > 300 || x < 10) dir *= -1;
}