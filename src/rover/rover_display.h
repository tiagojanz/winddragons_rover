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
#include "logo_bmp.h"

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
    ROVER_PAGE_NETWORK = 3,
    ROVER_PAGE_COUNT = 4
};

struct BootLogItem {
    char text[34];
    uint16_t color;
};

static const uint8_t BOOT_MAX_LINES = 24;
static const uint8_t BOOT_VISIBLE_LINES = 13;

class RoverDisplay {
public:
    RoverDisplay() : canvas(nullptr), currentPage(ROVER_PAGE_NAV), lastRender(0), _bootLogCount(0), _bootProgress(0) {}

    void begin() {
        // Disable onboard TF/MicroSD card CS (GPIO 4) to prevent SPI bus collision
        pinMode(PIN_SD_CS, OUTPUT);
        digitalWrite(PIN_SD_CS, HIGH);

        // Initialize manufacturer ST7789 display driver
        display.begin();
        display.setBacklight(85); // 85% brightness

        // Create high-speed in-memory canvas configured in Landscape
        canvas = new Arduino_Canvas(LCD_WIDTH, LCD_HEIGHT, nullptr, 0, 0, DISPLAY_ROTATION);
        if (canvas) {
            canvas->begin();
            canvas->fillScreen(UI_BLACK);
            display.drawPixelBuffer(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, canvas->getFramebuffer());
        }
    }

    void showBootScreen(uint8_t roverId = 1) {
        if (!canvas) return;
        _bootLogCount = 0;
        _bootProgress = 0;

        // Limpar ecran
        canvas->fillScreen(UI_BLACK);

        // Painel Esquerdo: Logotipo + Marca + ID + Barra de Progresso (w=101, h=164)
        canvas->fillRect(4, 4, 101, 164, UI_PANEL_BG);
        canvas->drawRect(4, 4, 101, 164, UI_CARD_BG);

        // Desenhar Logotipo Dragao centrado (85x80) em Ciano
        // x = 4 + (101 - 85)/2 = 12, y = 10
        canvas->drawBitmap(12, 10, LOGO_BMP, LOGO_BMP_W, LOGO_BMP_H, UI_CYAN);

        // Texto Marca "WINDDRAGONS" centrado
        canvas->setTextSize(1);
        canvas->setTextColor(UI_WHITE);
        canvas->setCursor(20, 94);
        canvas->print("WINDDRAGONS");

        // Identificador Rover
        canvas->setTextSize(2);
        canvas->setTextColor(UI_YELLOW);
        canvas->setCursor(18, 106);
        canvas->printf("RVR-%02d", roverId);

        // Subtitulo do sistema
        canvas->setTextSize(1);
        canvas->setTextColor(UI_CYAN);
        canvas->setCursor(16, 126);
        canvas->print("AUTONOMOUS");
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(24, 136);
        canvas->print("BUOY v2");

        // Moldura da barra de progresso no fundo do painel esquerdo
        canvas->drawRect(12, 150, 85, 10, UI_CARD_BG);
        canvas->fillRect(14, 152, 81, 6, UI_BLACK);

        // Painel Direito: Consola de Arranque (Scroll vertical, w=208, h=164)
        canvas->fillRect(108, 4, 208, 164, UI_PANEL_BG);
        canvas->drawRect(108, 4, 208, 164, UI_CARD_BG);

        // Barra de Titulo da Consola
        canvas->fillRect(109, 5, 206, 17, UI_NAVY);
        canvas->drawFastHLine(109, 22, 206, UI_CARD_BG);

        canvas->setTextSize(1);
        canvas->setTextColor(UI_YELLOW);
        canvas->setCursor(115, 9);
        canvas->print("CONSOLA DE ARRANQUE");

        canvas->setTextColor(UI_CYAN);
        canvas->setCursor(270, 9);
        canvas->print("[POST]");

        // Enviar imagem inicial para o display
        display.drawPixelBuffer(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, canvas->getFramebuffer());
    }

