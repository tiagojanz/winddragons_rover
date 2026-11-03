#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <canvas/Arduino_Canvas.h>
#include "Display_ST7789.h"
#include "config_common.h"
#include "rover_version.h"
#include "lora_protocol.h"
#include "gps_tracker.h"
#include "battery_monitor.h"
#include "actuators.h"
#include "logo_bmp.h"
#include "qrcode_helper.h"
#include "wifi_config_manager.h"

// Colors (RGB565)
#define UI_BLACK     0x0000
#define UI_WHITE     0xFFFF
#define UI_RED       0xF800
#define UI_GREEN     0x07E0
#define UI_BLUE      0x001F
#define UI_CYAN      0x07FF
#define UI_YELLOW    0xFFE0
#define UI_ORANGE    0xFD20
#define UI_MAGENTA   0xF81F
#define UI_PURPLE    0x780F
#define UI_LIGHTGREY 0xC618
#define UI_DARKGREY  0x4208
#define UI_NAVY      0x0841
#define UI_CARD_BG   0x18E3
#define UI_PANEL_BG  0x10A2

enum RoverScreenPage : uint8_t {
    ROVER_PAGE_HOME             = 0,  // Home Menu (Logótipo à esquerda + Operação / Configurações à direita)
    ROVER_PAGE_OP_MENU          = 1,  // Menu Operação: Topo Estado + itens (Navegação, Bateria, Atuadores)
    ROVER_PAGE_NAV              = 2,  // Operação -> Navegação (GPS, Velocidade, Proa/BNO055, Coordenadas)
    ROVER_PAGE_POWER            = 3,  // Operação -> Bateria (LiPo %, Tensão, V/célula, Corrente, Consumo)
    ROVER_PAGE_ACTUATORS        = 4,  // Operação -> Atuadores (Motor ESC, Leme, Cremalheira, Âncora, LoRa)
    ROVER_PAGE_CONFIG_MENU      = 5,  // Menu Configurações: WiFi, ID, Firmware, LoRa
    ROVER_PAGE_CONFIG_WIFI      = 6,  // Configurações -> WiFi (Estado + item "Lista")
    ROVER_PAGE_CONFIG_WIFI_LIST = 7,  // Configurações -> WiFi -> Lista (Scan + Redes Guardadas)
    ROVER_PAGE_CONFIG_WIFI_SCAN = 8,  // Configurações -> WiFi -> Lista -> Scan de Redes
    ROVER_PAGE_CONFIG_WIFI_PASS = 9,  // Configurações -> WiFi -> Scan -> Password QWERTY
    ROVER_PAGE_CONFIG_ID        = 10, // Configurações -> ID (QRCode + Nome/ID + MAC Address)
    ROVER_PAGE_CONFIG_FW        = 11, // Configurações -> Firmware (Master Lafvin + Coprocessador C6)
    ROVER_PAGE_CONFIG_LORA      = 12, // Configurações -> LoRa (Diagnóstico Rádio 433MHz)
    ROVER_PAGE_COUNT            = 13,

    // Aliases de compatibilidade
    ROVER_PAGE_NETWORK          = ROVER_PAGE_CONFIG_WIFI,
    ROVER_PAGE_MAC              = ROVER_PAGE_CONFIG_ID
};

struct BootLogItem {
    char text[34];
    uint16_t color;
};

static const uint8_t BOOT_MAX_LINES = 24;
static const uint8_t BOOT_VISIBLE_LINES = 13;

class RoverDisplay {
public:
    RoverDisplay()
        : canvas(nullptr),
          currentPage(ROVER_PAGE_HOME),
          lastRender(0),
          _bootLogCount(0),
          _bootProgress(0),
          _brightness(85),
          _roverId(1),
          _otaActive(false),
          _homeMenuIdx(0),
          _opMenuIdx(0),
          _configMenuIdx(0),
          _wifiListIdx(0),
          _wifiScanIdx(0),
          _wifiKbRow(0),
          _wifiKbCol(0),
          _wifiKbMode(0),
          _wifiTextCursorPos(0),
          _wifiConfig(nullptr),
          _feedbackUntilMs(0),
          _loraTxTelems(0),
          _loraRxPolls(0),
          _loraLastRxSecAgo(999999),
          _loraSnr(0.0f),
          _loraHasContact(false) {
        strncpy(_c6Version, "--", sizeof(_c6Version));
        _wifiTargetSsid[0] = '\0';
        _wifiPassBuf[0] = '\0';
        _feedbackMsg[0] = '\0';
    }

    void begin(uint8_t initialBrightness = 85) {
        // Disable onboard TF/MicroSD card CS (GPIO 4) to prevent SPI bus collision
        pinMode(PIN_SD_CS, OUTPUT);
        digitalWrite(PIN_SD_CS, HIGH);

        // Initialize manufacturer ST7789 display driver
        display.begin();
        setBrightness(initialBrightness);

        // Create high-speed in-memory canvas configured in Landscape (320x172)
        canvas = new Arduino_Canvas(LCD_WIDTH, LCD_HEIGHT, nullptr, 0, 0, DISPLAY_ROTATION);
        if (canvas) {
            canvas->begin();
            canvas->fillScreen(UI_BLACK);
            display.drawPixelBuffer(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, canvas->getFramebuffer());
        }
    }

    void setBrightness(uint8_t brightness) {
        if (brightness > 100) brightness = 100;
        _brightness = brightness;
        display.setBacklight(_brightness);
    }

    uint8_t getBrightness() const {
        return _brightness;
    }

    void setWiFiConfigManager(WiFiConfigManager *wcm) {
        _wifiConfig = wcm;
    }

    void setLoraStats(uint32_t txTelems, uint32_t rxPolls, uint32_t lastRxSecAgo, float snr, bool hasContact) {
        _loraTxTelems = txTelems;
        _loraRxPolls = rxPolls;
        _loraLastRxSecAgo = lastRxSecAgo;
        _loraSnr = snr;
        _loraHasContact = hasContact;
    }

    // =========================================================================
    // BOOT SCREEN (ARRANQUE) - LANDSCAPE 320x172
    // =========================================================================
    void showBootScreen(uint8_t roverId = 1) {
        if (!canvas) return;
        _roverId = roverId;
        _bootLogCount = 0;
        _bootProgress = 0;

        canvas->fillScreen(UI_BLACK);

        // Painel Esquerdo: Logótipo + Marca + ID + Barra de Progresso (w=101, h=164)
        renderBrandLeftPanel(true, roverId, MOTOR_IDLE);

        // Painel Direito: Consola de Arranque (Scroll vertical, w=208, h=164)
        canvas->fillRect(108, 4, 208, 164, UI_PANEL_BG);
        canvas->drawRect(108, 4, 208, 164, UI_CARD_BG);

        // Barra de Título da Consola
        canvas->fillRect(109, 5, 206, 17, UI_NAVY);
        canvas->drawFastHLine(109, 22, 206, UI_CARD_BG);

        canvas->setTextSize(1);
        canvas->setTextColor(UI_YELLOW);
        canvas->setCursor(115, 9);
        canvas->print("CONSOLA DE ARRANQUE");

        canvas->setTextColor(UI_CYAN);
        canvas->setCursor(270, 9);
        canvas->print("[POST]");

        display.drawPixelBuffer(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, canvas->getFramebuffer());
    }

    void bootLog(const char* msg, uint16_t color = UI_LIGHTGREY, uint8_t progressPct = 0) {
        if (!canvas) return;

        if (_bootLogCount < BOOT_MAX_LINES) {
            strncpy(_bootLogs[_bootLogCount].text, msg, sizeof(_bootLogs[_bootLogCount].text) - 1);
            _bootLogs[_bootLogCount].text[sizeof(_bootLogs[_bootLogCount].text) - 1] = '\0';
            _bootLogs[_bootLogCount].color = color;
            _bootLogCount++;
        } else {
            memmove(&_bootLogs[0], &_bootLogs[1], sizeof(BootLogItem) * (BOOT_MAX_LINES - 1));
            strncpy(_bootLogs[BOOT_MAX_LINES - 1].text, msg, sizeof(_bootLogs[BOOT_MAX_LINES - 1].text) - 1);
            _bootLogs[BOOT_MAX_LINES - 1].text[sizeof(_bootLogs[BOOT_MAX_LINES - 1].text) - 1] = '\0';
            _bootLogs[BOOT_MAX_LINES - 1].color = color;
        }

        if (progressPct > 0) {
            _bootProgress = constrain(progressPct, 0, 100);
            int barW = map(_bootProgress, 0, 100, 0, 81);
            canvas->fillRect(14, 152, 81, 6, UI_BLACK);
            if (barW > 0) {
                uint16_t barColor = (_bootProgress == 100) ? UI_GREEN : UI_CYAN;
                canvas->fillRect(14, 152, barW, 6, barColor);
            }
        }

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
        currentPage = ROVER_PAGE_HOME;
        lastRender = 0;
    }

