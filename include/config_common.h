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
#define LCD_WIDTH          172
#define LCD_HEIGHT         320
#define DISPLAY_ROTATION   1   // 1 = Landscape (320x172), 3 = Landscape 180°
#define SCREEN_W           320
#define SCREEN_H           172

// --- Onboard Peripherals ---
#define PIN_RGB_LED    8   // WS2812 NeoPixel
#define PIN_BTN_BOOT   9   // Onboard BOOT button (active LOW)

// --- LoRa SX1278 (Ra-02 433MHz) on SPI (Left Header - Direct Linear Route) ---
#define PIN_LORA_SCK   7   // Hardware SPI SCLK
#define PIN_LORA_MOSI  6   // Hardware SPI MOSI
#define PIN_LORA_MISO  5   // Hardware SPI MISO
#define PIN_LORA_NSS   4   // CS for LoRa
#define PIN_LORA_DIO0  10  // Packet IRQ (Left Header, aligned with Ra-02)
#define PIN_LORA_RST   -1  // Not connected / internal pullup (prevents conflict with USB_N IO12)

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
// GPS Quectel LC29H on Right Header (Hardware UART on IO16/IO17)
#define PIN_GPS_RX     17  // ESP32 RX <- Quectel LC29H TX (Right Header)
#define PIN_GPS_TX     16  // ESP32 TX -> Quectel LC29H RX (Right Header)
#define GPS_BAUDRATE   115200

// APM Power Module (28V / 90A) on Left Header (ADC1 channels)
#define PIN_BATTERY_VOLT_ADC   0   // ADC1_CH0 (Left Header)
#define PIN_BATTERY_CURR_ADC   1   // ADC1_CH1 (Left Header - aligned with APM connector)

// Actuators & Outputs on Right Header (Direct Collinear Route)
#define PIN_MOTOR_ESC      18  // Main propulsion ESC (Right Header)
#define PIN_SERVO_RUD      19  // Rudder servo (Right Header)
#define PIN_WINCH          20  // Anchor winch servo (Right Header)
#define PIN_SERVO_RACK     23  // Anchor rack/clutch servo (Right Header)
#define PIN_ALARM_BUZZER   11  // Strobe siren / buzzer output (Left Header)

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
// 4-pin Joystick connector on Left Header
#define PIN_JOYSTICK_X  0   // ADC1_CH0 - Throttle / Forward-Back
#define PIN_JOYSTICK_Y  1   // ADC1_CH1 - Steering / Rudder Left-Right
#define PIN_JOYSTICK_SW 2   // Joystick push click (IO2, prevents conflict with USB_P IO13)

// Physical buttons on Base (Right Header - Linear Direct Route)
#define PIN_BTN_ANCHOR_UP   18  // External button to hoist anchor
#define PIN_BTN_ANCHOR_DOWN 19  // External button to drop anchor
#define PIN_BTN_MODE        20  // External button for mode toggle (Right Header)
