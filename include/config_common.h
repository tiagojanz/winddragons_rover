#pragma once

#include <Arduino.h>

// ==========================================
// PIN DEFINITIONS - LAFVIN ESP32-C6 1.47" LCD
// ==========================================

// --- Onboard LCD (ST7789 172x320 - internal hardware SPI) ---
#define PIN_LCD_MOSI 6
#define PIN_LCD_SCLK 7
#define PIN_LCD_CS 14
#define PIN_LCD_DC 15
#define PIN_LCD_RST 21
#define PIN_LCD_BL 22
#define LCD_WIDTH 172
#define LCD_HEIGHT 320
#define DISPLAY_ROTATION 1 // 1 = Landscape (320x172), 3 = Landscape 180°
#define SCREEN_W 320
#define SCREEN_H 172

// --- Onboard MicroSD / TF Card (Shares SPI bus with ST7789 LCD) ---
#define PIN_SD_CS                                                              \
  4 // MicroSD Chip Select (Active LOW, Top Header Pin 2 / onboard TF)
#define PIN_SD_MOSI PIN_LCD_MOSI // GPIO 6 (Shared Hardware SPI MOSI)
#define PIN_SD_SCLK PIN_LCD_SCLK // GPIO 7 (Shared Hardware SPI SCLK)
#define PIN_SD_MISO 5            // GPIO 5 (Shared Hardware SPI MISO)

// --- Onboard Peripherals ---
#define PIN_RGB_LED 8 // WS2812 NeoPixel
#define PIN_BTN_BOOT                                                           \
  9 // Hardware BOOT button on ESP32-C6 & SW4 (BOOT) on Carrier PCB
#define PIN_BTN_SW1 18 // Navigation SW1 Left (UP) on Carrier PCB

// --- LoRa SX1278 (Ra-02 433MHz) on Dedicated Header SPI ---
// NOTA CRÍTICA ESP32-C6: O ESP32-C6 tem apenas UM barramento SPI de utilizador
// (FSPI). Além disso, GPIO 4 e 5 são partilhados com o leitor MicroSD TF
// onboard. Definir ENABLE_LORA como 1 apenas quando o módulo LoRa estiver
// fisicamente ligado e a ser usado.
#ifndef ENABLE_LORA
#define ENABLE_LORA 1
#endif

// Top Header: Pin 4 (SCK=2), Pin 3 (MOSI=3)
// Bottom Header: Pin 3 (MISO=19), Pin 4 (NSS=20), Pin 5 (DIO0=23)
#define PIN_LORA_SCK 2  // SPI SCLK (Top Header Pin 4)
#define PIN_LORA_MOSI 3 // SPI MOSI (Top Header Pin 3)
#define PIN_LORA_MISO                                                          \
  19 // SPI MISO (Bottom Header Pin 3 / GPIO 19 - sem conflito com MicroSD)
#define PIN_LORA_NSS                                                           \
  20 // CS/NSS for LoRa (Bottom Header Pin 4 / GPIO 20 - sem conflito com
     // MicroSD)
#define PIN_LORA_DIO0                                                          \
  23 // LoRa DIO0 IRQ (Bottom Header Pin 5 / GPIO 23 - sem conflito com USB)
#define PIN_LORA_RST -1 // Not connected / internal pullup

// LoRa RF Settings (433MHz Band)
#define LORA_FREQUENCY 433.0 // MHz
#define LORA_BANDWIDTH 250.0 // kHz (fast throughput for multi-rover polling)
#define LORA_SPREAD_FACTOR 7 // SF7 (low latency, high data rate)
#define LORA_CODING_RATE 5   // 4/5
#define LORA_SYNC_WORD 0x12  // Private network sync word
#define LORA_OUTPUT_POWER 17 // dBm (up to 20 dBm supported)
#define LORA_PREAMBLE_LEN 8

