#pragma once

#include "Display_ST7789.h"
#include "base_version.h"
#include "config_common.h"
#include "fleet_manager.h"
#include "logo_bmp.h"
#include "network_config.h"
#include "qrcode_helper.h"
#include "wifi_config_manager.h"
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Preferences.h>
#include <WiFi.h>
#include <canvas/Arduino_Canvas.h>

// Standard 16-bit RGB565 color definitions
#define UI_BLACK 0x0000
#define UI_WHITE 0xFFFF
#define UI_RED 0xF800
#define UI_GREEN 0x07E0
#define UI_BLUE 0x001F
#define UI_CYAN 0x07FF
#define UI_MAGENTA 0xF81F
#define UI_YELLOW 0xFFE0
#define UI_ORANGE 0xFD20
#define UI_LIGHTGREY 0xC618
#define UI_DARKGREY 0x4208
#define UI_NAVY 0x0841
#define UI_CARD_BG 0x18E3
#define UI_PANEL_BG 0x10A2

enum BaseScreenPage : uint8_t {
  BASE_PAGE_HOME =
      0, // Home Menu (Logótipo à esquerda + itens de menu à direita)
  BASE_PAGE_MISSION_MENU = 1,  // Menu Missão: Estado, Lista de Rovers
  BASE_PAGE_MISSION_STATE = 2, // Missão -> Estado (alterar estado da missão)
  BASE_PAGE_ROVER_LIST = 3,    // Missão -> Lista de Rovers
  BASE_PAGE_ROVER_MENU =
      4, // Missão -> Lista de Rovers -> Rover Selecionado (Modo, Comando)
  BASE_PAGE_ROVER_MODE =
      5, // Rover Selecionado -> Modo (Manual, Hold, Return Home)
  BASE_PAGE_ROVER_CONTROL =
      6, // Rover Selecionado -> Comando (ecrã manual direto com telemetria)
  BASE_PAGE_CONFIG_MENU = 7, // Menu Configurações: WiFi, Joystick, ID, Firmware
  BASE_PAGE_CONFIG_WIFI = 8, // Configurações -> WiFi
  BASE_PAGE_CONFIG_JOY = 9,  // Configurações -> Joystick
  BASE_PAGE_CONFIG_ID = 10,  // Configurações -> ID (ecrã com QRCode)
  BASE_PAGE_CONFIG_FW = 11,  // Configurações -> Firmware
  BASE_PAGE_CONFIG_WIFI_LIST =
      12, // Configurações -> WiFi -> Lista (Scan + Redes Guardadas)
  BASE_PAGE_CONFIG_WIFI_SCAN =
      13, // Configurações -> WiFi -> Lista -> Varrimento de Redes
  BASE_PAGE_CONFIG_WIFI_PASS =
      14, // Configurações -> WiFi -> Scan -> Password QWERTY
  BASE_PAGE_CONFIG_PORTAL =
      15, // Configurações -> Portal (Estado da Ligação ao Portal Cloud)
  BASE_PAGE_CONFIG_NAME =
      16, // Configurações -> Nome (Alterar Nome da Estação Base)
  BASE_PAGE_ROVER_REMOVE =
      17, // Rover Selecionado -> Remover (Ecrã de Confirmação)
  BASE_PAGE_COUNT = 18,

  // Aliases de compatibilidade
  BASE_PAGE_STATION = BASE_PAGE_CONFIG_ID,
  BASE_PAGE_WIFI = BASE_PAGE_CONFIG_WIFI,
  BASE_PAGE_ROVER = BASE_PAGE_ROVER_CONTROL,
  BASE_PAGE_JOYSTICK = BASE_PAGE_CONFIG_JOY
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
      : canvas(nullptr), currentPage(BASE_PAGE_HOME), lastRender(0),
        _bootLogCount(0), _bootProgress(0), _brightness(85),
        _joyScreenEnabled(true), _otaActive(false), _homeMenuIdx(0),
        _missionMenuIdx(0), _stateCursorIdx(0), _roverListIdx(0),
        _roverMenuIdx(0), _roverModeIdx(0), _roverRemoveIdx(0),
        _configMenuIdx(0), _wifiListIdx(0), _wifiScanIdx(0), _wifiKbRow(0),
        _wifiKbCol(0), _wifiKbMode(0), _wifiTextCursorPos(0),
        _wifiConfig(nullptr), _controlExitPromptActive(false),
        _controlExitCursorIdx(0), _bootHoldPct(0), _feedbackUntilMs(0) {
    strncpy(_c6Version, "Desconhecido", sizeof(_c6Version));
    _feedbackMsg[0] = '\0';
    _wifiTargetSsid[0] = '\0';
    _wifiPassBuf[0] = '\0';
  }

  void begin(uint8_t initialBrightness = 85) {
    // Desativar CS do cartão MicroSD onboard para evitar colisão SPI
    pinMode(PIN_SD_CS, OUTPUT);
    digitalWrite(PIN_SD_CS, HIGH);

    // Carregar configurações guardadas na NVS (brilho)
    Preferences prefs;
    if (prefs.begin("base_display", true)) {
      _brightness = prefs.getUChar("brightness", initialBrightness);
      _joyScreenEnabled = prefs.getBool("joy_scr", true);
      prefs.end();
    } else {
      _brightness = initialBrightness;
    }

    // Inicializar driver de hardware ST7789
    display.begin();
    setBrightness(_brightness);

    // Criar canvas em RAM configurado em Landscape (320x172)
    if (!canvas) {
      canvas = new Arduino_Canvas(LCD_WIDTH, LCD_HEIGHT, nullptr, 0, 0,
                                  DISPLAY_ROTATION);
    }
    if (canvas) {
      canvas->begin();
      canvas->setTextWrap(false);
      canvas->fillScreen(UI_BLACK);
      display.drawPixelBuffer(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1,
                              canvas->getFramebuffer());
    }
  }

  void setBrightness(uint8_t brightness) {
    if (brightness > 100)
      brightness = 100;
    _brightness = brightness;
    display.setBacklight(_brightness);
  }

  void saveBrightness(uint8_t brightness) {
    setBrightness(brightness);
    Preferences prefs;
    if (prefs.begin("base_display", false)) {
      prefs.putUChar("brightness", _brightness);
      prefs.end();
    }
  }

  uint8_t getBrightness() const { return _brightness; }

  // =========================================================================
  // BOOT SCREEN (ARRANQUE) - LANDSCAPE 320x172
  // =========================================================================
  void showBootScreen() {
    if (!canvas)
      return;
    _bootLogCount = 0;
    _bootProgress = 0;

    canvas->fillScreen(UI_BLACK);

    // Painel Esquerdo: Logótipo + Marca + Título Base + Barra de Progresso
    // (w=101, h=164)
    renderBrandLeftPanel(true, BASE_OP_STORAGE);

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

    display.drawPixelBuffer(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1,
                            canvas->getFramebuffer());
  }

  void bootLog(const char *msg, uint16_t color = UI_LIGHTGREY,
               uint8_t progressPct = 0) {
    if (!canvas)
      return;

    if (_bootLogCount < BOOT_MAX_LINES) {
      strncpy(_bootLogs[_bootLogCount].text, msg,
              sizeof(_bootLogs[_bootLogCount].text) - 1);
      _bootLogs[_bootLogCount].text[sizeof(_bootLogs[_bootLogCount].text) - 1] =
          '\0';
      _bootLogs[_bootLogCount].color = color;
      _bootLogCount++;
    } else {
      memmove(&_bootLogs[0], &_bootLogs[1],
              sizeof(BootLogItem) * (BOOT_MAX_LINES - 1));
      strncpy(_bootLogs[BOOT_MAX_LINES - 1].text, msg,
              sizeof(_bootLogs[BOOT_MAX_LINES - 1].text) - 1);
      _bootLogs[BOOT_MAX_LINES - 1]
          .text[sizeof(_bootLogs[BOOT_MAX_LINES - 1].text) - 1] = '\0';
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

    display.drawPixelBuffer(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1,
                            canvas->getFramebuffer());
  }

  void bootLogf(uint16_t color, uint8_t progressPct, const char *format, ...) {
    char buf[36];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    bootLog(buf, color, progressPct);
  }

  void endBootScreen(uint32_t delayMs = 1200) {
    bootLogf(UI_GREEN, 100, "[SYS ] Base operacional!");
    if (delayMs > 0)
      delay(delayMs);
    currentPage = BASE_PAGE_HOME;
    lastRender = 0;
  }

  // =========================================================================
  // CONTROLO DE PÁGINAS E CURSORES DE MENU
  // =========================================================================
  void setJoystickScreenEnabled(bool enabled) {
    _joyScreenEnabled = enabled;
    lastRender = 0;
  }

  bool isJoystickScreenEnabled() const { return _joyScreenEnabled; }

  void setPage(BaseScreenPage page) {
    if (page < BASE_PAGE_COUNT) {
      currentPage = page;
    } else {
      currentPage = BASE_PAGE_HOME;
    }
    lastRender = 0;
  }

  BaseScreenPage getCurrentPage() const { return currentPage; }

  uint8_t getHomeMenuIdx() const { return _homeMenuIdx; }
  void setHomeMenuIdx(uint8_t idx) {
    _homeMenuIdx = idx % 2;
    lastRender = 0;
  }

  uint8_t getMissionMenuIdx() const { return _missionMenuIdx; }
  void setMissionMenuIdx(uint8_t idx) {
    _missionMenuIdx = idx % 2;
    lastRender = 0;
  }

  uint8_t getStateCursorIdx() const { return _stateCursorIdx; }
  void setStateCursorIdx(uint8_t idx) {
    _stateCursorIdx = idx % 4;
    lastRender = 0;
  }

  uint8_t getRoverListIdx() const { return _roverListIdx; }
  void setRoverListIdx(uint8_t idx, size_t totalRovers) {
    if (totalRovers == 0)
      _roverListIdx = 0;
    else
      _roverListIdx = idx % totalRovers;
    lastRender = 0;
  }

  uint8_t getRoverMenuIdx() const { return _roverMenuIdx; }
  void setRoverMenuIdx(uint8_t idx) {
    _roverMenuIdx = idx % 3;
    lastRender = 0;
  }

  uint8_t getRoverRemoveIdx() const { return _roverRemoveIdx; }
  void setRoverRemoveIdx(uint8_t idx) {
    _roverRemoveIdx = idx % 2;
    lastRender = 0;
  }

  uint8_t getRoverModeIdx() const { return _roverModeIdx; }
  void setRoverModeIdx(uint8_t idx) {
    _roverModeIdx = idx % 3;
    lastRender = 0;
  }

  uint8_t getConfigMenuIdx() const { return _configMenuIdx; }
  void setConfigMenuIdx(uint8_t idx) {
    _configMenuIdx = idx % 6;
    lastRender = 0;
  }

  void setWiFiConfigManager(WiFiConfigManager *wcm) { _wifiConfig = wcm; }
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
    _wifiKbMode = 0; // 0 = abc, 1 = ABC, 2 = 123/#
    lastRender = 0;
  }

