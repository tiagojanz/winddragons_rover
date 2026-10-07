# Hardware Display & Carrier PCB Orientation Rules

- **Universal Carrier PCB**: Both **Rover** and **Base Station** share the exact same Universal Carrier PCB hardware design and mounting.
- **Display Orientation**:
  - The LCD ST7789 1.47" display has **the exact same physical orientation on both Rover and Base**.
  - Orientation is configured in **Landscape** with `DISPLAY_ROTATION = 3` (`SCREEN_W = 320`, `SCREEN_H = 172`):
    ```cpp
    canvas = new Arduino_Canvas(LCD_WIDTH, LCD_HEIGHT, nullptr, 0, 0, DISPLAY_ROTATION);
    ```
  - Both devices must use `DISPLAY_ROTATION` (3) for proper rendering.
- **Base Station Multi-Screen Architecture**:
  - Page 0: `BASE_PAGE_CONTROL` — Selected Rover live telemetry (GPS, speed, battery, anchor, RSSI) and joystick live control (throttle, rudder, nav mode).
  - Page 1: `BASE_PAGE_FLEET` — 2x4 grid listing all 8 Rovers (ROVER-01 .. ROVER-08) with online/offline status, battery %, speed, and signal RSSI. Joystick navigates and selects rovers.
  - Page 2: `BASE_PAGE_NETWORK` — Full-screen network & WiFi dashboard matching the Rover layout (SSID, Big IP, RSSI/clients, AP status).
  - BOOT button: Cycles through pages in sequence.
  - Boot screen: Landscape 320x172 with dragon logo on left panel and scrolling POST console on right panel.
