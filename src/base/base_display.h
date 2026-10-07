#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <canvas/Arduino_Canvas.h>
#include "Display_ST7789.h"
#include "config_common.h"
#include "fleet_manager.h"
#include "logo_bmp.h"

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

enum BaseScreenPage {
    BASE_PAGE_CONTROL = 0, // Controlo e telemetria do Rover selecionado
    BASE_PAGE_FLEET   = 1, // Lista da frota de Rovers (1 a 8)
    BASE_PAGE_NETWORK = 2, // Página de WiFi / Rede (igual à do Rover)
    BASE_PAGE_COUNT   = 3
};

struct BootLogItem {
    char text[34];
    uint16_t color;
};

static const uint8_t BOOT_MAX_LINES = 24;
static const uint8_t BOOT_VISIBLE_LINES = 13;

class BaseDisplay {
public:
    BaseDisplay() 
        : canvas(nullptr), currentPage(BASE_PAGE_CONTROL), lastRender(0),
          _bootLogCount(0), _bootProgress(0), _brightness(85) {}

    void begin(uint8_t initialBrightness = 85) {
        // Desativar CS do cartão MicroSD onboard para evitar colisão SPI
        pinMode(PIN_SD_CS, OUTPUT);
        digitalWrite(PIN_SD_CS, HIGH);

        // Inicializar driver de hardware ST7789
        display.begin();
        setBrightness(initialBrightness);

        // Criar canvas em RAM configurado em Landscape (320x172) com DISPLAY_ROTATION (3)
        // Mesma orientação e PCB do Rover
        if (!canvas) {
            canvas = new Arduino_Canvas(LCD_WIDTH, LCD_HEIGHT, nullptr, 0, 0, DISPLAY_ROTATION);
        }
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

    // =========================================================================
    // BOOT SCREEN (ARRANQUE) - IGUAL AO DO ROVER (LANDSCAPE 320x172)
    // =========================================================================
    void showBootScreen() {
        if (!canvas) return;
        _bootLogCount = 0;
        _bootProgress = 0;

        // Limpar ecrã
        canvas->fillScreen(UI_BLACK);

        // Painel Esquerdo: Logótipo + Marca + Título Base + Barra de Progresso (w=101, h=164)
        canvas->fillRect(4, 4, 101, 164, UI_PANEL_BG);
        canvas->drawRect(4, 4, 101, 164, UI_CARD_BG);

        // Logótipo Dragão centrado (85x80) em Ciano
        // x = 4 + (101 - 85)/2 = 12, y = 10
        canvas->drawBitmap(12, 10, LOGO_BMP, LOGO_BMP_W, LOGO_BMP_H, UI_CYAN);

        // Texto Marca "WINDDRAGONS" centrado
        canvas->setTextSize(1);
        canvas->setTextColor(UI_WHITE);
        canvas->setCursor(20, 94);
        canvas->print("WINDDRAGONS");

        // Identificador Base Station
        canvas->setTextSize(2);
        canvas->setTextColor(UI_YELLOW);
        canvas->setCursor(8, 106);
        canvas->print("BASE STN");

        // Subtítulo do sistema
        canvas->setTextSize(1);
        canvas->setTextColor(UI_CYAN);
        canvas->setCursor(12, 126);
        canvas->print("CONTROLLER");
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(18, 136);
        canvas->print("FLEET v2");

        // Moldura da barra de progresso no fundo do painel esquerdo
        canvas->drawRect(12, 150, 85, 10, UI_CARD_BG);
        canvas->fillRect(14, 152, 81, 6, UI_BLACK);

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
            // Scroll interno descartando a mais antiga
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

        // Redesenhar a área de texto da consola (scroll vertical)
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

        // Atualizar ecrã LCD
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
        bootLogf(UI_GREEN, 100, "[SYS ] Base operacional!");
        if (delayMs > 0) delay(delayMs);
    }

    // =========================================================================
    // NAVEGAÇÃO DE PÁGINAS
    // =========================================================================
    void nextPage() {
        currentPage = static_cast<BaseScreenPage>((currentPage + 1) % BASE_PAGE_COUNT);
        lastRender = 0; // Forçar renderização imediata
    }

    void prevPage() {
        currentPage = static_cast<BaseScreenPage>((currentPage + BASE_PAGE_COUNT - 1) % BASE_PAGE_COUNT);
        lastRender = 0;
    }

    void setPage(BaseScreenPage page) {
        currentPage = page;
        lastRender = 0;
    }

    BaseScreenPage getCurrentPage() const {
        return currentPage;
    }

    // =========================================================================
    // ATUALIZAÇÃO GERAL DO ECRÃ EM RUNTIME
    // =========================================================================
    void update(FleetManager &fleet, bool wifiOk, bool cloudOk, 
                int8_t throttle, int8_t rudder, uint8_t navMode, bool sdOk = false,
                bool isApMode = false, const char* ip = nullptr, const char* ssid = nullptr,
                int8_t wifiRssi = 0, uint8_t apClients = 0, const char* apPassword = nullptr,
                bool force = false) {
        uint32_t now = millis();
        if (!force && (now - lastRender < 200)) return; // 5Hz refresh
        lastRender = now;

        if (!canvas) return;

        // 1. Cabeçalho Superior Global (Altura: 24px)
        renderHeader(fleet, wifiOk, cloudOk, sdOk, isApMode);

        // 2. Renderizar a Página Ativa
        switch (currentPage) {
            case BASE_PAGE_CONTROL:
                renderControlPage(fleet, throttle, rudder, navMode);
                break;
            case BASE_PAGE_FLEET:
                renderFleetPage(fleet);
                break;
            case BASE_PAGE_NETWORK:
                renderNetworkPage(isApMode, ip, ssid, wifiRssi, apClients, apPassword);
                break;
            default:
                break;
        }

        // Enviar imagem composta para o display ST7789
        display.drawPixelBuffer(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, canvas->getFramebuffer());
    }

    // =========================================================================
    // MENU POPUP DE AÇÕES
    // =========================================================================
    void renderMenu(const char* const items[], uint8_t count, uint8_t selectedIndex, const char* feedbackMsg = nullptr) {
        if (!canvas) return;

        // Moldura do Menu Centrada em Landscape (w=296, h=152 em 320x172)
        canvas->fillRect(12, 10, 296, 152, UI_PANEL_BG);
        canvas->drawRect(12, 10, 296, 152, UI_CARD_BG);

        // Cabeçalho do Menu
        canvas->fillRect(13, 11, 294, 18, UI_NAVY);
        canvas->setTextColor(UI_YELLOW);
        canvas->setTextSize(1);
        canvas->setCursor(20, 16);
        canvas->print("=== MENU DE CONTROLO ===");

        // 2 Colunas de 4 Opções
        // Coluna 0: itens 0..3 (x=18, w=140)
        // Coluna 1: itens 4..7 (x=162, w=140)
        int itemH = 24;
        for (uint8_t i = 0; i < count; ++i) {
            int col = i / 4;
            int row = i % 4;
            int x = (col == 0) ? 18 : 162;
            int y = 34 + row * itemH;
            int w = 140;

            bool isSel = (i == selectedIndex);
            canvas->fillRect(x, y, w, itemH - 2, isSel ? UI_NAVY : UI_CARD_BG);
            if (isSel) {
                canvas->drawRect(x, y, w, itemH - 2, UI_CYAN);
                canvas->setTextColor(UI_WHITE);
            } else {
                canvas->setTextColor(UI_LIGHTGREY);
            }
            canvas->setCursor(x + 5, y + 6);
            canvas->print(items[i]);
        }

        // Rodapé de Feedback / Instruções
        canvas->fillRect(13, 134, 294, 27, UI_NAVY);
        canvas->setTextSize(1);
        if (feedbackMsg != nullptr) {
            canvas->setTextColor(UI_YELLOW);
            canvas->setCursor(20, 143);
            canvas->print(feedbackMsg);
        } else {
            canvas->setTextColor(UI_CYAN);
            canvas->setCursor(20, 143);
            canvas->print("JOY: NAVEGAR | CLIQUE: EXECUTAR");
        }

        // Push frame to ST7789
        display.drawPixelBuffer(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, canvas->getFramebuffer());
    }

    // =========================================================================
    // PÁGINA DE REDE (WIFI / AP) - IDÊNTICA À DO ROVER
    // =========================================================================
    void renderNetworkPage(bool isApMode, const char* ip, const char* ssid, 
                           int8_t wifiRssi, uint8_t apClients, const char* apPassword = nullptr) {
        int cardY = 26;
        int cardH = 122;

        // Card de Largura Total (x=4..316, w=312, h=122)
        canvas->fillRect(4, cardY, 312, cardH, UI_PANEL_BG);
        canvas->drawRect(4, cardY, 312, cardH, UI_CARD_BG);

        if (!isApMode) {
            // MODO WIFI (STA CONECTADO)
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
            canvas->print("Web: /fleet, /control, /wifi");

        } else {
            // MODO AP (PONTO DE ACESSO / HOTSPOT)
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
            canvas->printf("%.21s", ssid ? ssid : "WindDragons-Base");

            // Password em TAMANHO GRANDE (Size 2)
            canvas->setTextSize(1);
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->setCursor(12, cardY + 48);
            canvas->print("PASS:");

            canvas->setTextSize(2);
            canvas->setTextColor(UI_WHITE);
            canvas->setCursor(55, cardY + 45);
            canvas->printf("%.21s", (apPassword && strlen(apPassword) > 0) ? apPassword : "12345678");

            // Endereço IP em TAMANHO GIGANTE (Size 3)
            canvas->setTextSize(1);
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->setCursor(12, cardY + 70);
            canvas->print("ENDERECO IP:");

            canvas->setTextSize(3);
            canvas->setTextColor(UI_CYAN);
            canvas->setCursor(12, cardY + 84);
            canvas->print(ip ? ip : "192.168.4.1");

            // Dica de configuração
            canvas->setTextSize(1);
            canvas->setTextColor(UI_YELLOW);
            canvas->setCursor(180, cardY + 106);
            canvas->print("Ligue-se ao WiFi");
        }

        // Rodapé
        canvas->fillRect(0, 150, SCREEN_W, 22, UI_NAVY);
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setTextSize(1);
        canvas->setCursor(8, 156);
        canvas->print("BOOT: Próx Ecrã | JOY CLIQUE: Voltar");
    }

private:
    // =========================================================================
    // CABEÇALHO GLOBAL (TOPO 24px)
    // =========================================================================
    void renderHeader(FleetManager &fleet, bool wifiOk, bool cloudOk, bool sdOk, bool isApMode) {
        canvas->fillRect(0, 0, SCREEN_W, 24, UI_NAVY);
        canvas->setTextSize(1);
        canvas->setTextColor(UI_CYAN);
        canvas->setCursor(8, 8);
        canvas->print("WINDDRAGONS");
        canvas->setTextColor(UI_WHITE);
        canvas->print(" BASE");

        // Nome da página ativa no centro
        canvas->setCursor(132, 8);
        if (currentPage == BASE_PAGE_CONTROL) {
            canvas->setTextColor(UI_YELLOW);
            canvas->print("[1.COMANDO]");
        } else if (currentPage == BASE_PAGE_FLEET) {
            canvas->setTextColor(UI_CYAN);
            canvas->print("[2.FROTA]");
        } else if (currentPage == BASE_PAGE_NETWORK) {
            canvas->setTextColor(UI_GREEN);
            canvas->print("[3.REDE/IP]");
        }

        // Ícones de Estado: SD, WiFi, Cloud
        canvas->fillCircle(240, 12, 4, sdOk ? UI_CYAN : UI_DARKGREY);
        canvas->fillCircle(256, 12, 4, isApMode ? UI_ORANGE : (wifiOk ? UI_GREEN : UI_RED));
        canvas->fillCircle(272, 12, 4, cloudOk ? UI_GREEN : UI_DARKGREY);

        // Pontos indicadores de página
        for (uint8_t i = 0; i < BASE_PAGE_COUNT; ++i) {
            int dotX = 292 + i * 8;
            if (i == currentPage) {
                canvas->fillCircle(dotX, 12, 3, UI_YELLOW);
            } else {
                canvas->drawCircle(dotX, 12, 2, UI_LIGHTGREY);
            }
        }
    }

    // =========================================================================
    // PÁGINA 1: COMANDAR ROVER SELECIONADO
    // =========================================================================
    void renderControlPage(FleetManager &fleet, int8_t throttle, int8_t rudder, uint8_t navMode) {
        int cardY = 26;
        int cardH = 122;
        FleetRover *selected = fleet.getSelectedRover();

        // --- Card Esquerdo: Telemetria do Rover Selecionado (w=154, h=122) ---
        canvas->fillRect(4, cardY, 154, cardH, UI_PANEL_BG);
        canvas->drawRect(4, cardY, 154, cardH, UI_CARD_BG);

        if (selected) {
            // Rover ID e Badge de Online
            canvas->setTextSize(2);
            canvas->setTextColor(UI_YELLOW);
            canvas->setCursor(8, cardY + 5);
            canvas->print(selected->code);

            canvas->setTextSize(1);
            if (selected->is_online) {
                canvas->setTextColor(UI_GREEN);
                canvas->setCursor(102, cardY + 5);
                canvas->print("ONLINE");
                canvas->setTextColor(UI_LIGHTGREY);
                canvas->setCursor(102, cardY + 15);
                canvas->printf("%ddBm", selected->rssi);
            } else {
                canvas->setTextColor(UI_RED);
                canvas->setCursor(102, cardY + 8);
                canvas->print("OFFLINE");
            }

            canvas->drawFastHLine(8, cardY + 24, 146, UI_CARD_BG);

            // Coordenadas GPS
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->setCursor(8, cardY + 28);
            canvas->print("GPS:");
            canvas->setTextColor(UI_WHITE);
            canvas->setCursor(34, cardY + 28);
            if (selected->lat != 0.0 || selected->lng != 0.0) {
                canvas->printf("%.4f, %.4f", selected->lat, selected->lng);
            } else {
                canvas->print("Sem Fix GPS");
            }

            // Velocidade e Rumo
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->setCursor(8, cardY + 41);
            canvas->print("VEL:");
            canvas->setTextColor(UI_CYAN);
            canvas->setCursor(34, cardY + 41);
            canvas->printf("%.1f kn | %.0f deg", selected->speed_knots, selected->heading_deg);

            // Bateria LiPo
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->setCursor(8, cardY + 54);
            canvas->print("BAT:");
            uint16_t bCol = (selected->battery_pct < 25) ? UI_RED : ((selected->battery_pct < 50) ? UI_YELLOW : UI_GREEN);
            canvas->setTextColor(bCol);
            canvas->setCursor(34, cardY + 54);
            canvas->printf("%u%% (%.1fV)", selected->battery_pct, selected->battery_voltage);

            // Estado do Motor
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->setCursor(8, cardY + 67);
            canvas->print("MOT:");
            canvas->setTextColor(UI_YELLOW);
            canvas->setCursor(34, cardY + 67);
            canvas->print(get_motor_status_str(selected->motor_status));

            // Âncora e Profundidade
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->setCursor(8, cardY + 80);
            canvas->print("ANC:");
            canvas->setTextColor(UI_CYAN);
            canvas->setCursor(34, cardY + 80);
            canvas->printf("%s (%.1fm)", get_anchor_status_str(selected->anchor_status), selected->anchor_depth_m);

            // Modo de Navegação Ativo
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->setCursor(8, cardY + 95);
            canvas->print("MOD:");
            canvas->setTextColor(navMode == 0 ? UI_GREEN : UI_CYAN);
            canvas->setCursor(34, cardY + 95);
            canvas->printf("[%s]", navMode == 0 ? "MANUAL" : "HOLD STATION");

        } else {
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->setTextSize(1);
            canvas->setCursor(24, cardY + 50);
            canvas->print("Sem Rover");
            canvas->setCursor(24, cardY + 65);
            canvas->print("Selecionado");
        }

        // --- Card Direito: Painel de Controlo / Joystick Inputs (w=154, h=122) ---
        canvas->fillRect(162, cardY, 154, cardH, UI_PANEL_BG);
        canvas->drawRect(162, cardY, 154, cardH, UI_CARD_BG);

        canvas->setTextSize(1);
        canvas->setTextColor(UI_YELLOW);
        canvas->setCursor(168, cardY + 6);
        canvas->print("COMANDO DO ROVER");

        canvas->drawFastHLine(166, cardY + 18, 146, UI_CARD_BG);

        // Throttle (Propulsão ESC)
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(168, cardY + 24);
        canvas->printf("PROPULSÃO: %+3d%%", throttle);

        canvas->drawRect(168, cardY + 36, 142, 10, UI_CARD_BG);
        int barT = map(throttle, -100, 100, 0, 138);
        canvas->fillRect(170, cardY + 38, 138, 6, UI_BLACK);
        if (barT > 0) {
            uint16_t tCol = (throttle >= 0) ? UI_GREEN : UI_RED;
            canvas->fillRect(170, cardY + 38, barT, 6, tCol);
        }

        // Rudder (Leme)
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(168, cardY + 53);
        canvas->printf("LEME / DIR: %+3d%%", rudder);

        canvas->drawRect(168, cardY + 65, 142, 10, UI_CARD_BG);
        int barR = map(rudder, -100, 100, 0, 138);
        canvas->fillRect(170, cardY + 67, 138, 6, UI_BLACK);
        if (barR > 0) {
            canvas->fillRect(170, cardY + 67, barR, 6, UI_BLUE);
        }

        // Dica de botões de âncora
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(168, cardY + 83);
        canvas->print("ÂNCORA: ");
        canvas->setTextColor(UI_WHITE);
        canvas->print("IO18:UP|IO20:DN");

        // Frequência LoRa
        canvas->setTextColor(UI_GREEN);
        canvas->setCursor(168, cardY + 98);
        canvas->print("LoRa TX: 10Hz @ 433MHz");

        // --- Rodapé de Instruções ---
        canvas->fillRect(0, 150, SCREEN_W, 22, UI_NAVY);
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setTextSize(1);
        canvas->setCursor(8, 156);
        canvas->print("BOOT: Próx Ecrã | JOY CLIQUE: Menu / Modo");
    }

    // =========================================================================
    // PÁGINA 2: LISTA DE ROVERS DA FROTA (GRID 2x4)
    // =========================================================================
    void renderFleetPage(FleetManager &fleet) {
        const auto &rovers = fleet.getRovers();
        uint8_t selectedId = fleet.getSelectedRoverId();

        for (size_t i = 0; i < rovers.size() && i < 8; ++i) {
            const auto &r = rovers[i];
            int col = i % 4;
            int row = i / 4;
            int x = 4 + col * 79;
            int y = 26 + row * 62;
            int w = 75;
            int h = 58;

            bool isSel = (r.id == selectedId);

            // Fundo do mini-card
            canvas->fillRect(x, y, w, h, isSel ? UI_NAVY : UI_PANEL_BG);
            canvas->drawRect(x, y, w, h, isSel ? UI_YELLOW : UI_CARD_BG);

            // Nome do Rover
            canvas->setTextSize(1);
            canvas->setTextColor(isSel ? UI_YELLOW : UI_WHITE);
            canvas->setCursor(x + 4, y + 4);
            canvas->printf("RVR-%02d", r.id);

            // Ponto indicador de estado Online / Offline
            int dotX = x + 62;
            int dotY = y + 7;
            canvas->fillCircle(dotX, dotY, 3, r.is_online ? UI_GREEN : UI_RED);

            // Bateria LiPo
            canvas->setCursor(x + 4, y + 18);
            if (r.is_online) {
                uint16_t bCol = (r.battery_pct < 25) ? UI_RED : ((r.battery_pct < 50) ? UI_YELLOW : UI_GREEN);
                canvas->setTextColor(bCol);
                canvas->printf("BAT: %u%%", r.battery_pct);
            } else {
                canvas->setTextColor(UI_DARKGREY);
                canvas->print("BAT: --");
            }

            // Velocidade e RSSI
            canvas->setCursor(x + 4, y + 30);
            if (r.is_online) {
                canvas->setTextColor(UI_CYAN);
                canvas->printf("%.1fkn", r.speed_knots);
                canvas->setTextColor(UI_LIGHTGREY);
                canvas->setCursor(x + 4, y + 42);
                canvas->printf("%ddBm", r.rssi);
            } else {
                canvas->setTextColor(UI_DARKGREY);
                canvas->print("OFFLINE");
                canvas->setCursor(x + 4, y + 42);
                canvas->print("-- dBm");
            }

            // Realce duplo de seleção
            if (isSel) {
                canvas->drawRect(x + 1, y + 1, w - 2, h - 2, UI_CYAN);
            }
        }

        // --- Rodapé de Instruções ---
        canvas->fillRect(0, 150, SCREEN_W, 22, UI_NAVY);
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setTextSize(1);
        canvas->setCursor(8, 156);
        canvas->print("JOY: Mudar Rover | CLIQUE: Comandar Rover");
    }

    ST7789Display display;
    Arduino_Canvas *canvas;
    BaseScreenPage currentPage;
    uint32_t lastRender;
    BootLogItem _bootLogs[BOOT_MAX_LINES];
    uint8_t _bootLogCount;
    uint8_t _bootProgress;
    uint8_t _brightness;
};