    // =========================================================================
    // CONTROLO DE PÁGINAS E CURSORES DE MENU
    // =========================================================================
    void setPage(RoverScreenPage page) {
        if (page < ROVER_PAGE_COUNT) {
            currentPage = page;
        } else {
            currentPage = ROVER_PAGE_HOME;
        }
        lastRender = 0;
    }

    RoverScreenPage getCurrentPage() const {
        return currentPage;
    }

    void nextPage() {
        currentPage = static_cast<RoverScreenPage>((currentPage + 1) % ROVER_PAGE_COUNT);
        lastRender = 0;
    }

    void prevPage() {
        currentPage = static_cast<RoverScreenPage>((currentPage + ROVER_PAGE_COUNT - 1) % ROVER_PAGE_COUNT);
        lastRender = 0;
    }

    uint8_t getHomeMenuIdx() const { return _homeMenuIdx; }
    void setHomeMenuIdx(uint8_t idx) {
        _homeMenuIdx = idx % 2;
        lastRender = 0;
    }

    uint8_t getOpMenuIdx() const { return _opMenuIdx; }
    void setOpMenuIdx(uint8_t idx) {
        _opMenuIdx = idx % 3;
        lastRender = 0;
    }

    uint8_t getConfigMenuIdx() const { return _configMenuIdx; }
    void setConfigMenuIdx(uint8_t idx) {
        _configMenuIdx = idx % 4;
        lastRender = 0;
    }

    uint8_t getWifiListIdx() const { return _wifiListIdx; }
    void setWifiListIdx(uint8_t idx, size_t total) {
        _wifiListIdx = (total > 0) ? (idx % total) : 0;
        lastRender = 0;
    }

    uint8_t getWifiScanIdx() const { return _wifiScanIdx; }
    void setWifiScanIdx(uint8_t idx, size_t total) {
        _wifiScanIdx = (total > 0) ? (idx % total) : 0;
        lastRender = 0;
    }

    // --- Controlo do Teclado QWERTY para Password WiFi ---
    void beginWifiPasswordEntry(const char *ssid) {
        if (ssid) {
            strncpy(_wifiTargetSsid, ssid, sizeof(_wifiTargetSsid) - 1);
            _wifiTargetSsid[sizeof(_wifiTargetSsid) - 1] = '\0';
        } else {
            _wifiTargetSsid[0] = '\0';
        }
        _wifiPassBuf[0] = '\0';
        _wifiTextCursorPos = 0;
        _wifiKbRow = 0;
        _wifiKbCol = 0;
        _wifiKbMode = 0;
        lastRender = 0;
    }

    const char *getWifiTargetSsid() const { return _wifiTargetSsid; }
    const char *getWifiPassBuffer() const { return _wifiPassBuf; }

    void moveWifiKbCursor(int8_t dx, int8_t dy) {
        size_t len = strlen(_wifiPassBuf);
        if (_wifiTextCursorPos > len) _wifiTextCursorPos = (uint8_t)len;

        if (dy != 0) {
            uint8_t oldRow = _wifiKbRow;
            _wifiKbRow = (uint8_t)((_wifiKbRow + 5 + dy) % 5);
            if (_wifiKbRow == 4) {
                // Entrou na caixa de texto no topo
            } else if (oldRow == 4) {
                _wifiKbCol = (_wifiKbRow == 3) ? 5 : 0;
            } else if (_wifiKbRow == 3 && oldRow < 3) {
                const uint8_t mapTo3[10] = {0, 0, 1, 1, 2, 2, 3, 4, 5, 5};
                _wifiKbCol = mapTo3[_wifiKbCol % 10];
            } else if (_wifiKbRow < 3 && oldRow == 3) {
                const uint8_t mapFrom3[6] = {0, 2, 4, 6, 7, 9};
                _wifiKbCol = mapFrom3[_wifiKbCol % 6];
            }
        }
        if (dx != 0) {
            if (_wifiKbRow == 4) {
                int16_t nextPos = (int16_t)_wifiTextCursorPos + dx;
                if (nextPos < 0) nextPos = 0;
                if (nextPos > (int16_t)len) nextPos = (int16_t)len;
                _wifiTextCursorPos = (uint8_t)nextPos;
            } else {
                uint8_t cols = (_wifiKbRow == 3) ? 6 : 10;
                _wifiKbCol = (uint8_t)((_wifiKbCol + cols + dx) % cols);
            }
        }
        lastRender = 0;
    }

    void backspaceWifiPass() {
        size_t len = strlen(_wifiPassBuf);
        if (_wifiTextCursorPos > len) _wifiTextCursorPos = (uint8_t)len;
        if (_wifiTextCursorPos > 0 && len > 0) {
            memmove(&_wifiPassBuf[_wifiTextCursorPos - 1],
                    &_wifiPassBuf[_wifiTextCursorPos],
                    len - _wifiTextCursorPos + 1);
            _wifiTextCursorPos--;
            lastRender = 0;
        }
    }

    int8_t pressWifiKbKey() {
        static const char *const kbRows[3][3] = {
            {"qwertyuiop", "asdfghjkl-", "zxcvbnm._@"},
            {"QWERTYUIOP", "ASDFGHJKL-", "ZXCVBNM._@"},
            {"1234567890", "!@#$%^&*()", "-_=+/?:;,.~"}};

        size_t len = strlen(_wifiPassBuf);
        if (_wifiTextCursorPos > len) _wifiTextCursorPos = (uint8_t)len;

        if (_wifiKbRow == 4) {
            _wifiKbRow = 0;
            lastRender = 0;
            return 0;
        }

        if (_wifiKbRow < 3) {
            if (len < sizeof(_wifiPassBuf) - 1) {
                char c = kbRows[_wifiKbMode % 3][_wifiKbRow][_wifiKbCol % 10];
                memmove(&_wifiPassBuf[_wifiTextCursorPos + 1],
                        &_wifiPassBuf[_wifiTextCursorPos],
                        len - _wifiTextCursorPos + 1);
                _wifiPassBuf[_wifiTextCursorPos] = c;
                _wifiTextCursorPos++;
            }
            lastRender = 0;
            return 0;
        }

        switch (_wifiKbCol % 6) {
        case 0: // ABC / abc
            _wifiKbMode = (_wifiKbMode == 1) ? 0 : 1;
            lastRender = 0;
            return 0;
        case 1: // 123# / ABC
            _wifiKbMode = (_wifiKbMode == 2) ? 0 : 2;
            lastRender = 0;
            return 0;
        case 2: // ESP
            if (len < sizeof(_wifiPassBuf) - 1) {
                memmove(&_wifiPassBuf[_wifiTextCursorPos + 1],
                        &_wifiPassBuf[_wifiTextCursorPos],
                        len - _wifiTextCursorPos + 1);
                _wifiPassBuf[_wifiTextCursorPos] = ' ';
                _wifiTextCursorPos++;
            }
            lastRender = 0;
            return 0;
        case 3: // <-DEL
            backspaceWifiPass();
            return 0;
        case 4: // SAIR
            return -1;
        case 5: // LIGAR
            return 1;
        }
        return 0;
    }

    void showFeedback(const char* msg, uint32_t durationMs = 1500) {
        if (msg) {
            strncpy(_feedbackMsg, msg, sizeof(_feedbackMsg) - 1);
            _feedbackMsg[sizeof(_feedbackMsg) - 1] = '\0';
            _feedbackUntilMs = millis() + durationMs;
        } else {
            _feedbackMsg[0] = '\0';
            _feedbackUntilMs = 0;
        }
        lastRender = 0;
    }

    // =========================================================================
    // ATUALIZAÇÃO GERAL DO ECRÃ EM RUNTIME
    // =========================================================================
    void update(uint8_t roverId, GPSTracker &gps, BatteryMonitor &batt,
                ActuatorController &actuators, uint8_t motorStatus,
                int16_t lastRssi, bool failsafeActive, bool force = false, bool sdOk = false,
                bool isApMode = false, const char* ip = nullptr, const char* ssid = nullptr,
                int8_t wifiRssi = 0, uint8_t apClients = 0, const char* apPassword = nullptr) {
        _roverId = roverId;
        if (_otaActive) return;

        uint32_t now = millis();
        if (_feedbackUntilMs > 0 && now > _feedbackUntilMs) {
            _feedbackMsg[0] = '\0';
            _feedbackUntilMs = 0;
            force = true;
        }

        if (!force && (now - lastRender < 180)) return;
        lastRender = now;

        if (!canvas) return;

        canvas->fillScreen(UI_BLACK);

        if (currentPage == ROVER_PAGE_HOME) {
            renderHomePage(roverId, gps, batt, motorStatus, lastRssi, failsafeActive, sdOk, isApMode);
        } else {
            renderHeader(roverId, motorStatus, failsafeActive, sdOk, isApMode);
            switch (currentPage) {
            case ROVER_PAGE_OP_MENU:
                renderOpMenuPage(gps, batt, actuators, motorStatus, lastRssi, failsafeActive);
                break;
            case ROVER_PAGE_NAV:
                renderNavPage(gps, motorStatus);
                break;
            case ROVER_PAGE_POWER:
                renderPowerPage(batt);
                break;
            case ROVER_PAGE_ACTUATORS:
                renderActuatorsPage(actuators, motorStatus, lastRssi, failsafeActive);
                break;
            case ROVER_PAGE_CONFIG_MENU:
                renderConfigMenuPage(roverId, isApMode, ip, lastRssi, failsafeActive);
                break;
            case ROVER_PAGE_CONFIG_WIFI:
                renderNetworkPage(isApMode, ip, ssid, wifiRssi, apClients, apPassword);
                break;
            case ROVER_PAGE_CONFIG_WIFI_LIST:
                renderWifiListPage();
                break;
            case ROVER_PAGE_CONFIG_WIFI_SCAN:
                renderWifiScanPage();
                break;
            case ROVER_PAGE_CONFIG_WIFI_PASS:
                renderWifiPassPage("LIGAR");
                break;
            case ROVER_PAGE_CONFIG_ID:
                renderMacPage(roverId);
                break;
            case ROVER_PAGE_CONFIG_FW:
                renderFirmwarePage();
                break;
            case ROVER_PAGE_CONFIG_LORA:
                renderLoraPage(lastRssi, failsafeActive);
                break;
            default:
                renderHomePage(roverId, gps, batt, motorStatus, lastRssi, failsafeActive, sdOk, isApMode);
                break;
            }
        }

        display.drawPixelBuffer(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, canvas->getFramebuffer());
    }

