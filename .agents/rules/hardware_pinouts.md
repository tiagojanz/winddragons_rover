# Hardware Pinout & Inter-ESP Validation Rules

Consult `HARDWARE_PINOUT_MEMORY.md` for full context.

## Master Board (Lafvin ESP32-C6 LCD 1.47" + TF SD):
- **LCD ST7789 (Internal SPI):** MOSI=GPIO 6, SCLK=GPIO 7, CS=GPIO 14, DC=GPIO 15, RST=GPIO 21, BL=GPIO 22.
- **TF MicroSD (Shared SPI):** CS=GPIO 4 (LOCKED, do NOT use for UART or GPIO!), MISO=GPIO 5, MOSI=GPIO 6, SCLK=GPIO 7.
- **Front Header J1:**
  - Pad 1 (Serigrafia 5): GPIO 5 (`/IO5_FREE` / TF MISO)
  - Pad 2 (Serigrafia 4): GPIO 4 (`/IO4_FREE` / TF CS)
  - Pad 3 (Serigrafia 3): GPIO 3 (`/JOY_X` - Joystick Eixo X)
  - Pad 4 (Serigrafia 2): GPIO 2 (`/INTER_TX_M2S` - Master UART TX)
  - Pad 5 (Serigrafia 1): GPIO 1 (`/INTER_RX_S2M` - Master UART RX)
  - Pad 6 (Serigrafia 0): GPIO 0 (`/JOY_Y` - Joystick Eixo Y)
  - Pad 7: +3V3
  - Pad 8: GND
  - Pad 9: +5V

## Slave Board (ESP32-C6 DevKitC-1):
- **Inter-ESP UART:** TX=GPIO 13 (to Master RX GPIO 1), RX=GPIO 22 (from Master TX GPIO 2), Baud=921600.
- **SX1278 LoRa (Ra-02):** NSS=GPIO 6, SCK=GPIO 5, MOSI=GPIO 7, MISO=GPIO 4, DIO0=GPIO 0, RST=GPIO 1.
- **Servos / Actuators:** ESC=GPIO 21, Rudder=GPIO 20, Winch=GPIO 19, Rack=GPIO 18.
- **Sensors:** Water=GPIO 15, Batt Volt=GPIO 2, Batt Curr=GPIO 3, GPS=GPIO 17/16.

## Wiring Colors:
- 🟢 Green: Lafvin GPIO 2 (TX) <-> DevKitC GPIO 22 (RX)
- 🟡 Yellow: DevKitC GPIO 13 (TX) <-> Lafvin GPIO 1 (RX)
- ⚫ Black: GND <-> GND