  void beginStationNameEntry(const char *currentName) {
    _wifiTargetSsid[0] = '\0';
    if (currentName) {
      strncpy(_wifiPassBuf, currentName, 24);
      _wifiPassBuf[24] = '\0';
    } else {
      _wifiPassBuf[0] = '\0';
    }
    _wifiTextCursorPos = (uint8_t)strlen(_wifiPassBuf);
    _wifiKbRow = 0;
    _wifiKbCol = 0;
    _wifiKbMode = 1; // 1 = ABC (maiúsculas por defeito para nome da estação)
    lastRender = 0;
  }

  const char *getWifiTargetSsid() const { return _wifiTargetSsid; }
  const char *getWifiPassBuf() const { return _wifiPassBuf; }
  uint8_t getWifiKbRow() const { return _wifiKbRow; }
  uint8_t getWifiKbCol() const { return _wifiKbCol; }
  uint8_t getWifiKbMode() const { return _wifiKbMode; }
  uint8_t getWifiTextCursorPos() const { return _wifiTextCursorPos; }

  void insertCharAtWifiCursor(char ch) {
    size_t len = strlen(_wifiPassBuf);
    if (len >= sizeof(_wifiPassBuf) - 1)
      return;
    if (_wifiTextCursorPos > len)
      _wifiTextCursorPos = (uint8_t)len;
    memmove(&_wifiPassBuf[_wifiTextCursorPos + 1],
            &_wifiPassBuf[_wifiTextCursorPos], len - _wifiTextCursorPos + 1);
    _wifiPassBuf[_wifiTextCursorPos] = ch;
    _wifiTextCursorPos++;
    lastRender = 0;
  }

  void moveWifiKbCursor(int8_t dx, int8_t dy) {
    size_t len = strlen(_wifiPassBuf);
    if (_wifiTextCursorPos > len)
      _wifiTextCursorPos = (uint8_t)len;

    // _wifiKbRow: 0..2 = teclas QWERTY, 3 = barra de ações, 4 = Caixa de Texto
    if (dy != 0) {
      int nextRow = ((int)_wifiKbRow + 5 + dy) % 5;
      if (_wifiKbRow < 3 && nextRow == 3) {
        // Mapear coluna 0..9 para 0..5
        _wifiKbCol = (_wifiKbCol * 6) / 10;
      } else if (_wifiKbRow == 3 && nextRow < 3) {
        // Mapear coluna 0..5 para 0..9
        const uint8_t mapTo10[6] = {0, 2, 4, 6, 7, 9};
        _wifiKbCol = mapTo10[_wifiKbCol % 6];
      } else if (_wifiKbRow == 4 && nextRow == 3) {
        if (_wifiKbCol >= 6)
          _wifiKbCol = (_wifiKbCol * 6) / 10;
      } else if (_wifiKbRow == 4 && nextRow < 3) {
        if (_wifiKbCol >= 10)
          _wifiKbCol = 9;
      }
      _wifiKbRow = (uint8_t)nextRow;
    }

    if (dx != 0) {
      if (_wifiKbRow == 4) {
        // Na caixa de texto: X move a posição do cursor de edição (0..len)
        int nextPos = (int)_wifiTextCursorPos + dx;
        if (nextPos < 0)
          nextPos = 0;
        if (nextPos > (int)len)
          nextPos = (int)len;
        _wifiTextCursorPos = (uint8_t)nextPos;
      } else {
        uint8_t maxCols = (_wifiKbRow == 3) ? 6 : 10;
        _wifiKbCol = (uint8_t)(((int)_wifiKbCol + maxCols + dx) % maxCols);
      }
    }
    lastRender = 0;
  }

  void backspaceWifiPass() {
    size_t len = strlen(_wifiPassBuf);
    if (len == 0)
      return;
    if (_wifiTextCursorPos > len)
      _wifiTextCursorPos = (uint8_t)len;
    if (_wifiTextCursorPos > 0) {
      memmove(&_wifiPassBuf[_wifiTextCursorPos - 1],
              &_wifiPassBuf[_wifiTextCursorPos], len - _wifiTextCursorPos + 1);
      _wifiTextCursorPos--;
    } else {
      memmove(&_wifiPassBuf[0], &_wifiPassBuf[1], len);
    }
    lastRender = 0;
  }

  // Retorna: 0 = tecla normal processada, 1 = utilizador escolheu SAIR, 2 =
  // utilizador escolheu LIGAR/GUARDAR
  uint8_t pressWifiKbKey() {
    static const char *const kbRows[3][3] = {
        {"qwertyuiop", "asdfghjkl-", "zxcvbnm._@"},
        {"QWERTYUIOP", "ASDFGHJKL-", "ZXCVBNM._@"},
        {"1234567890", "!@#$%^&*()", "-_=+/?:;,.~"}};

    if (_wifiKbRow == 4) {
      // Clique na caixa de texto regressa ao teclado QWERTY
      _wifiKbRow = 0;
      lastRender = 0;
      return 0;
    } else if (_wifiKbRow < 3) {
      uint8_t m = _wifiKbMode % 3;
      uint8_t c = _wifiKbCol % 10;
      char ch = kbRows[m][_wifiKbRow][c];
      insertCharAtWifiCursor(ch);
      return 0;
    } else {
      // Linha 3 de botões especiais:
      // 0: Aa (Shift), 1: 123# (Números/Símbolos), 2: ESP, 3: DEL, 4: SAIR, 5:
      // LIGAR/GUARDAR
      uint8_t act = _wifiKbCol % 6;
      if (act == 0) {
        // Alternar maiúsculas/minúsculas
        if (_wifiKbMode == 0)
          _wifiKbMode = 1;
        else
          _wifiKbMode = 0;
        lastRender = 0;
        return 0;
      } else if (act == 1) {
        // Alternar números/símbolos <-> letras
        if (_wifiKbMode == 2)
          _wifiKbMode = 0;
        else
          _wifiKbMode = 2;
        lastRender = 0;
        return 0;
      } else if (act == 2) {
        // Espaço
        insertCharAtWifiCursor(' ');
        return 0;
      } else if (act == 3) {
        // Apagar (Backspace)
        backspaceWifiPass();
        return 0;
      } else if (act == 4) {
        // SAIR
        return 1;
      } else {
        // LIGAR / GUARDAR
        return 2;
      }
    }
  }

  bool isControlExitPromptActive() const { return _controlExitPromptActive; }
  void setControlExitPromptActive(bool active) {
    _controlExitPromptActive = active;
    if (active)
      _controlExitCursorIdx = 0; // "SAIR" selecionado por defeito
    lastRender = 0;
  }

  uint8_t getControlExitCursorIdx() const { return _controlExitCursorIdx; }
  void setControlExitCursorIdx(uint8_t idx) {
    _controlExitCursorIdx = idx % 2;
    lastRender = 0;
  }

  void setBootHoldProgressPct(uint8_t pct) {
    if (_bootHoldPct != pct) {
      _bootHoldPct = pct;
      lastRender = 0;
    }
  }

