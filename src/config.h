#pragma once
#include <Arduino.h>

// ESP32-C3 SuperMini pins. Strapping pins 2, 8, 9 avoided.
constexpr uint8_t PIN_TOUCH_A = 0;   // default role: dim / off
constexpr uint8_t PIN_TOUCH_B = 1;   // default role: brighten / on
constexpr uint8_t PIN_LED_PWM = 10;  // IRLZ44N gate via 100R

// 5 kHz keeps switching loss low with the slow 3.3 V gate drive.
constexpr uint32_t PWM_FREQ_HZ = 5000;
constexpr uint8_t PWM_BITS = 12;
constexpr uint8_t PWM_CHANNEL = 0;

// Brightness is perceptual per mille; gamma is applied at the output.
constexpr uint16_t LEVEL_MAX = 1000;
constexpr uint16_t DEFAULT_LEVEL = 500;
constexpr float GAMMA = 2.2f;

constexpr uint32_t DEBOUNCE_MS = 20;
constexpr uint32_t HOLD_THRESHOLD_MS = 400;

constexpr uint32_t RAMP_FULL_MS = 3000;  // hold: 0 to 100 %
constexpr uint32_t FADE_FULL_MS = 400;   // tap on/off fade

// Delay NVS writes until changes settle (flash wear).
constexpr uint32_t SAVE_DELAY_MS = 2000;