    void showWifiScanningScreen() {
        if (!canvas) return;
        canvas->fillScreen(UI_BLACK);
        canvas->fillRect(0, 0, SCREEN_W, 24, UI_NAVY);
        canvas->setTextSize(1);
        canvas->setTextColor(UI_CYAN);
        canvas->setCursor(8, 8);
        canvas->print("WIFI > SCAN DE REDES");

        canvas->fillRect(16, 46, 288, 82, UI_PANEL_BG);
        canvas->drawRect(16, 46, 288, 82, UI_CYAN);

        canvas->setTextSize(2);
        canvas->setTextColor(UI_YELLOW);
        canvas->setCursor(32, 66);
        canvas->print("A PROCURAR REDES...");

        canvas->setTextSize(1);
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(32, 98);
        canvas->print("A varrer canais WiFi 2.4GHz disponiveis...");

        renderFooter("Aguarde pelo fim do varrimento...");
        display.drawPixelBuffer(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, canvas->getFramebuffer());
    }

    void showWifiConnectingScreen(const char *targetSsid) {
        if (!canvas) return;
        canvas->fillScreen(UI_BLACK);
        canvas->fillRect(0, 0, SCREEN_W, 24, UI_NAVY);
        canvas->setTextSize(1);
        canvas->setTextColor(UI_CYAN);
        canvas->setCursor(8, 8);
        canvas->print("WIFI > A CONECTAR");

        canvas->fillRect(16, 42, 288, 90, UI_PANEL_BG);
        canvas->drawRect(16, 42, 288, 90, UI_CYAN);

        canvas->setTextSize(1);
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(32, 58);
        canvas->print("A LIGAR A REDE WIFI:");

        canvas->setTextSize(2);
        canvas->setTextColor(UI_YELLOW);
        canvas->setCursor(32, 76);
        canvas->printf("%.20s", targetSsid ? targetSsid : "");

        canvas->setTextSize(1);
        canvas->setTextColor(UI_CYAN);
        canvas->setCursor(32, 108);
        canvas->print("Aguarde (ate 8 segundos)...");

        renderFooter("A estabelecer ligacao WiFi...");
        display.drawPixelBuffer(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, canvas->getFramebuffer());
    }

    void showOtaScreen(const char* targetModule, const char* stepTitle, uint8_t progressPct, const char* detailMsg, uint16_t accentColor = UI_CYAN) {
        if (!canvas) return;
        _otaActive = true;

        pinMode(PIN_SD_CS, OUTPUT);
        digitalWrite(PIN_SD_CS, HIGH);

        canvas->fillScreen(UI_BLACK);

        // 1. Top Header Bar (Height: 24px)
        canvas->fillRect(0, 0, SCREEN_W, 24, UI_NAVY);
        canvas->setTextColor(UI_WHITE);
        canvas->setTextSize(2);
        canvas->setCursor(8, 4);
        canvas->printf("ROVER-%02d", _roverId);

        canvas->setTextSize(2);
        canvas->setTextColor(accentColor);
        canvas->setCursor(112, 4);
        canvas->print("MODO OTA");

        static bool pulseDot = false;
        pulseDot = !pulseDot;
        canvas->fillCircle(304, 12, 5, pulseDot ? accentColor : UI_DARKGREY);

        // 2. Card Superior: Alvo e Passo (x=4..316, w=312, h=50)
        int card1Y = 28;
        int card1H = 50;
        canvas->fillRect(4, card1Y, 312, card1H, UI_PANEL_BG);
        canvas->drawRect(4, card1Y, 312, card1H, UI_CARD_BG);

        canvas->setTextSize(1);
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(10, card1Y + 6);
        canvas->print("ALVO EM ATUALIZACAO:");

        canvas->setTextSize(2);
        canvas->setTextColor(accentColor);
        canvas->setCursor(10, card1Y + 18);
        canvas->printf("%s", targetModule ? targetModule : "SISTEMA ROVER");

        canvas->setTextSize(1);
        canvas->setTextColor(UI_YELLOW);
        canvas->setCursor(10, card1Y + 36);
        canvas->printf("%s", stepTitle ? stepTitle : "A processar...");

        // 3. Card Inferior: Barra de Progresso e Detalhes (x=4..316, w=312, h=64)
        int card2Y = 82;
        int card2H = 64;
        canvas->fillRect(4, card2Y, 312, card2H, UI_PANEL_BG);
        canvas->drawRect(4, card2Y, 312, card2H, UI_CARD_BG);

        canvas->setTextSize(1);
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(10, card2Y + 8);
        canvas->print("PROGRESSO:");

        canvas->setTextSize(2);
        canvas->setTextColor(UI_WHITE);
        canvas->setCursor(256, card2Y + 6);
        canvas->printf("%3d%%", progressPct > 100 ? 100 : progressPct);

        int barX = 10;
        int barY = card2Y + 24;
        int barW = 292;
        int barH = 14;
        canvas->drawRect(barX, barY, barW, barH, UI_CARD_BG);
        canvas->fillRect(barX + 2, barY + 2, barW - 4, barH - 4, UI_BLACK);

        int fillW = map(constrain((int)progressPct, 0, 100), 0, 100, 0, barW - 4);
        if (fillW > 0) {
            uint16_t barCol = (progressPct >= 100) ? UI_GREEN : accentColor;
            canvas->fillRect(barX + 2, barY + 2, fillW, barH - 4, barCol);
        }

        canvas->setTextSize(1);
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(10, card2Y + 46);
        if (detailMsg && strlen(detailMsg) > 0) {
            canvas->printf("%.44s", detailMsg);
        } else {
            canvas->printf("Master: %s | C6: %s", ROVER_LAFVIN_VERSION, _c6Version);
        }

        // 4. Footer Inferior de Aviso (h=22)
        canvas->fillRect(0, 150, SCREEN_W, 22, (accentColor == UI_RED) ? UI_RED : UI_NAVY);
        canvas->setTextSize(1);
        if (accentColor == UI_RED) {
            canvas->setTextColor(UI_WHITE);
            canvas->setCursor(12, 156);
            canvas->print("! ERRO NO OTA - VERIFICAR LOGS DA CONSOLA !");
        } else if (progressPct >= 100) {
            canvas->setTextColor(UI_GREEN);
            canvas->setCursor(12, 156);
            canvas->print("* CONCLUIDO COM SUCESSO! A REINICIAR ROVER *");
        } else {
            canvas->setTextColor(UI_YELLOW);
            canvas->setCursor(10, 156);
            canvas->print("! ATENCAO: NAO DESLIGAR A ALIMENTACAO !");
        }

        display.drawPixelBuffer(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, canvas->getFramebuffer());
    }

    void setOtaActive(bool active) {
        _otaActive = active;
        if (!_otaActive) {
            lastRender = 0;
        }
    }

    bool isOtaActive() const {
        return _otaActive;
    }

    void setC6Version(const char* ver) {
        if (ver && strlen(ver) > 0) {
            strncpy(_c6Version, ver, sizeof(_c6Version) - 1);
            _c6Version[sizeof(_c6Version) - 1] = '\0';
        }
    }

    const char* getC6Version() const {
        return _c6Version;
    }

private:
    const char *getActiveFeedback() const {
        if (_feedbackUntilMs > 0 && millis() <= _feedbackUntilMs && _feedbackMsg[0] != '\0') {
            return _feedbackMsg;
        }
        return nullptr;
    }