    void bootLog(const char* msg, uint16_t color = UI_LIGHTGREY, uint8_t progressPct = 0) {
        if (!canvas) return;

        // Adicionar mensagem ao buffer de logs
        if (_bootLogCount < BOOT_MAX_LINES) {
            strncpy(_bootLogs[_bootLogCount].text, msg, sizeof(_bootLogs[_bootLogCount].text) - 1);
            _bootLogs[_bootLogCount].text[sizeof(_bootLogs[_bootLogCount].text) - 1] = '\0';
            _bootLogs[_bootLogCount].color = color;
            _bootLogCount++;
        } else {
            // Fazer scroll interno descartando o mais antigo
            memmove(&_bootLogs[0], &_bootLogs[1], sizeof(BootLogItem) * (BOOT_MAX_LINES - 1));
            strncpy(_bootLogs[BOOT_MAX_LINES - 1].text, msg, sizeof(_bootLogs[BOOT_MAX_LINES - 1].text) - 1);
            _bootLogs[BOOT_MAX_LINES - 1].text[sizeof(_bootLogs[BOOT_MAX_LINES - 1].text) - 1] = '\0';
            _bootLogs[BOOT_MAX_LINES - 1].color = color;
        }

        // Atualizar barra de progresso se especificado
        if (progressPct > 0) {
            _bootProgress = constrain(progressPct, 0, 100);
            int barW = map(_bootProgress, 0, 100, 0, 81);
            canvas->fillRect(14, 152, 81, 6, UI_BLACK);
            if (barW > 0) {
                uint16_t barColor = (_bootProgress == 100) ? UI_GREEN : UI_CYAN;
                canvas->fillRect(14, 152, barW, 6, barColor);
            }
        }

        // Redesenhar a area de texto da consola (scroll vertical)
        canvas->fillRect(110, 24, 204, 142, UI_PANEL_BG);

        uint8_t startIdx = 0;
        uint8_t count = _bootLogCount;
        if (_bootLogCount > BOOT_VISIBLE_LINES) {
            startIdx = _bootLogCount - BOOT_VISIBLE_LINES;
            count = BOOT_VISIBLE_LINES;
        }

        canvas->setTextSize(1);
        for (uint8_t i = 0; i < count; i++) {
            uint8_t idx = startIdx + i;
            int y = 26 + i * 10;
            canvas->setTextColor(_bootLogs[idx].color);
            canvas->setCursor(114, y);
            canvas->print(_bootLogs[idx].text);
        }

        // Atualizar ecran LCD
        display.drawPixelBuffer(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, canvas->getFramebuffer());
    }

    void bootLogf(uint16_t color, uint8_t progressPct, const char* format, ...) {
        char buf[36];
        va_list args;
        va_start(args, format);
        vsnprintf(buf, sizeof(buf), format, args);
        va_end(args);
        bootLog(buf, color, progressPct);
    }