// --- I2C Expansion Bus (ESP32-C6 IO0 & IO1) ---
#define PIN_I2C_SDA 0 // ESP32 GPIO 0 (SDA)
#define PIN_I2C_SCL 1 // ESP32 GPIO 1 (SCL)

// --- Ilha de Expansão J_EXP na PCB C6 (Bússola GY-BNO055 9-DOF) ---
// J_EXP Pino 1 (IO1)  -> GPIO 1  (I2C SDA)
// J_EXP Pino 2 (3V3)  -> +3.3V
// J_EXP Pino 3 (IO11) -> GPIO 11 (I2C SCL)
// J_EXP Pino 5/6 (GND)-> GND
#define PIN_C6_EXP_SDA 1
#define PIN_C6_EXP_SCL 11
#define BNO055_I2C_ADDR_A 0x28
#define BNO055_I2C_ADDR_B 0x29

// ==========================================
// INTER-ESP UART DEFINITIONS (Lafvin Master <-> ESP32-C6 Slave)
// ==========================================
// Testado e 100% validado em bancada a 115200 bps (0% perdas, RTT ~36ms em
// cadeia) Amarelo: C6 TX (GPIO 13) -> Lafvin RX (GPIO 1 / Pad 5) Verde: Lafvin
// TX (GPIO 2 / Pad 4) -> C6 RX (GPIO 22) Preto:   GND Comum
#define PIN_INTER_ESP_LAFVIN_TX                                                \
  2 // Lafvin Header J1 Pad 4 (Serigrafia 2 / GPIO 2)
#define PIN_INTER_ESP_LAFVIN_RX                                                \
  1                            // Lafvin Header J1 Pad 5 (Serigrafia 1 / GPIO 1)
#define PIN_INTER_ESP_C6_TX 13 // C6 DevKitC Header J3 Pin 13 (GPIO 13)
#define PIN_INTER_ESP_C6_RX 22 // C6 DevKitC Header J3 Pin 6 (GPIO 22)
#define INTER_ESP_BAUDRATE 115200

// ==========================================
// C6 DEVKITC ROVER PIN DEFINITIONS (COPROCESSADOR)
// ==========================================
// LoRa SX1278 (Ra-02 433MHz no barramento de hardware FSPI do DevKitC)
#define PIN_C6_LORA_NSS 6  // GPIO 6
#define PIN_C6_LORA_SCK 5  // GPIO 5
#define PIN_C6_LORA_MOSI 7 // GPIO 7
#define PIN_C6_LORA_MISO 4 // GPIO 4
#define PIN_C6_LORA_DIO0 0 // GPIO 0
#define PIN_C6_LORA_RST -1 // Sem reset externo

// Power Module (LiPo 6S) via ADC interno do ESP32-C6
#define PIN_C6_ADC_VOLT 2 // GPIO 2
#define PIN_C6_ADC_CURR 3 // GPIO 3
#define BATT_VOLT_DIVIDER 10.803f
#define BATT_AMPS_PER_VOLT 27.32f
#define BATT_CURR_OFFSET_MV 6.5f

// Sensor de Água
#define PIN_C6_WATER_SENS 15 // GPIO 15 (Entrada Analógica)

// GPS Quectel LC29H (UART Hardware)
#define PIN_LAFVIN_GPS_RX 16 // Lafvin Header J2 Pad 8 (Serigrafia RX / GPIO 16)
#define PIN_LAFVIN_GPS_TX 17 // Lafvin Header J2 Pad 9 (Serigrafia TX / GPIO 17)
#define PIN_C6_GPS_RX 16     // DevKitC RX (GPIO 16)
#define PIN_C6_GPS_TX 17     // DevKitC TX (GPIO 17)
#define GPS_BAUDRATE 115200

// Atuadores / Servos (Sinais PWM Diretos)
// NOTA PCB: O conector WIN da PCB liga ao Pino 5 do J_DK_R (GPIO 23 no ESP32-C6
// DevKitC)
#define PIN_C6_WINCH 23 // GPIO 23 - Conector WIN na PCB (DevKitC J3 Pin 5)
#define PIN_C6_WINCH_ALT                                                       \
  -1 // Desativado: GPIO 19 dedicado a LEDs WS2812B Strobe