    uint16_t getMotorStatusColor(uint8_t motorStatus) const {
        switch (motorStatus) {
        case MOTOR_IDLE:              return UI_CYAN;
        case MOTOR_MANUAL:            return UI_GREEN;
        case MOTOR_HOLDING_STATION:   return UI_MAGENTA;
        case MOTOR_ANCHORED:          return UI_ORANGE;
        case MOTOR_MOVING_TO_TARGET:  return UI_BLUE;
        case MOTOR_RTL:               return UI_YELLOW;
        case MOTOR_FAILSAFE:          return UI_RED;
        default:                      return UI_LIGHTGREY;
        }
    }

    const char *getMotorStatusBadge(uint8_t motorStatus) const {
        switch (motorStatus) {
        case MOTOR_IDLE:              return "IDLE";
        case MOTOR_MANUAL:            return "MANU";
        case MOTOR_HOLDING_STATION:   return "HOLD";
        case MOTOR_ANCHORED:          return "ANCH";
        case MOTOR_MOVING_TO_TARGET:  return "GOTO";
        case MOTOR_RTL:               return "RTL";
        case MOTOR_FAILSAFE:          return "FAIL";
        default:                      return "IDLE";
        }
    }

    void renderBrandLeftPanel(bool isBoot, uint8_t roverId, uint8_t motorStatus) {
        canvas->fillRect(4, 4, 101, 164, UI_PANEL_BG);
        canvas->drawRect(4, 4, 101, 164, UI_CARD_BG);

        // Logótipo Dragão centrado (85x80) em Ciano
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

        // Subtítulo do sistema
        canvas->setTextSize(1);
        canvas->setTextColor(UI_CYAN);
        canvas->setCursor(16, 126);
        canvas->print("AUTONOMOUS");
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(22, 136);
        canvas->print("BOIA v2");

        if (isBoot) {
            canvas->drawRect(12, 150, 85, 10, UI_CARD_BG);
            canvas->fillRect(14, 152, 81, 6, UI_BLACK);
        } else {
            uint16_t stCol = getMotorStatusColor(motorStatus);
            canvas->fillRect(10, 148, 89, 15, UI_BLACK);
            canvas->drawRect(10, 148, 89, 15, stCol);
            canvas->setTextSize(1);
            canvas->setTextColor(stCol);
            const char *stStr = get_motor_status_str(motorStatus);
            int textW = strlen(stStr) * 6;
            int tx = 10 + max(2, (89 - textW) / 2);
            canvas->setCursor(tx, 152);
            canvas->printf("%.14s", stStr);
        }
    }

    void renderFooter(const char *defaultHint) {
        canvas->fillRect(0, 150, SCREEN_W, 22, UI_NAVY);
        canvas->setTextSize(1);
        const char *fb = getActiveFeedback();
        if (fb) {
            canvas->setTextColor(UI_GREEN);
            canvas->setCursor(8, 157);
            canvas->printf(">> %s", fb);
        } else {
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->setCursor(8, 157);
            canvas->print(defaultHint);
        }
    }

    // =========================================================================
    // HOME MENU (PAINEL ESQUERDO IGUAL AO BOOT + ITENS DE MENU À DIREITA)
    // =========================================================================
    void renderHomePage(uint8_t roverId, GPSTracker &gps, BatteryMonitor &batt,
                        uint8_t motorStatus, int16_t lastRssi, bool failsafeActive,
                        bool sdOk, bool isApMode) {
        (void)lastRssi;
        // 1. Painel Esquerdo com o Logótipo (idêntico ao Boot)
        renderBrandLeftPanel(false, roverId, motorStatus);

        // 2. Painel Direito: Menu Principal (w=208, h=164)
        canvas->fillRect(108, 4, 208, 164, UI_PANEL_BG);
        canvas->drawRect(108, 4, 208, 164, UI_CARD_BG);

        // Barra de Título do Menu
        canvas->fillRect(109, 5, 206, 18, UI_NAVY);
        canvas->drawFastHLine(109, 23, 206, UI_CARD_BG);

        canvas->setTextSize(1);
        canvas->setTextColor(UI_YELLOW);
        canvas->setCursor(115, 10);
        canvas->print("HOME MENU");

        // Indicadores de Estado no topo direito (SD, WiFi/AP, LoRa)
        canvas->fillCircle(272, 14, 4, sdOk ? UI_CYAN : UI_DARKGREY);
        canvas->fillCircle(286, 14, 4, isApMode ? UI_ORANGE : UI_GREEN);
        uint16_t loraDot = failsafeActive ? UI_RED : (_loraHasContact ? UI_GREEN : UI_DARKGREY);
        canvas->fillCircle(300, 14, 4, loraDot);

        // Item 0: Operação
        {
            bool sel = (_homeMenuIdx == 0);
            int x = 114, y = 30, w = 196, h = 54;
            canvas->fillRect(x, y, w, h, sel ? UI_NAVY : UI_CARD_BG);
            canvas->drawRect(x, y, w, h, sel ? UI_CYAN : UI_DARKGREY);
            if (sel) {
                canvas->fillRect(x, y, 4, h, UI_CYAN);
            }

            canvas->setTextSize(2);
            canvas->setTextColor(sel ? UI_YELLOW : UI_WHITE);
            canvas->setCursor(x + 10, y + 10);
            canvas->print("OPERACAO");

            canvas->setTextColor(sel ? UI_CYAN : UI_LIGHTGREY);
            canvas->setCursor(x + w - 18, y + 16);
            canvas->print(">");

            canvas->setTextSize(1);
            canvas->setTextColor(sel ? UI_WHITE : UI_LIGHTGREY);
            canvas->setCursor(x + 10, y + 34);
            if (gps.hasFix()) {
                canvas->printf("GPS: %uS | Bat: %u%% (%.1fV)",
                               gps.getSatellites(), batt.getPercentage(),
                               batt.getVoltageMv() / 1000.0f);
            } else {
                canvas->printf("GPS: Sem Fix | Bat: %u%%", batt.getPercentage());
            }
        }

        // Item 1: Configurações
        {
            bool sel = (_homeMenuIdx == 1);
            int x = 114, y = 90, w = 196, h = 54;
            canvas->fillRect(x, y, w, h, sel ? UI_NAVY : UI_CARD_BG);
            canvas->drawRect(x, y, w, h, sel ? UI_CYAN : UI_DARKGREY);
            if (sel) {
                canvas->fillRect(x, y, 4, h, UI_CYAN);
            }

            canvas->setTextSize(2);
            canvas->setTextColor(sel ? UI_YELLOW : UI_WHITE);
            canvas->setCursor(x + 10, y + 10);
            canvas->print("CONFIGURACOES");

            canvas->setTextColor(sel ? UI_CYAN : UI_LIGHTGREY);
            canvas->setCursor(x + w - 18, y + 16);
            canvas->print(">");

            canvas->setTextSize(1);
            canvas->setTextColor(sel ? UI_WHITE : UI_LIGHTGREY);
            canvas->setCursor(x + 10, y + 34);
            canvas->print("WiFi, ID, Firmware, LoRa");
        }

        // Rodapé do Painel Direito
        canvas->fillRect(109, 149, 206, 18, UI_NAVY);
        canvas->setTextSize(1);
        const char *fb = getActiveFeedback();
        if (fb) {
            canvas->setTextColor(UI_GREEN);
            canvas->setCursor(115, 154);
            canvas->print(fb);
        } else {
            canvas->setTextColor(UI_CYAN);
            canvas->setCursor(115, 154);
            canvas->print("Y/BOOT: Menu | X->/Hold: OK");
        }
    }

