#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <canvas/Arduino_Canvas.h>
#include "Display_ST7789.h"
#include "config_common.h"
#include "lora_protocol.h"
#include "gps_tracker.h"
#include "battery_monitor.h"
#include "actuators.h"

// Colors (RGB565)
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

enum RoverScreenPage {
    ROVER_PAGE_NAV = 0,
    ROVER_PAGE_POWER = 1,
    ROVER_PAGE_ACTUATORS = 2,
    ROVER_PAGE_COUNT = 3
};

class RoverDisplay {
public:
    RoverDisplay() : canvas(nullptr), currentPage(ROVER_PAGE_NAV), lastRender(0) {}

    void begin() {
        // Disable onboard TF/MicroSD card CS (GPIO 4) to prevent SPI bus collision
        pinMode(4, OUTPUT);
        digitalWrite(4, HIGH);

        // Initialize manufacturer ST7789 display driver
        display.begin();
        display.setBacklight(85); // 85% brightness

        // Create high-speed in-memory canvas configured in Landscape (DISPLAY_ROTATION = 1)
        canvas = new Arduino_Canvas(LCD_WIDTH, LCD_HEIGHT, nullptr, 0, 0, DISPLAY_ROTATION);
        if (canvas) {
            canvas->begin();
            canvas->fillScreen(UI_BLACK);
            display.drawPixelBuffer(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, canvas->getFramebuffer());
        }
    }

    void nextPage() {
        currentPage = static_cast<RoverScreenPage>((currentPage + 1) % ROVER_PAGE_COUNT);
        lastRender = 0; // Force immediate re-render
    }

    void prevPage() {
        currentPage = static_cast<RoverScreenPage>((currentPage + ROVER_PAGE_COUNT - 1) % ROVER_PAGE_COUNT);
        lastRender = 0; // Force immediate re-render
    }

    RoverScreenPage getCurrentPage() const {
        return currentPage;
    }

