#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <canvas/Arduino_Canvas.h>
#include "Display_ST7789.h"
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
    BaseDisplay() : canvas(nullptr), lastRender(0) {}

    void begin() {
        // Disable onboard MicroSD card CS (GPIO 4)
        pinMode(PIN_SD_CS, OUTPUT);
        digitalWrite(PIN_SD_CS, HIGH);

        // Initialize Lafvin ST7789 display driver
        display.begin();
        display.setBacklight(80); // 80% brightness

        // Create high-speed off-screen RAM canvas
        canvas = new Arduino_Canvas(LCD_WIDTH, LCD_HEIGHT, nullptr);
        if (canvas) {
            canvas->begin();
            canvas->fillScreen(UI_BLACK);
            drawStaticHeader();
            display.drawPixelBuffer(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, canvas->getFramebuffer());
        }
    }

    void drawStaticHeader() {
        if (!canvas) return;
        canvas->fillRect(0, 0, LCD_WIDTH, 26, UI_NAVY);
        canvas->setTextColor(UI_WHITE);
        canvas->setTextSize(1);
        canvas->setCursor(6, 9);
        canvas->print("WINDDRAGONS BASE");
    }

    void update(FleetManager &fleet, bool wifiOk, bool cloudOk, 
                int8_t throttle, int8_t rudder, uint8_t navMode, bool sdOk = false) {
        uint32_t now = millis();
        if (now - lastRender < 200) return; // 5Hz UI refresh
        lastRender = now;

        if (!canvas) return;

        FleetRover *selected = fleet.getSelectedRover();

        // 1. Status Bar icons (SD, WiFi, Cloud)
        drawStaticHeader();
        canvas->fillRect(104, 4, 64, 18, UI_NAVY);
        canvas->fillCircle(114, 13, 4, sdOk ? UI_CYAN : UI_DARKGREY);
        canvas->fillCircle(132, 13, 4, wifiOk ? UI_GREEN : UI_RED);
        canvas->fillCircle(150, 13, 4, cloudOk ? UI_GREEN : UI_ORANGE);

        // 2. Active Rover Header
        canvas->fillRect(4, 30, LCD_WIDTH - 8, 28, UI_CARD_BG);
        canvas->setTextColor(UI_YELLOW);
        canvas->setTextSize(2);
        canvas->setCursor(8, 36);
        if (selected) {
            canvas->print(selected->code);
        } else {
            canvas->print("NO ROVER");
        }

        // Online & RSSI badge
        canvas->setTextSize(1);
        canvas->setCursor(110, 36);
        if (selected && selected->is_online) {
            canvas->setTextColor(UI_GREEN);
            canvas->print("ONLINE");
            canvas->setCursor(110, 47);
            canvas->setTextColor(UI_WHITE);
            canvas->printf("%d dBm", selected->rssi);
        } else {
            canvas->setTextColor(UI_RED);
            canvas->print("OFFLINE");
        }

        // 3. Telemetry Info Card
        int y = 64;
        canvas->fillRect(4, y, LCD_WIDTH - 8, 140, UI_PANEL_BG);

        canvas->setTextColor(UI_WHITE);
        canvas->setTextSize(1);

        if (selected && selected->is_online) {
            canvas->setCursor(8, y + 6);
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->print("GPS COORD:");
            canvas->setCursor(8, y + 17);
            canvas->setTextColor(UI_WHITE);
            canvas->printf("%.6f, %.6f", selected->lat, selected->lng);

            canvas->setCursor(8, y + 32);
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->print("SPEED / HEADING:");
            canvas->setCursor(8, y + 43);
            canvas->setTextColor(UI_WHITE);
            canvas->printf("%.1f kn | %.0f deg", selected->speed_knots, selected->heading_deg);

            canvas->setCursor(8, y + 58);
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->print("BATTERY:");
            canvas->setCursor(8, y + 69);
            canvas->setTextColor(selected->battery_pct < 25 ? UI_RED : UI_GREEN);
            canvas->printf("%u%% (%.1f V)", selected->battery_pct, selected->battery_voltage);

            canvas->setCursor(8, y + 84);
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->print("MOTOR STATUS:");
            canvas->setCursor(8, y + 95);
            canvas->setTextColor(UI_YELLOW);
            canvas->print(get_motor_status_str(selected->motor_status));

            canvas->setCursor(8, y + 110);
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->print("ANCHOR:");
            canvas->setCursor(8, y + 121);
            canvas->setTextColor(UI_CYAN);
            canvas->printf("%s (%.1fm)", get_anchor_status_str(selected->anchor_status), selected->anchor_depth_m);
        } else {
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->setCursor(14, y + 60);
            canvas->print("Waiting for telemetry...");
        }

        // 4. Joystick Controls Card
        y = 210;
        canvas->fillRect(4, y, LCD_WIDTH - 8, 70, UI_CARD_BG);
        canvas->setTextColor(UI_WHITE);
        canvas->setTextSize(1);
        canvas->setCursor(8, y + 6);
        canvas->printf("JOYSTICK INPUT [%s]", (navMode == 0) ? "MANUAL" : "STATION");

        // Throttle indicator bar
        canvas->setCursor(8, y + 22);
        canvas->printf("THR: %+3d%%", throttle);
        canvas->drawRect(70, y + 20, 90, 10, UI_WHITE);
        int barW = map(throttle, -100, 100, 0, 88);
        canvas->fillRect(71, y + 21, barW, 8, throttle >= 0 ? UI_GREEN : UI_RED);

        // Rudder indicator bar
        canvas->setCursor(8, y + 42);
        canvas->printf("RUD: %+3d%%", rudder);
        canvas->drawRect(70, y + 40, 90, 10, UI_WHITE);
        int barR = map(rudder, -100, 100, 0, 88);
        canvas->fillRect(71, y + 41, barR, 8, UI_BLUE);

        // 5. Bottom Navigation Hint
        canvas->fillRect(0, 290, LCD_WIDTH, 30, UI_NAVY);
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setTextSize(1);
        canvas->setCursor(6, 298);
        canvas->print("BOOT: Cycle Rover [1..8]");

        // Push frame to ST7789
        display.drawPixelBuffer(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, canvas->getFramebuffer());
    }

    void renderMenu(const char* const items[], uint8_t count, uint8_t selectedIndex, const char* feedbackMsg = nullptr) {
        if (!canvas) return;

        canvas->fillRect(4, 30, LCD_WIDTH - 8, 255, UI_PANEL_BG);
        canvas->setTextColor(UI_YELLOW);
        canvas->setTextSize(1);
        canvas->setCursor(10, 38);
        canvas->print("=== MENU DE CONTROLO ===");

        int startY = 56;
        int itemH = 26;

        for (uint8_t i = 0; i < count; ++i) {
            int y = startY + i * itemH;
            if (i == selectedIndex) {
                canvas->fillRect(8, y, LCD_WIDTH - 16, itemH - 3, UI_NAVY);
                canvas->drawRect(8, y, LCD_WIDTH - 16, itemH - 3, UI_CYAN);
                canvas->setTextColor(UI_WHITE);
            } else {
                canvas->fillRect(8, y, LCD_WIDTH - 16, itemH - 3, UI_CARD_BG);
                canvas->setTextColor(UI_LIGHTGREY);
            }
            canvas->setCursor(14, y + 6);
            canvas->print(items[i]);
        }

        // Bottom Feedback / Hint
        canvas->fillRect(0, 290, LCD_WIDTH, 30, UI_NAVY);
        canvas->setTextSize(1);
        if (feedbackMsg != nullptr) {
            canvas->setTextColor(UI_YELLOW);
            canvas->setCursor(6, 298);
            canvas->print(feedbackMsg);
        } else {
            canvas->setTextColor(UI_CYAN);
            canvas->setCursor(6, 298);
            canvas->print("JOY: NAVEGAR | CLIQUE: OK");
        }

        // Push frame to ST7789
        display.drawPixelBuffer(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, canvas->getFramebuffer());
    }

    void renderNetworkScreen(bool isApMode, const char* ssid, const char* ip, int8_t rssi, uint8_t clients = 0) {
        if (!canvas) return;

        drawStaticHeader();

        // 1. Status Banner
        canvas->fillRect(4, 30, LCD_WIDTH - 8, 255, UI_PANEL_BG);
        canvas->drawRect(4, 30, LCD_WIDTH - 8, 255, UI_CARD_BG);

        if (!isApMode) {
            // === MODO WIFI (STA CONECTADO) ===
            canvas->fillRect(8, 34, LCD_WIDTH - 16, 26, UI_NAVY);
            canvas->drawRect(8, 34, LCD_WIDTH - 16, 26, UI_GREEN);
            canvas->setTextColor(UI_GREEN);
            canvas->setTextSize(1);
            canvas->setCursor(14, 43);
            canvas->print("ESTADO: WIFI CONECTADO");

            // Endereço IP em destaque
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->setCursor(14, 68);
            canvas->print("ENDERECO IP:");
            canvas->setCursor(14, 82);
            canvas->setTextColor(UI_CYAN);
            canvas->setTextSize(2);
            canvas->print(ip ? ip : "0.0.0.0");

            // SSID
            canvas->setTextSize(1);
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->setCursor(14, 110);
            canvas->print("REDE CONECTADA:");
            canvas->setCursor(14, 124);
            canvas->setTextColor(UI_WHITE);
            canvas->setTextSize(1);
            canvas->printf("%.20s", ssid ? ssid : "");

            // Sinal RSSI
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->setCursor(14, 145);
            canvas->print("SINAL DE REDE:");
            canvas->setCursor(14, 158);
            canvas->setTextColor(UI_GREEN);
            canvas->printf("%d dBm (Bom)", rssi);

            // Instrucao de acesso HTTP Web
            canvas->fillRect(8, 180, LCD_WIDTH - 16, 95, UI_CARD_BG);
            canvas->setTextColor(UI_YELLOW);
            canvas->setCursor(14, 188);
            canvas->print("ACESSO WEB SERVER:");
            canvas->setTextColor(UI_WHITE);
            canvas->setCursor(14, 204);
            canvas->printf("http://%s", ip ? ip : "");
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->setCursor(14, 222);
            canvas->print("Disponivel:");
            canvas->setCursor(14, 234);
            canvas->print("- GPS e Mapas");
            canvas->setCursor(14, 246);
            canvas->print("- Bateria LiPo 4S");
            canvas->setCursor(14, 258);
            canvas->print("- Comando de Boias");
        } else {
            // === MODO AP (PONTO DE ACESSO) ===
            canvas->fillRect(8, 34, LCD_WIDTH - 16, 26, UI_NAVY);
            canvas->drawRect(8, 34, LCD_WIDTH - 16, 26, UI_YELLOW);
            canvas->setTextColor(UI_YELLOW);
            canvas->setTextSize(1);
            canvas->setCursor(14, 43);
            canvas->print("ESTADO: MODO AP (PONTO ACESSO)");

            // SSID em destaque
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->setCursor(14, 68);
            canvas->print("SSID DA BASE (LIGUE-SE):");
            canvas->setCursor(14, 82);
            canvas->setTextColor(UI_YELLOW);
            canvas->setTextSize(2);
            canvas->printf("%.14s", ssid ? ssid : "WindDragons");

            // Password
            canvas->setTextSize(1);
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->setCursor(14, 110);
            canvas->print("PASSWORD WIFI:");
            canvas->setCursor(14, 124);
            canvas->setTextColor(UI_WHITE);
            canvas->print("12345678");

            // Endereço IP do AP
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->setCursor(14, 145);
            canvas->print("ENDERECO CONFIG:");
            canvas->setCursor(14, 158);
            canvas->setTextColor(UI_CYAN);
            canvas->printf("http://%s", ip ? ip : "192.168.4.1");

            // Dica de configuração
            canvas->fillRect(8, 180, LCD_WIDTH - 16, 95, UI_CARD_BG);
            canvas->setTextColor(UI_CYAN);
            canvas->setCursor(14, 188);
            canvas->print("CONFIGURACAO WIFI:");
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->setCursor(14, 204);
            canvas->print("1. Conecte o telemovel");
            canvas->setCursor(14, 218);
            canvas->printf("   ao WiFi '%s'", ssid ? ssid : "");
            canvas->setCursor(14, 234);
            canvas->print("2. Abra o browser em");
            canvas->setCursor(14, 248);
            canvas->setTextColor(UI_GREEN);
            canvas->printf("   http://%s", ip ? ip : "192.168.4.1");
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->setCursor(14, 262);
            canvas->printf("Clientes: %d", clients);
        }

        // Rodapé
        canvas->fillRect(0, 290, LCD_WIDTH, 30, UI_NAVY);
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setTextSize(1);
        canvas->setCursor(6, 298);
        canvas->print("BOOT: MENU / VOLTAR");

        // Push frame to ST7789
        display.drawPixelBuffer(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, canvas->getFramebuffer());
    }

    ST7789Display display;
    Arduino_Canvas *canvas;
    uint32_t lastRender;
};
