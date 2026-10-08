#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <canvas/Arduino_Canvas.h>
#include "Display_ST7789.h"
#include "config_common.h"
#include "fleet_manager.h"
#include "logo_bmp.h"
#include "network_config.h"
#include <WiFi.h>

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
    BASE_PAGE_CONTROL  = 0, // Controlo e telemetria do Rover selecionado
    BASE_PAGE_FLEET    = 1, // Lista da frota de Rovers (1 a 8)
    BASE_PAGE_NETWORK  = 2, // Página de WiFi / Rede
    BASE_PAGE_CLOUD    = 3, // Página de Ligação ao Portal Cloud
    BASE_PAGE_JOYSTICK = 4, // Página de Teste do Joystick e Botões
    BASE_PAGE_COUNT    = 5
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
                bool force = false,
                int lastHttpStatus = 0, uint32_t cloudSuccessCount = 0, uint32_t cloudFailCount = 0,
                uint32_t lastCloudSyncMs = 0, const char* macStr = nullptr,
                int rawX = 2048, int rawY = 2048, int deadband = 180,
                bool btnJoy = false, bool btnMode = false,
                bool btnUp = false, bool btnDown = false, uint16_t joyClicks = 0) {
        uint32_t now = millis();
        uint32_t minInterval = (currentPage == BASE_PAGE_JOYSTICK) ? 60 : 200;
        if (!force && (now - lastRender < minInterval)) return;
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
            case BASE_PAGE_CLOUD:
                renderCloudPage(wifiOk, cloudOk, lastHttpStatus, cloudSuccessCount, cloudFailCount, lastCloudSyncMs, fleet, macStr);
                break;
            case BASE_PAGE_JOYSTICK:
                renderJoystickPage(rawX, rawY, throttle, rudder, deadband, btnJoy, btnMode, btnUp, btnDown, joyClicks);
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

    // =========================================================================
    // PÁGINA 4: ESTADO DA LIGAÇÃO AO PORTAL CLOUD
    // =========================================================================
    void renderCloudPage(bool wifiOk, bool cloudOk, int lastHttpStatus, 
                         uint32_t successCount, uint32_t failCount, 
                         uint32_t lastSyncMs, FleetManager &fleet, const char* macStr = nullptr) {
        int cardY = 26;
        int cardH = 122;

        // Card de Largura Total (x=4..316, w=312, h=122)
        canvas->fillRect(4, cardY, 312, cardH, UI_PANEL_BG);
        canvas->drawRect(4, cardY, 312, cardH, UI_CARD_BG);

        // Barra superior do card: ESTADO DO PORTAL CLOUD
        canvas->setTextSize(1);
        if (!wifiOk) {
            canvas->setTextColor(UI_RED);
            canvas->setCursor(12, cardY + 6);
            canvas->print("ESTADO: SEM LIGACAO WIFI");
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->setCursor(205, cardY + 6);
            canvas->print("PORTAL: DESLIGADO");
        } else if (cloudOk) {
            canvas->setTextColor(UI_GREEN);
            canvas->setCursor(12, cardY + 6);
            canvas->print("ESTADO: SINCRONIZADO (ONLINE)");
            canvas->setTextColor(UI_CYAN);
            canvas->setCursor(225, cardY + 6);
            canvas->printf("HTTP: %d", lastHttpStatus > 0 ? lastHttpStatus : 200);
        } else {
            canvas->setTextColor(UI_YELLOW);
            canvas->setCursor(12, cardY + 6);
            canvas->print("ESTADO: A LIGAR / ERRO");
            canvas->setTextColor(UI_RED);
            canvas->setCursor(215, cardY + 6);
            canvas->printf("HTTP: %d", lastHttpStatus);
        }

        canvas->drawFastHLine(8, cardY + 18, 304, UI_CARD_BG);

        // Linha 1: ENDPOINT & ID DA ESTAÇÃO
        canvas->setTextSize(1);
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(12, cardY + 24);
        canvas->print("PORTAL:");
        canvas->setTextColor(UI_WHITE);
        canvas->setCursor(65, cardY + 24);
        canvas->print("winddragons.app/api/circuits");

        // MAC ID único de hardware em destaque (Size 2)
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(12, cardY + 39);
        canvas->print("ID MAC:");

        canvas->setTextSize(2);
        canvas->setTextColor(UI_CYAN);
        canvas->setCursor(65, cardY + 36);
        if (macStr && strlen(macStr) > 0) {
            canvas->print(macStr);
        } else {
            canvas->print(WiFi.macAddress().c_str());
        }

        // Linha 2: Estatísticas de Sincronização (Cards internos)
        int boxY = cardY + 58;
        int boxH = 34;

        // Caixa 1: Total Syncs OK
        canvas->fillRect(12, boxY, 92, boxH, UI_NAVY);
        canvas->drawRect(12, boxY, 92, boxH, UI_CARD_BG);
        canvas->setTextSize(1);
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(18, boxY + 4);
        canvas->print("SYNCS OK");
        canvas->setTextSize(2);
        canvas->setTextColor(UI_GREEN);
        canvas->setCursor(18, boxY + 16);
        canvas->printf("%u", successCount);

        // Caixa 2: Falhas
        canvas->fillRect(110, boxY, 92, boxH, UI_NAVY);
        canvas->drawRect(110, boxY, 92, boxH, UI_CARD_BG);
        canvas->setTextSize(1);
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(116, boxY + 4);
        canvas->print("FALHAS");
        canvas->setTextSize(2);
        canvas->setTextColor(failCount == 0 ? UI_LIGHTGREY : UI_RED);
        canvas->setCursor(116, boxY + 16);
        canvas->printf("%u", failCount);

        // Caixa 3: Rovers Ativos
        canvas->fillRect(208, boxY, 102, boxH, UI_NAVY);
        canvas->drawRect(208, boxY, 102, boxH, UI_CARD_BG);
        canvas->setTextSize(1);
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(214, boxY + 4);
        canvas->print("ROVERS ATIVOS");
        canvas->setTextSize(2);
        canvas->setTextColor(UI_YELLOW);
        canvas->setCursor(214, boxY + 16);
        uint8_t onlineCnt = 0;
        for (const auto &r : fleet.getRovers()) {
            if (r.is_online) onlineCnt++;
        }
        canvas->printf("%u / %u", onlineCnt, MAX_FLEET_ROVERS);

        // Linha 3: Rodapé interno do card
        canvas->setTextSize(1);
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(12, cardY + 102);
        uint32_t agoSec = (millis() >= lastSyncMs && lastSyncMs > 0) ? (millis() - lastSyncMs) / 1000 : 0;
        canvas->printf("Ciclo: 3s  |  Ultimo sync: ha %us  |  Estacao: %s", agoSec, BASE_STATION_ID);

        // Rodapé de Navegação
        canvas->fillRect(0, 150, SCREEN_W, 22, UI_NAVY);
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setTextSize(1);
        canvas->setCursor(8, 156);
        canvas->print("BOOT: Prox Ecra | JOY CLIQUE: Menu");
    }

    // =========================================================================
    // PÁGINA 5: TESTE DE HARDWARE DO JOYSTICK E BOTÕES
    // =========================================================================
    void renderJoystickPage(int rawX, int rawY, int8_t throttle, int8_t rudder,
                            int deadband, bool btnJoy, bool btnMode,
                            bool btnUp, bool btnDown, uint16_t joyClicks) {
        int cardY = 26;
        int cardH = 122;

        // -------------------------------------------------------------
        // PAINEL ESQUERDO: Radar 2D / Mira Analógica do Joystick
        // -------------------------------------------------------------
        int leftW = 126;
        canvas->fillRect(4, cardY, leftW, cardH, UI_PANEL_BG);
        canvas->drawRect(4, cardY, leftW, cardH, UI_CARD_BG);

        // Barra de Título do Painel Esquerdo
        canvas->fillRect(5, cardY + 1, leftW - 2, 14, UI_NAVY);
        canvas->setTextSize(1);
        canvas->setTextColor(UI_YELLOW);
        canvas->setCursor(14, cardY + 4);
        canvas->print("MIRA 2D ANALOGICA");

        // Área do Radar (80x80 centralizada em x=27, y=44)
        int scopeX = 27;
        int scopeY = 44;
        int scopeSize = 80;
        int centerX = scopeX + scopeSize / 2; // 67
        int centerY = scopeY + scopeSize / 2; // 84

        canvas->fillRect(scopeX, scopeY, scopeSize, scopeSize, UI_BLACK);
        canvas->drawRect(scopeX, scopeY, scopeSize, scopeSize, UI_CARD_BG);

        // Grelha e Eixos de Referência
        canvas->drawFastHLine(scopeX + 1, centerY, scopeSize - 2, UI_DARKGREY);
        canvas->drawFastVLine(centerX, scopeY + 1, scopeSize - 2, UI_DARKGREY);

        // Círculo de alcance máximo e zona morta
        canvas->drawCircle(centerX, centerY, 36, UI_DARKGREY);
        canvas->drawRect(centerX - 6, centerY - 6, 13, 13, 0x2124); // Deadband box visual

        // Letras cardeais (Frente / Trás / Esquerda / Direita)
        canvas->setTextSize(1);
        canvas->setTextColor(UI_CYAN);
        canvas->setCursor(centerX - 2, scopeY + 2);
        canvas->print("F");
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(centerX - 2, scopeY + scopeSize - 9);
        canvas->print("T");
        canvas->setCursor(scopeX + 3, centerY - 3);
        canvas->print("E");
        canvas->setCursor(scopeX + scopeSize - 9, centerY - 3);
        canvas->print("D");

        // Cálculo da posição da mira (Puck)
        // throttle > 0: para a frente (cima no ecrã -> diminui Y)
        // rudder > 0: para a direita (direita no ecrã -> aumenta X)
        int puckX = centerX + (rudder * 34 / 100);
        int puckY = centerY - (throttle * 34 / 100);
        puckX = constrain(puckX, scopeX + 3, scopeX + scopeSize - 4);
        puckY = constrain(puckY, scopeY + 3, scopeY + scopeSize - 4);

        // Vetor de deflexão desde o centro
        if (puckX != centerX || puckY != centerY) {
            canvas->drawLine(centerX, centerY, puckX, puckY, UI_CARD_BG);
        }

        // Desenhar indicador da posição do stick
        bool inDeadband = (throttle == 0 && rudder == 0);
        if (inDeadband) {
            canvas->fillCircle(puckX, puckY, 4, UI_LIGHTGREY);
            canvas->drawCircle(puckX, puckY, 4, UI_WHITE);
        } else {
            uint16_t puckColor = (throttle != 0) ? ((throttle > 0) ? UI_GREEN : UI_RED) : UI_CYAN;
            canvas->fillCircle(puckX, puckY, 5, puckColor);
            canvas->fillCircle(puckX, puckY, 2, UI_WHITE);
            canvas->drawCircle(puckX, puckY, 6, UI_YELLOW);
        }

        // Rodapé do painel esquerdo: Modo / Direção
        canvas->setCursor(8, cardY + 107);
        if (inDeadband) {
            canvas->setTextColor(UI_GREEN);
            canvas->print("[ CENTRO / NEUTRO ]");
        } else {
            canvas->setTextColor(UI_YELLOW);
            canvas->printf("T:%+3d%%  R:%+3d%%", throttle, rudder);
        }

        // -------------------------------------------------------------
        // PAINEL DIREITO: Métricas ADC Detalhadas e Estado dos Botões
        // -------------------------------------------------------------
        int rightX = 134;
        int rightW = 182;
        canvas->fillRect(rightX, cardY, rightW, cardH, UI_PANEL_BG);
        canvas->drawRect(rightX, cardY, rightW, cardH, UI_CARD_BG);

        // Barra de Título do Painel Direito
        canvas->fillRect(rightX + 1, cardY + 1, rightW - 2, 14, UI_NAVY);
        canvas->setTextColor(UI_YELLOW);
        canvas->setCursor(rightX + 18, cardY + 4);
        canvas->print("SINAIS ADC & BOTOES");

        // 1. EIXO X (Throttle / Propulsão - IO0)
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(rightX + 6, cardY + 18);
        canvas->print("X (PROP): ");
        canvas->setTextColor(UI_WHITE);
        canvas->printf("%4d", rawX);
        uint16_t tColor = (throttle > 0) ? UI_GREEN : ((throttle < 0) ? UI_RED : UI_LIGHTGREY);
        canvas->setTextColor(tColor);
        canvas->setCursor(rightX + 115, cardY + 18);
        canvas->printf("%+4d%%", throttle);

        // Barra bipolar Eixo X (w=170, centro em x=225)
        int barY1 = cardY + 28;
        canvas->fillRect(rightX + 6, barY1, 170, 6, UI_BLACK);
        canvas->drawRect(rightX + 6, barY1, 170, 6, UI_CARD_BG);
        int barCenterX = rightX + 6 + 85; // 225
        canvas->drawFastVLine(barCenterX, barY1 - 1, 8, UI_WHITE);
        if (throttle > 0) {
            int bw = map(throttle, 0, 100, 0, 83);
            canvas->fillRect(barCenterX + 1, barY1 + 1, bw, 4, UI_GREEN);
        } else if (throttle < 0) {
            int bw = map(-throttle, 0, 100, 0, 83);
            canvas->fillRect(barCenterX - bw, barY1 + 1, bw, 4, UI_RED);
        }

        // 2. EIXO Y (Rudder / Leme - IO1)
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(rightX + 6, cardY + 38);
        canvas->print("Y (LEME): ");
        canvas->setTextColor(UI_WHITE);
        canvas->printf("%4d", rawY);
        uint16_t rColor = (rudder != 0) ? UI_CYAN : UI_LIGHTGREY;
        canvas->setTextColor(rColor);
        canvas->setCursor(rightX + 115, cardY + 38);
        canvas->printf("%+4d%%", rudder);

        // Barra bipolar Eixo Y (w=170, centro em x=225)
        int barY2 = cardY + 48;
        canvas->fillRect(rightX + 6, barY2, 170, 6, UI_BLACK);
        canvas->drawRect(rightX + 6, barY2, 170, 6, UI_CARD_BG);
        canvas->drawFastVLine(barCenterX, barY2 - 1, 8, UI_WHITE);
        if (rudder > 0) {
            int bw = map(rudder, 0, 100, 0, 83);
            canvas->fillRect(barCenterX + 1, barY2 + 1, bw, 4, UI_CYAN);
        } else if (rudder < 0) {
            int bw = map(-rudder, 0, 100, 0, 83);
            canvas->fillRect(barCenterX - bw, barY2 + 1, bw, 4, UI_ORANGE);
        }

        // Linha divisória subtil
        canvas->drawFastHLine(rightX + 6, cardY + 58, 170, UI_CARD_BG);

        // 3. ESTADO DOS 4 BOTÕES FÍSICOS (Grid 2x2)
        int btnW = 82;
        int btnH = 14;
        int btnCol1 = rightX + 6;
        int btnCol2 = rightX + 94;
        int btnRow1 = cardY + 62;
        int btnRow2 = cardY + 79;

        // Botão 1: JOY SW / BOOT (GPIO 9)
        if (btnJoy) {
            canvas->fillRect(btnCol1, btnRow1, btnW, btnH, UI_GREEN);
            canvas->setTextColor(UI_BLACK);
            canvas->setCursor(btnCol1 + 4, btnRow1 + 3);
            canvas->print("JOY SW: ON");
        } else {
            canvas->fillRect(btnCol1, btnRow1, btnW, btnH, UI_NAVY);
            canvas->drawRect(btnCol1, btnRow1, btnW, btnH, UI_CARD_BG);
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->setCursor(btnCol1 + 4, btnRow1 + 3);
            canvas->print("JOY SW: OFF");
        }

        // Botão 2: MODE (GPIO 19)
        if (btnMode) {
            canvas->fillRect(btnCol2, btnRow1, btnW, btnH, UI_YELLOW);
            canvas->setTextColor(UI_BLACK);
            canvas->setCursor(btnCol2 + 4, btnRow1 + 3);
            canvas->print("MODE: ON");
        } else {
            canvas->fillRect(btnCol2, btnRow1, btnW, btnH, UI_NAVY);
            canvas->drawRect(btnCol2, btnRow1, btnW, btnH, UI_CARD_BG);
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->setCursor(btnCol2 + 4, btnRow1 + 3);
            canvas->print("MODE: OFF");
        }

        // Botão 3: ANCHOR UP (GPIO 18)
        if (btnUp) {
            canvas->fillRect(btnCol1, btnRow2, btnW, btnH, UI_CYAN);
            canvas->setTextColor(UI_BLACK);
            canvas->setCursor(btnCol1 + 4, btnRow2 + 3);
            canvas->print("ANC UP: ON");
        } else {
            canvas->fillRect(btnCol1, btnRow2, btnW, btnH, UI_NAVY);
            canvas->drawRect(btnCol1, btnRow2, btnW, btnH, UI_CARD_BG);
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->setCursor(btnCol1 + 4, btnRow2 + 3);
            canvas->print("ANC UP: OFF");
        }

        // Botão 4: ANCHOR DOWN (GPIO 20)
        if (btnDown) {
            canvas->fillRect(btnCol2, btnRow2, btnW, btnH, UI_ORANGE);
            canvas->setTextColor(UI_BLACK);
            canvas->setCursor(btnCol2 + 4, btnRow2 + 3);
            canvas->print("ANC DN: ON");
        } else {
            canvas->fillRect(btnCol2, btnRow2, btnW, btnH, UI_NAVY);
            canvas->drawRect(btnCol2, btnRow2, btnW, btnH, UI_CARD_BG);
            canvas->setTextColor(UI_LIGHTGREY);
            canvas->setCursor(btnCol2 + 4, btnRow2 + 3);
            canvas->print("ANC DN: OFF");
        }

        // 4. INFORMAÇÕES DE CALIBRAÇÃO & CONTADOR DE CLIQUES
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(rightX + 6, cardY + 97);
        canvas->printf("Cliques SW: ");
        canvas->setTextColor(UI_YELLOW);
        canvas->printf("%u", joyClicks);

        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(rightX + 90, cardY + 97);
        canvas->printf("Deadband: +-%d", deadband);

        canvas->setCursor(rightX + 6, cardY + 108);
        canvas->print("Calibracao: 2048 +/- 180 (12-bit)");

        // -------------------------------------------------------------
        // RODAPÉ GLOBAL DE INSTRUÇÕES DE NAVEGAÇÃO (Altura: 22px)
        // -------------------------------------------------------------
        canvas->fillRect(0, 150, SCREEN_W, 22, UI_NAVY);
        canvas->setTextSize(1);
        canvas->setTextColor(UI_YELLOW);
        canvas->setCursor(8, 156);
        canvas->print("SEGURA JOY (1s) OU MODE: Sair | BOOT: Prox");
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
        canvas->setCursor(120, 8);
        if (currentPage == BASE_PAGE_CONTROL) {
            canvas->setTextColor(UI_YELLOW);
            canvas->print("[1.COMANDO]");
        } else if (currentPage == BASE_PAGE_FLEET) {
            canvas->setTextColor(UI_CYAN);
            canvas->print("[2.FROTA]");
        } else if (currentPage == BASE_PAGE_NETWORK) {
            canvas->setTextColor(UI_GREEN);
            canvas->print("[3.REDE/IP]");
        } else if (currentPage == BASE_PAGE_CLOUD) {
            canvas->setTextColor(UI_CYAN);
            canvas->print("[4.PORTAL]");
        } else if (currentPage == BASE_PAGE_JOYSTICK) {
            canvas->setTextColor(UI_YELLOW);
            canvas->print("[5.TESTE JOY]");
        }

        // Ícones de Estado: SD, WiFi, Cloud
        canvas->fillCircle(236, 12, 4, sdOk ? UI_CYAN : UI_DARKGREY);
        canvas->fillCircle(250, 12, 4, isApMode ? UI_ORANGE : (wifiOk ? UI_GREEN : UI_RED));
        canvas->fillCircle(264, 12, 4, cloudOk ? UI_GREEN : UI_DARKGREY);

        // Pontos indicadores de página (BASE_PAGE_COUNT páginas)
        for (uint8_t i = 0; i < BASE_PAGE_COUNT; ++i) {
            int dotX = 278 + i * 8;
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