    void update(uint8_t roverId, GPSTracker &gps, BatteryMonitor &batt,
                ActuatorController &actuators, uint8_t motorStatus,
                int16_t lastRssi, bool failsafeActive, bool force = false) {
        uint32_t now = millis();
        if (!force && (now - lastRender < 200)) return; // 5Hz UI refresh unless forced
        lastRender = now;

        if (!canvas) return;

        // 1. Top Header Bar (Height: 24px)
        canvas->fillRect(0, 0, SCREEN_W, 24, UI_NAVY);
        canvas->setTextColor(UI_WHITE);
        canvas->setTextSize(2);
        canvas->setCursor(8, 4);
        canvas->printf("ROVER-%02d", roverId);

        // Current page label in center
        canvas->setTextSize(2);
        canvas->setTextColor(UI_CYAN);
        canvas->setCursor(120, 4);
        if (currentPage == ROVER_PAGE_NAV) {
            canvas->print("NAV/GPS");
        } else if (currentPage == ROVER_PAGE_POWER) {
            canvas->print("BATERIA");
        } else {
            canvas->print("SISTEMA");
        }

        // Page tabs indicators (3 dots at top right)
        for (int i = 0; i < ROVER_PAGE_COUNT; ++i) {
            uint16_t dotCol = (i == currentPage) ? UI_YELLOW : UI_DARKGREY;
            canvas->fillCircle(276 + i * 14, 12, 5, dotCol);
        }

        // 2. Render Page Content in 2-Column Landscape Layout
        switch (currentPage) {
            case ROVER_PAGE_NAV:
                renderNavPage(gps, motorStatus);
                break;
            case ROVER_PAGE_POWER:
                renderPowerPage(batt);
                break;
            case ROVER_PAGE_ACTUATORS:
                renderActuatorsPage(actuators, motorStatus, lastRssi, failsafeActive);
                break;
            default:
                break;
        }

        // 3. Bottom Footer Bar (Height: 22px)
        canvas->fillRect(0, 150, SCREEN_W, 22, UI_NAVY);
        canvas->setTextColor(UI_YELLOW);
        canvas->setTextSize(1);
        canvas->setCursor(10, 156);
        canvas->printf("PAG %d/3 | CLIQUE [BOOT] P/ MUDAR PAGINA", currentPage + 1);

        // 4. Push complete frame to physical ST7789 display
        display.drawPixelBuffer(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, canvas->getFramebuffer());
    }

private:
    void renderNavPage(GPSTracker &gps, uint8_t motorStatus) {
        int cardY = 26;
        int cardH = 122;

        // --- Left Card: Status & Speed (x=4..158) ---
        canvas->fillRect(4, cardY, 154, cardH, UI_PANEL_BG);
        canvas->drawRect(4, cardY, 154, cardH, UI_CARD_BG);

        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setTextSize(1);
        canvas->setCursor(10, cardY + 6);
        canvas->print("ESTADO GPS:");

        canvas->setCursor(10, cardY + 18);
        canvas->setTextSize(2);
        if (gps.hasFix()) {
            canvas->setTextColor(UI_GREEN);
            canvas->printf("FIX (%dS)", gps.getSatellites());
        } else {
            canvas->setTextColor(UI_RED);
            canvas->print("SEM FIX");
        }

        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setTextSize(1);
        canvas->setCursor(10, cardY + 44);
        canvas->print("VELOCIDADE:");
        canvas->setCursor(10, cardY + 56);
        canvas->setTextSize(2);
        canvas->setTextColor(UI_CYAN);
        canvas->printf("%.1f kn", gps.getSpeedKnots());

        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setTextSize(1);
        canvas->setCursor(10, cardY + 82);
        canvas->print("RUMO DA PROA:");
        canvas->setCursor(10, cardY + 94);
        canvas->setTextSize(2);
        canvas->setTextColor(UI_WHITE);
        canvas->printf("%.0f deg", gps.getHeading());

        // --- Right Card: Coordinates & Mode (x=162..316) ---
        canvas->fillRect(162, cardY, 154, cardH, UI_PANEL_BG);
        canvas->drawRect(162, cardY, 154, cardH, UI_CARD_BG);

        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setTextSize(1);
        canvas->setCursor(168, cardY + 6);
        canvas->print("MODO OPERACIONAL:");
        canvas->setCursor(168, cardY + 18);
        canvas->setTextSize(2);
        canvas->setTextColor(UI_YELLOW);
        canvas->print(get_motor_status_str(motorStatus));

        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setTextSize(1);
        canvas->setCursor(168, cardY + 44);
        canvas->print("LATITUDE:");
        canvas->setCursor(168, cardY + 56);
        canvas->setTextSize(2);
        canvas->setTextColor(UI_WHITE);
        canvas->printf("%.5f", gps.getLatitude());

        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setTextSize(1);
        canvas->setCursor(168, cardY + 82);
        canvas->print("LONGITUDE:");
        canvas->setCursor(168, cardY + 94);
        canvas->setTextSize(2);
        canvas->setTextColor(UI_WHITE);
        canvas->printf("%.5f", gps.getLongitude());
    }

    void renderPowerPage(BatteryMonitor &batt) {
        int cardY = 26;
        int cardH = 122;

        // --- Left Card: Big Battery & Total Voltage (x=4..158) ---
        canvas->fillRect(4, cardY, 154, cardH, UI_PANEL_BG);
        canvas->drawRect(4, cardY, 154, cardH, UI_CARD_BG);

        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setTextSize(1);
        canvas->setCursor(10, cardY + 6);
        canvas->print("BATERIA LIPO 4S:");

        // Big Percentage Number (Size 3)
        canvas->setCursor(10, cardY + 20);
        canvas->setTextSize(3);
        uint8_t pct = batt.getPercentage();
        canvas->setTextColor(pct < 25 ? UI_RED : (pct < 50 ? UI_YELLOW : UI_GREEN));
        canvas->printf("%u%%", pct);

        // Progress Bar
        int barW = map(constrain(pct, 0, 100), 0, 100, 0, 134);
        canvas->drawRect(10, cardY + 52, 138, 12, UI_WHITE);
        uint16_t bCol = (pct < 25 ? UI_RED : (pct < 50 ? UI_YELLOW : UI_GREEN));
        canvas->fillRect(12, cardY + 54, barW, 8, bCol);

        // Total Voltage
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setTextSize(1);
        canvas->setCursor(10, cardY + 76);
        canvas->print("TENSAO TOTAL:");
        canvas->setCursor(10, cardY + 88);
        canvas->setTextSize(2);
        canvas->setTextColor(UI_WHITE);
        canvas->printf("%.2f V", batt.getVoltageMv() / 1000.0f);

        // --- Right Card: Cell Avg, Current & Consumption (x=162..316) ---
        canvas->fillRect(162, cardY, 154, cardH, UI_PANEL_BG);
        canvas->drawRect(162, cardY, 154, cardH, UI_CARD_BG);

        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setTextSize(1);
        canvas->setCursor(168, cardY + 6);
        canvas->print("MEDIA P/ CELULA (4S):");
        canvas->setCursor(168, cardY + 18);
        canvas->setTextSize(2);
        canvas->setTextColor(batt.getCellAverageV() < 3.4f ? UI_RED : UI_CYAN);
        canvas->printf("%.2f V/cel", batt.getCellAverageV());

        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setTextSize(1);
        canvas->setCursor(168, cardY + 44);
        canvas->print("CORRENTE ATUAL:");
        canvas->setCursor(168, cardY + 56);
        canvas->setTextSize(2);
        canvas->setTextColor(UI_WHITE);
        canvas->printf("%.2f A", batt.getCurrentAmps());

        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setTextSize(1);
        canvas->setCursor(168, cardY + 82);
        canvas->print("CONSUMO TOTAL:");
        canvas->setCursor(168, cardY + 94);
        canvas->setTextSize(2);
        canvas->setTextColor(UI_ORANGE);
        canvas->printf("%.0f mAh", batt.getConsumedMah());
    }