    void endBootScreen(uint32_t delayMs = 1200) {
        bootLogf(UI_GREEN, 100, "[SYS ] Rover operacional!");
        if (delayMs > 0) delay(delayMs);
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
                int16_t lastRssi, bool failsafeActive, bool force = false, bool sdOk = false,
                bool isApMode = false, const char* ip = nullptr, const char* ssid = nullptr,
                int8_t wifiRssi = 0, uint8_t apClients = 0, const char* apPassword = nullptr) {
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
        canvas->setCursor(112, 4);
        if (currentPage == ROVER_PAGE_NAV) {
            canvas->print("NAV/GPS");
        } else if (currentPage == ROVER_PAGE_POWER) {
            canvas->print("BATERIA");
        } else if (currentPage == ROVER_PAGE_ACTUATORS) {
            canvas->print("ATUADOR");
        } else {
            canvas->print("WIFI/AP");
        }

        // MicroSD status indicator dot (Cyan = Ready, Dark Grey = Not present)
        canvas->fillCircle(250, 12, 4, sdOk ? UI_CYAN : UI_DARKGREY);

        // Page tabs indicators (4 dots at top right)
        for (int i = 0; i < ROVER_PAGE_COUNT; ++i) {
            uint16_t dotCol = (i == currentPage) ? UI_YELLOW : UI_DARKGREY;
            canvas->fillCircle(268 + i * 12, 12, 4, dotCol);
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
            case ROVER_PAGE_NETWORK:
                renderNetworkPage(isApMode, ip, ssid, wifiRssi, apClients, apPassword);
                break;
            default:
                break;
        }

        // 3. Bottom Footer Bar (Height: 22px)
        canvas->fillRect(0, 150, SCREEN_W, 22, UI_NAVY);
        canvas->setTextColor(UI_YELLOW);
        canvas->setTextSize(1);
        canvas->setCursor(10, 156);
        canvas->printf("PAG %d/4 | CLIQUE [BOOT] P/ MUDAR PAGINA", currentPage + 1);

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

    void renderNetworkPage(bool isApMode, const char* ip, const char* ssid, int8_t wifiRssi, uint8_t apClients, const char* apPassword = nullptr) {
        int cardY = 26;
        int cardH = 122;

        // --- Card de Largura Total (x=4..316, w=312, h=122) ---
        canvas->fillRect(4, cardY, 312, cardH, UI_PANEL_BG);
        canvas->drawRect(4, cardY, 312, cardH, UI_CARD_BG);

        if (!isApMode) {
            // ==========================================
            // MODO WIFI (STA CONECTADO À REDE)
            // ==========================================
            // Topo do card: Estado e Sinal RSSI
            canvas->setTextSize(1);
            canvas->setTextColor(UI_GREEN);
            canvas->setCursor(12, cardY + 6);
            canvas->print("ESTADO: WIFI CONECTADO");

            canvas->setTextColor(UI_WHITE);
            canvas->setCursor(205, cardY + 6);
            canvas->printf("SINAL: %d dBm", wifiRssi);

            canvas->drawFastHLine(8, cardY + 18, 304, UI_CARD_BG);

            // Rede SSID
            canvas->setTextSize(1);
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->setCursor(12, cardY + 25);
            canvas->print("REDE:");

            canvas->setTextSize(2);
            canvas->setTextColor(UI_WHITE);
            canvas->setCursor(60, cardY + 22);
            canvas->printf("%.18s", ssid ? ssid : "");

            // Endereço IP em TAMANHO GIGANTE (Size 3)
            canvas->setTextSize(1);
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->setCursor(12, cardY + 48);
            canvas->print("ENDERECO IP (ACESSO NO BROWSER):");

            canvas->setTextSize(3);
            canvas->setTextColor(UI_CYAN);
            canvas->setCursor(12, cardY + 62);
            canvas->print(ip ? ip : "0.0.0.0");

            // URL completa e páginas
            canvas->setTextSize(1);
            canvas->setTextColor(UI_YELLOW);
            canvas->setCursor(12, cardY + 98);
            canvas->printf("http://%s", ip ? ip : "");

            canvas->setTextColor(UI_LIGHTGREY);
            canvas->setCursor(170, cardY + 98);
            canvas->print("Web: /wifi, /gps, /control");

        } else {
            // ==========================================
            // MODO AP (PONTO DE ACESSO / HOTSPOT)
            // ==========================================
            // Topo do card: Estado e Clientes
            canvas->setTextSize(1);
            canvas->setTextColor(UI_YELLOW);
            canvas->setCursor(12, cardY + 6);
            canvas->print("ESTADO: MODO AP (HOTSPOT)");

            canvas->setTextColor(UI_LIGHTGREY);
            canvas->setCursor(205, cardY + 6);
            canvas->printf("CLIENTES: %u", apClients);

            canvas->drawFastHLine(8, cardY + 18, 304, UI_CARD_BG);

            // SSID da Rede (Size 2)
            canvas->setTextSize(1);
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->setCursor(12, cardY + 25);
            canvas->print("SSID:");

            canvas->setTextSize(2);
            canvas->setTextColor(UI_YELLOW);
            canvas->setCursor(55, cardY + 22);
            canvas->printf("%.21s", ssid ? ssid : "ROVER-01-AP");

            // Password em TAMANHO GRANDE (Size 2)
            canvas->setTextSize(1);
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->setCursor(12, cardY + 48);
            canvas->print("PASS:");

            canvas->setTextSize(2);
            canvas->setTextColor(UI_WHITE);
            canvas->setCursor(55, cardY + 45);
            canvas->printf("%.21s", (apPassword && strlen(apPassword) > 0) ? apPassword : "password123");

            // Endereço IP em TAMANHO GIGANTE (Size 3)
            canvas->setTextSize(1);
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->setCursor(12, cardY + 70);
            canvas->print("ENDERECO IP:");

            canvas->setTextSize(3);
            canvas->setTextColor(UI_CYAN);
            canvas->setCursor(12, cardY + 84);
            canvas->print(ip ? ip : "192.168.4.1");
        }
    }

    ST7789Display display;
    Arduino_Canvas *canvas;
    RoverScreenPage currentPage;
    uint32_t lastRender;
    BootLogItem _bootLogs[BOOT_MAX_LINES];
    uint8_t _bootLogCount;
    uint8_t _bootProgress;
};