    // =========================================================================
    // CABEÇALHO GLOBAL PARA SUBMENUS E PÁGINAS (TOPO 24px)
    // =========================================================================
    void renderHeader(uint8_t roverId, uint8_t motorStatus, bool failsafeActive,
                      bool sdOk, bool isApMode) {
        (void)roverId;
        canvas->fillRect(0, 0, SCREEN_W, 24, UI_NAVY);
        canvas->setTextSize(1);

        canvas->setCursor(8, 8);
        canvas->setTextColor(UI_CYAN);
        switch (currentPage) {
        case ROVER_PAGE_OP_MENU:
            canvas->print("HOME > ");
            canvas->setTextColor(UI_YELLOW);
            canvas->print("OPERACAO");
            break;
        case ROVER_PAGE_NAV:
            canvas->print("OPERACAO > ");
            canvas->setTextColor(UI_YELLOW);
            canvas->print("NAVEGACAO");
            break;
        case ROVER_PAGE_POWER:
            canvas->print("OPERACAO > ");
            canvas->setTextColor(UI_YELLOW);
            canvas->print("BATERIA");
            break;
        case ROVER_PAGE_ACTUATORS:
            canvas->print("OPERACAO > ");
            canvas->setTextColor(UI_YELLOW);
            canvas->print("ATUADORES");
            break;
        case ROVER_PAGE_CONFIG_MENU:
            canvas->print("HOME > ");
            canvas->setTextColor(UI_YELLOW);
            canvas->print("CONFIGURACOES");
            break;
        case ROVER_PAGE_CONFIG_WIFI:
            canvas->print("CONFIG > ");
            canvas->setTextColor(UI_YELLOW);
            canvas->print("WIFI");
            break;
        case ROVER_PAGE_CONFIG_WIFI_LIST:
            canvas->print("WIFI > ");
            canvas->setTextColor(UI_YELLOW);
            canvas->print("LISTA");
            break;
        case ROVER_PAGE_CONFIG_WIFI_SCAN:
            canvas->print("WIFI > ");
            canvas->setTextColor(UI_YELLOW);
            canvas->print("SCAN REDES");
            break;
        case ROVER_PAGE_CONFIG_WIFI_PASS:
            canvas->print("WIFI > ");
            canvas->setTextColor(UI_YELLOW);
            canvas->printf("PASS: %.16s", _wifiTargetSsid);
            break;
        case ROVER_PAGE_CONFIG_ID:
            canvas->print("CONFIG > ");
            canvas->setTextColor(UI_YELLOW);
            canvas->print("ID (QRCODE)");
            break;
        case ROVER_PAGE_CONFIG_FW:
            canvas->print("CONFIG > ");
            canvas->setTextColor(UI_YELLOW);
            canvas->print("FIRMWARE");
            break;
        case ROVER_PAGE_CONFIG_LORA:
            canvas->print("CONFIG > ");
            canvas->setTextColor(UI_YELLOW);
            canvas->print("LORA 433MHZ");
            break;
        default:
            canvas->printf("ROVER-%02d", _roverId);
            break;
        }

        // Badge de Estado do Rover (IDLE, MANU, HOLD, ANCH, GOTO, RTL, FAIL)
        uint16_t stCol = getMotorStatusColor(motorStatus);
        const char *badge = getMotorStatusBadge(motorStatus);
        canvas->fillRect(226, 4, 34, 15, UI_BLACK);
        canvas->drawRect(226, 4, 34, 15, stCol);
        canvas->setTextSize(1);
        canvas->setTextColor(stCol);
        canvas->setCursor(231, 8);
        canvas->print(badge);

        // Ícones de Estado: SD, WiFi/AP, LoRa
        canvas->fillCircle(272, 12, 4, sdOk ? UI_CYAN : UI_DARKGREY);
        canvas->fillCircle(288, 12, 4, isApMode ? UI_ORANGE : UI_GREEN);
        uint16_t loraDot = failsafeActive ? UI_RED : (_loraHasContact ? UI_GREEN : UI_DARKGREY);
        canvas->fillCircle(304, 12, 4, loraDot);
    }

    // =========================================================================
    // SUBMENU: OPERAÇÃO (Topo: Estado do Rover em letra grande | Abaixo: itens de 1 linha)
    // =========================================================================
    void renderOpMenuPage(GPSTracker &gps, BatteryMonitor &batt,
                          ActuatorController &actuators, uint8_t motorStatus,
                          int16_t lastRssi, bool failsafeActive) {
        uint16_t stCol = getMotorStatusColor(motorStatus);

        // --- Topo da página: Modo Operacional em letra grande (x=6, y=26, w=308, h=36) ---
        canvas->fillRect(6, 26, 308, 36, UI_PANEL_BG);
        canvas->drawRect(6, 26, 308, 36, UI_CARD_BG);

        canvas->setTextSize(1);
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(12, 29);
        canvas->printf("ROVER-%02d | MODO OPERACIONAL:", _roverId);

        canvas->setTextSize(2);
        canvas->setTextColor(stCol);
        canvas->setCursor(12, 41);
        canvas->printf("%.16s", get_motor_status_str(motorStatus));

        canvas->setTextSize(1);
        canvas->setTextColor(failsafeActive ? UI_RED : (_loraHasContact ? UI_GREEN : UI_LIGHTGREY));
        canvas->setCursor(214, 46);
        if (failsafeActive) {
            canvas->print("LORA: FAILSAFE");
        } else if (_loraHasContact || lastRssi != 0) {
            canvas->printf("RF: %d dBm", lastRssi);
        } else {
            canvas->print("RF: A aguardar");
        }

        // --- Lista de 3 itens de 1 linha (Navegacao, Bateria, Atuadores) ---
        const char *titles[3] = {"Navegacao", "Bateria", "Atuadores"};
        for (uint8_t i = 0; i < 3; ++i) {
            bool sel = (_opMenuIdx == i);
            int x = 6;
            int y = 66 + i * 28;
            int w = 308;
            int h = 26;

            if (sel) {
                canvas->fillRect(x, y, w, h, UI_NAVY);
                canvas->drawRect(x, y, w, h, UI_CYAN);
                canvas->fillRect(x, y, 4, h, UI_CYAN);
            }

            canvas->setTextSize(2);
            canvas->setTextColor(sel ? UI_YELLOW : UI_WHITE);
            canvas->setCursor(x + 10, y + 5);
            canvas->print(titles[i]);

            canvas->setTextSize(1);
            if (i == 0) {
                canvas->setTextColor(gps.hasFix() ? UI_GREEN : UI_RED);
                canvas->setCursor(x + 140, y + 9);
                if (gps.hasFix()) {
                    canvas->printf("FIX (%uS) | %.1fkn | %.0fdeg",
                                   gps.getSatellites(), gps.getSpeedKnots(), gps.getHeading());
                } else {
                    canvas->printf("SEM FIX | Proa: %.0fdeg", gps.getHeading());
                }
            } else if (i == 1) {
                uint8_t pct = batt.getPercentage();
                uint16_t bCol = (pct < 25) ? UI_RED : ((pct < 50) ? UI_YELLOW : UI_GREEN);
                canvas->setTextColor(bCol);
                canvas->setCursor(x + 140, y + 9);
                canvas->printf("%u%% | %.2fV | %.1fA",
                               pct, batt.getVoltageMv() / 1000.0f, batt.getCurrentAmps());
            } else {
                canvas->setTextColor(UI_CYAN);
                canvas->setCursor(x + 140, y + 9);
                canvas->printf("T:%+d%% R:%+d%% | %.1fm",
                               actuators.getThrottle(), actuators.getRudder(),
                               actuators.getAnchorDepthCm() / 100.0f);
            }

            canvas->setTextSize(2);
            canvas->setTextColor(sel ? UI_CYAN : UI_DARKGREY);
            canvas->setCursor(x + w - 16, y + 5);
            canvas->print(">");
        }

        renderFooter("<-X: Home | Y/BOOT: Item | X->/Hold: Entrar");
    }

    // =========================================================================
    // OPERAÇÃO -> NAVEGAÇÃO (GPS & BÚSSOLA BNO055)
    // =========================================================================
    void renderNavPage(GPSTracker &gps, uint8_t motorStatus) {
        int cardY = 26;
        int cardH = 122;

        // --- Left Card: Status, Speed & Heading (x=4..158) ---
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
        canvas->print(gps.hasCompass() ? "PROA (BNO055):" : "RUMO DA PROA:");
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
        canvas->printf("%.12s", get_motor_status_str(motorStatus));

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

        renderFooter("<-X / Clique BOOT: Voltar a Operacao");
    }

    // =========================================================================
    // OPERAÇÃO -> BATERIA
    // =========================================================================
    void renderPowerPage(BatteryMonitor &batt) {
        int cardY = 26;
        int cardH = 122;

        // --- Left Card: Big Battery & Total Voltage (x=4..158) ---
        canvas->fillRect(4, cardY, 154, cardH, UI_PANEL_BG);
        canvas->drawRect(4, cardY, 154, cardH, UI_CARD_BG);

        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setTextSize(1);
        canvas->setCursor(10, cardY + 6);
        canvas->printf("BATERIA LIPO %uS:", batt.getCellCount());

        canvas->setCursor(10, cardY + 20);
        canvas->setTextSize(3);
        uint8_t pct = batt.getPercentage();
        canvas->setTextColor(pct < 25 ? UI_RED : (pct < 50 ? UI_YELLOW : UI_GREEN));
        canvas->printf("%u%%", pct);

        int barW = map(constrain(pct, 0, 100), 0, 100, 0, 134);
        canvas->drawRect(10, cardY + 52, 138, 12, UI_WHITE);
        uint16_t bCol = (pct < 25 ? UI_RED : (pct < 50 ? UI_YELLOW : UI_GREEN));
        canvas->fillRect(12, cardY + 54, barW, 8, bCol);

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
        canvas->printf("MEDIA P/ CELULA (%uS):", batt.getCellCount());
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

        renderFooter("<-X / Clique BOOT: Voltar a Operacao");
    }

    // =========================================================================
    // OPERAÇÃO -> ATUADORES
    // =========================================================================
    void renderActuatorsPage(ActuatorController &actuators, uint8_t motorStatus,
                             int16_t lastRssi, bool failsafeActive) {
        (void)motorStatus;
        int cardY = 26;
        int cardH = 122;

        // --- Left Card: Propulsion, Rudder & Rack (x=4..158) ---
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

        renderFooter("<-X / Clique BOOT: Voltar a Operacao");
    }