    void renderActuatorsPage(ActuatorController &actuators, uint8_t motorStatus,
                            int16_t lastRssi, bool failsafeActive) {
        int cardY = 26;
        int cardH = 122;

        // --- Left Card: Propulsion & Rudder (x=4..158) ---
        canvas->fillRect(4, cardY, 154, cardH, UI_PANEL_BG);
        canvas->drawRect(4, cardY, 154, cardH, UI_CARD_BG);

        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setTextSize(1);
        canvas->setCursor(10, cardY + 6);
        canvas->print("MOTOR ESC:");
        canvas->setCursor(10, cardY + 18);
        canvas->setTextSize(2);
        canvas->setTextColor(actuators.getThrottle() != 0 ? UI_GREEN : UI_WHITE);
        canvas->printf("THR: %+3d%%", actuators.getThrottle());

        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setTextSize(1);
        canvas->setCursor(10, cardY + 44);
        canvas->print("LEME / RUDDER:");
        canvas->setCursor(10, cardY + 56);
        canvas->setTextSize(2);
        canvas->setTextColor(UI_CYAN);
        canvas->printf("RUD: %+3d%%", actuators.getRudder());

        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setTextSize(1);
        canvas->setCursor(10, cardY + 82);
        canvas->print("CREMALHEIRA TRAVAO:");
        canvas->setCursor(10, cardY + 94);
        canvas->setTextSize(2);
        bool isEngaged = !actuators.isRackReleased();
        canvas->setTextColor(isEngaged ? UI_GREEN : UI_ORANGE);
        canvas->printf("%s", isEngaged ? "TRAVADA" : "LIVRE");

        // --- Right Card: Anchor & LoRa Link (x=162..316) ---
        canvas->fillRect(162, cardY, 154, cardH, UI_PANEL_BG);
        canvas->drawRect(162, cardY, 154, cardH, UI_CARD_BG);

        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setTextSize(1);
        canvas->setCursor(168, cardY + 6);
        canvas->print("ESTADO DA ANCORA:");
        canvas->setCursor(168, cardY + 18);
        canvas->setTextSize(2);
        canvas->setTextColor(UI_CYAN);
        canvas->printf("%s", get_anchor_status_str(actuators.getAnchorStatus()));

        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setTextSize(1);
        canvas->setCursor(168, cardY + 44);
        canvas->print("PROFUNDIDADE:");
        canvas->setCursor(168, cardY + 56);
        canvas->setTextSize(2);
        canvas->setTextColor(UI_WHITE);
        canvas->printf("%.1f m", actuators.getAnchorDepthCm() / 100.0f);

        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setTextSize(1);
        canvas->setCursor(168, cardY + 82);
        canvas->print("LINK LORA 433MHZ:");
        canvas->setCursor(168, cardY + 94);
        canvas->setTextSize(2);
        if (failsafeActive) {
            canvas->setTextColor(UI_RED);
            canvas->print("FAILSAFE");
        } else {
            canvas->setTextColor(UI_GREEN);
            canvas->printf("%d dBm", lastRssi);
        }
    }

    ST7789Display display;
    Arduino_Canvas *canvas;
    RoverScreenPage currentPage;
    uint32_t lastRender;
};
