#include "schedule.h"
#include "../core/globals.h"
#include "../relay/relay.h"
#include "../config.h"   // ✅ THÊM DÒNG NÀY
#include <time.h>

void resetScheduleDaily() {
  static int lastDay = -1;

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  if (timeinfo.tm_mday != lastDay) {
    lastDay = timeinfo.tm_mday;

    for (int i = 0; i < MAX_SCHEDULE; i++) {
      schedules[i].firedOn = false;
      schedules[i].firedOff = false;
    }
  }
}

void handleSchedule() {

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  int h = timeinfo.tm_hour;
  int m = timeinfo.tm_min;

  for (int i = 0; i < MAX_SCHEDULE; i++) {

    if (!schedules[i].active) continue;

    if (!schedules[i].firedOn &&
        h == schedules[i].onH &&
        m == schedules[i].onM) {

      schedules[i].firedOn = true;

      if (schedules[i].relay == 1) relay1_on();
      if (schedules[i].relay == 2) relay2_on();
      if (schedules[i].relay == 3) relay3_on();
    }

    if (!schedules[i].firedOff &&
        h == schedules[i].offH &&
        m == schedules[i].offM) {

      schedules[i].firedOff = true;

      if (schedules[i].relay == 1) relay1_off();
      if (schedules[i].relay == 2) relay2_off();
      if (schedules[i].relay == 3) relay3_off();
    }
  }
}