    // =========================================================================
    // SUBMENU: CONFIGURAÇÕES (WIFI, ID, FIRMWARE, LORA)
    // =========================================================================
    void renderConfigMenuPage(uint8_t roverId, bool isApMode, const char *ip,
                              int16_t lastRssi, bool failsafeActive) {
        (void)roverId;
        String mac = getDeviceMacAddress();

        const uint8_t total = 4;
        const char *titles[4] = {"WIFI", "ID", "FIRMWARE", "LORA"};

        for (uint8_t row = 0; row < total; ++row) {
            bool sel = (_configMenuIdx == row);
            int x = 6;
            int y = 27 + row * 30;
            int rowW = 308;
            int h = 28;

            if (sel) {
                canvas->fillRect(x, y, rowW, h, UI_NAVY);
                canvas->drawRect(x, y, rowW, h, UI_CYAN);
                canvas->fillRect(x, y, 4, h, UI_CYAN);
            }

            canvas->setTextSize(2);
            canvas->setTextColor(sel ? UI_YELLOW : UI_WHITE);
            canvas->setCursor(x + 10, y + 6);
            canvas->print(titles[row]);

            canvas->setTextSize(1);
            if (row == 0) {
                canvas->setTextColor(isApMode ? UI_ORANGE : UI_GREEN);
                canvas->setCursor(x + 124, y + 10);
                canvas->printf("%s | %s", isApMode ? "AP" : "STA", ip ? ip : "0.0.0.0");
            } else if (row == 1) {
                canvas->setTextColor(UI_CYAN);
                canvas->setCursor(x + 124, y + 10);
                canvas->printf("%s", mac.c_str());
            } else if (row == 2) {
                canvas->setTextColor(UI_LIGHTGREY);
                canvas->setCursor(x + 124, y + 10);
                canvas->printf("M:%s | C6:%s", ROVER_LAFVIN_VERSION, _c6Version);
            } else {
                canvas->setTextColor(failsafeActive ? UI_RED : (_loraHasContact ? UI_GREEN : UI_LIGHTGREY));
                canvas->setCursor(x + 124, y + 10);
                if (failsafeActive) {
                    canvas->print("FAILSAFE (Sem Sinal)");
                } else if (_loraHasContact || lastRssi != 0) {
                    canvas->printf("433MHz | %d dBm", lastRssi);
                } else {
                    canvas->print("433MHz | A aguardar");
                }
            }

            canvas->setTextSize(2);
            canvas->setTextColor(sel ? UI_CYAN : UI_DARKGREY);
            canvas->setCursor(x + rowW - 18, y + 6);
            canvas->print(">");
        }

        renderFooter("<-X: Home | Y/BOOT: Mover | X->/Hold: Entrar");
    }

    // =========================================================================
    // CONFIGURAÇÕES -> WIFI
    // =========================================================================
    void renderNetworkPage(bool isApMode, const char* ip, const char* ssid,
                           int8_t wifiRssi, uint8_t apClients, const char* apPassword = nullptr) {
        int cardY = 26;
        int cardH = 122;

        canvas->fillRect(4, cardY, 312, cardH, UI_PANEL_BG);
        canvas->drawRect(4, cardY, 312, cardH, UI_CARD_BG);

        if (!isApMode) {
            canvas->setTextSize(1);
            canvas->setTextColor(UI_GREEN);
            canvas->setCursor(12, cardY + 6);
            canvas->print("ESTADO: WIFI CONECTADO");

            canvas->setTextColor(UI_WHITE);
            canvas->setCursor(205, cardY + 6);
            canvas->printf("SINAL: %d dBm", wifiRssi);

            canvas->drawFastHLine(8, cardY + 18, 304, UI_CARD_BG);

            canvas->setTextSize(1);
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->setCursor(12, cardY + 25);
            canvas->print("REDE:");

            canvas->setTextSize(2);
            canvas->setTextColor(UI_WHITE);
            canvas->setCursor(52, cardY + 21);
            canvas->printf("%.21s", ssid ? ssid : "");

            canvas->setTextSize(3);
            canvas->setTextColor(UI_CYAN);
            canvas->setCursor(12, cardY + 46);
            canvas->print(ip ? ip : "0.0.0.0");
        } else {
            canvas->setTextSize(1);
            canvas->setTextColor(UI_YELLOW);
            canvas->setCursor(12, cardY + 6);
            canvas->print("ESTADO: MODO AP (HOTSPOT)");

            canvas->setTextColor(UI_LIGHTGREY);
            canvas->setCursor(205, cardY + 6);
            canvas->printf("CLIENTES: %u", apClients);

            canvas->drawFastHLine(8, cardY + 18, 304, UI_CARD_BG);

            canvas->setTextSize(1);
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->setCursor(12, cardY + 26);
            canvas->print("SSID:");
            canvas->setTextColor(UI_YELLOW);
            canvas->printf(" %.17s", ssid ? ssid : "ROVER-AP");

            canvas->setTextColor(UI_LIGHTGREY);
            canvas->setCursor(190, cardY + 26);
            canvas->print("PASS:");
            canvas->setTextColor(UI_WHITE);
            canvas->printf(" %.12s", (apPassword && strlen(apPassword) > 0) ? apPassword : "winddragon");

            canvas->setTextSize(3);
            canvas->setTextColor(UI_CYAN);
            canvas->setCursor(12, cardY + 46);
            canvas->print(ip ? ip : "192.168.4.1");
        }

        // Item selecionável "Lista" no espaço inferior para avançar para a lista de redes guardadas
        int itemX = 10;
        int itemY = cardY + 80;
        int itemW = 300;
        int itemH = 34;

        canvas->fillRect(itemX, itemY, itemW, itemH, UI_NAVY);
        canvas->drawRect(itemX, itemY, itemW, itemH, UI_CYAN);
        canvas->fillRect(itemX, itemY, 5, itemH, UI_CYAN);

        canvas->setTextSize(2);
        canvas->setTextColor(UI_YELLOW);
        canvas->setCursor(itemX + 14, itemY + 9);
        canvas->print("Lista");

        canvas->setTextColor(UI_CYAN);
        canvas->setCursor(itemX + itemW - 20, itemY + 9);
        canvas->print(">");

        renderFooter("<-X: Configuracoes | X->/Hold: Abrir Lista");
    }

    // =========================================================================
    // CONFIGURAÇÕES -> WIFI -> LISTA (ITEM 0 = SCAN, ITENS 1..N = REDES GUARDADAS)
    // =========================================================================
    void renderWifiListPage() {
        size_t netsCount = _wifiConfig ? _wifiConfig->getNetworks().size() : 0;
        uint8_t total = (uint8_t)(1 + netsCount);
        if (_wifiListIdx >= total) _wifiListIdx = 0;

        const uint8_t visibleRows = 4;
        uint8_t startIdx = 0;
        if (_wifiListIdx >= visibleRows) {
            startIdx = _wifiListIdx - (visibleRows - 1);
        }

        int rowW = (total > visibleRows) ? 298 : 308;

        for (uint8_t row = 0; row < visibleRows && (startIdx + row) < total; ++row) {
            uint8_t idx = startIdx + row;
            bool sel = (idx == _wifiListIdx);

            int x = 6;
            int y = 27 + row * 30;
            int h = 28;

            if (idx == 0) {
                if (sel) {
                    canvas->fillRect(x, y, rowW, h, UI_NAVY);
                    canvas->drawRect(x, y, rowW, h, UI_CYAN);
                    canvas->fillRect(x, y, 4, h, UI_CYAN);
                }

                canvas->setTextSize(2);
                canvas->setTextColor(sel ? UI_YELLOW : UI_ORANGE);
                canvas->setCursor(x + 10, y + 6);
                canvas->print("Scan");

                canvas->setTextSize(1);
                canvas->setTextColor(sel ? UI_CYAN : UI_LIGHTGREY);
                canvas->setCursor(x + rowW - 86, y + 10);
                canvas->print("[VARRER WIFI] >");
            } else {
                const auto &net = _wifiConfig->getNetworks()[idx - 1];
                bool isConn = _wifiConfig->isConnectedTo(net.ssid);

                if (sel) {
                    canvas->fillRect(x, y, rowW, h, UI_NAVY);
                    canvas->drawRect(x, y, rowW, h, UI_CYAN);
                    canvas->fillRect(x, y, 4, h, UI_CYAN);
                }

                canvas->setTextSize(2);
                canvas->setTextColor(sel ? UI_YELLOW : (isConn ? UI_GREEN : UI_WHITE));
                canvas->setCursor(x + 10, y + 6);
                canvas->printf("%.17s", net.ssid.c_str());

                canvas->setTextSize(1);
                if (isConn) {
                    canvas->setTextColor(UI_GREEN);
                    canvas->setCursor(x + rowW - 56, y + 10);
                    canvas->print("[ATIVA]");
                } else if (net.is_default) {
                    canvas->setTextColor(UI_YELLOW);
                    canvas->setCursor(x + rowW - 56, y + 10);
                    canvas->print("[DEF]");
                } else if (sel) {
                    canvas->setTextColor(UI_CYAN);
                    canvas->setCursor(x + rowW - 58, y + 10);
                    canvas->print("X-> LIGAR");
                }
            }
        }

        if (total > visibleRows) {
            int sbX = 308, sbY = 27, sbW = 6, sbH = 118;
            canvas->drawRect(sbX, sbY, sbW, sbH, UI_CARD_BG);
            int thumbH = max(16, sbH * visibleRows / total);
            int thumbY = sbY + (sbH - thumbH) * _wifiListIdx / max(1, (int)total - 1);
            canvas->fillRect(sbX + 1, thumbY, sbW - 2, thumbH, UI_CYAN);
        }

        renderFooter("<-X: WiFi | Y/BOOT: Item | X->/Hold: Ligar");
    }