#define PIN_C6_SERVO_RACK 21 // GPIO 21 - Conector RCK na PCB (DevKitC J3 Pin 7)
#define PIN_C6_SERVO_RUD 15  // GPIO 15 - Conector RUD na PCB (DevKitC J3 Pin 4)
#define PIN_C6_MOTOR_ESC 17  // GPIO 17 - Conector ESC na PCB (DevKitC J3 Pin 3)

// Fim de Curso da Âncora (Microswitch topo)
#define PIN_ANCHOR_DETECTOR                                                    \
  18 // GPIO 18 no DevKitC (INPUT_PULLUP, fecha ao GND no topo)

// Sensor de Efeito Hall do Tambor da Âncora (DRV5032)
#define PIN_ANCHOR_HALL                                                        \
  20 // GPIO 20 no DevKitC (INPUT_PULLUP, contador de voltas do tambor)

// Fita / Módulo de LEDs WS2812B Strobe Náutico
#define PIN_STROBE_LEDS 19 // GPIO 19 no DevKitC (Linha de dados DIN WS2812B)
#define NUM_STROBE_LEDS 8  // Quantidade padrão de LEDs na fita/anel

// Aliases e compatibilidade
#define PIN_GPS_RX PIN_C6_GPS_RX
#define PIN_GPS_TX PIN_C6_GPS_TX
#define ADS1115_I2C_ADDR 0x48
#define ADS_CH_BATT_VOLT 0
#define ADS_CH_BATT_CURR 1
#define ADS_CH_WATER_SENS 2
#define ADS_CH_AUX 3

// PCA9685 I2C Channels (Opcional / Compatibilidade)
#define PCA9685_I2C_ADDR 0x40
#define PCA_CH_MOTOR 0  // Main propulsion ESC
#define PCA_CH_RUDDER 1 // Rudder servo
#define PCA_CH_WINCH 2  // Anchor winch servo
#define PCA_CH_RACK 3   // Anchor rack/clutch servo

// Posições padrão da cremalheira (configuráveis em NVS / SD)
#define RACK_POS_ENGAGED 0   // Ângulo padrão de encosto (cremalheira engatada / travada)
#define RACK_POS_RELEASED                                                      \
  90 // Ângulo padrão livre (cremalheira levantada para queda da âncora por gravidade)
#define RACK_SERVO_INVERTED_DEFAULT                                            \
  true // Direção do servo da cremalheira invertida por defeito

// Limites de pulso PWM
#define PWM_PULSE_MIN 1000
#define PWM_PULSE_MID 1500
#define PWM_PULSE_MAX 2000

// ==========================================
// BASE PIN DEFINITIONS
// ==========================================
// 4-pin Joystick connector on Top Header J1 (Pads 3 e 6)
#define PIN_JOYSTICK_X 3 // ADC1_CH3 (Lafvin J1 Pad 3 / Serigrafia 3)
#define PIN_JOYSTICK_Y 0 // ADC1_CH0 (Lafvin J1 Pad 6 / Serigrafia 0)
#define PIN_JOYSTICK_SW                                                        \
  -1 // Desativado no joystick: GPIO 9 e o botao BOOT com interrupcao dedicada
     // para mudar de pagina

// Physical buttons on Base (Bottom Header)
#define PIN_BTN_ANCHOR_UP                                                      \
  18 // Button to hoist anchor / UP (SW1 Left - Bottom Header Pin 2 / IO18)
#define PIN_BTN_ANCHOR_DOWN                                                    \
  20 // Button to drop anchor / DOWN (SW3 Bottom - Bottom Header Pin 4 / IO20)
#define PIN_BTN_MODE                                                           \
  19 // Button for mode toggle / MODE (SW2 Right - Bottom Header Pin 3 / IO19)
