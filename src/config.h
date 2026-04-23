#pragma once

// ===== WIFI =====
#define MAX_WIFI 5

// ===== RELAY =====
#define RELAY1 27
#define RELAY2 22
#define RELAY3 21

// ===== RGB =====
#define LED_R 4
#define LED_G 16
#define LED_B 17

#define CH_R 0
#define CH_G 1
#define CH_B 2

#define PWM_FREQ 5000
#define PWM_RES 8

// ===== BUTTON =====
#define BTN_RESET 0

// ===== TOUCH =====
#define TOUCH_CS 33

// ===== MQTT =====
#define MQTT_SERVER "broker.hivemq.com"

// ===== TIME =====
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET (7 * 3600)

// ===== SCHEDULE =====
#define MAX_SCHEDULE 10