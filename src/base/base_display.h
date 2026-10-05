#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "config_common.h"
#include "fleet_manager.h"

// Standard 16-bit RGB565 color definitions
#define UI_BLACK     0x0000
#define UI_WHITE     0xFFFF
#define UI_RED       0xF800
#define UI_GREEN     0x07E0
#define UI_BLUE      0x001F
#define UI_CYAN      0x07FF
#define UI_YELLOW    0xFFE0
#define UI_ORANGE    0xFD20
#define UI_LIGHTGREY 0xC618
#define UI_DARKGREY  0x4208
#define UI_NAVY      0x0841
#define UI_CARD_BG   0x18E3
#define UI_PANEL_BG  0x10A2

class BaseDisplay {
public:
    BaseDisplay() : gfx(nullptr), lastRender(0) {}

    void begin() {
        // Turn on LCD Backlight
        pinMode(PIN_LCD_BL, OUTPUT);
        digitalWrite(PIN_LCD_BL, HIGH);

        // ST7789 1.47" (172x320) SPI Bus configuration
        Arduino_DataBus *bus = new Arduino_ESP32SPI(
            PIN_LCD_DC, PIN_LCD_CS, PIN_LCD_SCLK, PIN_LCD_MOSI, GFX_NOT_DEFINED
        );

        // ST7789 (width: 172, height: 320, col_offset: 34, row_offset: 0)
        gfx = new Arduino_ST7789(
            bus, PIN_LCD_RST, 0 /* rotation */, true /* IPS */,
            LCD_WIDTH, LCD_HEIGHT, 34, 0, 34, 0
        );

        if (gfx) {
            gfx->begin();
            gfx->fillScreen(UI_BLACK);
            drawStaticHeader();
        }
    }

    void drawStaticHeader() {
        if (!gfx) return;
        gfx->fillRect(0, 0, LCD_WIDTH, 26, UI_NAVY); // Dark Navy Blue Header
        gfx->setTextColor(UI_WHITE);
        gfx->setTextSize(1);
        gfx->setCursor(6, 9);
        gfx->print("WINDDRAGONS BASE");
    }

    void update(FleetManager &fleet, bool wifiOk, bool cloudOk, int8_t throttle, int8_t rudder) {
        uint32_t now = millis();
        if (now - lastRender < 200) return; // 5Hz UI refresh
        lastRender = now;

        if (!gfx) return;

        FleetRover *selected = fleet.getSelectedRover();

        // 1. Status Bar icons
        gfx->fillRect(120, 4, 46, 18, UI_NAVY);
        // WiFi indicator
        gfx->fillCircle(132, 13, 4, wifiOk ? UI_GREEN : UI_RED);
        // Cloud sync indicator
        gfx->fillCircle(150, 13, 4, cloudOk ? UI_CYAN : UI_ORANGE);

        // 2. Active Rover Header
        gfx->fillRect(4, 30, LCD_WIDTH - 8, 28, UI_CARD_BG); // Card BG
        gfx->setTextColor(UI_YELLOW);
        gfx->setTextSize(2);
        gfx->setCursor(8, 36);
        if (selected) {
            gfx->print(selected->code);
        } else {
            gfx->print("NO ROVER");
        }

        // Online & RSSI badge
        gfx->setTextSize(1);
        gfx->setCursor(110, 36);
        if (selected && selected->is_online) {
            gfx->setTextColor(UI_GREEN);
            gfx->print("ONLINE");
            gfx->setCursor(110, 47);
            gfx->setTextColor(UI_WHITE);
            gfx->printf("%d dBm", selected->rssi);
        } else {
            gfx->setTextColor(UI_RED);
            gfx->print("OFFLINE");
        }

        // 3. Telemetry Info Card
        int y = 64;
        gfx->fillRect(4, y, LCD_WIDTH - 8, 140, UI_PANEL_BG); // Dark slate

        gfx->setTextColor(UI_WHITE);
        gfx->setTextSize(1);

        if (selected && selected->is_online) {
            gfx->setCursor(8, y + 6);
            gfx->setTextColor(UI_LIGHTGREY);
            gfx->print("GPS COORD:");
            gfx->setCursor(8, y + 17);
            gfx->setTextColor(UI_WHITE);
            gfx->printf("%.6f, %.6f", selected->lat, selected->lng);

            gfx->setCursor(8, y + 32);
            gfx->setTextColor(UI_LIGHTGREY);
            gfx->print("SPEED / HEADING:");
            gfx->setCursor(8, y + 43);
            gfx->setTextColor(UI_WHITE);
            gfx->printf("%.1f kn | %.0f deg", selected->speed_knots, selected->heading_deg);

            gfx->setCursor(8, y + 58);
            gfx->setTextColor(UI_LIGHTGREY);
            gfx->print("BATTERY:");
            gfx->setCursor(8, y + 69);
            gfx->setTextColor(selected->battery_pct < 25 ? UI_RED : UI_GREEN);
            gfx->printf("%u%% (%.1f V)", selected->battery_pct, selected->battery_voltage);

            gfx->setCursor(8, y + 84);
            gfx->setTextColor(UI_LIGHTGREY);
            gfx->print("MOTOR STATUS:");
            gfx->setCursor(8, y + 95);
            gfx->setTextColor(UI_YELLOW);
            gfx->print(get_motor_status_str(selected->motor_status));

            gfx->setCursor(8, y + 110);
            gfx->setTextColor(UI_LIGHTGREY);
            gfx->print("ANCHOR:");
            gfx->setCursor(8, y + 121);
            gfx->setTextColor(UI_CYAN);
            gfx->printf("%s (%.1fm)", get_anchor_status_str(selected->anchor_status), selected->anchor_depth_m);
        } else {
            gfx->setTextColor(UI_LIGHTGREY);
            gfx->setCursor(14, y + 60);
            gfx->print("Waiting for telemetry...");
        }

        // 4. Joystick Controls Card
        y = 210;
        gfx->fillRect(4, y, LCD_WIDTH - 8, 70, UI_CARD_BG);
        gfx->setTextColor(UI_WHITE);
        gfx->setTextSize(1);
        gfx->setCursor(8, y + 6);
        gfx->print("JOYSTICK INPUT");

        // Throttle indicator bar
        gfx->setCursor(8, y + 22);
        gfx->printf("THR: %+3d%%", throttle);
        gfx->drawRect(70, y + 20, 90, 10, UI_WHITE);
        int barW = map(throttle, -100, 100, 0, 88);
        gfx->fillRect(71, y + 21, barW, 8, throttle >= 0 ? UI_GREEN : UI_RED);

        // Rudder indicator bar
        gfx->setCursor(8, y + 42);
        gfx->printf("RUD: %+3d%%", rudder);
        gfx->drawRect(70, y + 40, 90, 10, UI_WHITE);
        int barR = map(rudder, -100, 100, 0, 88);
        gfx->fillRect(71, y + 41, barR, 8, UI_BLUE);

        // 5. Bottom Navigation Hint
        gfx->fillRect(0, 290, LCD_WIDTH, 30, UI_NAVY);
        gfx->setTextColor(UI_LIGHTGREY);
        gfx->setTextSize(1);
        gfx->setCursor(6, 298);
        gfx->print("BOOT: Cycle Rover [1..8]");
    }

private:
    Arduino_GFX *gfx;
    uint32_t lastRender;
};
