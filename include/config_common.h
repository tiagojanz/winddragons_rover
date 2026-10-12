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
#define DISPLAY_ROTATION   3   // 1 = Landscape (320x172), 3 = Landscape 180°
#define SCREEN_W           320
#define SCREEN_H           172

// --- Onboard MicroSD / TF Card (Shares SPI bus with ST7789 LCD) ---
#define PIN_SD_CS      4   // MicroSD Chip Select (Active LOW, Top Header Pin 2 / onboard TF)
#define PIN_SD_MOSI    PIN_LCD_MOSI  // GPIO 6 (Shared Hardware SPI MOSI)
#define PIN_SD_SCLK    PIN_LCD_SCLK  // GPIO 7 (Shared Hardware SPI SCLK)
#define PIN_SD_MISO    5             // GPIO 5 (Shared Hardware SPI MISO)

// --- Onboard Peripherals ---
#define PIN_RGB_LED    8   // WS2812 NeoPixel
#define PIN_BTN_BOOT   18  // Navigation BOOT button (SW1 Left - Bottom Header Pin 2 / IO18)

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

// --- I2C Expansion Bus (ESP32-C6 IO0 & IO1) ---
#define PIN_I2C_SDA        0   // ESP32 GPIO 0 (SDA)
#define PIN_I2C_SCL        1   // ESP32 GPIO 1 (SCL)

// ==========================================
// ROVER PIN DEFINITIONS
// ==========================================
// GPS Quectel LC29H on Bottom Header (Hardware UART on RX/TX)
#define PIN_GPS_RX         16  // ESP32 RX <- Quectel LC29H TX (Bottom Header Pin 8 - RX)
#define PIN_GPS_TX         17  // ESP32 TX -> Quectel LC29H RX (Bottom Header Pin 9 - TX)
#define GPS_BAUDRATE       115200

// ADS1115 16-Bit I2C ADC (Address 0x48) - High Precision Telemetry
#define ADS1115_I2C_ADDR   0x48
#define ADS_CH_BATT_VOLT   0   // AIN0: APM Power Module Voltage (10.1:1 divider)
#define ADS_CH_BATT_CURR   1   // AIN1: APM Power Module Current (27.32 A/V)
#define ADS_CH_WATER_SENS  2   // AIN2: Water detector probe
#define ADS_CH_AUX         3   // AIN3: Auxiliary analog input

// PCA9685 16-Channel 12-Bit PWM Driver (Address 0x40) - 50Hz Servo/ESC Controller
#define PCA9685_I2C_ADDR   0x40
#define PCA_CH_MOTOR       0   // Main propulsion ESC
#define PCA_CH_RUDDER      1   // Rudder servo
#define PCA_CH_WINCH       2   // Anchor winch servo
#define PCA_CH_RACK        3   // Anchor rack/clutch servo

// Anchor Hoisted Limit Detector (Reed / End-Stop switch on Bottom Header J5)
#define PIN_ANCHOR_DETECTOR 13 // Active LOW when anchor is fully hoisted / at top (Bottom Header Pin 7)
#define PIN_ALARM_BUZZER   -1  // Repurposed on Rover to PIN_ANCHOR_DETECTOR

// Actuators & Direct GPIO Fallbacks
#define PIN_MOTOR_ESC      18  // Main propulsion ESC (direct fallback)
#define PIN_SERVO_RUD      20  // Rudder servo (direct fallback)
#define PIN_WINCH          19  // Anchor winch servo (direct fallback)
#define PIN_SERVO_RACK     23  // Anchor rack/clutch servo (direct fallback)

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
#define PIN_JOYSTICK_SW -1  // Desativado no joystick: GPIO 9 e o botao BOOT com interrupcao dedicada para mudar de pagina

// Physical buttons on Base (Bottom Header)
#define PIN_BTN_ANCHOR_UP   20  // Button to hoist anchor / UP (SW3 Top - Bottom Header Pin 4 / IO20)
#define PIN_BTN_ANCHOR_DOWN 9   // Button to drop anchor / DOWN (SW4 Bottom - Bottom Header Pin 1 / IO9)
#define PIN_BTN_MODE        19  // Button for mode toggle / MODE (SW2 Right - Bottom Header Pin 3 / IO19)