    // =========================================================================
    // CONFIGURAÇÕES -> WIFI -> LISTA -> SCAN DE REDES DISPONÍVEIS
    // =========================================================================
    void renderWifiScanPage() {
        if (!_wifiConfig || _wifiConfig->getScannedNetworks().empty()) {
            canvas->fillRect(6, 30, 308, 114, UI_PANEL_BG);
            canvas->drawRect(6, 30, 308, 114, UI_CARD_BG);
            canvas->setTextSize(2);
            canvas->setTextColor(UI_YELLOW);
            canvas->setCursor(24, 65);
            canvas->print("SEM REDES NO ALCANCE");
            renderFooter("<-X / Clique BOOT: Voltar a Lista");
            return;
        }

        const auto &scanned = _wifiConfig->getScannedNetworks();
        uint8_t total = (uint8_t)scanned.size();
        if (_wifiScanIdx >= total) _wifiScanIdx = 0;

        const uint8_t visibleRows = 4;
        uint8_t startIdx = 0;
        if (_wifiScanIdx >= visibleRows) {
            startIdx = _wifiScanIdx - (visibleRows - 1);
        }

        int rowW = (total > visibleRows) ? 298 : 308;

        for (uint8_t row = 0; row < visibleRows && (startIdx + row) < total; ++row) {
            uint8_t idx = startIdx + row;
            const auto &sn = scanned[idx];
            bool sel = (idx == _wifiScanIdx);
            bool isConn = _wifiConfig->isConnectedTo(sn.ssid);
            bool isKnown = _wifiConfig->hasSavedNetwork(sn.ssid);

            int x = 6;
            int y = 27 + row * 30;
            int h = 28;

            if (sel) {
                canvas->fillRect(x, y, rowW, h, UI_NAVY);
                canvas->drawRect(x, y, rowW, h, UI_CYAN);
                canvas->fillRect(x, y, 4, h, UI_CYAN);
            }

            canvas->setTextSize(2);
            canvas->setTextColor(sel ? UI_YELLOW : (isConn ? UI_GREEN : UI_WHITE));
            canvas->setCursor(x + 10, y + 6);
            canvas->printf("%.16s", sn.ssid.c_str());

            canvas->setTextSize(1);
            if (isConn) {
                canvas->setTextColor(UI_GREEN);
                canvas->setCursor(x + rowW - 84, y + 10);
                canvas->printf("[ATIVA] %ddB", (int)sn.rssi);
            } else if (isKnown) {
                canvas->setTextColor(UI_CYAN);
                canvas->setCursor(x + rowW - 88, y + 10);
                canvas->printf("[GUARD.] %ddB", (int)sn.rssi);
            } else {
                canvas->setTextColor(UI_LIGHTGREY);
                canvas->setCursor(x + rowW - 76, y + 10);
                canvas->printf("%s %ddBm", sn.open ? "OPEN" : "WPA ", (int)sn.rssi);
            }
        }

        if (total > visibleRows) {
            int sbX = 308, sbY = 27, sbW = 6, sbH = 118;
            canvas->drawRect(sbX, sbY, sbW, sbH, UI_CARD_BG);
            int thumbH = max(16, sbH * visibleRows / total);
            int thumbY = sbY + (sbH - thumbH) * _wifiScanIdx / max(1, (int)total - 1);
            canvas->fillRect(sbX + 1, thumbY, sbW - 2, thumbH, UI_CYAN);
        }

        renderFooter("<-X: Lista | Y/BOOT: Rede | X->/Hold: Ligar");
    }

    // =========================================================================
    // CONFIGURAÇÕES -> WIFI -> SCAN -> INSERIR PASSWORD (TECLADO QWERTY)
    // =========================================================================
    void renderWifiPassPage(const char *confirmLabel = "LIGAR") {
        static const char *const kbRows[3][3] = {
            {"qwertyuiop", "asdfghjkl-", "zxcvbnm._@"},
            {"QWERTYUIOP", "ASDFGHJKL-", "ZXCVBNM._@"},
            {"1234567890", "!@#$%^&*()", "-_=+/?:;,.~"}};

        bool inTextBox = (_wifiKbRow == 4);
        canvas->fillRect(6, 26, 308, 22, inTextBox ? UI_NAVY : UI_PANEL_BG);
        canvas->drawRect(6, 26, 308, 22, inTextBox ? UI_YELLOW : UI_CYAN);
        if (inTextBox) {
            canvas->fillRect(6, 26, 4, 22, UI_YELLOW);
        }

        size_t passLen = strlen(_wifiPassBuf);
        uint8_t curPos = (_wifiTextCursorPos > passLen) ? (uint8_t)passLen : _wifiTextCursorPos;
        const size_t maxVisChars = 22;
        size_t winStart = 0;
        if (curPos > maxVisChars) {
            winStart = curPos - maxVisChars;
        }

        canvas->setTextSize(2);
        for (size_t i = 0; i <= maxVisChars; ++i) {
            size_t pos = winStart + i;
            if (pos > passLen) break;
            int cx = 14 + (int)i * 12;
            bool isCur = (pos == curPos);

            if (pos < passLen) {
                char chStr[2] = {_wifiPassBuf[pos], '\0'};
                if (isCur) {
                    canvas->fillRect(cx - 1, 28, 12, 18, inTextBox ? UI_YELLOW : UI_CYAN);
                    canvas->setTextColor(UI_BLACK);
                } else {
                    canvas->setTextColor(UI_WHITE);
                }
                canvas->setCursor(cx, 30);
                canvas->print(chStr);
                if (isCur && !inTextBox) {
                    canvas->drawFastHLine(cx - 1, 46, 12, UI_YELLOW);
                }
            } else if (isCur) {
                if (inTextBox) {
                    canvas->fillRect(cx, 29, 10, 16, UI_YELLOW);
                } else {
                    canvas->setTextColor(UI_CYAN);
                    canvas->setCursor(cx, 30);
                    canvas->print("_");
                }
            }
        }

        uint8_t m = _wifiKbMode % 3;
        for (uint8_t r = 0; r < 3; ++r) {
            int ky = 50 + r * 23;
            for (uint8_t c = 0; c < 10; ++c) {
                int kx = 6 + c * 31;
                int kw = 29;
                int kh = 21;
                bool sel = (_wifiKbRow == r && _wifiKbCol == c);

                canvas->fillRect(kx, ky, kw, kh, sel ? UI_CYAN : UI_PANEL_BG);
                canvas->drawRect(kx, ky, kw, kh, sel ? UI_YELLOW : UI_CARD_BG);

                char chStr[2] = {kbRows[m][r][c], '\0'};
                canvas->setTextSize(2);
                canvas->setTextColor(sel ? UI_BLACK : UI_WHITE);
                canvas->setCursor(kx + 9, ky + 3);
                canvas->print(chStr);
            }
        }

        const int btnX[6] = {6, 54, 110, 158, 208, 258};
        const int btnW[6] = {46, 54, 46, 48, 48, 56};
        const char *btnLabels[6] = {(_wifiKbMode == 1) ? "abc" : "ABC",
                                    (_wifiKbMode == 2) ? "ABC" : "123#",
                                    "ESP",
                                    "<-DEL",
                                    "SAIR",
                                    confirmLabel ? confirmLabel : "LIGAR"};

        int by = 120;
        int bh = 24;
        for (uint8_t b = 0; b < 6; ++b) {
            bool sel = (_wifiKbRow == 3 && _wifiKbCol == b);
            uint16_t bgCol = UI_NAVY;
            uint16_t borderCol = UI_CARD_BG;
            uint16_t txtCol = UI_WHITE;

            if (b == 1) {
                borderCol = (_wifiKbMode == 2) ? UI_GREEN : UI_ORANGE;
                txtCol = (_wifiKbMode == 2) ? UI_GREEN : UI_ORANGE;
            } else if (b == 4) {
                borderCol = UI_RED;
                txtCol = UI_RED;
            } else if (b == 5) {
                borderCol = UI_GREEN;
                txtCol = UI_GREEN;
            }

            if (sel) {
                bgCol = (b == 5) ? UI_GREEN : ((b == 4) ? UI_RED : UI_CYAN);
                borderCol = UI_YELLOW;
                txtCol = UI_BLACK;
            }

            canvas->fillRect(btnX[b], by, btnW[b], bh, bgCol);
            canvas->drawRect(btnX[b], by, btnW[b], bh, borderCol);

            canvas->setTextSize(1);
            canvas->setTextColor(txtCol);
            int textW = strlen(btnLabels[b]) * 6;
            int tx = btnX[b] + (btnW[b] - textW) / 2;
            canvas->setCursor(tx, by + 8);
            canvas->print(btnLabels[b]);
        }

        if (inTextBox) {
            renderFooter("X: Posicao Cursor | Y: Voltar ao QWERTY");
        } else {
            renderFooter("XY: Mover | Clique: Tecla | Hold: Cancelar");
        }
    }

