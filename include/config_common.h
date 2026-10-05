#pragma once

#include <Arduino.h>

// ==========================================
// PIN DEFINITIONS - LAFVIN ESP32-C6 1.47" LCD
// ==========================================

// --- Onboard LCD (ST7789 172x320) ---
#define PIN_LCD_MOSI   6
#define PIN_LCD_SCLK   7
#define PIN_LCD_CS     14
#define PIN_LCD_DC     15
#define PIN_LCD_RST    21
#define PIN_LCD_BL     22
#define LCD_WIDTH      172
#define LCD_HEIGHT     320

// --- Onboard Peripherals ---
#define PIN_RGB_LED    8   // WS2812 NeoPixel
#define PIN_BTN_BOOT   9   // Onboard BOOT button (active LOW)

// --- LoRa SX1278 (Ra-02 433MHz) on SPI ---
// Shares Hardware SPI SCLK (IO7) & MOSI (IO6)
#define PIN_LORA_SCK   7
#define PIN_LORA_MOSI  6
#define PIN_LORA_MISO  5   // Shared with SD MISO
#define PIN_LORA_NSS   4   // CS for LoRa (replaces SD_CS)
#define PIN_LORA_RST   2   // Dedicated Reset
#define PIN_LORA_DIO0  20  // Packet IRQ

// LoRa RF Settings (433MHz Band)
#define LORA_FREQUENCY      433.0   // MHz
#define LORA_BANDWIDTH      250.0   // kHz (fast throughput for multi-rover polling)
#define LORA_SPREAD_FACTOR  7       // SF7 (low latency, high data rate)
#define LORA_CODING_RATE    5       // 4/5
#define LORA_SYNC_WORD      0x12    // Private network sync word
#define LORA_OUTPUT_POWER   17      // dBm (up to 20 dBm supported)
#define LORA_PREAMBLE_LEN   8

// ==========================================
// ROVER PIN DEFINITIONS
// ==========================================
#define PIN_GPS_RX     19  // ESP32 RX <- Quectel LC29H TX
#define PIN_GPS_TX     18  // ESP32 TX -> Quectel LC29H RX
#define GPS_BAUDRATE   115200

#define PIN_MOTOR_ESC      0   // Main propulsion ESC (50Hz PWM)
#define PIN_SERVO_RUD      1   // Rudder servo (50Hz PWM)
#define PIN_WINCH          23  // Anchor winch servo / motor (50Hz PWM)
#define PIN_SERVO_RACK     10  // Anchor rack/clutch servo - levanta cremalheira para queda livre (50Hz PWM)
#define PIN_ALARM_BUZZER   3   // Strobe siren / buzzer output

// Rack servo positions (degrees)
#define RACK_POS_ENGAGED   0   // Cremalheira engatada / travada (para recolher ou travar âncora)
#define RACK_POS_RELEASED  90  // Cremalheira levantada (livre para queda da âncora por gravidade)

// ESC / Servo pulse limits (microseconds)
#define PWM_PULSE_MIN      1000
#define PWM_PULSE_MID      1500
#define PWM_PULSE_MAX      2000

// ==========================================
// BASE PIN DEFINITIONS
// ==========================================
// 4-pin Joystick connector (VCC, GND, VRx, VRy)
#define PIN_JOYSTICK_X 0   // ADC1_CH0 - Throttle / Forward-Back
#define PIN_JOYSTICK_Y 1   // ADC1_CH1 - Steering / Rudder Left-Right

// Optional physical buttons on Base (active LOW)
#define PIN_BTN_ANCHOR_UP   18  // External button to hoist anchor
#define PIN_BTN_ANCHOR_DOWN 19  // External button to drop anchor
#define PIN_BTN_MODE        23  // External button for mode toggle
