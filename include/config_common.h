#pragma once

#include <Arduino.h>

// ==========================================
// PIN DEFINITIONS - LAFVIN ESP32-C6 1.47" LCD
// ==========================================

// --- Onboard LCD (ST7789 172x320 - internal hardware SPI) ---
#define PIN_LCD_MOSI   6
#define PIN_LCD_SCLK   7
#define PIN_LCD_CS     14
#define PIN_LCD_DC     15
#define PIN_LCD_RST    21
#define PIN_LCD_BL     22
#define LCD_WIDTH          172
#define LCD_HEIGHT         320
#define DISPLAY_ROTATION   1   // 1 = Landscape (320x172), 3 = Landscape 180°
#define SCREEN_W           320
#define SCREEN_H           172

// --- Onboard MicroSD / TF Card (Shares SPI bus with ST7789 LCD) ---
#define PIN_SD_CS      4   // MicroSD Chip Select (Active LOW, Top Header Pin 2 / onboard TF)
#define PIN_SD_MOSI    PIN_LCD_MOSI  // GPIO 6 (Shared Hardware SPI MOSI)
#define PIN_SD_SCLK    PIN_LCD_SCLK  // GPIO 7 (Shared Hardware SPI SCLK)
#define PIN_SD_MISO    5             // GPIO 5 (Shared Hardware SPI MISO)

// --- Onboard Peripherals ---
#define PIN_RGB_LED    8   // WS2812 NeoPixel
#define PIN_BTN_BOOT   9   // Onboard BOOT button (active LOW, Bottom Header Pin 1)

// --- LoRa SX1278 (Ra-02 433MHz) on Dedicated Header SPI ---
// NOTA CRÍTICA ESP32-C6: O ESP32-C6 tem apenas UM barramento SPI de utilizador (FSPI).
// Além disso, GPIO 4 e 5 são partilhados com o leitor MicroSD TF onboard.
// Definir ENABLE_LORA como 1 apenas quando o módulo LoRa estiver fisicamente ligado e a ser usado.
#ifndef ENABLE_LORA
#define ENABLE_LORA 0
#endif

// Top Header: Pin 4 (SCK=2), Pin 3 (MOSI=3), Pin 2 (MISO=4), Pin 1 (NSS=5)
// Bottom Header: Pin 6 (DIO0=12)
#define PIN_LORA_SCK   2   // Hardware SPI SCLK (Top Header Pin 4)
#define PIN_LORA_MOSI  3   // Hardware SPI MOSI (Top Header Pin 3)
#define PIN_LORA_MISO  4   // Hardware SPI MISO (Top Header Pin 2)
#define PIN_LORA_NSS   5   // CS for LoRa (Top Header Pin 1)
#define PIN_LORA_DIO0  -1  // Polling mode (RADIOLIB_NC): não usar GPIO 12 para proteger USB D- (CDC)
#define PIN_LORA_RST   -1  // Not connected / internal pullup

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
// GPS Quectel LC29H on Bottom Header (Hardware UART on RX/TX)
#define PIN_GPS_RX     16  // ESP32 RX <- Quectel LC29H TX (Bottom Header Pin 8 - RX)
#define PIN_GPS_TX     17  // ESP32 TX -> Quectel LC29H RX (Bottom Header Pin 9 - TX)
#define GPS_BAUDRATE   115200

// APM Power Module (28V / 90A) on Top Header (ADC1 channels)
#define PIN_BATTERY_VOLT_ADC   0   // ADC1_CH0 (Top Header Pin 6)
#define PIN_BATTERY_CURR_ADC   1   // ADC1_CH1 (Top Header Pin 5)

// Actuators & Outputs on Bottom Header
#define PIN_MOTOR_ESC      18  // Main propulsion ESC (Bottom Header Pin 2)
#define PIN_SERVO_RUD      20  // Rudder servo (Bottom Header Pin 4 - swapped with IO19)
#define PIN_WINCH          19  // Anchor winch servo (Bottom Header Pin 3 - swapped with IO20)
#define PIN_SERVO_RACK     23  // Anchor rack/clutch servo (Bottom Header Pin 5)
#define PIN_ALARM_BUZZER   13  // Siren / buzzer output (Bottom Header Pin 7, ativado na água/bateria e protegido em USB)

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
// 4-pin Joystick connector on Top Header
#define PIN_JOYSTICK_X  0   // ADC1_CH0 (Top Header Pin 6)
#define PIN_JOYSTICK_Y  1   // ADC1_CH1 (Top Header Pin 5)
#define PIN_JOYSTICK_SW 9   // Joystick push click (Bottom Header Pin 1 - BOOT)

// Physical buttons on Base (Bottom Header)
#define PIN_BTN_ANCHOR_UP   18  // External button to hoist anchor (Bottom Header Pin 2)
#define PIN_BTN_ANCHOR_DOWN 20  // External button to drop anchor (Bottom Header Pin 4 - swapped with IO19)
#define PIN_BTN_MODE        19  // External button for mode toggle (Bottom Header Pin 3 - swapped with IO20)