  void showFeedback(const char *msg, uint32_t durationMs = 1500) {
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
  void update(FleetManager &fleet, bool wifiOk, bool cloudOk, int8_t throttle,
              int8_t rudder, uint8_t navMode, bool sdOk = false,
              bool isApMode = false, const char *ip = nullptr,
              const char *ssid = nullptr, int8_t wifiRssi = 0,
              uint8_t apClients = 0, const char *apPassword = nullptr,
              bool force = false, int lastHttpStatus = 0,
              uint32_t cloudSuccessCount = 0, uint32_t cloudFailCount = 0,
              uint32_t lastCloudSyncMs = 0, const char *macStr = nullptr,
              int rawX = 2048, int rawY = 2048, int deadband = 180,
              bool btnJoy = false, bool btnMode = false, bool btnUp = false,
              bool btnDown = false, uint16_t joyClicks = 0) {
    if (_otaActive)
      return;

    uint32_t now = millis();
    if (_feedbackUntilMs > 0 && now > _feedbackUntilMs) {
      _feedbackMsg[0] = '\0';
      _feedbackUntilMs = 0;
      force = true;
    }

    uint32_t minInterval = (currentPage == BASE_PAGE_CONFIG_JOY ||
                            currentPage == BASE_PAGE_ROVER_CONTROL)
                               ? 60
                               : 180;
    if (!force && (now - lastRender < minInterval))
      return;
    lastRender = now;

    if (!canvas)
      return;

    canvas->fillScreen(UI_BLACK);

    if (currentPage == BASE_PAGE_HOME) {
      renderHomePage(fleet, wifiOk, cloudOk, sdOk, isApMode);
    } else {
      renderHeader(fleet, wifiOk, cloudOk, sdOk, isApMode);
      switch (currentPage) {
      case BASE_PAGE_MISSION_MENU:
        renderMissionMenuPage(fleet);
        break;
      case BASE_PAGE_MISSION_STATE:
        renderMissionStatePage(fleet);
        break;
      case BASE_PAGE_ROVER_LIST:
        renderRoverListPage(fleet);
        break;
      case BASE_PAGE_ROVER_MENU:
        renderRoverMenuPage(fleet);
        break;
      case BASE_PAGE_ROVER_MODE:
        renderRoverModePage(fleet, navMode);
        break;
      case BASE_PAGE_ROVER_REMOVE:
        renderRoverRemovePage(fleet);
        break;
      case BASE_PAGE_ROVER_CONTROL:
        renderControlPage(fleet, throttle, rudder, navMode);
        break;
      case BASE_PAGE_CONFIG_MENU:
        renderConfigMenuPage(fleet, isApMode, ip, ssid, macStr, cloudOk,
                             lastHttpStatus);
        break;
      case BASE_PAGE_CONFIG_WIFI:
        renderNetworkPage(isApMode, ip, ssid, wifiRssi, apClients, apPassword);
        break;
      case BASE_PAGE_CONFIG_JOY:
        renderJoystickPage(rawX, rawY, throttle, rudder, deadband, btnJoy,
                           btnMode, btnUp, btnDown, joyClicks);
        break;
      case BASE_PAGE_CONFIG_ID:
        renderStationPage(fleet, macStr);
        break;
      case BASE_PAGE_CONFIG_NAME:
        renderWifiPassPage("GUARDAR");
        break;
      case BASE_PAGE_CONFIG_FW:
        renderFirmwarePage();
        break;
      case BASE_PAGE_CONFIG_PORTAL:
        renderPortalPage(wifiOk, cloudOk, lastHttpStatus, cloudSuccessCount,
                         cloudFailCount, lastCloudSyncMs, fleet, macStr);
        break;
      case BASE_PAGE_CONFIG_WIFI_LIST:
        renderWifiListPage();
        break;
      case BASE_PAGE_CONFIG_WIFI_SCAN:
        renderWifiScanPage();
        break;
      case BASE_PAGE_CONFIG_WIFI_PASS:
        renderWifiPassPage("LIGAR");
        break;
      default:
        renderHomePage(fleet, wifiOk, cloudOk, sdOk, isApMode);
        break;
      }
    }

    display.drawPixelBuffer(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1,
                            canvas->getFramebuffer());
  }

private:
  const char *getActiveFeedback() const {
    if (_feedbackUntilMs > 0 && millis() <= _feedbackUntilMs &&
        _feedbackMsg[0] != '\0') {
      return _feedbackMsg;
    }
    return nullptr;
  }

  uint16_t getOpStateColor(BaseOperationState opState) const {
    switch (opState) {
    case BASE_OP_STORAGE:
      return UI_LIGHTGREY;
    case BASE_OP_WAITING_ROVERS:
      return UI_ORANGE;
    case BASE_OP_IDLE:
      return UI_CYAN;
    case BASE_OP_GO:
      return UI_GREEN;
    default:
      return UI_LIGHTGREY;
    }
  }

  void renderBrandLeftPanel(bool isBoot, BaseOperationState opState) {
    canvas->fillRect(4, 4, 101, 164, UI_PANEL_BG);
    canvas->drawRect(4, 4, 101, 164, UI_CARD_BG);

    // Logótipo Dragão centrado (85x80) em Ciano
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

    if (isBoot) {
      // Moldura da barra de progresso no fundo do painel esquerdo
      canvas->drawRect(12, 150, 85, 10, UI_CARD_BG);
      canvas->fillRect(14, 152, 81, 6, UI_BLACK);
    } else {
      // Badge do estado da missão no rodapé do painel esquerdo
      uint16_t stCol = getOpStateColor(opState);
      canvas->fillRect(10, 148, 89, 15, UI_BLACK);
      canvas->drawRect(10, 148, 89, 15, stCol);
      canvas->setTextSize(1);
      canvas->setTextColor(stCol);
      const char *stStr = get_base_op_state_str(opState);
      int textW = strlen(stStr) * 6;
      int tx = 10 + (89 - textW) / 2;
      if (tx < 12)
        tx = 12;
      canvas->setCursor(tx, 152);
      canvas->print(stStr);
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
  void renderHomePage(FleetManager &fleet, bool wifiOk, bool cloudOk, bool sdOk,
                      bool isApMode) {
    // 1. Painel Esquerdo com o Logótipo (idêntico ao Boot)
    renderBrandLeftPanel(false, fleet.getOperationState());

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

    // Indicadores de Estado no topo direito (SD, WiFi, Cloud)
    canvas->fillCircle(272, 14, 4, sdOk ? UI_CYAN : UI_DARKGREY);
    canvas->fillCircle(286, 14, 4,
                       isApMode ? UI_ORANGE : (wifiOk ? UI_GREEN : UI_RED));
    canvas->fillCircle(300, 14, 4, cloudOk ? UI_GREEN : UI_DARKGREY);

    // Contagem de Rovers Online
    uint8_t onlineCnt = 0;
    for (const auto &r : fleet.getRovers()) {
      if (r.is_online)
        onlineCnt++;
    }

    // Item 0: Missão
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
      canvas->print("MISSAO");

      canvas->setTextColor(sel ? UI_CYAN : UI_LIGHTGREY);
      canvas->setCursor(x + w - 18, y + 16);
      canvas->print(">");

      canvas->setTextSize(1);
      canvas->setTextColor(sel ? UI_WHITE : UI_LIGHTGREY);
      canvas->setCursor(x + 10, y + 34);
      canvas->printf("Est: %s | Rovers: %u/%u", fleet.getOperationStateBadge(),
                     onlineCnt, (unsigned)fleet.getRovers().size());
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
      canvas->print("WiFi, Joystick, ID, Firmware");
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
      canvas->print("Y: Iterar Menu | X->: Entrar");
    }
  }

  // =========================================================================
  // CABEÇALHO GLOBAL PARA SUBMENUS E PÁGINAS (TOPO 24px)
  // =========================================================================
  void renderHeader(FleetManager &fleet, bool wifiOk, bool cloudOk, bool sdOk,
                    bool isApMode) {
    canvas->fillRect(0, 0, SCREEN_W, 24, UI_NAVY);
    canvas->setTextSize(1);

    FleetRover *selRover = fleet.getSelectedRover();
    const char *rCode = selRover ? selRover->code : "ROVER";

    // Título / Breadcrumb à esquerda
    canvas->setCursor(8, 8);
    canvas->setTextColor(UI_CYAN);
    switch (currentPage) {
    case BASE_PAGE_MISSION_MENU:
      canvas->print("HOME > ");
      canvas->setTextColor(UI_YELLOW);
      canvas->print("MISSAO");
      break;
    case BASE_PAGE_MISSION_STATE:
      canvas->print("MISSAO > ");
      canvas->setTextColor(UI_YELLOW);
      canvas->print("ESTADO");
      break;
    case BASE_PAGE_ROVER_LIST:
      canvas->print("MISSAO > ");
      canvas->setTextColor(UI_YELLOW);
      canvas->print("LISTA DE ROVERS");
      break;
    case BASE_PAGE_ROVER_MENU:
      canvas->print("ROVERS > ");
      canvas->setTextColor(UI_YELLOW);
      canvas->printf("%s", rCode);
      break;
    case BASE_PAGE_ROVER_MODE:
      canvas->printf("%s > ", rCode);
      canvas->setTextColor(UI_YELLOW);
      canvas->print("MODO");
      break;
    case BASE_PAGE_ROVER_REMOVE:
      canvas->printf("%s > ", rCode);
      canvas->setTextColor(UI_RED);
      canvas->print("REMOVER");
      break;
    case BASE_PAGE_ROVER_CONTROL:
      canvas->printf("%s > ", rCode);
      canvas->setTextColor(UI_YELLOW);
      canvas->print("COMANDO");
      break;
    case BASE_PAGE_CONFIG_MENU:
      canvas->print("HOME > ");
      canvas->setTextColor(UI_YELLOW);
      canvas->print("CONFIGURACOES");
      break;
    case BASE_PAGE_CONFIG_WIFI:
      canvas->print("CONFIG > ");
      canvas->setTextColor(UI_YELLOW);
      canvas->print("WIFI");
      break;
    case BASE_PAGE_CONFIG_JOY:
      canvas->print("CONFIG > ");
      canvas->setTextColor(UI_YELLOW);
      canvas->print("JOYSTICK");
      break;
    case BASE_PAGE_CONFIG_ID:
      canvas->print("CONFIG > ");
      canvas->setTextColor(UI_YELLOW);
      canvas->print("ID (QRCODE)");
      break;
    case BASE_PAGE_CONFIG_NAME:
      canvas->print("CONFIG > ");
      canvas->setTextColor(UI_YELLOW);
      canvas->print("ALTERAR NOME");
      break;
    case BASE_PAGE_CONFIG_FW:
      canvas->print("CONFIG > ");
      canvas->setTextColor(UI_YELLOW);
      canvas->print("FIRMWARE");
      break;
    case BASE_PAGE_CONFIG_PORTAL:
      canvas->print("CONFIG > ");
      canvas->setTextColor(UI_YELLOW);
      canvas->print("PORTAL");
      break;
    case BASE_PAGE_CONFIG_WIFI_LIST:
      canvas->print("WIFI > ");
      canvas->setTextColor(UI_YELLOW);
      canvas->print("LISTA");
      break;
    case BASE_PAGE_CONFIG_WIFI_SCAN:
      canvas->print("WIFI > ");
      canvas->setTextColor(UI_YELLOW);
      canvas->print("SCAN REDES");
      break;
    case BASE_PAGE_CONFIG_WIFI_PASS:
      canvas->print("WIFI > ");
      canvas->setTextColor(UI_YELLOW);
      canvas->printf("PASS: %.16s", _wifiTargetSsid);
      break;
    default:
      canvas->print("WINDDRAGONS BASE");
      break;
    }

    // Badge de Estado da Estação Base (STOR, WAIT, IDLE, GO)
    BaseOperationState opState = fleet.getOperationState();
    uint16_t opColor = getOpStateColor(opState);
    const char *opBadge = fleet.getOperationStateBadge();

    canvas->fillRect(226, 4, 34, 15, UI_BLACK);
    canvas->drawRect(226, 4, 34, 15, opColor);
    canvas->setTextSize(1);
    canvas->setTextColor(opColor);
    canvas->setCursor(opState == BASE_OP_GO ? 237 : 231, 8);
    canvas->print(opBadge);

    // Ícones de Estado: SD, WiFi, Cloud
    canvas->fillCircle(272, 12, 4, sdOk ? UI_CYAN : UI_DARKGREY);
    canvas->fillCircle(288, 12, 4,
                       isApMode ? UI_ORANGE : (wifiOk ? UI_GREEN : UI_RED));
    canvas->fillCircle(304, 12, 4, cloudOk ? UI_GREEN : UI_DARKGREY);
  }

  // =========================================================================
  // SUBMENU: MISSÃO (Topo: Circuito em letra grande | Abaixo: Estado, Rovers em 1 linha)
  // =========================================================================
  void renderMissionMenuPage(FleetManager &fleet) {
    BaseOperationState opState = fleet.getOperationState();
    uint16_t stCol = getOpStateColor(opState);

    uint8_t onlineCnt = 0;
    for (const auto &r : fleet.getRovers()) {
      if (r.is_online)
        onlineCnt++;
    }

    // --- Topo da página: Nome do Circuito em letra grande (x=6, y=27, w=308, h=44) ---
    canvas->fillRect(6, 27, 308, 44, UI_PANEL_BG);
    canvas->drawRect(6, 27, 308, 44, UI_CARD_BG);

    canvas->setTextSize(1);
    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(14, 32);
    canvas->print("CIRCUITO:");

    canvas->setTextSize(2);
    canvas->setTextColor(UI_YELLOW);
    canvas->setCursor(14, 46);
    canvas->printf("%.24s", fleet.getMissionName());

    // --- Lista de itens de 1 linha (Estado, Rovers) ---
    // Item 0: Estado
    {
      bool sel = (_missionMenuIdx == 0);
      int x = 6, y = 76, w = 308, h = 32;
      if (sel) {
        canvas->fillRect(x, y, w, h, UI_NAVY);
        canvas->drawRect(x, y, w, h, UI_CYAN);
        canvas->fillRect(x, y, 4, h, UI_CYAN);
      }

      canvas->setTextSize(2);
      canvas->setTextColor(sel ? UI_YELLOW : UI_WHITE);
      canvas->setCursor(x + 12, y + 8);
      canvas->print("Estado");

      canvas->setTextSize(1);
      canvas->setTextColor(stCol);
      canvas->setCursor(x + 140, y + 12);
      canvas->printf("%.20s", fleet.getOperationStateStr());

      canvas->setTextSize(2);
      canvas->setTextColor(sel ? UI_CYAN : UI_DARKGREY);
      canvas->setCursor(x + w - 18, y + 8);
      canvas->print(">");
    }

    // Item 1: Rovers
    {
      bool sel = (_missionMenuIdx == 1);
      int x = 6, y = 112, w = 308, h = 32;
      if (sel) {
        canvas->fillRect(x, y, w, h, UI_NAVY);
        canvas->drawRect(x, y, w, h, UI_CYAN);
        canvas->fillRect(x, y, 4, h, UI_CYAN);
      }

      canvas->setTextSize(2);
      canvas->setTextColor(sel ? UI_YELLOW : UI_WHITE);
      canvas->setCursor(x + 12, y + 8);
      canvas->print("Rovers");

      canvas->setTextSize(1);
      canvas->setTextColor(onlineCnt > 0 ? UI_GREEN : UI_LIGHTGREY);
      canvas->setCursor(x + 140, y + 12);
      canvas->printf("%u / %u ONLINE", onlineCnt,
                     (unsigned)fleet.getRovers().size());

      canvas->setTextSize(2);
      canvas->setTextColor(sel ? UI_CYAN : UI_DARKGREY);
      canvas->setCursor(x + w - 18, y + 8);
      canvas->print(">");
    }

    renderFooter("<-X: Home | Y: Mover | X->: Entrar");
  }

  // =========================================================================
  // MISSÃO -> ESTADO (PERMITE MUDAR O ESTADO DA MISSÃO)
  // =========================================================================
  void renderMissionStatePage(FleetManager &fleet) {
    BaseOperationState activeState = fleet.getOperationState();

    const BaseOperationState states[4] = {
        BASE_OP_STORAGE, BASE_OP_WAITING_ROVERS, BASE_OP_IDLE, BASE_OP_GO};

    for (uint8_t i = 0; i < 4; ++i) {
      BaseOperationState st = states[i];
      bool isCursor = (_stateCursorIdx == i);
      bool isActive = (activeState == st);
      uint16_t stCol = getOpStateColor(st);

      int x = 6;
      int y = 27 + i * 30;
      int w = 308;
      int h = 28;

      canvas->fillRect(x, y, w, h, isCursor ? UI_NAVY : UI_PANEL_BG);
      canvas->drawRect(x, y, w, h,
                       isCursor ? UI_CYAN : (isActive ? stCol : UI_CARD_BG));
      if (isCursor) {
        canvas->fillRect(x, y, 4, h, UI_CYAN);
      }

      // Nome do estado em tamanho grande (sem 2ª linha)
      canvas->setTextSize(2);
      canvas->setTextColor(isCursor ? UI_YELLOW : stCol);
      canvas->setCursor(x + 10, y + 6);
      canvas->printf("%u. %s", i + 1, get_base_op_state_str(st));

      canvas->setTextSize(1);
      if (isActive) {
        canvas->fillRect(x + w - 66, y + 6, 56, 16, UI_BLACK);
        canvas->drawRect(x + w - 66, y + 6, 56, 16, UI_GREEN);
        canvas->setTextColor(UI_GREEN);
        canvas->setCursor(x + w - 58, y + 10);
        canvas->print("[ATIVO]");
      } else if (isCursor) {
        canvas->setTextColor(UI_CYAN);
        canvas->setCursor(x + w - 62, y + 10);
        canvas->print("X-> SET");
      }
    }

    renderFooter("<-X: Missao | Y: Estado | X->: Aplicar");
  }

  // =========================================================================
  // MISSÃO -> LISTA DE ROVERS
  // =========================================================================
  void renderRoverListPage(FleetManager &fleet) {
    const auto &rovers = fleet.getRovers();
    if (rovers.empty()) {
      canvas->fillRect(6, 30, 308, 114, UI_PANEL_BG);
      canvas->drawRect(6, 30, 308, 114, UI_CARD_BG);
      canvas->setTextSize(2);
      canvas->setTextColor(UI_YELLOW);
      canvas->setCursor(40, 65);
      canvas->print("SEM ROVERS NA FROTA");
      canvas->setTextSize(1);
      canvas->setTextColor(UI_LIGHTGREY);
      canvas->setCursor(40, 95);
      canvas->print("Configure os rovers no Portal ou Web.");
      renderFooter("<-X: Voltar ao Menu Missao");
      return;
    }

    uint8_t total = (uint8_t)rovers.size();
    if (_roverListIdx >= total)
      _roverListIdx = 0;

    // Janela deslizante de até 4 rovers visíveis
    const uint8_t visibleRows = 4;
    uint8_t startIdx = 0;
    if (_roverListIdx >= visibleRows) {
      startIdx = _roverListIdx - (visibleRows - 1);
    }

    int rowW = (total > visibleRows) ? 298 : 308;

    for (uint8_t row = 0; row < visibleRows && (startIdx + row) < total;
         ++row) {
      uint8_t idx = startIdx + row;
      const auto &r = rovers[idx];
      bool sel = (idx == _roverListIdx);

      int x = 6;
      int y = 27 + row * 30;
      int h = 28;

      canvas->fillRect(x, y, rowW, h, sel ? UI_NAVY : UI_PANEL_BG);
      canvas->drawRect(x, y, rowW, h, sel ? UI_CYAN : UI_CARD_BG);
      if (sel) {
        canvas->fillRect(x, y, 4, h, UI_CYAN);
      }

      // LED Online/Offline
      canvas->fillCircle(x + 12, y + 14, 4, r.is_online ? UI_GREEN : UI_RED);

      // Código e Nome
      canvas->setTextSize(1);
      canvas->setTextColor(sel ? UI_YELLOW : UI_WHITE);
      canvas->setCursor(x + 22, y + 5);
      canvas->printf("%s", r.code);

      if (strlen(r.role) > 0) {
        canvas->setTextColor(UI_CYAN);
        canvas->setCursor(x + 82, y + 5);
        canvas->printf("[%.12s]", r.role);
      }

      // Linha inferior do item: Nome + Modo/Telemetria
      canvas->setTextColor(UI_LIGHTGREY);
      canvas->setCursor(x + 22, y + 16);
      if (r.is_online) {
        uint16_t bCol = (r.battery_pct < 25)
                            ? UI_RED
                            : ((r.battery_pct < 50) ? UI_YELLOW : UI_GREEN);
        canvas->setTextColor(bCol);
        canvas->printf("Bat:%u%%", r.battery_pct);
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->printf(" | %ddBm | %s", r.rssi,
                       get_motor_status_str(r.motor_status));
      } else {
        canvas->printf("%.16s | OFFLINE", strlen(r.name) > 0 ? r.name : r.code);
      }

      // Seta à direita
      canvas->setTextSize(2);
      canvas->setTextColor(sel ? UI_CYAN : UI_DARKGREY);
      canvas->setCursor(x + rowW - 16, y + 6);
      canvas->print(">");
    }

    // Barra de scroll vertical se houver mais de 4 rovers
    if (total > visibleRows) {
      int sbX = 308, sbY = 27, sbW = 6, sbH = 118;
      canvas->drawRect(sbX, sbY, sbW, sbH, UI_CARD_BG);
      int thumbH = max(16, sbH * visibleRows / total);
      int thumbY =
          sbY + (sbH - thumbH) * _roverListIdx / max(1, (int)total - 1);
      canvas->fillRect(sbX + 1, thumbY, sbW - 2, thumbH, UI_CYAN);
    }

    renderFooter("<-X: Missao | Y: Rover | X->: Comandos");
  }

  // =========================================================================
  // SUBMENU DO ROVER SELECIONADO (-Modo, -Comando)
  // =========================================================================
  void renderRoverMenuPage(FleetManager &fleet) {
    FleetRover *r = fleet.getSelectedRover();
    if (!r) {
      renderRoverListPage(fleet);
      return;
    }

    int cardY = 26;
    int cardH = 122;

    // --- Painel Esquerdo: Resumo do Rover Selecionado (x=4, w=142, h=122) ---
    canvas->fillRect(4, cardY, 142, cardH, UI_PANEL_BG);
    canvas->drawRect(4, cardY, 142, cardH, UI_CARD_BG);

    canvas->setTextSize(2);
    canvas->setTextColor(UI_YELLOW);
    canvas->setCursor(10, cardY + 6);
    canvas->printf("%.10s", r->code);

    canvas->setTextSize(1);
    canvas->fillCircle(134, cardY + 12, 4, r->is_online ? UI_GREEN : UI_RED);

    canvas->setTextColor(UI_CYAN);
    canvas->setCursor(10, cardY + 24);
    canvas->printf("Papel: %.14s", strlen(r->role) > 0 ? r->role : "N/A");

    canvas->drawFastHLine(8, cardY + 35, 134, UI_CARD_BG);

    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(10, cardY + 40);
    canvas->print("EST: ");
    canvas->setTextColor(r->is_online ? UI_GREEN : UI_RED);
    if (r->is_online) {
      canvas->printf("ONLINE (%ddBm)", r->rssi);
    } else {
      canvas->print("OFFLINE");
    }

    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(10, cardY + 54);
    canvas->print("BAT: ");
    uint16_t bCol = (r->battery_pct < 25)
                        ? UI_RED
                        : ((r->battery_pct < 50) ? UI_YELLOW : UI_GREEN);
    canvas->setTextColor(bCol);
    canvas->printf("%u%% (%.1fV)", r->battery_pct, r->battery_voltage);

    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(10, cardY + 68);
    canvas->print("MOD: ");
    canvas->setTextColor(UI_YELLOW);
    canvas->printf("%.14s", get_motor_status_str(r->motor_status));

    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(10, cardY + 82);
    canvas->print("ANC: ");
    canvas->setTextColor(UI_CYAN);
    canvas->printf("%s (%.1fm)", get_anchor_status_str(r->anchor_status),
                   r->anchor_depth_m);

    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(10, cardY + 96);
    if (r->lat != 0.0 || r->lng != 0.0) {
      canvas->printf("%.4f, %.4f", r->lat, r->lng);
    } else {
      canvas->print("GPS: Sem Fix");
    }

    // --- Painel Direito: Opções de 1 linha (Modo, Comando, Remover) (x=150, w=166, h=122) ---
    canvas->fillRect(150, cardY, 166, cardH, UI_PANEL_BG);
    canvas->drawRect(150, cardY, 166, cardH, UI_CARD_BG);

    const char *menuTitles[3] = {"Modo", "Comando", "Remover"};
    for (uint8_t i = 0; i < 3; ++i) {
      bool sel = (_roverMenuIdx == i);
      int x = 154;
      int y = 30 + i * 39;
      int w = 158;
      int h = 34;
      uint16_t accentCol = (i == 2) ? UI_RED : UI_CYAN;

      if (sel) {
        canvas->fillRect(x, y, w, h, UI_NAVY);
        canvas->drawRect(x, y, w, h, accentCol);
        canvas->fillRect(x, y, 4, h, accentCol);
      }

      canvas->setTextSize(2);
      canvas->setTextColor(sel ? UI_YELLOW : ((i == 2) ? UI_RED : UI_WHITE));
      canvas->setCursor(x + 10, y + 9);
      canvas->print(menuTitles[i]);

      canvas->setTextColor(sel ? accentCol : UI_DARKGREY);
      canvas->setCursor(x + w - 16, y + 9);
      canvas->print(">");
    }

    renderFooter("<-X: Rovers | Y: Opcao | X->: Entrar");
  }

  // =========================================================================
  // =========================================================================
  // ROVER SELECIONADO -> REMOVER (ECRÃ DE CONFIRMAÇÃO IGUAL AO DO JOYSTICK)
  // =========================================================================
  void renderRoverRemovePage(FleetManager &fleet) {
    FleetRover *r = fleet.getSelectedRover();
    if (!r) {
      renderRoverListPage(fleet);
      return;
    }

    // Desenhar o fundo do menu do Rover por trás do modal
    renderRoverMenuPage(fleet);

    // Popup centralizado idêntico ao de saída do Joystick: REMOVER | CANCELAR
    int mx = 22, my = 36, mw = 276, mh = 96;
    canvas->fillRect(mx - 2, my - 2, mw + 4, mh + 4, UI_BLACK);
    canvas->fillRect(mx, my, mw, mh, UI_PANEL_BG);
    canvas->drawRect(mx, my, mw, mh, UI_CYAN);

    canvas->fillRect(mx + 1, my + 1, mw - 2, 20, UI_NAVY);
    canvas->setTextSize(1);
    canvas->setTextColor(UI_YELLOW);
    char titleBuf[44];
    snprintf(titleBuf, sizeof(titleBuf), "REMOVER %s DA FROTA?", r->code);
    int titleW = (int)strlen(titleBuf) * 6;
    int tx = mx + max(8, (mw - titleW) / 2);
    canvas->setCursor(tx, my + 7);
    canvas->print(titleBuf);

    // Botão 0: REMOVER
    bool selRem = (_roverRemoveIdx == 0);
    int b1x = mx + 12, b1y = my + 32, b1w = 118, b1h = 38;
    canvas->fillRect(b1x, b1y, b1w, b1h, selRem ? UI_NAVY : UI_CARD_BG);
    canvas->drawRect(b1x, b1y, b1w, b1h, selRem ? UI_CYAN : UI_DARKGREY);
    if (selRem) {
      canvas->drawRect(b1x + 1, b1y + 1, b1w - 2, b1h - 2, UI_YELLOW);
    }
    canvas->setTextSize(2);
    canvas->setTextColor(selRem ? UI_YELLOW : UI_LIGHTGREY);
    canvas->setCursor(b1x + 17, b1y + 11);
    canvas->print("REMOVER");

    // Separador "|"
    canvas->setTextSize(2);
    canvas->setTextColor(UI_DARKGREY);
    canvas->setCursor(mx + 134, b1y + 11);
    canvas->print("|");

    // Botão 1: CANCELAR
    bool selCanc = (_roverRemoveIdx == 1);
    int b2x = mx + 146, b2y = my + 32, b2w = 118, b2h = 38;
    canvas->fillRect(b2x, b2y, b2w, b2h, selCanc ? UI_NAVY : UI_CARD_BG);
    canvas->drawRect(b2x, b2y, b2w, b2h, selCanc ? UI_CYAN : UI_DARKGREY);
    if (selCanc) {
      canvas->drawRect(b2x + 1, b2y + 1, b2w - 2, b2h - 2, UI_YELLOW);
    }
    canvas->setTextSize(2);
    canvas->setTextColor(selCanc ? UI_GREEN : UI_LIGHTGREY);
    canvas->setCursor(b2x + 11, b2y + 11);
    canvas->print("CANCELAR");

    // Dica na base do popup
    canvas->setTextSize(1);
    canvas->setTextColor(UI_CYAN);
    canvas->setCursor(mx + 34, my + 78);
    canvas->print("Y/X: Escolher  |  BOOT: Confirmar");

    renderFooter("REMOVER | CANCELAR -> Clique BOOT p/ confirmar");
  }

  // =========================================================================
  // ROVER SELECIONADO -> MODO (Manual, Hold, Return Home)
  // =========================================================================
  void renderRoverModePage(FleetManager &fleet, uint8_t currentNavMode) {
    (void)currentNavMode;
    FleetRover *r = fleet.getSelectedRover();
    if (!r) {
      renderRoverListPage(fleet);
      return;
    }

    int cardY = 26;
    int cardH = 122;

    // Painel Esquerdo: Estado atual do Rover (x=4, w=116, h=122)
    canvas->fillRect(4, cardY, 116, cardH, UI_PANEL_BG);
    canvas->drawRect(4, cardY, 116, cardH, UI_CARD_BG);

    canvas->setTextSize(2);
    canvas->setTextColor(UI_YELLOW);
    canvas->setCursor(10, cardY + 8);
    canvas->printf("%.8s", r->code);

    canvas->setTextSize(1);
    canvas->setTextColor(r->is_online ? UI_GREEN : UI_RED);
    canvas->setCursor(10, cardY + 28);
    canvas->print(r->is_online ? "ONLINE" : "OFFLINE");

    canvas->drawFastHLine(8, cardY + 40, 108, UI_CARD_BG);

    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(10, cardY + 46);
    canvas->print("MODO ATUAL:");
    canvas->setTextColor(UI_CYAN);
    canvas->setCursor(10, cardY + 58);
    canvas->printf("%.16s", get_motor_status_str(r->motor_status));

    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(10, cardY + 76);
    canvas->printf("BAT: %u%% (%.1fV)", r->battery_pct, r->battery_voltage);

    canvas->setCursor(10, cardY + 90);
    canvas->printf("ANC: %.1fm", r->anchor_depth_m);

    // Painel Direito: 3 Opções de Modo (Manual, Hold, Return Home)
    const char *modeTitles[3] = {"1. MANUAL", "2. HOLD", "3. RETURN HOME"};
    const char *modeDescs[3] = {"Conducao manual por joystick",
                                "Manter posicao GPS (Hold)",
                                "Regressar a base (RTL)"};

    for (uint8_t i = 0; i < 3; ++i) {
      bool sel = (_roverModeIdx == i);
      int x = 124;
      int y = 26 + i * 42;
      int w = 192;
      int h = 38;

      canvas->fillRect(x, y, w, h, sel ? UI_NAVY : UI_PANEL_BG);
      canvas->drawRect(x, y, w, h, sel ? UI_CYAN : UI_CARD_BG);
      if (sel)
        canvas->fillRect(x, y, 4, h, UI_CYAN);

      canvas->setTextSize(1);
      canvas->setTextColor(sel ? UI_YELLOW : UI_WHITE);
      canvas->setCursor(x + 10, y + 7);
      canvas->print(modeTitles[i]);

      canvas->setTextColor(sel ? UI_CYAN : UI_LIGHTGREY);
      canvas->setCursor(x + w - 48, y + 7);
      canvas->print(sel ? "X-> OK" : "");

      canvas->setTextColor(UI_LIGHTGREY);
      canvas->setCursor(x + 10, y + 21);
      canvas->print(modeDescs[i]);
    }

    renderFooter("<-X: Voltar | Y: Modo | X->: Enviar");
  }

  // =========================================================================
  // SUBMENU: CONFIGURAÇÕES (-WiFi, -Joystick, -ID, -Nome, -Firmware, -Portal)
  // =========================================================================
  void renderConfigMenuPage(FleetManager &fleet, bool isApMode, const char *ip,
                            const char *ssid, const char *macStr,
                            bool cloudOk = false, int lastHttpStatus = 0) {
    (void)ssid;
    String mac =
        (macStr && strlen(macStr) > 0) ? String(macStr) : getDeviceMacAddress();

    const uint8_t total = 6;
    const uint8_t visibleRows = 4;
    const char *titles[6] = {"WIFI", "JOYSTICK", "ID", "NOME", "FIRMWARE",
                             "PORTAL"};

    uint8_t startIdx = 0;
    if (_configMenuIdx >= visibleRows) {
      startIdx = _configMenuIdx - (visibleRows - 1);
    }

    int rowW = (total > visibleRows) ? 298 : 308;

    for (uint8_t row = 0; row < visibleRows && (startIdx + row) < total;
         ++row) {
      uint8_t i = startIdx + row;
      bool sel = (_configMenuIdx == i);
      int x = 6;
      int y = 27 + row * 30;
      int h = 28;

      if (sel) {
        canvas->fillRect(x, y, rowW, h, UI_NAVY);
        canvas->drawRect(x, y, rowW, h, UI_CYAN);
        canvas->fillRect(x, y, 4, h, UI_CYAN);
      }

      canvas->setTextSize(2);
      canvas->setTextColor(sel ? UI_YELLOW : UI_WHITE);
      canvas->setCursor(x + 10, y + 6);
      canvas->print(titles[i]);

      canvas->setTextSize(1);
      if (i == 0) {
        canvas->setTextColor(isApMode ? UI_ORANGE : UI_GREEN);
        canvas->setCursor(x + 124, y + 10);
        canvas->printf("%s | %s", isApMode ? "AP" : "STA", ip ? ip : "0.0.0.0");
      } else if (i == 1) {
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(x + 124, y + 10);
        canvas->print("Mira 2D, ADC & Botoes");
      } else if (i == 2) {
        canvas->setTextColor(UI_CYAN);
        canvas->setCursor(x + 124, y + 10);
        canvas->printf("%s", mac.c_str());
      } else if (i == 3) {
        canvas->setTextColor(UI_YELLOW);
        canvas->setCursor(x + 124, y + 10);
        canvas->printf("%.22s", fleet.getStationName());
      } else if (i == 4) {
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(x + 124, y + 10);
        canvas->printf("M:%s | C6:%s", BASE_LAFVIN_VERSION, _c6Version);
      } else {
        canvas->setTextColor(cloudOk ? UI_GREEN : UI_LIGHTGREY);
        canvas->setCursor(x + 124, y + 10);
        if (cloudOk) {
          canvas->printf("ONLINE (HTTP %d)",
                         lastHttpStatus > 0 ? lastHttpStatus : 200);
        } else {
          canvas->print("winddragons.app");
        }
      }

      canvas->setTextSize(2);
      canvas->setTextColor(sel ? UI_CYAN : UI_DARKGREY);
      canvas->setCursor(x + rowW - 18, y + 6);
      canvas->print(">");
    }

    // Barra de scroll vertical (6 itens, 4 visíveis)
    int sbX = 308, sbY = 27, sbW = 6, sbH = 118;
    canvas->drawRect(sbX, sbY, sbW, sbH, UI_CARD_BG);
    int thumbH = max(16, sbH * visibleRows / total);
    int thumbY =
        sbY + (sbH - thumbH) * _configMenuIdx / max(1, (int)total - 1);
    canvas->fillRect(sbX + 1, thumbY, sbW - 2, thumbH, UI_CYAN);

    renderFooter("<-X: Home | Y: Mover | X->: Entrar");
  }

  // =========================================================================
  // CONFIGURAÇÕES -> WIFI
  // =========================================================================
  void renderNetworkPage(bool isApMode, const char *ip, const char *ssid,
                         int8_t wifiRssi, uint8_t apClients,
                         const char *apPassword = nullptr) {
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
      canvas->printf(" %.17s", ssid ? ssid : "WindDragons-Base");

      canvas->setTextColor(UI_LIGHTGREY);
      canvas->setCursor(190, cardY + 26);
      canvas->print("PASS:");
      canvas->setTextColor(UI_WHITE);
      canvas->printf(" %.12s", (apPassword && strlen(apPassword) > 0)
                                   ? apPassword
                                   : "12345678");

      canvas->setTextSize(3);
      canvas->setTextColor(UI_CYAN);
      canvas->setCursor(12, cardY + 46);
      canvas->print(ip ? ip : "192.168.4.1");
    }

    // Item selecionável "Lista" no espaço inferior para avançar para a lista de
    // redes guardadas
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

    renderFooter("<-X: Configuracoes | X->: Abrir Lista");
  }

  // =========================================================================
  // CONFIGURAÇÕES -> WIFI -> LISTA (ITEM 0 = SCAN, ITENS 1..N = REDES
  // GUARDADAS)
  // =========================================================================
  void renderWifiListPage() {
    size_t netsCount = _wifiConfig ? _wifiConfig->getNetworks().size() : 0;
    uint8_t total =
        (uint8_t)(1 + netsCount); // 0 = "Scan", 1..N = redes guardadas
    if (_wifiListIdx >= total)
      _wifiListIdx = 0;

    const uint8_t visibleRows = 4;
    uint8_t startIdx = 0;
    if (_wifiListIdx >= visibleRows) {
      startIdx = _wifiListIdx - (visibleRows - 1);
    }

    int rowW = (total > visibleRows) ? 298 : 308;

    for (uint8_t row = 0; row < visibleRows && (startIdx + row) < total;
         ++row) {
      uint8_t idx = startIdx + row;
      bool sel = (idx == _wifiListIdx);

      int x = 6;
      int y = 27 + row * 30;
      int h = 28;

      if (idx == 0) {
        // Item 0: "Scan" para varrimento de redes disponíveis
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

        // Nome da rede (SSID) em tamanho grande
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

    renderFooter("<-X: WiFi | Y: Item | X->: Selecionar");
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
      renderFooter("<-X: Voltar a Lista");
      return;
    }

    const auto &scanned = _wifiConfig->getScannedNetworks();
    uint8_t total = (uint8_t)scanned.size();
    if (_wifiScanIdx >= total)
      _wifiScanIdx = 0;

    const uint8_t visibleRows = 4;
    uint8_t startIdx = 0;
    if (_wifiScanIdx >= visibleRows) {
      startIdx = _wifiScanIdx - (visibleRows - 1);
    }

    int rowW = (total > visibleRows) ? 298 : 308;

    for (uint8_t row = 0; row < visibleRows && (startIdx + row) < total;
         ++row) {
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

    renderFooter("<-X: Lista | Y: Rede | X->: Selecionar");
  }

  // =========================================================================
  // CONFIGURAÇÕES -> WIFI -> SCAN -> INSERIR PASSWORD (TECLADO QWERTY)
  // =========================================================================
  void renderWifiPassPage(const char *confirmLabel = "LIGAR") {
    static const char *const kbRows[3][3] = {
        {"qwertyuiop", "asdfghjkl-", "zxcvbnm._@"},
        {"QWERTYUIOP", "ASDFGHJKL-", "ZXCVBNM._@"},
        {"1234567890", "!@#$%^&*()", "-_=+/?:;,.~"}};

    // 1. Caixa de texto (y=26..48, h=22) - selecionável como linha 4 (_wifiKbRow == 4)
    bool inTextBox = (_wifiKbRow == 4);
    canvas->fillRect(6, 26, 308, 22, inTextBox ? UI_NAVY : UI_PANEL_BG);
    canvas->drawRect(6, 26, 308, 22, inTextBox ? UI_YELLOW : UI_CYAN);
    if (inTextBox) {
      canvas->fillRect(6, 26, 4, 22, UI_YELLOW);
    }

    size_t passLen = strlen(_wifiPassBuf);
    uint8_t curPos = (_wifiTextCursorPos > passLen) ? (uint8_t)passLen
                                                    : _wifiTextCursorPos;
    const size_t maxVisChars = 22;
    size_t winStart = 0;
    if (curPos > maxVisChars) {
      winStart = curPos - maxVisChars;
    }

    canvas->setTextSize(2);
    for (size_t i = 0; i <= maxVisChars; ++i) {
      size_t pos = winStart + i;
      if (pos > passLen)
        break;
      int cx = 14 + (int)i * 12;
      bool isCur = (pos == curPos);

      if (pos < passLen) {
        char chStr[2] = {_wifiPassBuf[pos], '\0'};
        if (isCur) {
          canvas->fillRect(cx - 1, 28, 12, 18,
                           inTextBox ? UI_YELLOW : UI_CYAN);
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
        // Cursor no fim do texto
        if (inTextBox) {
          canvas->fillRect(cx, 29, 10, 16, UI_YELLOW);
        } else {
          canvas->setTextColor(UI_CYAN);
          canvas->setCursor(cx, 30);
          canvas->print("_");
        }
      }
    }

    // 2. Linhas 0..2 do Teclado QWERTY (10 colunas por linha)
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

    // 3. Linha 3: Botões de modificação (Maiúsculas, Números/Símbolos) e Ações
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
        // Botão de modificar números / símbolos em destaque
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
      renderFooter("XY: Mover | Clique: Tecla | Mode: Apagar");
    }
  }

public:
  void showWifiScanningScreen() {
    if (!canvas)
      return;
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
    display.drawPixelBuffer(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1,
                            canvas->getFramebuffer());
  }

  void showWifiConnectingScreen(const char *targetSsid) {
    if (!canvas)
      return;
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
    canvas->print("A LIGAR A REDE WIFI GUARDADA:");

    canvas->setTextSize(2);
    canvas->setTextColor(UI_YELLOW);
    canvas->setCursor(32, 76);
    canvas->printf("%.20s", targetSsid ? targetSsid : "");

    canvas->setTextSize(1);
    canvas->setTextColor(UI_CYAN);
    canvas->setCursor(32, 108);
    canvas->print("Aguarde (ate 8 segundos)...");

    renderFooter("A estabelecer ligacao WiFi STA...");
    display.drawPixelBuffer(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1,
                            canvas->getFramebuffer());
  }

private:
  // =========================================================================
  // CONFIGURAÇÕES -> JOYSTICK
  // =========================================================================
  void renderJoystickPage(int rawX, int rawY, int8_t throttle, int8_t rudder,
                          int deadband, bool btnJoy, bool btnMode, bool btnUp,
                          bool btnDown, uint16_t joyClicks) {
    int cardY = 26;
    int cardH = 122;

    // Painel Esquerdo: Radar 2D / Mira Analógica
    int leftW = 126;
    canvas->fillRect(4, cardY, leftW, cardH, UI_PANEL_BG);
    canvas->drawRect(4, cardY, leftW, cardH, UI_CARD_BG);

    canvas->fillRect(5, cardY + 1, leftW - 2, 14, UI_NAVY);
    canvas->setTextSize(1);
    canvas->setTextColor(UI_YELLOW);
    canvas->setCursor(14, cardY + 4);
    canvas->print("MIRA 2D ANALOGICA");

    int scopeX = 27;
    int scopeY = 44;
    int scopeSize = 80;
    int centerX = scopeX + scopeSize / 2;
    int centerY = scopeY + scopeSize / 2;

    canvas->fillRect(scopeX, scopeY, scopeSize, scopeSize, UI_BLACK);
    canvas->drawRect(scopeX, scopeY, scopeSize, scopeSize, UI_CARD_BG);

    canvas->drawFastHLine(scopeX + 1, centerY, scopeSize - 2, UI_DARKGREY);
    canvas->drawFastVLine(centerX, scopeY + 1, scopeSize - 2, UI_DARKGREY);

    canvas->drawCircle(centerX, centerY, 36, UI_DARKGREY);
    canvas->drawRect(centerX - 6, centerY - 6, 13, 13, 0x2124);

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

    int puckX = centerX + (rudder * 34 / 100);
    int puckY = centerY - (throttle * 34 / 100);
    puckX = constrain(puckX, scopeX + 3, scopeX + scopeSize - 4);
    puckY = constrain(puckY, scopeY + 3, scopeY + scopeSize - 4);

    if (puckX != centerX || puckY != centerY) {
      canvas->drawLine(centerX, centerY, puckX, puckY, UI_CARD_BG);
    }

    bool inDeadband = (throttle == 0 && rudder == 0);
    if (inDeadband) {
      canvas->fillCircle(puckX, puckY, 4, UI_LIGHTGREY);
      canvas->drawCircle(puckX, puckY, 4, UI_WHITE);
    } else {
      uint16_t puckColor =
          (throttle != 0) ? ((throttle > 0) ? UI_GREEN : UI_RED) : UI_CYAN;
      canvas->fillCircle(puckX, puckY, 5, puckColor);
      canvas->fillCircle(puckX, puckY, 2, UI_WHITE);
      canvas->drawCircle(puckX, puckY, 6, UI_YELLOW);
    }

    canvas->setCursor(8, cardY + 107);
    if (inDeadband) {
      canvas->setTextColor(UI_GREEN);
      canvas->print("[ CENTRO / NEUTRO ]");
    } else {
      canvas->setTextColor(UI_YELLOW);
      canvas->printf("T:%+3d%%  R:%+3d%%", throttle, rudder);
    }

    // Painel Direito: Métricas ADC e Botões
    int rightX = 134;
    int rightW = 182;
    canvas->fillRect(rightX, cardY, rightW, cardH, UI_PANEL_BG);
    canvas->drawRect(rightX, cardY, rightW, cardH, UI_CARD_BG);

    canvas->fillRect(rightX + 1, cardY + 1, rightW - 2, 14, UI_NAVY);
    canvas->setTextColor(UI_YELLOW);
    canvas->setCursor(rightX + 18, cardY + 4);
    canvas->print("SINAIS ADC & BOTOES");

    // 1. EIXO Y
    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(rightX + 6, cardY + 18);
    canvas->print("IO0(Y): ");
    canvas->setTextColor(UI_WHITE);
    canvas->printf("%4d", rawY);
    uint16_t tColor =
        (throttle > 0) ? UI_GREEN : ((throttle < 0) ? UI_RED : UI_LIGHTGREY);
    canvas->setTextColor(tColor);
    canvas->setCursor(rightX + 115, cardY + 18);
    canvas->printf("%+4d%%", throttle);

    int barY1 = cardY + 28;
    canvas->fillRect(rightX + 6, barY1, 170, 6, UI_BLACK);
    canvas->drawRect(rightX + 6, barY1, 170, 6, UI_CARD_BG);
    int barCenterX = rightX + 6 + 85;
    canvas->drawFastVLine(barCenterX, barY1 - 1, 8, UI_WHITE);
    if (throttle > 0) {
      int bw = map(throttle, 0, 100, 0, 83);
      canvas->fillRect(barCenterX + 1, barY1 + 1, bw, 4, UI_GREEN);
    } else if (throttle < 0) {
      int bw = map(-throttle, 0, 100, 0, 83);
      canvas->fillRect(barCenterX - bw, barY1 + 1, bw, 4, UI_RED);
    }

    // 2. EIXO X
    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(rightX + 6, cardY + 38);
    canvas->print("IO3(X): ");
    canvas->setTextColor(UI_WHITE);
    canvas->printf("%4d", rawX);
    uint16_t rColor = (rudder != 0) ? UI_CYAN : UI_LIGHTGREY;
    canvas->setTextColor(rColor);
    canvas->setCursor(rightX + 115, cardY + 38);
    canvas->printf("%+4d%%", rudder);

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

    canvas->drawFastHLine(rightX + 6, cardY + 58, 170, UI_CARD_BG);

    // 3. ESTADO DOS 4 BOTÕES FÍSICOS
    int btnW = 82;
    int btnH = 14;
    int btnCol1 = rightX + 6;
    int btnCol2 = rightX + 94;
    int btnRow1 = cardY + 62;
    int btnRow2 = cardY + 79;

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

    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(rightX + 6, cardY + 97);
    canvas->print("Cliques SW: ");
    canvas->setTextColor(UI_YELLOW);
    canvas->printf("%u", joyClicks);

    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(rightX + 90, cardY + 97);
    canvas->printf("Deadband: +-%d", deadband);

    canvas->setCursor(rightX + 6, cardY + 108);
    canvas->print("HOLD BOOT (3s) p/ voltar");

    renderHoldBootExitModalOrFooter("SAIR DO ECRA DE JOYSTICK?");
  }

  // =========================================================================
  // CONFIGURAÇÕES -> ID (ECRÃ COM QRCODE)
  // =========================================================================
  void renderStationPage(FleetManager &fleet, const char *macStr = nullptr) {
    (void)fleet;
    int cardY = 26;
    int cardH = 122;
    String mac =
        (macStr && strlen(macStr) > 0) ? String(macStr) : getDeviceMacAddress();

    // Painel Esquerdo: QR Code do MAC (x=4..102, w=98, h=122)
    canvas->fillRect(4, cardY, 98, cardH, UI_PANEL_BG);
    canvas->drawRect(4, cardY, 98, cardH, UI_CARD_BG);
    drawQrCode(canvas, mac.c_str(), 7, cardY + 6, 92, 92);

    canvas->setTextSize(1);
    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(18, cardY + 106);
    canvas->print("QR MAC ID");

    // Painel Direito: Nome da Estação e MAC Address (x=105..317, w=212, h=122)
    int rx = 105;
    int rw = 212;
    canvas->fillRect(rx, cardY, rw, cardH, UI_PANEL_BG);
    canvas->drawRect(rx, cardY, rw, cardH, UI_CARD_BG);

    canvas->setTextSize(1);
    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(rx + 6, cardY + 16);
    canvas->print("NOME DA ESTACAO:");

    canvas->setTextSize(2);
    canvas->setTextColor(UI_YELLOW);
    canvas->setCursor(rx + 6, cardY + 30);
    canvas->printf("%.16s", fleet.getStationName());

    canvas->drawFastHLine(rx + 4, cardY + 60, rw - 8, UI_CARD_BG);

    canvas->setTextSize(1);
    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(rx + 6, cardY + 72);
    canvas->print("MAC-ADDRESS:");

    canvas->setTextSize(2);
    canvas->setTextColor(UI_CYAN);
    canvas->setCursor(rx + 4, cardY + 86);
    canvas->printf("%s", mac.c_str());

    renderFooter("<-X: Voltar a Configuracoes");
  }

  // =========================================================================
  // CONFIGURAÇÕES -> FIRMWARE
  // =========================================================================
  void renderFirmwarePage() {
    // Card 1: Master Lafvin (LCD, SD, WiFi, WebServer)
    int x = 6;
    int w = 308;
    int c1y = 28;
    int ch = 56;

    canvas->fillRect(x, c1y, w, ch, UI_PANEL_BG);
    canvas->drawRect(x, c1y, w, ch, UI_CARD_BG);
    canvas->fillRect(x, c1y, 4, ch, UI_CYAN);

    canvas->setTextSize(1);
    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(x + 12, c1y + 8);
    canvas->print("MASTER LAFVIN (LCD / WIFI / WEB):");

    canvas->setTextSize(2);
    canvas->setTextColor(UI_CYAN);
    canvas->setCursor(x + 12, c1y + 22);
    canvas->printf("%s", BASE_LAFVIN_VERSION);

    canvas->setTextSize(1);
    canvas->setTextColor(UI_WHITE);
    canvas->setCursor(x + 140, c1y + 28);
    canvas->printf("Build: %s", BASE_BUILD_DATE);

    // Card 2: Coprocessador C6 (LoRa 433MHz)
    int c2y = 90;
    canvas->fillRect(x, c2y, w, ch, UI_PANEL_BG);
    canvas->drawRect(x, c2y, w, ch, UI_CARD_BG);
    canvas->fillRect(x, c2y, 4, ch, UI_YELLOW);

    canvas->setTextSize(1);
    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(x + 12, c2y + 8);
    canvas->print("COPROCESSADOR C6 (LORA 433MHZ):");

    canvas->setTextSize(2);
    canvas->setTextColor(UI_YELLOW);
    canvas->setCursor(x + 12, c2y + 22);
    canvas->printf("%s", _c6Version);

    canvas->setTextSize(1);
    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(x + 140, c2y + 28);
    canvas->print("UART Inter-ESP 115200");

    renderFooter("<-X: Voltar a Configuracoes");
  }

  // =========================================================================
  // CONFIGURAÇÕES -> PORTAL (ESTADO DA LIGAÇÃO AO PORTAL CLOUD)
  // =========================================================================
  void renderPortalPage(bool wifiOk, bool cloudOk, int lastHttpStatus,
                        uint32_t successCount, uint32_t failCount,
                        uint32_t lastSyncMs, FleetManager &fleet,
                        const char *macStr = nullptr) {
    (void)macStr;
    int cardY = 26;
    int cardH = 122;

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

    // Linha 1: ENDPOINT DO PORTAL
    canvas->setTextSize(1);
    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(12, cardY + 25);
    canvas->print("PORTAL:");
    canvas->setTextColor(UI_WHITE);
    canvas->setCursor(65, cardY + 25);
    canvas->print("winddragons.app/api/circuits");

    // Linha 2: Missão / Circuito sincronizado
    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(12, cardY + 40);
    canvas->print("MISSAO:");
    canvas->setTextColor(UI_CYAN);
    canvas->setCursor(65, cardY + 40);
    canvas->printf("%.38s", fleet.getMissionName());

    // Linha 3: Estatísticas de Sincronização (3 Caixas)
    int boxY = cardY + 56;
    int boxH = 36;

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
      if (r.is_online)
        onlineCnt++;
    }
    canvas->printf("%u / %u", onlineCnt, (unsigned)fleet.getRovers().size());

    // Linha 4: Rodapé interno do card
    canvas->setTextSize(1);
    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(12, cardY + 102);
    uint32_t agoSec = (millis() >= lastSyncMs && lastSyncMs > 0)
                          ? (millis() - lastSyncMs) / 1000
                          : 0;
    canvas->printf("Ciclo: 3s | Ultimo sync: ha %us | Est: %s", agoSec,
                   fleet.getOperationStateBadge());

    renderFooter("<-X: Voltar a Configuracoes");
  }

  // =========================================================================
  // ROVER SELECIONADO -> COMANDO (ECRÃ MANUAL DIRETO COM TELEMETRIA)
  // =========================================================================
  void renderControlPage(FleetManager &fleet, int8_t throttle, int8_t rudder,
                         uint8_t navMode) {
    int cardY = 26;
    int cardH = 122;
    FleetRover *selected = fleet.getSelectedRover();

    if (!selected) {
      renderRoverListPage(fleet);
      return;
    }

    // Card Esquerdo: Telemetria do Rover Selecionado (w=154, h=122)
    canvas->fillRect(4, cardY, 154, cardH, UI_PANEL_BG);
    canvas->drawRect(4, cardY, 154, cardH, UI_CARD_BG);

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

    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(8, cardY + 41);
    canvas->print("VEL:");
    canvas->setTextColor(UI_CYAN);
    canvas->setCursor(34, cardY + 41);
    canvas->printf("%.1f kn | %.0f deg", selected->speed_knots,
                   selected->heading_deg);

    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(8, cardY + 54);
    canvas->print("BAT:");
    uint16_t bCol = (selected->battery_pct < 25)
                        ? UI_RED
                        : ((selected->battery_pct < 50) ? UI_YELLOW : UI_GREEN);
    canvas->setTextColor(bCol);
    canvas->setCursor(34, cardY + 54);
    canvas->printf("%u%% (%.1fV)", selected->battery_pct,
                   selected->battery_voltage);

    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(8, cardY + 67);
    canvas->print("MOT:");
    canvas->setTextColor(UI_YELLOW);
    canvas->setCursor(34, cardY + 67);
    canvas->print(get_motor_status_str(selected->motor_status));

    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(8, cardY + 80);
    canvas->print("ANC:");
    canvas->setTextColor(UI_CYAN);
    canvas->setCursor(34, cardY + 80);
    canvas->printf("%s (%.1fm)", get_anchor_status_str(selected->anchor_status),
                   selected->anchor_depth_m);

    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(8, cardY + 95);
    canvas->print("MOD:");
    canvas->setTextColor(navMode == 0 ? UI_GREEN : UI_CYAN);
    canvas->setCursor(34, cardY + 95);
    canvas->printf("[%s]",
                   navMode == 0 ? "MANUAL" : (navMode == 1 ? "HOLD" : "RTL"));

    // Card Direito: Painel de Controlo / Joystick Inputs (w=154, h=122)
    canvas->fillRect(162, cardY, 154, cardH, UI_PANEL_BG);
    canvas->drawRect(162, cardY, 154, cardH, UI_CARD_BG);

    canvas->setTextSize(1);
    canvas->setTextColor(UI_YELLOW);
    canvas->setCursor(168, cardY + 6);
    canvas->print("COMANDO DIRETO ROVER");

    canvas->drawFastHLine(166, cardY + 18, 146, UI_CARD_BG);

    // Throttle
    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(168, cardY + 24);
    canvas->printf("PROPULSAO: %+3d%%", throttle);

    canvas->drawRect(168, cardY + 36, 142, 10, UI_CARD_BG);
    int barT = map(throttle, -100, 100, 0, 138);
    canvas->fillRect(170, cardY + 38, 138, 6, UI_BLACK);
    if (barT > 0) {
      uint16_t tCol = (throttle >= 0) ? UI_GREEN : UI_RED;
      canvas->fillRect(170, cardY + 38, barT, 6, tCol);
    }

    // Rudder
    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(168, cardY + 53);
    canvas->printf("LEME / DIR: %+3d%%", rudder);

    canvas->drawRect(168, cardY + 65, 142, 10, UI_CARD_BG);
    int barR = map(rudder, -100, 100, 0, 138);
    canvas->fillRect(170, cardY + 67, 138, 6, UI_BLACK);
    if (barR > 0) {
      canvas->fillRect(170, cardY + 67, barR, 6, UI_BLUE);
    }

    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(168, cardY + 83);
    canvas->print("ANCORA: ");
    canvas->setTextColor(UI_WHITE);
    canvas->print("IO18:UP|IO20:DN");

    canvas->setTextColor(UI_GREEN);
    canvas->setCursor(168, cardY + 98);
    canvas->print("LoRa TX: 10Hz @ 433MHz");

    renderHoldBootExitModalOrFooter("SAIR DO ECRA DE COMANDO?");
  }

  void renderHoldBootExitModalOrFooter(const char *modalTitle) {
    if (_controlExitPromptActive) {
      // Popup centralizado: Opção SAIR | CONTINUAR
      int mx = 22, my = 36, mw = 276, mh = 96;
      canvas->fillRect(mx - 2, my - 2, mw + 4, mh + 4, UI_BLACK);
      canvas->fillRect(mx, my, mw, mh, UI_PANEL_BG);
      canvas->drawRect(mx, my, mw, mh, UI_CYAN);

      canvas->fillRect(mx + 1, my + 1, mw - 2, 20, UI_NAVY);
      canvas->setTextSize(1);
      canvas->setTextColor(UI_YELLOW);
      canvas->setCursor(mx + 46, my + 7);
      canvas->print(modalTitle ? modalTitle : "SAIR DO ECRA?");

      // Botão 0: SAIR
      bool selSair = (_controlExitCursorIdx == 0);
      int b1x = mx + 12, b1y = my + 32, b1w = 118, b1h = 38;
      canvas->fillRect(b1x, b1y, b1w, b1h, selSair ? UI_NAVY : UI_CARD_BG);
      canvas->drawRect(b1x, b1y, b1w, b1h, selSair ? UI_CYAN : UI_DARKGREY);
      if (selSair) {
        canvas->drawRect(b1x + 1, b1y + 1, b1w - 2, b1h - 2, UI_YELLOW);
      }
      canvas->setTextSize(2);
      canvas->setTextColor(selSair ? UI_YELLOW : UI_LIGHTGREY);
      canvas->setCursor(b1x + 35, b1y + 11);
      canvas->print("SAIR");

      // Separador "|"
      canvas->setTextSize(2);
      canvas->setTextColor(UI_DARKGREY);
      canvas->setCursor(mx + 134, b1y + 11);
      canvas->print("|");

      // Botão 1: CONTINUAR
      bool selCont = (_controlExitCursorIdx == 1);
      int b2x = mx + 146, b2y = my + 32, b2w = 118, b2h = 38;
      canvas->fillRect(b2x, b2y, b2w, b2h, selCont ? UI_NAVY : UI_CARD_BG);
      canvas->drawRect(b2x, b2y, b2w, b2h, selCont ? UI_CYAN : UI_DARKGREY);
      if (selCont) {
        canvas->drawRect(b2x + 1, b2y + 1, b2w - 2, b2h - 2, UI_YELLOW);
      }
      canvas->setTextSize(2);
      canvas->setTextColor(selCont ? UI_GREEN : UI_LIGHTGREY);
      canvas->setCursor(b2x + 5, b2y + 11);
      canvas->print("CONTINUAR");

      // Dica na base do popup
      canvas->setTextSize(1);
      canvas->setTextColor(UI_CYAN);
      canvas->setCursor(mx + 34, my + 78);
      canvas->print("Y/X: Escolher  |  BOOT: Confirmar");

      renderFooter("SAIR | CONTINUAR -> Clique BOOT p/ confirmar");
    } else if (_bootHoldPct > 0) {
      // Barra de progresso no rodapé enquanto mantém BOOT premido (3s)
      canvas->fillRect(0, 150, SCREEN_W, 22, UI_NAVY);
      canvas->setTextSize(1);
      canvas->setTextColor(UI_YELLOW);
      canvas->setCursor(8, 157);
      canvas->print("HOLD BOOT (3s):");

      int bx = 104, by = 155, bw = 204, bh = 12;
      canvas->drawRect(bx, by, bw, bh, UI_CYAN);
      canvas->fillRect(bx + 2, by + 2, bw - 4, bh - 4, UI_BLACK);
      int fillW = map(constrain((int)_bootHoldPct, 0, 100), 0, 100, 0, bw - 4);
      if (fillW > 0) {
        canvas->fillRect(bx + 2, by + 2, fillW, bh - 4, UI_YELLOW);
      }
    } else {
      renderFooter("HOLD BOOT (3s): Sair | Continuar");
    }
  }

public:
  void showOtaScreen(const char *targetModule, const char *stepTitle,
                     uint8_t progressPct, const char *detailMsg,
                     uint16_t accentColor = UI_CYAN) {
    if (!canvas)
      return;
    _otaActive = true;

    pinMode(PIN_SD_CS, OUTPUT);
    digitalWrite(PIN_SD_CS, HIGH);

    canvas->fillScreen(UI_BLACK);

    // 1. Barra de Topo (Altura: 24px)
    canvas->fillRect(0, 0, SCREEN_W, 24, UI_NAVY);
    canvas->setTextColor(UI_CYAN);
    canvas->setTextSize(1);
    canvas->setCursor(8, 8);
    canvas->print("WINDDRAGONS");
    canvas->setTextColor(UI_WHITE);
    canvas->print(" BASE");

    canvas->setTextSize(2);
    canvas->setTextColor(accentColor);
    canvas->setCursor(140, 4);
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
    canvas->printf("%s", targetModule ? targetModule : "BASE STATION");

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
      canvas->printf("Lafvin: %s | C6: %s", BASE_LAFVIN_VERSION, _c6Version);
    }

    // 4. Footer Inferior de Aviso (h=22)
    canvas->fillRect(0, 150, SCREEN_W, 22,
                     (accentColor == UI_RED) ? UI_RED : UI_NAVY);
    canvas->setTextSize(1);
    if (accentColor == UI_RED) {
      canvas->setTextColor(UI_WHITE);
      canvas->setCursor(12, 156);
      canvas->print("! ERRO NO OTA - VERIFICAR LOGS DA CONSOLA !");
    } else if (progressPct >= 100) {
      canvas->setTextColor(UI_GREEN);
      canvas->setCursor(12, 156);
      canvas->print("* CONCLUIDO COM SUCESSO! A REINICIAR BASE *");
    } else {
      canvas->setTextColor(UI_YELLOW);
      canvas->setCursor(10, 156);
      canvas->print("! ATENCAO: NAO DESLIGAR A ALIMENTACAO !");
    }

    display.drawPixelBuffer(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1,
                            canvas->getFramebuffer());
  }

  void setOtaActive(bool active) {
    _otaActive = active;
    if (!_otaActive) {
      lastRender = 0;
    }
  }

  bool isOtaActive() const { return _otaActive; }

  void setC6Version(const char *ver) {
    if (ver && strlen(ver) > 0) {
      strncpy(_c6Version, ver, sizeof(_c6Version) - 1);
      _c6Version[sizeof(_c6Version) - 1] = '\0';
    }
  }

  const char *getC6Version() const { return _c6Version; }

private:
  ST7789Display display;
  Arduino_Canvas *canvas;
  BaseScreenPage currentPage;
  uint32_t lastRender;
  BootLogItem _bootLogs[BOOT_MAX_LINES];
  uint8_t _bootLogCount;
  uint8_t _bootProgress;
  uint8_t _brightness;
  bool _joyScreenEnabled;
  bool _otaActive;
  char _c6Version[24];

  // Índices de cursores dos menus hierárquicos
  uint8_t _homeMenuIdx;
  uint8_t _missionMenuIdx;
  uint8_t _stateCursorIdx;
  uint8_t _roverListIdx;
  uint8_t _roverMenuIdx;
  uint8_t _roverModeIdx;
  uint8_t _roverRemoveIdx;
  uint8_t _configMenuIdx;
  uint8_t _wifiListIdx;
  uint8_t _wifiScanIdx;
  uint8_t _wifiKbRow;
  uint8_t _wifiKbCol;
  uint8_t _wifiKbMode;
  uint8_t _wifiTextCursorPos;
  char _wifiTargetSsid[33];
  char _wifiPassBuf[64];
  WiFiConfigManager *_wifiConfig;
  bool _controlExitPromptActive;
  uint8_t _controlExitCursorIdx;
  uint8_t _bootHoldPct;

  // Feedback temporário na barra inferior
  char _feedbackMsg[40];
  uint32_t _feedbackUntilMs;
};