    // =========================================================================
    // CONFIGURAÇÕES -> ID (ECRÃ COM QRCODE, IGUAL À BASE)
    // =========================================================================
    void renderMacPage(uint8_t roverId) {
        int cardY = 26;
        int cardH = 122;
        String mac = getDeviceMacAddress();

        // Painel Esquerdo: QR Code do MAC (x=4..102, w=98, h=122)
        canvas->fillRect(4, cardY, 98, cardH, UI_PANEL_BG);
        canvas->drawRect(4, cardY, 98, cardH, UI_CARD_BG);
        drawQrCode(canvas, mac.c_str(), 7, cardY + 6, 92, 92);

        canvas->setTextSize(1);
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(18, cardY + 106);
        canvas->print("QR MAC ID");

        // Painel Direito: Identificador do Rover e MAC Address (x=105..317, w=212, h=122)
        int rx = 105;
        int rw = 212;
        canvas->fillRect(rx, cardY, rw, cardH, UI_PANEL_BG);
        canvas->drawRect(rx, cardY, rw, cardH, UI_CARD_BG);

        canvas->setTextSize(1);
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(rx + 6, cardY + 16);
        canvas->print("UNIDADE AUTONOMA:");

        canvas->setTextSize(2);
        canvas->setTextColor(UI_YELLOW);
        canvas->setCursor(rx + 6, cardY + 30);
        canvas->printf("ROVER-%02d", roverId);

        canvas->drawFastHLine(rx + 4, cardY + 60, rw - 8, UI_CARD_BG);

        canvas->setTextSize(1);
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(rx + 6, cardY + 72);
        canvas->print("MAC-ADDRESS:");

        canvas->setTextSize(2);
        canvas->setTextColor(UI_CYAN);
        canvas->setCursor(rx + 4, cardY + 86);
        canvas->printf("%s", mac.c_str());

        renderFooter("<-X / Clique BOOT: Voltar a Configuracoes");
    }

    // =========================================================================
    // CONFIGURAÇÕES -> FIRMWARE (DEDICADO, IGUAL À BASE)
    // =========================================================================
    void renderFirmwarePage() {
        int x = 6;
        int w = 308;
        int c1y = 28;
        int ch = 56;

        // Card 1: Master Lafvin (LCD, SD, WiFi, GPS, WebServer)
        canvas->fillRect(x, c1y, w, ch, UI_PANEL_BG);
        canvas->drawRect(x, c1y, w, ch, UI_CARD_BG);
        canvas->fillRect(x, c1y, 4, ch, UI_CYAN);

        canvas->setTextSize(1);
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(x + 12, c1y + 8);
        canvas->print("MASTER LAFVIN (LCD / WIFI / GPS):");

        canvas->setTextSize(2);
        canvas->setTextColor(UI_CYAN);
        canvas->setCursor(x + 12, c1y + 22);
        canvas->printf("%s", ROVER_LAFVIN_VERSION);

        canvas->setTextSize(1);
        canvas->setTextColor(UI_WHITE);
        canvas->setCursor(x + 140, c1y + 28);
        canvas->printf("Build: %s", ROVER_BUILD_DATE);

        // Card 2: Coprocessador C6 (LoRa 433MHz, ESC, Servos, BNO055)
        int c2y = 90;
        canvas->fillRect(x, c2y, w, ch, UI_PANEL_BG);
        canvas->drawRect(x, c2y, w, ch, UI_CARD_BG);
        canvas->fillRect(x, c2y, 4, ch, UI_YELLOW);

        canvas->setTextSize(1);
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(x + 12, c2y + 8);
        canvas->print("COPROCESSADOR C6 (LORA / ESC / IMU):");

        canvas->setTextSize(2);
        canvas->setTextColor(UI_YELLOW);
        canvas->setCursor(x + 12, c2y + 22);
        canvas->printf("%s", _c6Version);

        canvas->setTextSize(1);
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(x + 140, c2y + 28);
        canvas->printf("Pacote: %s", ROVER_FIRMWARE_VERSION_STR);

        renderFooter("<-X / Clique BOOT: Voltar a Configuracoes");
    }

    // =========================================================================
    // CONFIGURAÇÕES -> LORA 433MHZ (DIAGNÓSTICO DO LINK COM A BASE)
    // =========================================================================
    void renderLoraPage(int16_t lastRssi, bool failsafeActive) {
        int cardY = 26;
        int cardH = 122;

        canvas->fillRect(4, cardY, 312, cardH, UI_PANEL_BG);
        canvas->drawRect(4, cardY, 312, cardH, UI_CARD_BG);

        canvas->setTextSize(1);
        if (failsafeActive) {
            canvas->setTextColor(UI_RED);
            canvas->setCursor(12, cardY + 6);
            canvas->print("ESTADO: FAILSAFE (SEM CONTACTO)");
        } else if (_loraHasContact || lastRssi != 0) {
            canvas->setTextColor(UI_GREEN);
            canvas->setCursor(12, cardY + 6);
            canvas->print("ESTADO: LINK LORA ATIVO (ONLINE)");
        } else {
            canvas->setTextColor(UI_YELLOW);
            canvas->setCursor(12, cardY + 6);
            canvas->print("ESTADO: A AGUARDAR POLL DA BASE");
        }

        canvas->setTextColor(UI_CYAN);
        canvas->setCursor(218, cardY + 6);
        canvas->print("SX1278 433MHz");

        canvas->drawFastHLine(8, cardY + 18, 304, UI_CARD_BG);

        // Sinal RSSI e SNR
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(12, cardY + 26);
        canvas->print("SINAL RSSI:");
        canvas->setTextSize(2);
        canvas->setTextColor(lastRssi != 0 ? UI_GREEN : UI_LIGHTGREY);
        canvas->setCursor(12, cardY + 38);
        canvas->printf("%d dBm", lastRssi);

        canvas->setTextSize(1);
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(168, cardY + 26);
        canvas->print("RELACAO SNR:");
        canvas->setTextSize(2);
        canvas->setTextColor(UI_CYAN);
        canvas->setCursor(168, cardY + 38);
        canvas->printf("%.1f dB", _loraSnr);

        canvas->drawFastHLine(8, cardY + 62, 304, UI_CARD_BG);

        // Contadores de pacotes RX Polls e TX Telemetria
        canvas->setTextSize(1);
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(12, cardY + 70);
        canvas->print("POLLS RECEBIDOS (RX):");
        canvas->setTextSize(2);
        canvas->setTextColor(UI_YELLOW);
        canvas->setCursor(12, cardY + 82);
        canvas->printf("%lu", (unsigned long)_loraRxPolls);

        canvas->setTextSize(1);
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(168, cardY + 70);
        canvas->print("TELEMETRIA ENVIADA:");
        canvas->setTextSize(2);
        canvas->setTextColor(UI_WHITE);
        canvas->setCursor(168, cardY + 82);
        canvas->printf("%lu", (unsigned long)_loraTxTelems);

        canvas->setTextSize(1);
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(12, cardY + 106);
        if (_loraLastRxSecAgo < 9999) {
            canvas->printf("Ultimo pacote da Base ha: %lu s", (unsigned long)_loraLastRxSecAgo);
        } else {
            canvas->print("Ultimo pacote da Base: Sem registo");
        }

        renderFooter("<-X / Clique BOOT: Voltar a Configuracoes");
    }

    ST7789Display display;
    Arduino_Canvas *canvas;
    RoverScreenPage currentPage;
    uint32_t lastRender;
    BootLogItem _bootLogs[BOOT_MAX_LINES];
    uint8_t _bootLogCount;
    uint8_t _bootProgress;
    uint8_t _brightness;
    uint8_t _roverId;
    bool _otaActive;
    char _c6Version[24];

    uint8_t _homeMenuIdx;
    uint8_t _opMenuIdx;
    uint8_t _configMenuIdx;
    uint8_t _wifiListIdx;
    uint8_t _wifiScanIdx;
    uint8_t _wifiKbRow;
    uint8_t _wifiKbCol;
    uint8_t _wifiKbMode;
    uint8_t _wifiTextCursorPos;
    char _wifiTargetSsid[36];
    char _wifiPassBuf[64];
    WiFiConfigManager *_wifiConfig;
    char _feedbackMsg[44];
    uint32_t _feedbackUntilMs;

    uint32_t _loraTxTelems;
    uint32_t _loraRxPolls;
    uint32_t _loraLastRxSecAgo;
    float _loraSnr;
    bool _loraHasContact;
};
