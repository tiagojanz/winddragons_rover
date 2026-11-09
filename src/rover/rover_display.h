#pragma once

#include "Display_ST7789.h"
#include "actuators.h"
#include "battery_monitor.h"
#include "config_common.h"
#include "gps_tracker.h"
#include "logo_bmp.h"
#include "lora_protocol.h"
#include "qrcode_helper.h"
#include "rover_version.h"
#include "wifi_config_manager.h"
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <canvas/Arduino_Canvas.h>

// Colors (RGB565)
#define UI_BLACK 0x0000
#define UI_WHITE 0xFFFF
#define UI_RED 0xF800
#define UI_GREEN 0x07E0
#define UI_BLUE 0x001F
#define UI_CYAN 0x07FF
#define UI_YELLOW 0xFFE0
#define UI_ORANGE 0xFD20
#define UI_MAGENTA 0xF81F
#define UI_PURPLE 0x780F
#define UI_LIGHTGREY 0xC618
#define UI_DARKGREY 0x4208
#define UI_NAVY 0x0841
#define UI_CARD_BG 0x18E3
#define UI_PANEL_BG 0x10A2

enum RoverScreenPage : uint8_t {
  ROVER_PAGE_HOME =
      0, // Home Menu (Logótipo à esquerda + Operação / Configurações à direita)
  ROVER_PAGE_OP_MENU =
      1, // Menu Operação: Topo Estado + itens (Navegação, Bateria, Atuadores)
  ROVER_PAGE_NAV =
      2, // Operação -> Navegação (GPS, Velocidade, Proa/BNO055, Coordenadas)
  ROVER_PAGE_POWER =
      3, // Operação -> Bateria (LiPo %, Tensão, V/célula, Corrente, Consumo)
  ROVER_PAGE_ACTUATORS =
      4, // Operação -> Atuadores (Motor ESC, Leme, Cremalheira, Âncora, LoRa)
  ROVER_PAGE_CONFIG_MENU =
      5, // Menu Configurações: WiFi, Bateria, JoyStick, ID, Firmware, LoRa
  ROVER_PAGE_CONFIG_WIFI = 6, // Configurações -> WiFi (Estado + item "Lista")
  ROVER_PAGE_CONFIG_WIFI_LIST =
      7, // Configurações -> WiFi -> Lista (Scan + Redes Guardadas)
  ROVER_PAGE_CONFIG_WIFI_SCAN =
      8, // Configurações -> WiFi -> Lista -> Scan de Redes
  ROVER_PAGE_CONFIG_WIFI_PASS =
      9, // Configurações -> WiFi -> Scan -> Password QWERTY
  ROVER_PAGE_CONFIG_ID =
      10, // Configurações -> ID (QRCode + Nome/ID + MAC Address)
  ROVER_PAGE_CONFIG_FW =
      11, // Configurações -> Firmware (Master Lafvin + Coprocessador C6)
  ROVER_PAGE_CONFIG_LORA =
      12, // Configurações -> LoRa (Diagnóstico Rádio 433MHz)
  ROVER_PAGE_CONFIG_BATT =
      13, // Configurações -> Bateria (Opções: 3S/4S/6S, mAh: numérico)
  ROVER_PAGE_CONFIG_BATT_CELLS =
      14, // Configurações -> Bateria -> Opções (3S, 4S, 6S)
  ROVER_PAGE_CONFIG_BATT_MAH =
      15, // Configurações -> Bateria -> mAh (Teclado Numérico)
  ROVER_PAGE_CONFIG_JOY = 16, // Configurações -> JoyStick (X: Inverter, Y:
                              // Inverter, Calibrar, Testar)
  ROVER_PAGE_CONFIG_JOY_TEST =
      17, // Configurações -> JoyStick -> Testar (Mira 2D, ADC & Botões)
  ROVER_PAGE_CONFIG_JOY_CAL =
      18, // Configurações -> JoyStick -> Calibrar (Wizard 5 passos)
  ROVER_PAGE_ANCHOR = 19, // Operação -> Âncora (Lançar c/ sensor, Libertar,
                          // Trancar, Recolher, Parar)
  ROVER_PAGE_CONFIG_NAV_WIZARD =
      20, // Configurações -> Teste Nav (Wizard 5 passos Bússola/GPS/RTL -> SD)
  ROVER_PAGE_CONFIG_MOTOR =
      21, // Configurações -> Motor (Sentido Normal / Invertido + Teste Pulso)
  ROVER_PAGE_CONFIG_COMPASS =
      22, // Configurações -> Calibrar Norte (Alinhar BNO055 a 0° c/ Garmin)
  ROVER_PAGE_COUNT = 23,

  // Aliases de compatibilidade
  ROVER_PAGE_NETWORK = ROVER_PAGE_CONFIG_WIFI,
  ROVER_PAGE_MAC = ROVER_PAGE_CONFIG_ID
};

struct NavWizardStepSample {
  bool recorded = false;
  uint32_t timestampMs = 0;
  bool gpsFix = false;
  uint8_t sats = 0;
  double lat = 0.0;
  double lng = 0.0;
  float gpsSpeedKnots = 0.0f;
  float gpsCourseDeg = 0.0f;
  bool compassActive = false;
  uint8_t i2cAddr = 0x28;
  float rawHeadingDeg = 0.0f;
  float offsetDeg = 0.0f;
  float fusedHeadingDeg = 0.0f;
  float rollDeg = 0.0f;
  float pitchDeg = 0.0f;
  uint8_t calSys = 0;
  uint8_t calGyro = 0;
  uint8_t calAccel = 0;
  uint8_t calMag = 0;
  float expectedHeadingDeg = 0.0f;
  float errorVsExpectedDeg = 0.0f;
  float deltaFromStep1Deg = 0.0f;
  double distFromHomeM = 0.0;
  float courseToHomeDeg = 0.0f;
  float gpsWalkAzimuthDeg = 0.0f;
  float rtlHeadingErrorDeg = 0.0f;
  int8_t simRudderPct = 0;
  int8_t simThrottlePct = 0;
  bool stepPass = false;
  char verdict[56];
};

struct NavWizardTrackPoint {
  uint32_t elapsedMs = 0;
  double lat = 0.0;
  double lng = 0.0;
  float distFromHomeM = 0.0f;
  float walkAzimuthDeg = 0.0f;
  float fusedHeadingDeg = 0.0f;
  float rawHeadingDeg = 0.0f;
  float gpsCogDeg = 0.0f;
  float speedKnots = 0.0f;
};

static const uint8_t NAV_WIZ_MAX_TRACK_PTS = 24;

struct BootLogItem {
  char text[34];
  uint16_t color;
};

static const uint8_t BOOT_MAX_LINES = 24;
static const uint8_t BOOT_VISIBLE_LINES = 13;

class RoverDisplay {
public:
  RoverDisplay()
      : canvas(nullptr), currentPage(ROVER_PAGE_HOME), lastRender(0),
        _bootLogCount(0), _bootProgress(0), _brightness(85), _roverId(1),
        _otaActive(false), _homeMenuIdx(0), _opMenuIdx(0), _anchorMenuIdx(0),
        _configMenuIdx(0), _wifiListIdx(0), _wifiScanIdx(0), _wifiKbRow(0),
        _wifiKbCol(0), _wifiKbMode(0), _wifiTextCursorPos(0), _battMenuIdx(0),
        _battCellsCursorIdx(1), _battMahKbRow(0), _battMahKbCol(0),
        _joyMenuIdx(0), _joyExitPromptActive(false), _joyExitCursorIdx(0),
        _joyCalStep(0), _joyCalCenterX(1600), _joyCalCenterY(1600),
        _joyCalXLeft(50), _joyCalXRight(3200), _joyCalYUp(50),
        _joyCalYDown(3200), _joyCalPeakDist(0), _bootHoldPct(0),
        _wifiConfig(nullptr), _feedbackUntilMs(0), _loraTxTelems(0),
        _loraRxPolls(0), _loraLastRxSecAgo(999999), _loraSnr(0.0f),
        _loraHasContact(false), _motorMenuIdx(0), _compassMenuIdx(0),
        _navWizStep(0), _navWizStartMs(0), _navWizLastSampleMs(0),
        _navWizTrackCount(0), _navWizRunId(0), _navWizSdSaved(false),
        _navWizRecommendedOffset(0.0f), _navWizOverallPass(false) {
    strncpy(_c6Version, "--", sizeof(_c6Version));
    strncpy(_battMahBuf, "3000", sizeof(_battMahBuf));
    _wifiTargetSsid[0] = '\0';
    _wifiPassBuf[0] = '\0';
    _feedbackMsg[0] = '\0';
    _navWizDiagLine1[0] = '\0';
    _navWizDiagLine2[0] = '\0';
    _navWizSdPath[0] = '\0';
    for (uint8_t i = 0; i < 5; ++i) {
      _navWizSteps[i].verdict[0] = '\0';
    }
  }

  void begin(uint8_t initialBrightness = 85) {
    // Disable onboard TF/MicroSD card CS (GPIO 4) to prevent SPI bus collision
    pinMode(PIN_SD_CS, OUTPUT);
    digitalWrite(PIN_SD_CS, HIGH);

    // Initialize manufacturer ST7789 display driver
    display.begin();
    setBrightness(initialBrightness);

    // Create high-speed in-memory canvas configured in Landscape (320x172)
    canvas = new Arduino_Canvas(LCD_WIDTH, LCD_HEIGHT, nullptr, 0, 0,
                                DISPLAY_ROTATION);
    if (canvas) {
      canvas->begin();
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

  uint8_t getBrightness() const { return _brightness; }

  void setWiFiConfigManager(WiFiConfigManager *wcm) { _wifiConfig = wcm; }

  void setLoraStats(uint32_t txTelems, uint32_t rxPolls, uint32_t lastRxSecAgo,
                    float snr, bool hasContact) {
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
    if (!canvas)
      return;
    _roverId = roverId;
    _bootLogCount = 0;
    _bootProgress = 0;

    canvas->fillScreen(UI_BLACK);

    // Painel Esquerdo: Logótipo + Marca + ID + Barra de Progresso (w=101,
    // h=164)
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
    bootLogf(UI_GREEN, 100, "[SYS ] Rover operacional!");
    if (delayMs > 0)
      delay(delayMs);
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

  RoverScreenPage getCurrentPage() const { return currentPage; }

  void nextPage() {
    currentPage =
        static_cast<RoverScreenPage>((currentPage + 1) % ROVER_PAGE_COUNT);
    lastRender = 0;
  }

  void prevPage() {
    currentPage = static_cast<RoverScreenPage>(
        (currentPage + ROVER_PAGE_COUNT - 1) % ROVER_PAGE_COUNT);
    lastRender = 0;
  }

  uint8_t getHomeMenuIdx() const { return _homeMenuIdx; }
  void setHomeMenuIdx(uint8_t idx) {
    _homeMenuIdx = idx % 2;
    lastRender = 0;
  }

  uint8_t getOpMenuIdx() const { return _opMenuIdx; }
  void setOpMenuIdx(uint8_t idx) {
    _opMenuIdx = idx % 5;
    lastRender = 0;
  }

  uint8_t getAnchorMenuIdx() const { return _anchorMenuIdx; }
  void setAnchorMenuIdx(uint8_t idx) {
    _anchorMenuIdx = idx % 5;
    lastRender = 0;
  }

  uint8_t getConfigMenuIdx() const { return _configMenuIdx; }
  void setConfigMenuIdx(uint8_t idx) {
    _configMenuIdx = idx % 9;
    lastRender = 0;
  }

  uint8_t getMotorMenuIdx() const { return _motorMenuIdx; }
  void setMotorMenuIdx(uint8_t idx) {
    _motorMenuIdx = idx % 2;
    lastRender = 0;
  }

  uint8_t getCompassMenuIdx() const { return _compassMenuIdx; }
  void setCompassMenuIdx(uint8_t idx) {
    _compassMenuIdx = idx % 2;
    lastRender = 0;
  }

  uint8_t getBattMenuIdx() const { return _battMenuIdx; }
  void setBattMenuIdx(uint8_t idx) {
    _battMenuIdx = idx % 2;
    lastRender = 0;
  }

  uint8_t getBattCellsCursorIdx() const { return _battCellsCursorIdx; }
  void setBattCellsCursorIdx(uint8_t idx) {
    _battCellsCursorIdx = idx % 3;
    lastRender = 0;
  }

  void beginBattMahEntry(uint16_t currentMah) {
    if (currentMah < 500 || currentMah > 50000)
      currentMah = 3000;
    snprintf(_battMahBuf, sizeof(_battMahBuf), "%u", (unsigned)currentMah);
    _battMahKbRow = 0;
    _battMahKbCol = 0;
    lastRender = 0;
  }

  const char *getBattMahBuffer() const { return _battMahBuf; }

  uint16_t getBattMahValue() const {
    long val = atol(_battMahBuf);
    if (val < 500)
      val = 500;
    if (val > 50000)
      val = 50000;
    return (uint16_t)val;
  }

  void moveBattMahKbCursor(int8_t dx, int8_t dy) {
    if (dy != 0) {
      uint8_t oldRow = _battMahKbRow;
      _battMahKbRow = (uint8_t)((_battMahKbRow + 3 + dy) % 3);
      uint8_t oldCols = (oldRow == 0) ? 10 : ((oldRow == 1) ? 5 : 4);
      uint8_t newCols =
          (_battMahKbRow == 0) ? 10 : ((_battMahKbRow == 1) ? 5 : 4);
      _battMahKbCol = (uint8_t)((_battMahKbCol * newCols) / oldCols);
      if (_battMahKbCol >= newCols)
        _battMahKbCol = newCols - 1;
    }
    if (dx != 0) {
      uint8_t cols = (_battMahKbRow == 0) ? 10 : ((_battMahKbRow == 1) ? 5 : 4);
      _battMahKbCol = (uint8_t)((_battMahKbCol + cols + dx) % cols);
    }
    lastRender = 0;
  }

  void backspaceBattMah() {
    size_t len = strlen(_battMahBuf);
    if (len > 0) {
      _battMahBuf[len - 1] = '\0';
      lastRender = 0;
    }
  }

  // Retorna: 0 = tecla normal, -1 = SAIR, 1 = GUARDAR
  int8_t pressBattMahKbKey() {
    static const char digits[10] = {'1', '2', '3', '4', '5',
                                    '6', '7', '8', '9', '0'};
    if (_battMahKbRow == 0) {
      size_t len = strlen(_battMahBuf);
      char d = digits[_battMahKbCol % 10];
      if (len == 1 && _battMahBuf[0] == '0') {
        _battMahBuf[0] = d;
        _battMahBuf[1] = '\0';
      } else if (len < 5) {
        _battMahBuf[len] = d;
        _battMahBuf[len + 1] = '\0';
      }
      lastRender = 0;
      return 0;
    } else if (_battMahKbRow == 1) {
      long val = atol(_battMahBuf);
      if (val <= 0)
        val = 3000;
      uint8_t c = _battMahKbCol % 5;
      if (c == 0)
        val = max(500L, val - 500L);
      else if (c == 1)
        val = max(500L, val - 100L);
      else if (c == 2)
        val = min(50000L, val + 100L);
      else if (c == 3)
        val = min(50000L, val + 500L);
      else
        val = 3000L;
      snprintf(_battMahBuf, sizeof(_battMahBuf), "%ld", val);
      lastRender = 0;
      return 0;
    } else {
      uint8_t c = _battMahKbCol % 4;
      if (c == 0) { // LIMPAR
        _battMahBuf[0] = '\0';
        lastRender = 0;
        return 0;
      } else if (c == 1) { // <-DEL
        backspaceBattMah();
        return 0;
      } else if (c == 2) { // SAIR
        return -1;
      } else { // GUARDAR
        return 1;
      }
    }
  }

  uint8_t getJoyMenuIdx() const { return _joyMenuIdx; }
  void setJoyMenuIdx(uint8_t idx) {
    _joyMenuIdx = idx % 4;
    lastRender = 0;
  }

  void beginJoyCalWizard(int rawX, int rawY) {
    _joyCalStep = 0;
    _joyCalCenterX = rawX;
    _joyCalCenterY = rawY;
    _joyCalXLeft = rawX;
    _joyCalXRight = rawX;
    _joyCalYUp = rawY;
    _joyCalYDown = rawY;
    _joyCalPeakDist = 0;
    lastRender = 0;
  }

  void updateJoyCalSample(int rawX, int rawY) {
    if (_joyCalStep == 0) {
      _joyCalCenterX = (_joyCalCenterX * 3 + rawX) / 4;
      _joyCalCenterY = (_joyCalCenterY * 3 + rawY) / 4;
    } else if (_joyCalStep == 1) {
      // Passo 2/5: CIMA (Eixo Y)
      int dist = abs(rawY - _joyCalCenterY);
      if (dist > _joyCalPeakDist) {
        _joyCalPeakDist = dist;
        _joyCalYUp = rawY;
        lastRender = 0;
      }
    } else if (_joyCalStep == 2) {
      // Passo 3/5: BAIXO (Eixo Y no sentido oposto a CIMA)
      int upSign = (_joyCalYUp >= _joyCalCenterY) ? 1 : -1;
      int downDelta = (rawY - _joyCalCenterY) * (-upSign);
      if (downDelta > _joyCalPeakDist) {
        _joyCalPeakDist = downDelta;
        _joyCalYDown = rawY;
        lastRender = 0;
      }
    } else if (_joyCalStep == 3) {
      // Passo 4/5: ESQUERDA (Eixo X)
      int dist = abs(rawX - _joyCalCenterX);
      if (dist > _joyCalPeakDist) {
        _joyCalPeakDist = dist;
        _joyCalXLeft = rawX;
        lastRender = 0;
      }
    } else if (_joyCalStep == 4) {
      // Passo 5/5: DIREITA (Eixo X no sentido oposto a ESQUERDA)
      int leftSign = (_joyCalXLeft >= _joyCalCenterX) ? 1 : -1;
      int rightDelta = (rawX - _joyCalCenterX) * (-leftSign);
      if (rightDelta > _joyCalPeakDist) {
        _joyCalPeakDist = rightDelta;
        _joyCalXRight = rawX;
        lastRender = 0;
      }
    }
  }

  bool isJoyCalStepReady() const {
    if (_joyCalStep == 0) {
      return (_joyCalCenterX >= 800 && _joyCalCenterX <= 3300 &&
              _joyCalCenterY >= 800 && _joyCalCenterY <= 3300);
    }
    return (_joyCalPeakDist >= 300);
  }

  uint8_t getJoyCalStep() const { return _joyCalStep; }
  void setJoyCalStep(uint8_t step) {
    _joyCalStep = step % 5;
    _joyCalPeakDist = 0;
    lastRender = 0;
  }

  uint16_t getJoyCalCenterX() const {
    return (uint16_t)constrain(_joyCalCenterX, 0, 4095);
  }
  uint16_t getJoyCalCenterY() const {
    return (uint16_t)constrain(_joyCalCenterY, 0, 4095);
  }
  uint16_t getJoyCalXLeft() const {
    return (uint16_t)constrain(_joyCalXLeft, 0, 4095);
  }
  uint16_t getJoyCalXRight() const {
    return (uint16_t)constrain(_joyCalXRight, 0, 4095);
  }
  uint16_t getJoyCalYUp() const {
    return (uint16_t)constrain(_joyCalYUp, 0, 4095);
  }
  uint16_t getJoyCalYDown() const {
    return (uint16_t)constrain(_joyCalYDown, 0, 4095);
  }

  bool isJoyExitPromptActive() const { return _joyExitPromptActive; }
  void setJoyExitPromptActive(bool active) {
    _joyExitPromptActive = active;
    if (active)
      _joyExitCursorIdx = 0;
    lastRender = 0;
  }

  uint8_t getJoyExitCursorIdx() const { return _joyExitCursorIdx; }
  void setJoyExitCursorIdx(uint8_t idx) {
    _joyExitCursorIdx = idx % 2;
    lastRender = 0;
  }

  uint8_t getBootHoldProgressPct() const { return _bootHoldPct; }
  void setBootHoldProgressPct(uint8_t pct) {
    if (_bootHoldPct != pct) {
      _bootHoldPct = pct;
      lastRender = 0;
    }
  }

  // =========================================================================
  // WIZARD DE TESTE DE NAVEGAÇÃO / BÚSSOLA / RTL (5 PASSOS + RELATÓRIO SD)
  // =========================================================================
  static float wrapNavAngle180(float angle) {
    while (angle > 180.0f)
      angle -= 360.0f;
    while (angle < -180.0f)
      angle += 360.0f;
    return angle;
  }

  static float wrapNavAngle360(float angle) {
    angle = fmodf(angle, 360.0f);
    if (angle < 0.0f)
      angle += 360.0f;
    return angle;
  }

  void beginNavWizard() {
    _navWizStep = 0;
    _navWizStartMs = millis();
    _navWizLastSampleMs = 0;
    _navWizTrackCount = 0;
    _navWizRunId++;
    _navWizSdSaved = false;
    _navWizRecommendedOffset = 0.0f;
    _navWizOverallPass = false;
    _navWizDiagLine1[0] = '\0';
    _navWizDiagLine2[0] = '\0';
    strncpy(_navWizSdPath, "/nav_test_wizard.txt", sizeof(_navWizSdPath) - 1);
    _navWizSdPath[sizeof(_navWizSdPath) - 1] = '\0';
    for (uint8_t i = 0; i < 5; ++i) {
      _navWizSteps[i] = NavWizardStepSample();
      _navWizSteps[i].verdict[0] = '\0';
    }
    lastRender = 0;
  }

  uint8_t getNavWizStep() const { return _navWizStep; }
  void setNavWizStep(uint8_t step) {
    _navWizStep = (step <= 5) ? step : 0;
    lastRender = 0;
  }

  void setNavWizSdSaved(bool ok, const char *path = "/nav_test_wizard.txt") {
    _navWizSdSaved = ok;
    if (path && strlen(path) > 0) {
      strncpy(_navWizSdPath, path, sizeof(_navWizSdPath) - 1);
      _navWizSdPath[sizeof(_navWizSdPath) - 1] = '\0';
    }
    lastRender = 0;
  }

  bool isNavWizSdSaved() const { return _navWizSdSaved; }
  const String &getLastNavWizardReport() const { return _navWizLastReport; }

  void updateNavWizardLiveSample(GPSTracker &gps) {
    if (_navWizStep != 3)
      return; // Apenas amostra trajeto contínuo durante Passo 4/5 (Caminhada)

    uint32_t now = millis();
    if (now - _navWizLastSampleMs < 600)
      return;
    _navWizLastSampleMs = now;

    double homeLat = _navWizSteps[0].recorded ? _navWizSteps[0].lat
                                              : gps.getHomeLatitude();
    double homeLng = _navWizSteps[0].recorded ? _navWizSteps[0].lng
                                              : gps.getHomeLongitude();
    double curLat = gps.getLatitude();
    double curLng = gps.getLongitude();

    float distM = 0.0f;
    float walkAz = 0.0f;
    if ((homeLat != 0.0 || homeLng != 0.0) && (curLat != 0.0 || curLng != 0.0)) {
      distM = (float)TinyGPSPlus::distanceBetween(homeLat, homeLng, curLat, curLng);
      if (distM >= 0.3f) {
        walkAz = (float)TinyGPSPlus::courseTo(homeLat, homeLng, curLat, curLng);
      }
    }

    if (_navWizTrackCount < NAV_WIZ_MAX_TRACK_PTS) {
      NavWizardTrackPoint &pt = _navWizTrack[_navWizTrackCount++];
      pt.elapsedMs = now - _navWizStartMs;
      pt.lat = curLat;
      pt.lng = curLng;
      pt.distFromHomeM = distM;
      pt.walkAzimuthDeg = walkAz;
      pt.fusedHeadingDeg = gps.getHeading();
      pt.rawHeadingDeg = gps.getCompassRawHeading();
      pt.gpsCogDeg = gps.getGpsCourse();
      pt.speedKnots = gps.getSpeedKnots();
    } else {
      // Desliza janela preservando os primeiros 4 pontos e os mais recentes
      for (uint8_t i = 4; i < NAV_WIZ_MAX_TRACK_PTS - 1; ++i) {
        _navWizTrack[i] = _navWizTrack[i + 1];
      }
      NavWizardTrackPoint &pt = _navWizTrack[NAV_WIZ_MAX_TRACK_PTS - 1];
      pt.elapsedMs = now - _navWizStartMs;
      pt.lat = curLat;
      pt.lng = curLng;
      pt.distFromHomeM = distM;
      pt.walkAzimuthDeg = walkAz;
      pt.fusedHeadingDeg = gps.getHeading();
      pt.rawHeadingDeg = gps.getCompassRawHeading();
      pt.gpsCogDeg = gps.getGpsCourse();
      pt.speedKnots = gps.getSpeedKnots();
    }
  }

  void confirmNavWizardStep(uint8_t stepIdx, GPSTracker &gps) {
    if (stepIdx >= 5)
      return;

    NavWizardStepSample &s = _navWizSteps[stepIdx];
    s.recorded = true;
    s.timestampMs = millis() - _navWizStartMs;
    s.gpsFix = gps.hasFix();
    s.sats = gps.getSatellites();
    s.lat = gps.getLatitude();
    s.lng = gps.getLongitude();
    s.gpsSpeedKnots = gps.getSpeedKnots();
    s.gpsCourseDeg = gps.getGpsCourse();
    s.compassActive = gps.hasCompass();
    s.i2cAddr = gps.getCompassI2cAddr();
    s.rawHeadingDeg = gps.getCompassRawHeading();
    s.offsetDeg = gps.getCompassOffset();
    s.fusedHeadingDeg = gps.getHeading();
    s.rollDeg = gps.getRoll();
    s.pitchDeg = gps.getPitch();
    s.calSys = gps.getCompassCalSys();
    s.calGyro = gps.getCompassCalGyro();
    s.calAccel = gps.getCompassCalAccel();
    s.calMag = gps.getCompassCalMag();

    double refHomeLat =
        (stepIdx == 0) ? s.lat : _navWizSteps[0].lat;
    double refHomeLng =
        (stepIdx == 0) ? s.lng : _navWizSteps[0].lng;

    if ((refHomeLat != 0.0 || refHomeLng != 0.0) &&
        (s.lat != 0.0 || s.lng != 0.0)) {
      s.distFromHomeM =
          TinyGPSPlus::distanceBetween(refHomeLat, refHomeLng, s.lat, s.lng);
      if (s.distFromHomeM >= 0.2) {
        s.gpsWalkAzimuthDeg = (float)TinyGPSPlus::courseTo(
            refHomeLat, refHomeLng, s.lat, s.lng);
        s.courseToHomeDeg = (float)TinyGPSPlus::courseTo(
            s.lat, s.lng, refHomeLat, refHomeLng);
      } else {
        s.gpsWalkAzimuthDeg = 0.0f;
        s.courseToHomeDeg = 0.0f;
      }
    } else {
      s.distFromHomeM = 0.0;
      s.gpsWalkAzimuthDeg = 0.0f;
      s.courseToHomeDeg = 0.0f;
    }

    s.deltaFromStep1Deg =
        (stepIdx == 0)
            ? 0.0f
            : wrapNavAngle180(s.fusedHeadingDeg -
                              _navWizSteps[0].fusedHeadingDeg);

    // Simulação exata da lei de controlo RTL do C6 (main_rover_c6.cpp)
    s.rtlHeadingErrorDeg =
        wrapNavAngle180(s.courseToHomeDeg - s.fusedHeadingDeg);
    s.simRudderPct =
        (int8_t)constrain((int)(s.rtlHeadingErrorDeg * 1.8f), -100, 100);
    s.simThrottlePct = (fabsf(s.rtlHeadingErrorDeg) > 60.0f) ? 25 : 50;

    if (stepIdx == 0) {
      // Passo 1/5: Apontar PROA para NORTE (0°) e definir HOME
      s.expectedHeadingDeg = 0.0f;
      s.errorVsExpectedDeg = wrapNavAngle180(s.fusedHeadingDeg - 0.0f);
      if (!s.compassActive) {
        s.stepPass = false;
        snprintf(s.verdict, sizeof(s.verdict),
                 "FALHA: BNO055 offline (sem bussola I2C)");
      } else if (fabsf(s.errorVsExpectedDeg) <= 30.0f) {
        s.stepPass = true;
        snprintf(s.verdict, sizeof(s.verdict),
                 "OK: Proa Norte=%.0f deg (Erro=%+.0f deg)", s.fusedHeadingDeg,
                 s.errorVsExpectedDeg);
      } else if (fabsf(s.errorVsExpectedDeg) >= 145.0f) {
        s.stepPass = false;
        snprintf(s.verdict, sizeof(s.verdict),
                 "INVERTIDO 180: Apontou N mas leu %.0f deg!",
                 s.fusedHeadingDeg);
      } else {
        s.stepPass = false;
        snprintf(s.verdict, sizeof(s.verdict),
                 "DESVIO: Apontou N(0) mas leu %.0f deg (%+.0f)",
                 s.fusedHeadingDeg, s.errorVsExpectedDeg);
      }
    } else if (stepIdx == 1) {
      // Passo 2/5: Rodar 90° para a DIREITA (ESTE = 90°)
      s.expectedHeadingDeg =
          wrapNavAngle360(_navWizSteps[0].fusedHeadingDeg + 90.0f);
      s.errorVsExpectedDeg = wrapNavAngle180(s.deltaFromStep1Deg - 90.0f);
      if (fabsf(s.deltaFromStep1Deg) < 15.0f) {
        s.stepPass = false;
        snprintf(s.verdict, sizeof(s.verdict),
                 "FALHA: Bussola nao rodou (dYaw=%+.0f deg)",
                 s.deltaFromStep1Deg);
      } else if (s.deltaFromStep1Deg <= -50.0f &&
                 s.deltaFromStep1Deg >= -130.0f) {
        s.stepPass = false;
        snprintf(s.verdict, sizeof(s.verdict),
                 "EIXO Z INVERTIDO: Rodou Dir mas dYaw=%+.0f deg",
                 s.deltaFromStep1Deg);
      } else if (s.deltaFromStep1Deg >= 55.0f &&
                 s.deltaFromStep1Deg <= 125.0f) {
        s.stepPass = true;
        snprintf(s.verdict, sizeof(s.verdict),
                 "OK: Rotacao +90 Dir correta (dYaw=%+.0f deg)",
                 s.deltaFromStep1Deg);
      } else {
        s.stepPass = false;
        snprintf(s.verdict, sizeof(s.verdict),
                 "DESVIO: Rodou Dir, mediu dYaw=%+.0f deg (alvo +90)",
                 s.deltaFromStep1Deg);
      }
    } else if (stepIdx == 2) {
      // Passo 3/5: Rodar 180° para SUL (oposto ao Passo 1)
      s.expectedHeadingDeg =
          wrapNavAngle360(_navWizSteps[0].fusedHeadingDeg + 180.0f);
      s.errorVsExpectedDeg = wrapNavAngle180(s.deltaFromStep1Deg - 180.0f);
      if (fabsf(s.errorVsExpectedDeg) <= 35.0f) {
        s.stepPass = true;
        snprintf(s.verdict, sizeof(s.verdict),
                 "OK: Meia-volta 180 confirmada (dYaw=%+.0f deg)",
                 s.deltaFromStep1Deg);
      } else {
        s.stepPass = false;
        snprintf(s.verdict, sizeof(s.verdict),
                 "DESVIO: Meia-volta mediu dYaw=%+.0f deg (erro %+.0f)",
                 s.deltaFromStep1Deg, s.errorVsExpectedDeg);
      }
    } else if (stepIdx == 3) {
      // Passo 4/5: Caminhar em frente 6-15m afastando-se do HOME sem virar a proa
      s.expectedHeadingDeg = s.gpsWalkAzimuthDeg;
      s.errorVsExpectedDeg =
          wrapNavAngle180(s.fusedHeadingDeg - s.gpsWalkAzimuthDeg);
      if (!s.gpsFix || !_navWizSteps[0].gpsFix || s.distFromHomeM < 2.0) {
        s.stepPass = false;
        snprintf(s.verdict, sizeof(s.verdict),
                 "AVISO: Dist GPS curta (%.1fm) p/ validar rumo GPS",
                 s.distFromHomeM);
      } else if (fabsf(s.errorVsExpectedDeg) <= 35.0f) {
        s.stepPass = true;
        snprintf(s.verdict, sizeof(s.verdict),
                 "OK: Proa(%.0f) = Azimute GPS(%.0f) em %.1fm",
                 s.fusedHeadingDeg, s.gpsWalkAzimuthDeg, s.distFromHomeM);
      } else if (fabsf(s.errorVsExpectedDeg) >= 135.0f) {
        s.stepPass = false;
        snprintf(s.verdict, sizeof(s.verdict),
                 "INVERTIDO 180: Andou p/ %.0f(GPS) mas Proa=%.0f!",
                 s.gpsWalkAzimuthDeg, s.fusedHeadingDeg);
      } else {
        s.stepPass = false;
        snprintf(s.verdict, sizeof(s.verdict),
                 "DESVIO: Andou p/ %.0f(GPS) mas Proa=%.0f (%+.0f)",
                 s.gpsWalkAzimuthDeg, s.fusedHeadingDeg, s.errorVsExpectedDeg);
      }
    } else if (stepIdx == 4) {
      // Passo 5/5: No ponto afastado, virar 180° e apontar a PROA diretamente para o HOME
      float refHomeCourse =
          (s.distFromHomeM >= 2.0)
              ? s.courseToHomeDeg
              : wrapNavAngle360(_navWizSteps[3].fusedHeadingDeg + 180.0f);
      s.expectedHeadingDeg = refHomeCourse;
      s.errorVsExpectedDeg = wrapNavAngle180(s.fusedHeadingDeg - refHomeCourse);
      s.rtlHeadingErrorDeg = wrapNavAngle180(refHomeCourse - s.fusedHeadingDeg);
      s.simRudderPct =
          (int8_t)constrain((int)(s.rtlHeadingErrorDeg * 1.8f), -100, 100);
      s.simThrottlePct = (fabsf(s.rtlHeadingErrorDeg) > 60.0f) ? 25 : 50;

      if (fabsf(s.rtlHeadingErrorDeg) <= 35.0f) {
        s.stepPass = true;
        snprintf(s.verdict, sizeof(s.verdict),
                 "OK: RTL vai DIRETO ao HOME (Erro=%+.0f Leme=%+d%%)",
                 s.rtlHeadingErrorDeg, (int)s.simRudderPct);
      } else if (fabsf(s.rtlHeadingErrorDeg) >= 135.0f) {
        s.stepPass = false;
        snprintf(s.verdict, sizeof(s.verdict),
                 "OPOSTO 180: RTL foge do HOME! (Erro=%+.0f Leme=%+d%%)",
                 s.rtlHeadingErrorDeg, (int)s.simRudderPct);
      } else {
        s.stepPass = false;
        snprintf(s.verdict, sizeof(s.verdict),
                 "DESVIO RTL: Erro=%+.0f deg -> Leme=%+d%%",
                 s.rtlHeadingErrorDeg, (int)s.simRudderPct);
      }

      computeFinalNavDiagnosis();
    }

    lastRender = 0;
  }

  void computeFinalNavDiagnosis() {
    const NavWizardStepSample &s1 = _navWizSteps[0];
    const NavWizardStepSample &s2 = _navWizSteps[1];
    const NavWizardStepSample &s3 = _navWizSteps[2];
    const NavWizardStepSample &s4 = _navWizSteps[3];
    const NavWizardStepSample &s5 = _navWizSteps[4];

    // Se tivermos deslocamento GPS válido (>=2.0m), a referência absoluta do
    // vetor GPS no Passo 5 (Rumo ao HOME) é a verdade física absoluta; caso
    // contrário usamos o Passo 1 (Norte = 0°).
    if (s5.distFromHomeM >= 2.0 && s5.gpsFix && s1.gpsFix) {
      _navWizRecommendedOffset =
          wrapNavAngle180(s5.courseToHomeDeg - s5.rawHeadingDeg);
    } else {
      _navWizRecommendedOffset = wrapNavAngle180(0.0f - s1.rawHeadingDeg);
    }

    bool zInverted =
        (s2.deltaFromStep1Deg <= -50.0f && s2.deltaFromStep1Deg >= -130.0f);
    bool rot90Ok =
        (s2.deltaFromStep1Deg >= 55.0f && s2.deltaFromStep1Deg <= 125.0f);
    bool rot180Ok = (fabsf(wrapNavAngle180(s3.deltaFromStep1Deg - 180.0f)) <= 40.0f);

    bool walkOpposite =
        (s4.distFromHomeM >= 2.0 && fabsf(s4.errorVsExpectedDeg) >= 135.0f);
    bool homeOpposite = (fabsf(s5.rtlHeadingErrorDeg) >= 135.0f);
    bool northOpposite = (fabsf(s1.errorVsExpectedDeg) >= 135.0f);

    if (!s1.compassActive && !s5.compassActive) {
      _navWizOverallPass = false;
      snprintf(_navWizDiagLine1, sizeof(_navWizDiagLine1),
               "FALHA: BNO055 NAO DETETADO NO BARRAMENTO I2C");
      snprintf(_navWizDiagLine2, sizeof(_navWizDiagLine2),
               "Verificar ligacao SDA=IO1 SCL=IO11 na placa C6");
    } else if (zInverted) {
      _navWizOverallPass = false;
      snprintf(_navWizDiagLine1, sizeof(_navWizDiagLine1),
               "FALHA: EIXO Z INVERTIDO (dYaw P1->P2 = %+.0f deg)",
               s2.deltaFromStep1Deg);
      snprintf(_navWizDiagLine2, sizeof(_navWizDiagLine2),
               "Placa GY-BNO055 montada de cabeca para baixo!");
    } else if (walkOpposite || homeOpposite || northOpposite) {
      _navWizOverallPass = false;
      snprintf(_navWizDiagLine1, sizeof(_navWizDiagLine1),
               "CAUSA: PROA INVERTIDA ~180 DEG FACE AO MOVIMENTO!");
      snprintf(_navWizDiagLine2, sizeof(_navWizDiagLine2),
               "Off atual:%+.0f | Usar Off:%+.0f ou rodar placa 180",
               s5.offsetDeg, _navWizRecommendedOffset);
    } else if (rot90Ok && rot180Ok && s1.stepPass && s5.stepPass) {
      _navWizOverallPass = true;
      snprintf(_navWizDiagLine1, sizeof(_navWizDiagLine1),
               "APROVADO: BUSSOLA E RTL 100%% ALINHADOS C/ HOME!");
      snprintf(_navWizDiagLine2, sizeof(_navWizDiagLine2),
               "Erro RTL=%+.0f deg | Leme=%+d%% | Offset=%+.0f deg",
               s5.rtlHeadingErrorDeg, (int)s5.simRudderPct, s5.offsetDeg);
    } else {
      _navWizOverallPass = false;
      snprintf(_navWizDiagLine1, sizeof(_navWizDiagLine1),
               "DESVIO DE PROA: Erro RTL=%+.0f deg (CalMag=%u/3)",
               s5.rtlHeadingErrorDeg, (unsigned)s5.calMag);
      snprintf(_navWizDiagLine2, sizeof(_navWizDiagLine2),
               "Offset atual:%+.0f deg -> Offset sugerido:%+.0f deg",
               s5.offsetDeg, _navWizRecommendedOffset);
    }
  }

  String buildNavWizardReport(uint8_t roverId, bool motorInverted = false) {
    String r;
    r.reserve(3600);

    r += "====================================================================\n";
    r += "  WINDDRAGONS ROVER - RELATORIO DO WIZARD DE NAVEGACAO / RTL\n";
    r += "====================================================================\n";
    char hdr[240];
    snprintf(hdr, sizeof(hdr),
             "Rover ID       : ROVER-%02u\n"
             "Firmware Master: %s (%s)\n"
             "Firmware C6    : %s\n"
             "Sessao / Uptime: Run #%u | Uptime=%lu s\n"
             "Sentido Motor  : %s\n"
             "Resultado Geral: %s\n"
             "Diagnostico 1  : %s\n"
             "Diagnostico 2  : %s\n"
             "Offset Atual   : %+.1f deg\n"
             "Offset Sugerido: %+.1f deg\n",
             (unsigned)roverId, ROVER_LAFVIN_VERSION, ROVER_BUILD_DATE,
             _c6Version, (unsigned)_navWizRunId, (unsigned long)(millis() / 1000UL),
             motorInverted ? "INVERTIDO (REV)" : "NORMAL (PADRAO)",
             _navWizOverallPass ? "APROVADO (OK)" : "ANOMALIA DETETADA",
             _navWizDiagLine1, _navWizDiagLine2,
             _navWizSteps[4].offsetDeg, _navWizRecommendedOffset);
    r += hdr;

    static const char *const stepTitles[5] = {
        "PASSO 1/5: APONTAR PROA PARA NORTE (0 deg) E DEFINIR HOME",
        "PASSO 2/5: RODAR 90 GRAUS PARA A DIREITA (ESTE = 90 deg)",
        "PASSO 3/5: RODAR 180 GRAUS PARA SUL (180 deg, OPOSTO AO PASSO 1)",
        "PASSO 4/5: CAMINHAR EM FRENTE (6-15m) COM A PROA APONTADA P/ A FRENTE",
        "PASSO 5/5: VIRAR 180 NO LOCAL E APONTAR A PROA DIRETA PARA O HOME"};

    static const char *const stepInstructions[5] = {
        "Segurar Rover no ponto HOME com a PROA apontada para NORTE (0 deg)",
        "No mesmo local, rodar o Rover +90 deg no sentido horario (Direita/Este)",
        "No mesmo local, apontar a PROA para SUL (180 deg, sentido oposto ao Passo 1)",
        "Caminhar afastando-se do HOME mantendo a PROA apontada na direcao da caminhada",
        "No ponto afastado, virar o Rover 180 deg apontando a PROA diretamente para o HOME"};

    for (uint8_t i = 0; i < 5; ++i) {
      const NavWizardStepSample &s = _navWizSteps[i];
      r += "\n--------------------------------------------------------------------\n";
      r += stepTitles[i];
      r += "\n--------------------------------------------------------------------\n";
      char buf[640];
      snprintf(
          buf, sizeof(buf),
          "Instrucao Enviada  : %s\n"
          "Tempo Decorrido    : %.1f s\n"
          "GPS Fix / Sats     : %s (%u sats) | Vel=%.2f kn | GPS COG=%.1f deg\n"
          "Coordenadas GPS    : Lat=%.7f, Lng=%.7f\n"
          "Distancia ao HOME  : %.2f m\n"
          "Azimute Caminhada  : %.1f deg (HOME -> Posicao Atual)\n"
          "Rumo p/ Voltar HOME: %.1f deg (Posicao Atual -> HOME)\n"
          "Bussola GY-BNO055  : %s (I2C 0x%02X) | Cal Sys=%u Gyr=%u Acc=%u Mag=%u\n"
          "Leitura BNO055     : Raw=%.1f deg | Offset=%+.1f deg -> Fused(Proa)=%.1f deg\n"
          "Inclinacao         : Roll=%+.1f deg | Pitch=%+.1f deg\n"
          "Comparacao c/ Alvo : Esperado=%.1f deg | Lido=%.1f deg | Erro=%+.1f deg\n"
          "Rotacao vs Passo 1 : dYaw=%+.1f deg\n"
          "Simulacao RTL C6   : targetCourse=%.1f | heading=%.1f | headingError=%+.1f deg\n"
          "Comando RTL C6     : Rudder=%+d%% (%s) | Throttle=%d%%\n"
          "Avaliacao do Passo : [%s] %s\n",
          stepInstructions[i], s.timestampMs / 1000.0f,
          s.gpsFix ? "FIX_3D" : "NO_FIX", (unsigned)s.sats, s.gpsSpeedKnots,
          s.gpsCourseDeg, s.lat, s.lng, s.distFromHomeM, s.gpsWalkAzimuthDeg,
          s.courseToHomeDeg, s.compassActive ? "ONLINE" : "OFFLINE",
          (unsigned)s.i2cAddr, (unsigned)s.calSys, (unsigned)s.calGyro,
          (unsigned)s.calAccel, (unsigned)s.calMag, s.rawHeadingDeg,
          s.offsetDeg, s.fusedHeadingDeg, s.rollDeg, s.pitchDeg,
          s.expectedHeadingDeg, s.fusedHeadingDeg, s.errorVsExpectedDeg,
          s.deltaFromStep1Deg, s.courseToHomeDeg, s.fusedHeadingDeg,
          s.rtlHeadingErrorDeg, (int)s.simRudderPct,
          (s.simRudderPct > 5)
              ? "VIRA DIREITA/ESTIBORDO"
              : ((s.simRudderPct < -5) ? "VIRA ESQUERDA/BOMBORDO"
                                       : "LEME AO CENTRO/DIRETO"),
          (int)s.simThrottlePct, s.stepPass ? "PASS" : "FAIL", s.verdict);
      r += buf;
    }

    r += "\n--------------------------------------------------------------------\n";
    r += "AMOSTRAS CONTINUAS DURANTE A CAMINHADA (PASSO 4):\n";
    r += "idx,tempo_s,lat,lng,dist_home_m,azimute_gps_deg,proa_fused_deg,proa_raw_deg,gps_cog_deg,vel_kn\n";
    for (uint8_t i = 0; i < _navWizTrackCount; ++i) {
      const NavWizardTrackPoint &pt = _navWizTrack[i];
      char line[160];
      snprintf(line, sizeof(line),
               "%u,%.1f,%.7f,%.7f,%.2f,%.1f,%.1f,%.1f,%.1f,%.2f\n",
               (unsigned)(i + 1), pt.elapsedMs / 1000.0f, pt.lat, pt.lng,
               pt.distFromHomeM, pt.walkAzimuthDeg, pt.fusedHeadingDeg,
               pt.rawHeadingDeg, pt.gpsCogDeg, pt.speedKnots);
      r += line;
    }
    r += "====================================================================\n";

    _navWizLastReport = r;
    return r;
  }

  String buildNavWizardCsvRow(uint8_t roverId) const {
    const NavWizardStepSample &s1 = _navWizSteps[0];
    const NavWizardStepSample &s2 = _navWizSteps[1];
    const NavWizardStepSample &s3 = _navWizSteps[2];
    const NavWizardStepSample &s4 = _navWizSteps[3];
    const NavWizardStepSample &s5 = _navWizSteps[4];
    char row[320];
    snprintf(
        row, sizeof(row),
        "%lu,ROVER-%02u,%u,%.7f,%.7f,%.7f,%.7f,%.2f,%.1f,%.1f,%.1f,%.1f,%.1f,"
        "%.1f,%.1f,%.1f,%.1f,%.1f,%d,%d,%s,\"%s\"\n",
        (unsigned long)(millis() / 1000UL), (unsigned)roverId,
        (unsigned)_navWizRunId, s1.lat, s1.lng, s5.lat, s5.lng,
        s5.distFromHomeM, s1.rawHeadingDeg, s1.offsetDeg, s1.fusedHeadingDeg,
        s2.deltaFromStep1Deg, s3.deltaFromStep1Deg, s4.gpsWalkAzimuthDeg,
        s4.fusedHeadingDeg, s5.courseToHomeDeg, s5.fusedHeadingDeg,
        s5.rtlHeadingErrorDeg, (int)s5.simRudderPct, (int)s5.simThrottlePct,
        _navWizOverallPass ? "PASS" : "FAIL", _navWizDiagLine1);
    return String(row);
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
    if (_wifiTextCursorPos > len)
      _wifiTextCursorPos = (uint8_t)len;

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
        if (nextPos < 0)
          nextPos = 0;
        if (nextPos > (int16_t)len)
          nextPos = (int16_t)len;
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
    if (_wifiTextCursorPos > len)
      _wifiTextCursorPos = (uint8_t)len;
    if (_wifiTextCursorPos > 0 && len > 0) {
      memmove(&_wifiPassBuf[_wifiTextCursorPos - 1],
              &_wifiPassBuf[_wifiTextCursorPos], len - _wifiTextCursorPos + 1);
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
    if (_wifiTextCursorPos > len)
      _wifiTextCursorPos = (uint8_t)len;

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
  void update(uint8_t roverId, GPSTracker &gps, BatteryMonitor &batt,
              ActuatorController &actuators, uint8_t motorStatus,
              int16_t lastRssi, bool failsafeActive, bool force = false,
              bool sdOk = false, bool isApMode = false,
              const char *ip = nullptr, const char *ssid = nullptr,
              int8_t wifiRssi = 0, uint8_t apClients = 0,
              const char *apPassword = nullptr, bool joyInvX = false,
              bool joyInvY = false, int rawX = 2048, int rawY = 2048,
              int8_t joyThr = 0, int8_t joyRud = 0, int deadband = 220,
              bool btnJoy = false, bool btnMode = false, bool btnUp = false,
              bool btnDown = false, uint16_t joyClicks = 0) {
    _roverId = roverId;
    if (_otaActive)
      return;

    uint32_t now = millis();
    if (_feedbackUntilMs > 0 && now > _feedbackUntilMs) {
      _feedbackMsg[0] = '\0';
      _feedbackUntilMs = 0;
      force = true;
    }

    uint32_t minInterval = (currentPage == ROVER_PAGE_CONFIG_JOY_TEST ||
                            currentPage == ROVER_PAGE_CONFIG_JOY_CAL ||
                            currentPage == ROVER_PAGE_CONFIG_NAV_WIZARD ||
                            currentPage == ROVER_PAGE_CONFIG_COMPASS ||
                            currentPage == ROVER_PAGE_ACTUATORS ||
                            currentPage == ROVER_PAGE_ANCHOR)
                               ? 60
                               : 180;
    if (!force && (now - lastRender < minInterval))
      return;
    lastRender = now;

    if (!canvas)
      return;

    canvas->fillScreen(UI_BLACK);

    if (currentPage == ROVER_PAGE_HOME) {
      renderHomePage(roverId, gps, batt, motorStatus, lastRssi, failsafeActive,
                     sdOk, isApMode);
    } else {
      renderHeader(roverId, motorStatus, failsafeActive, sdOk, isApMode);
      switch (currentPage) {
      case ROVER_PAGE_OP_MENU:
        renderOpMenuPage(gps, batt, actuators, motorStatus, lastRssi,
                         failsafeActive);
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
      case ROVER_PAGE_ANCHOR:
        renderAnchorPage(actuators, motorStatus, lastRssi, failsafeActive);
        break;
      case ROVER_PAGE_CONFIG_MENU:
        renderConfigMenuPage(roverId, gps, batt, joyInvX, joyInvY,
                             actuators.isMotorInverted(), isApMode, ip, lastRssi,
                             failsafeActive);
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
      case ROVER_PAGE_CONFIG_BATT:
        renderBattMenuPage(batt);
        break;
      case ROVER_PAGE_CONFIG_BATT_CELLS:
        renderBattCellsPage(batt);
        break;
      case ROVER_PAGE_CONFIG_BATT_MAH:
        renderBattMahPage();
        break;
      case ROVER_PAGE_CONFIG_JOY:
        renderJoyMenuPage(joyInvX, joyInvY);
        break;
      case ROVER_PAGE_CONFIG_JOY_TEST:
        renderJoyTestPage(rawX, rawY, joyThr, joyRud, deadband, joyInvX,
                          joyInvY, btnJoy, btnMode, btnUp, btnDown, joyClicks);
        break;
      case ROVER_PAGE_CONFIG_JOY_CAL:
        renderJoyCalWizardPage(rawX, rawY);
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
      case ROVER_PAGE_CONFIG_NAV_WIZARD:
        renderNavWizardPage(gps, sdOk);
        break;
      case ROVER_PAGE_CONFIG_MOTOR:
        renderMotorConfigPage(actuators.isMotorInverted());
        break;
      case ROVER_PAGE_CONFIG_COMPASS:
        renderCompassCalPage(gps);
        break;
      default:
        renderHomePage(roverId, gps, batt, motorStatus, lastRssi,
                       failsafeActive, sdOk, isApMode);
        break;
      }
    }

    display.drawPixelBuffer(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1,
                            canvas->getFramebuffer());
  }

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
    display.drawPixelBuffer(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1,
                            canvas->getFramebuffer());
  }

  void showOtaScreen(const char *targetModule, const char *stepTitle,
                     uint8_t progressPct, const char *detailMsg,
                     uint16_t accentColor = UI_CYAN) {
    if (!canvas)
      return;
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
      canvas->print("* CONCLUIDO COM SUCESSO! A REINICIAR ROVER *");
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
  const char *getActiveFeedback() const {
    if (_feedbackUntilMs > 0 && millis() <= _feedbackUntilMs &&
        _feedbackMsg[0] != '\0') {
      return _feedbackMsg;
    }
    return nullptr;
  }

  uint16_t getMotorStatusColor(uint8_t motorStatus) const {
    switch (motorStatus) {
    case MOTOR_IDLE:
      return UI_CYAN;
    case MOTOR_MANUAL:
      return UI_GREEN;
    case MOTOR_HOLDING_STATION:
      return UI_MAGENTA;
    case MOTOR_ANCHORED:
      return UI_ORANGE;
    case MOTOR_MOVING_TO_TARGET:
      return UI_BLUE;
    case MOTOR_RTL:
      return UI_YELLOW;
    case MOTOR_FAILSAFE:
      return UI_RED;
    default:
      return UI_LIGHTGREY;
    }
  }

  const char *getMotorStatusBadge(uint8_t motorStatus) const {
    switch (motorStatus) {
    case MOTOR_IDLE:
      return "IDLE";
    case MOTOR_MANUAL:
      return "MANU";
    case MOTOR_HOLDING_STATION:
      return "HOLD";
    case MOTOR_ANCHORED:
      return "ANCH";
    case MOTOR_MOVING_TO_TARGET:
      return "GOTO";
    case MOTOR_RTL:
      return "RTL";
    case MOTOR_FAILSAFE:
      return "FAIL";
    default:
      return "IDLE";
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
                      uint8_t motorStatus, int16_t lastRssi,
                      bool failsafeActive, bool sdOk, bool isApMode) {
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
    uint16_t loraDot =
        failsafeActive ? UI_RED : (_loraHasContact ? UI_GREEN : UI_DARKGREY);
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
        canvas->printf("GPS: %uS | Bat: %u%% (%.1fV)", gps.getSatellites(),
                       batt.getPercentage(), batt.getVoltageMv() / 1000.0f);
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
      canvas->print("WiFi, Bat, Joy, ID, FW, LoRa");
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
    case ROVER_PAGE_ANCHOR:
      canvas->print("OPERACAO > ");
      canvas->setTextColor(UI_YELLOW);
      canvas->print("ANCORA");
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
    case ROVER_PAGE_CONFIG_BATT:
      canvas->print("CONFIG > ");
      canvas->setTextColor(UI_YELLOW);
      canvas->print("BATERIA");
      break;
    case ROVER_PAGE_CONFIG_BATT_CELLS:
      canvas->print("BATERIA > ");
      canvas->setTextColor(UI_YELLOW);
      canvas->print("OPCOES (CELULAS)");
      break;
    case ROVER_PAGE_CONFIG_BATT_MAH:
      canvas->print("BATERIA > ");
      canvas->setTextColor(UI_YELLOW);
      canvas->print("CAPACIDADE MAH");
      break;
    case ROVER_PAGE_CONFIG_JOY:
      canvas->print("CONFIG > ");
      canvas->setTextColor(UI_YELLOW);
      canvas->print("JOYSTICK");
      break;
    case ROVER_PAGE_CONFIG_JOY_TEST:
      canvas->print("JOYSTICK > ");
      canvas->setTextColor(UI_YELLOW);
      canvas->print("TESTAR");
      break;
    case ROVER_PAGE_CONFIG_JOY_CAL:
      canvas->print("JOYSTICK > ");
      canvas->setTextColor(UI_YELLOW);
      canvas->printf("CALIBRAR (%u/5)", (unsigned)(_joyCalStep + 1));
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
    case ROVER_PAGE_CONFIG_NAV_WIZARD:
      canvas->print("CONFIG > ");
      canvas->setTextColor(UI_YELLOW);
      if (_navWizStep < 5) {
        canvas->printf("TESTE NAV (%u/5)", (unsigned)(_navWizStep + 1));
      } else {
        canvas->setTextColor(UI_GREEN);
        canvas->print("TESTE NAV (RESUMO)");
      }
      break;
    case ROVER_PAGE_CONFIG_MOTOR:
      canvas->print("CONFIG > ");
      canvas->setTextColor(UI_YELLOW);
      canvas->print("MOTOR (SENTIDO ESC)");
      break;
    case ROVER_PAGE_CONFIG_COMPASS:
      canvas->print("CONFIG > ");
      canvas->setTextColor(UI_YELLOW);
      canvas->print("CALIBRAR NORTE (0)");
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
    uint16_t loraDot =
        failsafeActive ? UI_RED : (_loraHasContact ? UI_GREEN : UI_DARKGREY);
    canvas->fillCircle(304, 12, 4, loraDot);
  }

  // =========================================================================
  // SUBMENU: OPERAÇÃO (Topo: Estado do Rover em letra grande | Abaixo: itens de
  // 1 linha)
  // =========================================================================
  void renderOpMenuPage(GPSTracker &gps, BatteryMonitor &batt,
                        ActuatorController &actuators, uint8_t motorStatus,
                        int16_t lastRssi, bool failsafeActive) {
    uint16_t stCol = getMotorStatusColor(motorStatus);

    // --- Topo da página: Modo Operacional em letra grande (x=6, y=26, w=308,
    // h=34) ---
    canvas->fillRect(6, 26, 308, 34, UI_PANEL_BG);
    canvas->drawRect(6, 26, 308, 34, UI_CARD_BG);

    canvas->setTextSize(1);
    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(12, 28);
    canvas->printf("ROVER-%02d | MODO OPERACIONAL:", _roverId);

    canvas->setTextSize(2);
    canvas->setTextColor(stCol);
    canvas->setCursor(12, 40);
    canvas->printf("%.16s", get_motor_status_str(motorStatus));

    canvas->setTextSize(1);
    canvas->setTextColor(
        failsafeActive ? UI_RED : (_loraHasContact ? UI_GREEN : UI_LIGHTGREY));
    canvas->setCursor(214, 44);
    if (failsafeActive) {
      canvas->print("LORA: FAILSAFE");
    } else if (_loraHasContact || lastRssi != 0) {
      canvas->printf("RF: %d dBm", lastRssi);
    } else {
      canvas->print("RF: A aguardar");
    }

    // --- Lista de 5 itens de 1 linha com scroll (Navegacao, Bateria,
    // Atuadores, Ancora, SET RTL) ---
    const uint8_t total = 5;
    const uint8_t visibleRows = 4;
    const char *titles[5] = {"Navegacao", "Bateria", "Atuadores", "Ancora",
                             "SET RTL"};
    if (_opMenuIdx >= total)
      _opMenuIdx = 0;
    uint8_t startIdx = 0;
    if (_opMenuIdx >= visibleRows) {
      startIdx = _opMenuIdx - (visibleRows - 1);
    }

    int w = 298;
    for (uint8_t row = 0; row < visibleRows && (startIdx + row) < total;
         ++row) {
      uint8_t i = startIdx + row;
      bool sel = (_opMenuIdx == i);
      int x = 6;
      int y = 63 + row * 21;
      int h = 20;

      if (sel) {
        canvas->fillRect(x, y, w, h, UI_NAVY);
        canvas->drawRect(x, y, w, h, UI_CYAN);
        canvas->fillRect(x, y, 4, h, UI_CYAN);
      }

      canvas->setTextSize(2);
      canvas->setTextColor(sel ? UI_YELLOW : UI_WHITE);
      canvas->setCursor(x + 10, y + 2);
      canvas->print(titles[i]);

      canvas->setTextSize(1);
      if (i == 0) {
        canvas->setTextColor(gps.hasFix() ? UI_GREEN : UI_RED);
        canvas->setCursor(x + 132, y + 6);
        if (gps.hasFix()) {
          canvas->printf("FIX (%uS) | %.1fkn | %.0fdeg", gps.getSatellites(),
                         gps.getSpeedKnots(), gps.getHeading());
        } else {
          canvas->printf("SEM FIX | Proa: %.0fdeg", gps.getHeading());
        }
      } else if (i == 1) {
        uint8_t pct = batt.getPercentage();
        uint16_t bCol =
            (pct < 25) ? UI_RED : ((pct < 50) ? UI_YELLOW : UI_GREEN);
        canvas->setTextColor(bCol);
        canvas->setCursor(x + 132, y + 6);
        canvas->printf("%u%% | %.2fV | %.1fA", pct,
                       batt.getVoltageMv() / 1000.0f, batt.getCurrentAmps());
      } else if (i == 2) {
        canvas->setTextColor(UI_CYAN);
        canvas->setCursor(x + 132, y + 6);
        canvas->printf("T:%+d%% R:%+d%% | %.1fm", actuators.getThrottle(),
                       actuators.getRudder(),
                       actuators.getAnchorDepthCm() / 100.0f);
      } else if (i == 3) {
        bool isFree = actuators.isRackReleased();
        canvas->setTextColor(isFree ? UI_ORANGE : UI_CYAN);
        canvas->setCursor(x + 132, y + 6);
        canvas->printf("%s | %.1fm | %s",
                       get_anchor_status_str(actuators.getAnchorStatus()),
                       actuators.getAnchorDepthCm() / 100.0f,
                       isFree ? "LIVRE" : "TRAV");
      } else {
        canvas->setTextColor(gps.hasHome() ? UI_GREEN : UI_YELLOW);
        canvas->setCursor(x + 132, y + 6);
        if (gps.hasHome()) {
          canvas->printf("HOME OK | %.0fm | %.0fdeg", gps.distanceToHome(),
                         gps.courseToHome());
        } else {
          canvas->print("GRAVAR POSICAO ATUAL");
        }
      }

      canvas->setTextSize(2);
      canvas->setTextColor(sel ? UI_CYAN : UI_DARKGREY);
      canvas->setCursor(x + w - 16, y + 2);
      canvas->print(">");
    }

    // Barra de scroll vertical (5 itens, 4 visíveis)
    int sbX = 308, sbY = 63, sbW = 6, sbH = 83;
    canvas->drawRect(sbX, sbY, sbW, sbH, UI_CARD_BG);
    int thumbH = max(14, sbH * visibleRows / total);
    int thumbY = sbY + (sbH - thumbH) * _opMenuIdx / max(1, (int)total - 1);
    canvas->fillRect(sbX + 1, thumbY, sbW - 2, thumbH, UI_CYAN);

    renderFooter("<-X: Home | Y/BOOT: Item | X->/Hold: Entrar");
  }

  // =========================================================================
  // OPERAÇÃO -> ÂNCORA (Comandos de Lançar c/ Sensor, Libertar, Trancar, etc.)
  // =========================================================================
  void renderAnchorPage(ActuatorController &actuators, uint8_t motorStatus,
                        int16_t lastRssi, bool failsafeActive) {
    (void)motorStatus;
    (void)lastRssi;
    (void)failsafeActive;
    int cardY = 26;
    int cardH = 122;

    // --- Painel Esquerdo: Telemetria da Âncora & Sensor Hall (x=4, w=122) ---
    canvas->fillRect(4, cardY, 122, cardH, UI_PANEL_BG);
    canvas->drawRect(4, cardY, 122, cardH, UI_CARD_BG);

    canvas->setTextSize(1);
    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(10, cardY + 5);
    canvas->print("ESTADO DA ANCORA:");

    uint8_t ancSt = actuators.getAnchorStatus();
    uint16_t ancCol = UI_CYAN;
    if (ancSt == ANCHOR_DEPLOYING)
      ancCol = UI_ORANGE;
    else if (ancSt == ANCHOR_DEPLOYED)
      ancCol = UI_YELLOW;
    else if (ancSt == ANCHOR_RETRACTING)
      ancCol = UI_MAGENTA;
    else if (ancSt == ANCHOR_RETRACTED)
      ancCol = UI_GREEN;

    canvas->setTextSize(2);
    canvas->setTextColor(ancCol);
    canvas->setCursor(10, cardY + 16);
    canvas->printf("%.9s", get_anchor_status_str(ancSt));

    canvas->drawFastHLine(8, cardY + 35, 114, UI_CARD_BG);

    canvas->setTextSize(1);
    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(10, cardY + 40);
    canvas->print("PROFUNDIDADE:");
    canvas->setTextSize(2);
    canvas->setTextColor(UI_WHITE);
    canvas->setCursor(10, cardY + 51);
    canvas->printf("%.2f m", actuators.getAnchorDepthCm() / 100.0f);

    canvas->setTextSize(1);
    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(10, cardY + 73);
    canvas->print("CREMALHEIRA:");
    bool isEngaged = !actuators.isRackReleased();
    canvas->setTextColor(isEngaged ? UI_GREEN : UI_ORANGE);
    canvas->setCursor(10, cardY + 84);
    canvas->print(isEngaged ? "TRAVADA (LOCK)" : "LIVRE (FREE)");

    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(10, cardY + 98);
    canvas->printf("HALL: %ld p", actuators.getAnchorHallPulses());
    if (actuators.isAnchorAtTopLimit()) {
      canvas->setTextColor(UI_GREEN);
      canvas->setCursor(10, cardY + 109);
      canvas->print("FIM CURSO: TOPO");
    } else {
      canvas->setTextColor(UI_DARKGREY);
      canvas->setCursor(10, cardY + 109);
      canvas->printf(" (%u cm)", actuators.getAnchorDepthCm());
    }

    // --- Painel Direito: 5 Comandos da Âncora (x=130, w=186, h=122) ---
    canvas->fillRect(130, cardY, 186, cardH, UI_PANEL_BG);
    canvas->drawRect(130, cardY, 186, cardH, UI_CARD_BG);

    const char *cmdTitles[5] = {"1.LANCAR", "2.LIBERTAR", "3.TRANCAR",
                                "4.RECOLHER", "5.PARAR"};
    const char *cmdHints[5] = {"SENSOR", "LIVRE", "LOCK", "SUBIR", "STOP"};
    const uint16_t cmdColors[5] = {UI_CYAN, UI_ORANGE, UI_GREEN, UI_YELLOW,
                                   UI_RED};

    for (uint8_t i = 0; i < 5; ++i) {
      bool sel = (_anchorMenuIdx == i);
      int x = 134;
      int y = 29 + i * 23;
      int w = 178;
      int h = 21;
      uint16_t accent = cmdColors[i];

      if (sel) {
        canvas->fillRect(x, y, w, h, UI_NAVY);
        canvas->drawRect(x, y, w, h, accent);
        canvas->fillRect(x, y, 4, h, accent);
      }

      canvas->setTextSize(2);
      canvas->setTextColor(sel ? UI_YELLOW : UI_WHITE);
      canvas->setCursor(x + 8, y + 3);
      canvas->print(cmdTitles[i]);

      canvas->setTextSize(1);
      canvas->setTextColor(sel ? accent : UI_LIGHTGREY);
      int hintW = (int)strlen(cmdHints[i]) * 6;
      canvas->setCursor(x + w - hintW - 6, y + 7);
      canvas->print(cmdHints[i]);
    }

    renderFooter("<-X: Operacao | Y: Comando | X->/BOOT: Executar");
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

    renderHoldBootExitModalOrFooter("SAIR DE ATUADORES?");
  }

  // =========================================================================
  // SUBMENU: CONFIGURAÇÕES (WIFI, BATERIA, JOYSTICK, ID, FIRMWARE, LORA)
  // =========================================================================
  void renderConfigMenuPage(uint8_t roverId, GPSTracker &gps,
                            BatteryMonitor &batt, bool joyInvX, bool joyInvY,
                            bool motorInverted, bool isApMode, const char *ip,
                            int16_t lastRssi, bool failsafeActive) {
    (void)roverId;
    String mac = getDeviceMacAddress();

    const uint8_t total = 9;
    const uint8_t visibleRows = 4;
    const char *titles[9] = {"WIFI",      "BATERIA",    "JOYSTICK",
                             "MOTOR",     "CAL. NORTE", "TESTE NAV",
                             "ID",        "FIRMWARE",   "LORA"};

    if (_configMenuIdx >= total)
      _configMenuIdx = 0;
    uint8_t startIdx = 0;
    if (_configMenuIdx >= visibleRows) {
      startIdx = _configMenuIdx - (visibleRows - 1);
    }

    int rowW = 298;

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
        canvas->setTextColor(UI_GREEN);
        canvas->setCursor(x + 124, y + 10);
        canvas->printf("LiPo %uS | %u mAh", batt.getCellCount(),
                       batt.getCapacityMah());
      } else if (i == 2) {
        canvas->setTextColor((joyInvX || joyInvY) ? UI_ORANGE : UI_CYAN);
        canvas->setCursor(x + 124, y + 10);
        canvas->printf("X:%s Y:%s | Testar", joyInvX ? "INV" : "NRM",
                       joyInvY ? "INV" : "NRM");
      } else if (i == 3) {
        canvas->setTextColor(motorInverted ? UI_ORANGE : UI_GREEN);
        canvas->setCursor(x + 124, y + 10);
        canvas->printf("Sentido: %s",
                       motorInverted ? "INVERTIDO (REV)" : "NORMAL (PADRAO)");
      } else if (i == 4) {
        canvas->setTextColor(UI_CYAN);
        canvas->setCursor(x + 140, y + 10);
        canvas->printf("Proa:%3.0f | Off:%+.0f", gps.getHeading(),
                       gps.getCompassOffset());
      } else if (i == 5) {
        canvas->setTextColor(UI_GREEN);
        canvas->setCursor(x + 124, y + 10);
        canvas->print("Wizard RTL/Bussola -> SD");
      } else if (i == 6) {
        canvas->setTextColor(UI_CYAN);
        canvas->setCursor(x + 124, y + 10);
        canvas->printf("%s", mac.c_str());
      } else if (i == 7) {
        canvas->setTextColor(UI_LIGHTGREY);
        canvas->setCursor(x + 124, y + 10);
        canvas->printf("M:%s | C6:%s", ROVER_LAFVIN_VERSION, _c6Version);
      } else {
        canvas->setTextColor(failsafeActive
                                 ? UI_RED
                                 : (_loraHasContact ? UI_GREEN : UI_LIGHTGREY));
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

    // Barra de scroll vertical (6 itens, 4 visíveis)
    int sbX = 308, sbY = 27, sbW = 6, sbH = 118;
    canvas->drawRect(sbX, sbY, sbW, sbH, UI_CARD_BG);
    int thumbH = max(16, sbH * visibleRows / total);
    int thumbY = sbY + (sbH - thumbH) * _configMenuIdx / max(1, (int)total - 1);
    canvas->fillRect(sbX + 1, thumbY, sbW - 2, thumbH, UI_CYAN);

    renderFooter("<-X: Home | Y/BOOT: Mover | X->/Hold: Entrar");
  }

  // =========================================================================
  // CONFIGURAÇÕES -> BATERIA (OPÇÕES: 3S, 4S, 6S | MAH: NUMÉRICO)
  // =========================================================================
  void renderBattMenuPage(BatteryMonitor &batt) {
    uint8_t cells = batt.getCellCount();
    uint16_t mah = batt.getCapacityMah();
    uint8_t pct = batt.getPercentage();
    uint16_t bCol = (pct < 25) ? UI_RED : ((pct < 50) ? UI_YELLOW : UI_GREEN);

    // Card Superior: Resumo da Bateria Ativa (x=6, y=26, w=308, h=40)
    canvas->fillRect(6, 26, 308, 40, UI_PANEL_BG);
    canvas->drawRect(6, 26, 308, 40, UI_CARD_BG);

    canvas->setTextSize(1);
    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(12, 30);
    canvas->print("BATERIA CONFIGURADA:");

    canvas->setTextSize(2);
    canvas->setTextColor(UI_YELLOW);
    canvas->setCursor(12, 43);
    canvas->printf("%uS LiPo | %u mAh", cells, mah);

    canvas->setTextSize(1);
    canvas->setTextColor(bCol);
    canvas->setCursor(214, 32);
    canvas->printf("ATUAL: %u%%", pct);
    canvas->setTextColor(UI_WHITE);
    canvas->setCursor(214, 46);
    canvas->printf("%.2fV (%.2fV/c)", batt.getVoltageMv() / 1000.0f,
                   batt.getCellAverageV());

    // Item 0: Opções (3S, 4S, 6S)
    {
      bool sel = (_battMenuIdx == 0);
      int x = 6, y = 71, w = 308, h = 36;
      canvas->fillRect(x, y, w, h, sel ? UI_NAVY : UI_PANEL_BG);
      canvas->drawRect(x, y, w, h, sel ? UI_CYAN : UI_CARD_BG);
      if (sel) {
        canvas->fillRect(x, y, 4, h, UI_CYAN);
      }

      canvas->setTextSize(2);
      canvas->setTextColor(sel ? UI_YELLOW : UI_WHITE);
      canvas->setCursor(x + 12, y + 10);
      canvas->print("Opcoes");

      // Badges 3S, 4S, 6S à direita
      const uint8_t opts[3] = {3, 4, 6};
      for (uint8_t k = 0; k < 3; ++k) {
        bool isCur = (cells == opts[k]);
        int bx = x + 132 + k * 50;
        int by = y + 7;
        int bw = 44;
        int bh = 22;
        canvas->fillRect(bx, by, bw, bh, isCur ? UI_GREEN : UI_BLACK);
        canvas->drawRect(bx, by, bw, bh, isCur ? UI_WHITE : UI_DARKGREY);
        canvas->setTextSize(2);
        canvas->setTextColor(isCur ? UI_BLACK : UI_LIGHTGREY);
        canvas->setCursor(bx + 10, by + 3);
        canvas->printf("%uS", opts[k]);
      }

      canvas->setTextSize(2);
      canvas->setTextColor(sel ? UI_CYAN : UI_DARKGREY);
      canvas->setCursor(x + w - 18, y + 10);
      canvas->print(">");
    }

    // Item 1: mAh (numérico)
    {
      bool sel = (_battMenuIdx == 1);
      int x = 6, y = 111, w = 308, h = 36;
      canvas->fillRect(x, y, w, h, sel ? UI_NAVY : UI_PANEL_BG);
      canvas->drawRect(x, y, w, h, sel ? UI_CYAN : UI_CARD_BG);
      if (sel) {
        canvas->fillRect(x, y, 4, h, UI_CYAN);
      }

      canvas->setTextSize(2);
      canvas->setTextColor(sel ? UI_YELLOW : UI_WHITE);
      canvas->setCursor(x + 12, y + 10);
      canvas->print("mAh");

      canvas->setTextSize(2);
      canvas->setTextColor(UI_CYAN);
      canvas->setCursor(x + 132, y + 10);
      canvas->printf("%u", mah);

      canvas->setTextSize(1);
      canvas->setTextColor(UI_LIGHTGREY);
      canvas->setCursor(x + 216, y + 14);
      canvas->print("mAh [NUM]");

      canvas->setTextSize(2);
      canvas->setTextColor(sel ? UI_CYAN : UI_DARKGREY);
      canvas->setCursor(x + w - 18, y + 10);
      canvas->print(">");
    }

    renderFooter("<-X: Config | Y: Item | X->/BOOT: Configurar");
  }

  // =========================================================================
  // CONFIGURAÇÕES -> BATERIA -> OPÇÕES (3S, 4S, 6S)
  // =========================================================================
  void renderBattCellsPage(BatteryMonitor &batt) {
    uint8_t activeCells = batt.getCellCount();
    const uint8_t cellOpts[3] = {3, 4, 6};
    const char *cellDesc[3] = {"9.9V - 12.6V (Nom 11.1V)",
                               "13.2V - 16.8V (Nom 14.8V)",
                               "19.8V - 25.2V (Nom 22.2V)"};

    for (uint8_t i = 0; i < 3; ++i) {
      uint8_t c = cellOpts[i];
      bool isCursor = (_battCellsCursorIdx == i);
      bool isActive = (activeCells == c);

      int x = 6;
      int y = 28 + i * 39;
      int w = 308;
      int h = 35;

      canvas->fillRect(x, y, w, h, isCursor ? UI_NAVY : UI_PANEL_BG);
      canvas->drawRect(x, y, w, h,
                       isCursor ? UI_CYAN : (isActive ? UI_GREEN : UI_CARD_BG));
      if (isCursor) {
        canvas->fillRect(x, y, 4, h, UI_CYAN);
      }

      canvas->setTextSize(2);
      canvas->setTextColor(isCursor ? UI_YELLOW
                                    : (isActive ? UI_GREEN : UI_WHITE));
      canvas->setCursor(x + 12, y + 9);
      canvas->printf("%uS LiPo", c);

      canvas->setTextSize(1);
      canvas->setTextColor(UI_LIGHTGREY);
      canvas->setCursor(x + 106, y + 13);
      canvas->print(cellDesc[i]);

      if (isActive) {
        canvas->fillRect(x + w - 58, y + 9, 50, 16, UI_BLACK);
        canvas->drawRect(x + w - 58, y + 9, 50, 16, UI_GREEN);
        canvas->setTextColor(UI_GREEN);
        canvas->setCursor(x + w - 52, y + 13);
        canvas->print("[ATIVO]");
      } else if (isCursor) {
        canvas->setTextColor(UI_CYAN);
        canvas->setCursor(x + w - 54, y + 13);
        canvas->print("X-> SET");
      }
    }

    renderFooter("<-X: Bateria | Y: Opcao | X->/BOOT: Aplicar");
  }

  // =========================================================================
  // CONFIGURAÇÕES -> BATERIA -> MAH (TECLADO NUMÉRICO)
  // =========================================================================
  void renderBattMahPage() {
    static const char digits[10] = {'1', '2', '3', '4', '5',
                                    '6', '7', '8', '9', '0'};

    // 1. Caixa de valor numérico no topo (y=26..50, h=24)
    canvas->fillRect(6, 26, 308, 24, UI_PANEL_BG);
    canvas->drawRect(6, 26, 308, 24, UI_CYAN);
    canvas->fillRect(6, 26, 4, 24, UI_YELLOW);

    canvas->setTextSize(1);
    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(14, 34);
    canvas->print("CAPACIDADE:");

    canvas->setTextSize(2);
    canvas->setTextColor(UI_YELLOW);
    canvas->setCursor(88, 30);
    canvas->printf("%s", _battMahBuf);
    int curX = 88 + (int)strlen(_battMahBuf) * 12;
    canvas->setTextColor(UI_CYAN);
    canvas->setCursor(curX, 30);
    canvas->print("_");

    canvas->setTextSize(1);
    canvas->setTextColor(UI_WHITE);
    canvas->setCursor(curX + 16, 34);
    canvas->print("mAh");

    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(224, 34);
    canvas->print("[500..50000]");

    // 2. Linha 0: Dígitos 1..9, 0 (10 botões, y=54, h=27)
    int r0y = 54;
    for (uint8_t c = 0; c < 10; ++c) {
      int kx = 6 + c * 31;
      int kw = 29;
      int kh = 27;
      bool sel = (_battMahKbRow == 0 && _battMahKbCol == c);

      canvas->fillRect(kx, r0y, kw, kh, sel ? UI_CYAN : UI_PANEL_BG);
      canvas->drawRect(kx, r0y, kw, kh, sel ? UI_YELLOW : UI_CARD_BG);

      char chStr[2] = {digits[c], '\0'};
      canvas->setTextSize(2);
      canvas->setTextColor(sel ? UI_BLACK : UI_WHITE);
      canvas->setCursor(kx + 9, r0y + 6);
      canvas->print(chStr);
    }

    // 3. Linha 1: Ajustes rápidos e Preset 3000 (5 botões, y=85, h=26)
    const char *stepLabels[5] = {"-500", "-100", "+100", "+500", "3000"};
    int r1y = 85;
    for (uint8_t c = 0; c < 5; ++c) {
      int kx = 6 + c * 62;
      int kw = 60;
      int kh = 26;
      bool sel = (_battMahKbRow == 1 && _battMahKbCol == c);
      uint16_t txtCol = (c < 2) ? UI_ORANGE : ((c < 4) ? UI_GREEN : UI_YELLOW);

      canvas->fillRect(kx, r1y, kw, kh, sel ? UI_CYAN : UI_NAVY);
      canvas->drawRect(kx, r1y, kw, kh, sel ? UI_YELLOW : UI_CARD_BG);

      canvas->setTextSize(2);
      canvas->setTextColor(sel ? UI_BLACK : txtCol);
      int tw = (int)strlen(stepLabels[c]) * 12;
      canvas->setCursor(kx + (kw - tw) / 2, r1y + 5);
      canvas->print(stepLabels[c]);
    }

    // 4. Linha 2: Botões de ação LIMPAR, <-DEL, SAIR, GUARDAR (4 botões, y=115,
    // h=28)
    const int btnX[4] = {6, 84, 162, 232};
    const int btnW[4] = {74, 74, 66, 82};
    const char *btnLabels[4] = {"LIMPAR", "<-DEL", "SAIR", "GUARDAR"};
    int r2y = 115;
    int r2h = 28;

    for (uint8_t b = 0; b < 4; ++b) {
      bool sel = (_battMahKbRow == 2 && _battMahKbCol == b);
      uint16_t bgCol = UI_NAVY;
      uint16_t borderCol = UI_CARD_BG;
      uint16_t txtCol = UI_WHITE;

      if (b == 0) {
        borderCol = UI_ORANGE;
        txtCol = UI_ORANGE;
      } else if (b == 2) {
        borderCol = UI_RED;
        txtCol = UI_RED;
      } else if (b == 3) {
        borderCol = UI_GREEN;
        txtCol = UI_GREEN;
      }

      if (sel) {
        bgCol = (b == 3) ? UI_GREEN : ((b == 2) ? UI_RED : UI_CYAN);
        borderCol = UI_YELLOW;
        txtCol = UI_BLACK;
      }

      canvas->fillRect(btnX[b], r2y, btnW[b], r2h, bgCol);
      canvas->drawRect(btnX[b], r2y, btnW[b], r2h, borderCol);

      canvas->setTextSize(1);
      canvas->setTextColor(txtCol);
      int tw = (int)strlen(btnLabels[b]) * 6;
      canvas->setCursor(btnX[b] + (btnW[b] - tw) / 2, r2y + 10);
      canvas->print(btnLabels[b]);
    }

    renderFooter("XY: Mover | BOOT: Tecla | MODE: Apagar");
  }

  // =========================================================================
  // CONFIGURAÇÕES -> JOYSTICK (X: INVERTER, Y: INVERTER, CALIBRAR, TESTAR)
  // =========================================================================
  void renderJoyMenuPage(bool joyInvX, bool joyInvY) {
    const char *titles[4] = {"X : Inverter", "Y : Inverter", "Calibrar",
                             "Testar"};

    for (uint8_t i = 0; i < 4; ++i) {
      bool sel = (_joyMenuIdx == i);
      int x = 6;
      int y = 27 + i * 30;
      int w = 308;
      int h = 28;

      canvas->fillRect(x, y, w, h, sel ? UI_NAVY : UI_PANEL_BG);
      canvas->drawRect(x, y, w, h, sel ? UI_CYAN : UI_CARD_BG);
      if (sel) {
        canvas->fillRect(x, y, 4, h, UI_CYAN);
      }

      canvas->setTextSize(2);
      canvas->setTextColor(sel ? UI_YELLOW : UI_WHITE);
      canvas->setCursor(x + 12, y + 6);
      canvas->print(titles[i]);

      if (i == 0 || i == 1) {
        bool isInv = (i == 0) ? joyInvX : joyInvY;
        int bx = x + w - 110;
        int by = y + 4;
        int bw = 98;
        int bh = 20;
        uint16_t badgeCol = isInv ? UI_ORANGE : UI_GREEN;
        canvas->fillRect(bx, by, bw, bh, UI_BLACK);
        canvas->drawRect(bx, by, bw, bh, badgeCol);
        canvas->setTextSize(1);
        canvas->setTextColor(badgeCol);
        const char *stTxt = isInv ? "[ INVERTIDO ]" : "[  NORMAL   ]";
        canvas->setCursor(bx + 10, by + 6);
        canvas->print(stTxt);
      } else if (i == 2) {
        canvas->setTextSize(1);
        canvas->setTextColor(UI_YELLOW);
        canvas->setCursor(x + 132, y + 10);
        canvas->print("Wizard 5 Passos (BOOT)");

        canvas->setTextSize(2);
        canvas->setTextColor(sel ? UI_CYAN : UI_DARKGREY);
        canvas->setCursor(x + w - 18, y + 6);
        canvas->print(">");
      } else {
        canvas->setTextSize(1);
        canvas->setTextColor(UI_CYAN);
        canvas->setCursor(x + 132, y + 10);
        canvas->print("Mira 2D, ADC & Botoes");

        canvas->setTextSize(2);
        canvas->setTextColor(sel ? UI_CYAN : UI_DARKGREY);
        canvas->setCursor(x + w - 18, y + 6);
        canvas->print(">");
      }
    }

    renderFooter("<-X: Config | Y: Opcao | X->/BOOT: Selecionar");
  }

  // =========================================================================
  // CONFIGURAÇÕES -> JOYSTICK -> CALIBRAR (WIZARD 5 PASSOS)
  // =========================================================================
  void renderJoyCalWizardPage(int rawX, int rawY) {
    int cardY = 26;
    int cardH = 122;

    // Painel Esquerdo: Diagrama Direcional Visual (x=4..122)
    int leftW = 118;
    canvas->fillRect(4, cardY, leftW, cardH, UI_PANEL_BG);
    canvas->drawRect(4, cardY, leftW, cardH, UI_CARD_BG);

    int cx = 4 + leftW / 2;
    int cy = cardY + 58;
    int r = 36;
    canvas->fillCircle(cx, cy, r, UI_BLACK);
    canvas->drawCircle(cx, cy, r, UI_CARD_BG);
    canvas->drawFastHLine(cx - r + 2, cy, (r - 2) * 2, UI_DARKGREY);
    canvas->drawFastVLine(cx, cy - r + 2, (r - 2) * 2, UI_DARKGREY);

    // Destacar o alvo do passo atual no diagrama
    if (_joyCalStep == 0) {
      // Centro
      canvas->drawCircle(cx, cy, 12, UI_YELLOW);
      canvas->fillCircle(cx, cy, 5, UI_GREEN);
    } else if (_joyCalStep == 1) {
      // Cima
      canvas->fillTriangle(cx, cy - r + 3, cx - 10, cy - 12, cx + 10, cy - 12,
                           UI_YELLOW);
    } else if (_joyCalStep == 2) {
      // Baixo
      canvas->fillTriangle(cx, cy + r - 3, cx - 10, cy + 12, cx + 10, cy + 12,
                           UI_YELLOW);
    } else if (_joyCalStep == 3) {
      // Esquerda
      canvas->fillTriangle(cx - r + 3, cy, cx - 12, cy - 10, cx - 12, cy + 10,
                           UI_YELLOW);
    } else {
      // Direita
      canvas->fillTriangle(cx + r - 3, cy, cx + 12, cy - 10, cx + 12, cy + 10,
                           UI_YELLOW);
    }

    // Indicador dos 5 passos na base do painel esquerdo
    for (uint8_t s = 0; s < 5; ++s) {
      int bx = 13 + s * 21;
      int by = cardY + 103;
      uint16_t col = (s < _joyCalStep)
                         ? UI_GREEN
                         : ((s == _joyCalStep) ? UI_YELLOW : UI_DARKGREY);
      canvas->fillRect(bx, by, 17, 12, col);
      canvas->setTextSize(1);
      canvas->setTextColor(s <= _joyCalStep ? UI_BLACK : UI_WHITE);
      canvas->setCursor(bx + 6, by + 2);
      canvas->printf("%u", (unsigned)(s + 1));
    }

    // Painel Direito: Instruções do Passo e Leitura ADC (x=126..316)
    int rx = 126;
    int rw = 190;
    canvas->fillRect(rx, cardY, rw, cardH, UI_PANEL_BG);
    canvas->drawRect(rx, cardY, rw, cardH, UI_CARD_BG);

    const char *stepTitles[5] = {"PASSO 1/5: CENTRO", "PASSO 2/5: CIMA",
                                 "PASSO 3/5: BAIXO", "PASSO 4/5: ESQUERDA",
                                 "PASSO 5/5: DIREITA"};
    const char *stepInstr1[5] = {"Largue o joystick", "Encoste o joystick",
                                 "Encoste o joystick", "Encoste o joystick",
                                 "Encoste o joystick"};
    const char *stepInstr2[5] = {"EM REPOUSO (CENTRO)", "TODO PARA CIMA",
                                 "TODO PARA BAIXO", "TODO A ESQUERDA",
                                 "TODO A DIREITA"};

    canvas->fillRect(rx + 1, cardY + 1, rw - 2, 16, UI_NAVY);
    canvas->setTextSize(1);
    canvas->setTextColor(UI_CYAN);
    canvas->setCursor(rx + 8, cardY + 5);
    canvas->print(stepTitles[_joyCalStep % 5]);

    canvas->setTextSize(1);
    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(rx + 8, cardY + 22);
    canvas->print(stepInstr1[_joyCalStep % 5]);

    canvas->setTextSize(2);
    canvas->setTextColor(UI_YELLOW);
    canvas->setCursor(rx + 8, cardY + 34);
    canvas->print(stepInstr2[_joyCalStep % 5]);

    canvas->drawFastHLine(rx + 6, cardY + 56, rw - 12, UI_CARD_BG);

    // Leituras em tempo real e pico capturado
    canvas->setTextSize(1);
    if (_joyCalStep == 0) {
      canvas->setTextColor(UI_LIGHTGREY);
      canvas->setCursor(rx + 8, cardY + 62);
      canvas->printf("Centro X: %4d  |  Centro Y: %4d", _joyCalCenterX,
                     _joyCalCenterY);

      canvas->setTextColor(UI_GREEN);
      canvas->setCursor(rx + 8, cardY + 80);
      canvas->print("[ PRONTO -> CLIQUE BOOT ]");
    } else {
      bool isYStep = (_joyCalStep == 1 || _joyCalStep == 2);
      int curVal = isYStep ? rawY : rawX;
      int centerVal = isYStep ? _joyCalCenterY : _joyCalCenterX;
      int peakVal =
          (_joyCalStep == 1)
              ? _joyCalYUp
              : ((_joyCalStep == 2)
                     ? _joyCalYDown
                     : ((_joyCalStep == 3) ? _joyCalXLeft : _joyCalXRight));
      canvas->setTextColor(UI_LIGHTGREY);
      canvas->setCursor(rx + 8, cardY + 61);
      canvas->printf("Atual: %4d | Centro: %4d", curVal, centerVal);

      canvas->setTextColor(UI_CYAN);
      canvas->setCursor(rx + 8, cardY + 73);
      canvas->printf("Extremo Capturado: %4d (d=%d)", peakVal, _joyCalPeakDist);

      // Barra de curso capturado proporcional ao extremo ADC real (0 ou 4095)
      int barX = rx + 8, barY = cardY + 86, barW = rw - 16, barH = 10;
      canvas->fillRect(barX, barY, barW, barH, UI_BLACK);
      canvas->drawRect(barX, barY, barW, barH, UI_CARD_BG);
      int maxAdcSpan = (peakVal >= centerVal) ? max(400, 4095 - centerVal)
                                              : max(400, centerVal);
      int pct = constrain(_joyCalPeakDist * 100 / maxAdcSpan, 0, 100);
      int fillW = map(pct, 0, 100, 0, barW - 2);
      bool ready = isJoyCalStepReady();
      if (fillW > 0) {
        canvas->fillRect(barX + 1, barY + 1, fillW, barH - 2,
                         ready ? UI_GREEN : UI_ORANGE);
      }

      canvas->setCursor(rx + 8, cardY + 101);
      if (ready) {
        canvas->setTextColor(UI_GREEN);
        canvas->print("Alcance OK! Clique BOOT p/ avancar");
      } else {
        canvas->setTextColor(UI_ORANGE);
        canvas->print("Encoste ate ao fim do curso...");
      }
    }

    if (_joyCalStep < 4) {
      renderFooter("BOOT: Avancar Passo  |  MODE: Cancelar");
    } else {
      renderFooter("BOOT: Concluir e Guardar  |  MODE: Cancelar");
    }
  }

  // =========================================================================
  // CONFIGURAÇÕES -> JOYSTICK -> TESTAR (MIRA 2D, ADC & BOTÕES)
  // =========================================================================
  void renderJoyTestPage(int rawX, int rawY, int8_t throttle, int8_t rudder,
                         int deadband, bool joyInvX, bool joyInvY, bool btnJoy,
                         bool btnMode, bool btnUp, bool btnDown,
                         uint16_t joyClicks) {
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
    canvas->setCursor(rightX + 6, cardY + 4);
    canvas->printf("ADC & BOTOES [X:%s Y:%s]", joyInvX ? "INV" : "NRM",
                   joyInvY ? "INV" : "NRM");

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

    renderHoldBootExitModalOrFooter("SAIR DO TESTE DE JOYSTICK?");
  }

  void renderHoldBootExitModalOrFooter(const char *modalTitle) {
    if (_joyExitPromptActive) {
      int mx = 22, my = 36, mw = 276, mh = 96;
      canvas->fillRect(mx - 2, my - 2, mw + 4, mh + 4, UI_BLACK);
      canvas->fillRect(mx, my, mw, mh, UI_PANEL_BG);
      canvas->drawRect(mx, my, mw, mh, UI_CYAN);

      canvas->fillRect(mx + 1, my + 1, mw - 2, 20, UI_NAVY);
      canvas->setTextSize(1);
      canvas->setTextColor(UI_YELLOW);
      canvas->setCursor(mx + 46, my + 7);
      canvas->print(modalTitle ? modalTitle : "SAIR DO ECRA?");

      bool selSair = (_joyExitCursorIdx == 0);
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

      canvas->setTextSize(2);
      canvas->setTextColor(UI_DARKGREY);
      canvas->setCursor(mx + 134, b1y + 11);
      canvas->print("|");

      bool selCont = (_joyExitCursorIdx == 1);
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

      canvas->setTextSize(1);
      canvas->setTextColor(UI_CYAN);
      canvas->setCursor(mx + 34, my + 78);
      canvas->print("Y/X: Escolher  |  BOOT: Confirmar");

      renderFooter("SAIR | CONTINUAR -> Clique BOOT p/ confirmar");
    } else if (_bootHoldPct > 0) {
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
      canvas->printf(" %.17s", ssid ? ssid : "ROVER-AP");

      canvas->setTextColor(UI_LIGHTGREY);
      canvas->setCursor(190, cardY + 26);
      canvas->print("PASS:");
      canvas->setTextColor(UI_WHITE);
      canvas->printf(" %.12s", (apPassword && strlen(apPassword) > 0)
                                   ? apPassword
                                   : "winddragon");

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

    renderFooter("<-X: Configuracoes | X->/Hold: Abrir Lista");
  }

  // =========================================================================
  // CONFIGURAÇÕES -> WIFI -> LISTA (ITEM 0 = SCAN, ITENS 1..N = REDES
  // GUARDADAS)
  // =========================================================================
  void renderWifiListPage() {
    size_t netsCount = _wifiConfig ? _wifiConfig->getNetworks().size() : 0;
    uint8_t total = (uint8_t)(1 + netsCount);
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
    uint8_t curPos =
        (_wifiTextCursorPos > passLen) ? (uint8_t)passLen : _wifiTextCursorPos;
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

    // Painel Direito: Identificador do Rover e MAC Address (x=105..317, w=212,
    // h=122)
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
      canvas->printf("Ultimo pacote da Base ha: %lu s",
                     (unsigned long)_loraLastRxSecAgo);
    } else {
      canvas->print("Ultimo pacote da Base: Sem registo");
    }

    renderFooter("<-X / Clique BOOT: Voltar a Configuracoes");
  }

  // =========================================================================
  // CONFIGURAÇÕES -> TESTE NAV (WIZARD DE BÚSSOLA, GPS WALK & RTL -> SD)
  // Layout de Alta Visibilidade (letras grandes Size 2 e Size 3)
  // =========================================================================
  void renderNavWizardPage(GPSTracker &gps, bool sdOk) {
    int cardY = 26;
    int cardH = 122;

    if (_navWizStep >= 5) {
      // Ecrã Final Simplificado de Alta Visibilidade
      canvas->fillRect(4, cardY, 312, cardH, UI_PANEL_BG);
      canvas->drawRect(4, cardY, 312, cardH,
                       _navWizOverallPass ? UI_GREEN : UI_ORANGE);

      // Topo: Estado da gravação no SD em letra grande (Size 2)
      canvas->fillRect(5, cardY + 1, 310, 24,
                       _navWizSdSaved ? UI_NAVY : UI_RED);
      canvas->setTextSize(2);
      canvas->setTextColor(_navWizSdSaved ? UI_GREEN : UI_WHITE);
      canvas->setCursor(12, cardY + 5);
      canvas->print(_navWizSdSaved ? "GUARDADO NO SD: OK"
                                   : (sdOk ? "ERRO GRAVAR SD!" : "SEM CARTAO SD!"));

      // 5 Caixas grandes com o resultado de cada passo (P1..P5)
      for (uint8_t i = 0; i < 5; ++i) {
        int bx = 10 + i * 60;
        int by = cardY + 32;
        bool ok = _navWizSteps[i].stepPass;
        canvas->fillRect(bx, by, 54, 30, ok ? UI_GREEN : UI_RED);
        canvas->drawRect(bx, by, 54, 30, UI_WHITE);
        canvas->setTextSize(2);
        canvas->setTextColor(ok ? UI_BLACK : UI_WHITE);
        canvas->setCursor(bx + 15, by + 7);
        canvas->printf("P%u", (unsigned)(i + 1));
      }

      // Resultado global + recomendação essencial em letra grande (Size 2)
      canvas->fillRect(8, cardY + 68, 304, 48, UI_BLACK);
      canvas->drawRect(8, cardY + 68, 304, 48,
                       _navWizOverallPass ? UI_GREEN : UI_YELLOW);

      canvas->setTextSize(2);
      canvas->setTextColor(_navWizOverallPass ? UI_GREEN : UI_ORANGE);
      canvas->setCursor(14, cardY + 73);
      canvas->print(_navWizOverallPass ? "TESTE: TUDO OK!" : "TESTE: VER SD");

      canvas->setTextColor(UI_YELLOW);
      canvas->setCursor(14, cardY + 94);
      float recDelta =
          wrapNavAngle180(_navWizRecommendedOffset - gps.getCompassOffset());
      if (fabsf(recDelta) > 25.0f) {
        canvas->printf("OFFSET SUG: %+.0f", _navWizRecommendedOffset);
      } else {
        canvas->print("TIRE O SD E ENVIE");
      }

      renderFooter("BOOT / <-X: Sair  |  Y Cima: Repetir");
      return;
    }

    // =====================================================================
    // PASSOS ATIVOS 1..5 (_navWizStep = 0..4) - ALTA VISIBILIDADE
    // =====================================================================
    float curFused = gps.getHeading();
    bool gpsFix = gps.hasFix();
    uint8_t sats = gps.getSatellites();

    double refHomeLat =
        (_navWizStep == 0 || !_navWizSteps[0].recorded) ? gps.getLatitude()
                                                        : _navWizSteps[0].lat;
    double refHomeLng =
        (_navWizStep == 0 || !_navWizSteps[0].recorded) ? gps.getLongitude()
                                                        : _navWizSteps[0].lng;
    double curLat = gps.getLatitude();
    double curLng = gps.getLongitude();

    double liveDistM = 0.0;
    float liveWalkAz = 0.0f;
    float liveCourseToHome = 0.0f;
    if (_navWizStep > 0 && (refHomeLat != 0.0 || refHomeLng != 0.0) &&
        (curLat != 0.0 || curLng != 0.0)) {
      liveDistM =
          TinyGPSPlus::distanceBetween(refHomeLat, refHomeLng, curLat, curLng);
      if (liveDistM >= 0.2) {
        liveWalkAz = (float)TinyGPSPlus::courseTo(refHomeLat, refHomeLng,
                                                  curLat, curLng);
        liveCourseToHome = (float)TinyGPSPlus::courseTo(curLat, curLng,
                                                        refHomeLat, refHomeLng);
      }
    }

    float targetDeg = 0.0f;
    if (_navWizStep == 0) {
      targetDeg = 0.0f;
    } else if (_navWizStep == 1) {
      targetDeg = wrapNavAngle360(_navWizSteps[0].fusedHeadingDeg + 90.0f);
    } else if (_navWizStep == 2) {
      targetDeg = wrapNavAngle360(_navWizSteps[0].fusedHeadingDeg + 180.0f);
    } else if (_navWizStep == 3) {
      targetDeg = (liveDistM >= 1.5) ? liveWalkAz : curFused;
    } else {
      targetDeg =
          (liveDistM >= 1.5)
              ? liveCourseToHome
              : wrapNavAngle360(_navWizSteps[3].fusedHeadingDeg + 180.0f);
    }

    float liveErr = wrapNavAngle180(curFused - targetDeg);
    float liveDeltaP1 =
        (_navWizStep == 0)
            ? 0.0f
            : wrapNavAngle180(curFused - _navWizSteps[0].fusedHeadingDeg);

    bool stepReady = false;
    if (_navWizStep == 0) {
      stepReady = (fabsf(liveErr) <= 30.0f);
    } else if (_navWizStep == 1) {
      stepReady = (liveDeltaP1 >= 55.0f && liveDeltaP1 <= 125.0f);
    } else if (_navWizStep == 2) {
      stepReady = (fabsf(wrapNavAngle180(liveDeltaP1 - 180.0f)) <= 35.0f);
    } else if (_navWizStep == 3) {
      stepReady = (liveDistM >= 5.0);
    } else {
      stepReady = (fabsf(liveErr) <= 35.0f);
    }

    canvas->fillRect(4, cardY, 312, cardH, UI_PANEL_BG);
    canvas->drawRect(4, cardY, 312, cardH, stepReady ? UI_GREEN : UI_CARD_BG);

    // 1. Barra de Título com a Ação Principal em Letra Grande (Size 2)
    static const char *const bigTitles[5] = {
        "1/5 APONTAR NORTE", "2/5 RODAR 90 DIR.", "3/5 RODAR 180 SUL",
        "4/5 ANDAR FRENTE", "5/5 APONTAR HOME"};

    canvas->fillRect(5, cardY + 1, 310, 24, UI_NAVY);
    canvas->setTextSize(2);
    canvas->setTextColor(UI_YELLOW);
    canvas->setCursor(12, cardY + 5);
    canvas->print(bigTitles[_navWizStep % 5]);

    // 2. Dois Cartões Gigantes (Size 3) apenas com os 2 números essenciais
    int boxY = cardY + 29;
    int boxH = 62;
    // Cartão Esquerdo (x=8..156)
    canvas->fillRect(8, boxY, 148, boxH, UI_BLACK);
    canvas->drawRect(8, boxY, 148, boxH, stepReady ? UI_GREEN : UI_CYAN);
    // Cartão Direito (x=164..312)
    canvas->fillRect(164, boxY, 148, boxH, UI_BLACK);
    canvas->drawRect(164, boxY, 148, boxH, UI_CARD_BG);

    if (_navWizStep == 3) {
      // Passo 4: Andar em Frente -> Mostrar DISTÂNCIA (m) e GPS SATS em Gigante
      canvas->setTextSize(2);
      canvas->setTextColor(UI_LIGHTGREY);
      canvas->setCursor(16, boxY + 6);
      canvas->print("DISTANCIA");

      canvas->setTextSize(3);
      canvas->setTextColor(liveDistM >= 5.0 ? UI_GREEN : UI_YELLOW);
      canvas->setCursor(16, boxY + 28);
      canvas->printf("%.1fm", liveDistM);

      canvas->setTextSize(2);
      canvas->setTextColor(UI_LIGHTGREY);
      canvas->setCursor(172, boxY + 6);
      canvas->print("GPS");

      canvas->setTextSize(3);
      canvas->setTextColor(gpsFix ? UI_GREEN : UI_RED);
      canvas->setCursor(172, boxY + 28);
      if (gpsFix) {
        canvas->printf("%uSAT", (unsigned)sats);
      } else {
        canvas->print("NOFIX");
      }
    } else if (_navWizStep == 1 || _navWizStep == 2) {
      // Passos 2 e 3: Rotação no local -> Mostrar RODOU vs ALVO em Gigante
      canvas->setTextSize(2);
      canvas->setTextColor(UI_LIGHTGREY);
      canvas->setCursor(16, boxY + 6);
      canvas->print("RODOU");

      canvas->setTextSize(3);
      canvas->setTextColor(stepReady ? UI_GREEN : UI_CYAN);
      canvas->setCursor(16, boxY + 28);
      canvas->printf("%+.0f", liveDeltaP1);

      canvas->setTextSize(2);
      canvas->setTextColor(UI_LIGHTGREY);
      canvas->setCursor(172, boxY + 6);
      canvas->print("ALVO");

      canvas->setTextSize(3);
      canvas->setTextColor(UI_YELLOW);
      canvas->setCursor(172, boxY + 28);
      canvas->print(_navWizStep == 1 ? "+90 E" : "180 S");
    } else {
      // Passos 1 e 5: Apontar Norte (0°) ou Apontar para HOME -> PROA vs ALVO
      canvas->setTextSize(2);
      canvas->setTextColor(UI_LIGHTGREY);
      canvas->setCursor(16, boxY + 6);
      canvas->print("PROA");

      canvas->setTextSize(3);
      canvas->setTextColor(stepReady ? UI_GREEN : UI_CYAN);
      canvas->setCursor(16, boxY + 28);
      canvas->printf("%3.0f", curFused);

      canvas->setTextSize(2);
      canvas->setTextColor(UI_LIGHTGREY);
      canvas->setCursor(172, boxY + 6);
      canvas->print(_navWizStep == 0 ? "ALVO" : "HOME");

      canvas->setTextSize(3);
      canvas->setTextColor(UI_YELLOW);
      canvas->setCursor(172, boxY + 28);
      if (_navWizStep == 0) {
        canvas->print("0 N");
      } else {
        canvas->printf("%3.0f", targetDeg);
      }
    }

    // 3. Botão Inferior Grande (Fica VERDE quando está alinhado/pronto!)
    int btnY = cardY + 95;
    canvas->fillRect(8, btnY, 304, 23, stepReady ? UI_GREEN : UI_NAVY);
    canvas->drawRect(8, btnY, 304, 23, UI_WHITE);
    canvas->setTextSize(2);
    canvas->setTextColor(stepReady ? UI_BLACK : UI_WHITE);
    canvas->setCursor(22, btnY + 4);
    if (_navWizStep < 4) {
      canvas->print(stepReady ? "OK! CLIQUE P/ GRAVAR" : "CLIQUE P/ GRAVAR");
    } else {
      canvas->print(stepReady ? "OK! GUARDAR NO SD" : "CLIQUE: GUARDAR SD");
    }

    renderFooter("BOOT / X->: Gravar  |  <-X: Sair");
  }

  // =========================================================================
  // CONFIGURAÇÕES -> MOTOR (INVERTER SENTIDO ESC + TESTE DE AVANÇO)
  // =========================================================================
  void renderMotorConfigPage(bool motorInverted) {
    // Card Superior: Estado Atual do Sentido do Motor ESC (x=6, y=26, w=308, h=42)
    canvas->fillRect(6, 26, 308, 42, UI_PANEL_BG);
    canvas->drawRect(6, 26, 308, 42, motorInverted ? UI_ORANGE : UI_CARD_BG);
    canvas->fillRect(6, 26, 4, 42, motorInverted ? UI_ORANGE : UI_GREEN);

    canvas->setTextSize(1);
    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(16, 32);
    canvas->print("SENTIDO DE PROPULSAO DO MOTOR ESC:");

    canvas->setTextSize(2);
    canvas->setTextColor(motorInverted ? UI_ORANGE : UI_GREEN);
    canvas->setCursor(16, 45);
    canvas->print(motorInverted ? "INVERTIDO (REV)" : "NORMAL (PADRAO)");

    canvas->setTextSize(1);
    canvas->setTextColor(UI_CYAN);
    canvas->setCursor(204, 49);
    canvas->print(motorInverted ? "+100%->1000us" : "+100%->2000us");

    // Dois itens selecionáveis (Sentido Normal/Invertido e Pulso Teste Frente)
    for (uint8_t i = 0; i < 2; ++i) {
      bool sel = (_motorMenuIdx == i);
      int x = 6;
      int y = 73 + i * 36;
      int w = 308;
      int h = 32;

      canvas->fillRect(x, y, w, h, sel ? UI_NAVY : UI_PANEL_BG);
      canvas->drawRect(x, y, w, h, sel ? UI_CYAN : UI_CARD_BG);
      if (sel) {
        canvas->fillRect(x, y, 4, h, UI_CYAN);
      }

      if (i == 0) {
        canvas->setTextSize(2);
        canvas->setTextColor(sel ? UI_YELLOW : UI_WHITE);
        canvas->setCursor(x + 12, y + 8);
        canvas->print("SENTIDO:");

        canvas->setTextColor(motorInverted ? UI_ORANGE : UI_GREEN);
        canvas->setCursor(x + 114, y + 8);
        canvas->print(motorInverted ? "INVERTIDO" : "NORMAL");

        canvas->setTextSize(1);
        canvas->setTextColor(sel ? UI_CYAN : UI_LIGHTGREY);
        canvas->setCursor(x + 236, y + 12);
        canvas->print("[ALTERNAR]");
      } else {
        canvas->setTextSize(2);
        canvas->setTextColor(sel ? UI_YELLOW : UI_WHITE);
        canvas->setCursor(x + 12, y + 8);
        canvas->print("TESTAR FRENTE");

        canvas->setTextSize(1);
        canvas->setTextColor(sel ? UI_GREEN : UI_CYAN);
        canvas->setCursor(x + 184, y + 12);
        canvas->print("[+25% POR 1 SEG]");
      }
    }

    renderFooter("<-X: Config | Y: Mover | BOOT/X->: Aplicar");
  }

  // =========================================================================
  // CONFIGURAÇÕES -> CAL. NORTE (ALINHAR BÚSSOLA BNO055 A 0° C/ GARMIN)
  // Layout de Alta Visibilidade (letras grandes Size 2 e Size 3)
  // =========================================================================
  void renderCompassCalPage(GPSTracker &gps) {
    int cardY = 26;
    int cardH = 122;

    float curFused = gps.getHeading();
    float curOff = gps.getCompassOffset();
    float errFromNorth = wrapNavAngle180(curFused);
    bool alignedNorth = (fabsf(errFromNorth) <= 5.0f);

    canvas->fillRect(4, cardY, 312, cardH, UI_PANEL_BG);
    canvas->drawRect(4, cardY, 312, cardH, alignedNorth ? UI_GREEN : UI_CARD_BG);

    // 1. Topo em Letra Grande (Size 2)
    canvas->fillRect(5, cardY + 1, 310, 24, UI_NAVY);
    canvas->setTextSize(2);
    canvas->setTextColor(UI_YELLOW);
    canvas->setCursor(12, cardY + 5);
    canvas->print("APONTE PROA P/ NORTE");

    // 2. Dois Cartões Gigantes (Size 3): PROA ATUAL vs OFFSET
    int boxY = cardY + 29;
    int boxH = 62;
    canvas->fillRect(8, boxY, 148, boxH, UI_BLACK);
    canvas->drawRect(8, boxY, 148, boxH, alignedNorth ? UI_GREEN : UI_CYAN);

    canvas->fillRect(164, boxY, 148, boxH, UI_BLACK);
    canvas->drawRect(164, boxY, 148, boxH, UI_CARD_BG);

    canvas->setTextSize(2);
    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(16, boxY + 6);
    canvas->print("PROA");

    canvas->setTextSize(3);
    canvas->setTextColor(alignedNorth ? UI_GREEN : UI_CYAN);
    canvas->setCursor(16, boxY + 28);
    canvas->printf("%3.0f", curFused);

    canvas->setTextSize(2);
    canvas->setTextColor(UI_LIGHTGREY);
    canvas->setCursor(172, boxY + 6);
    canvas->print("OFFSET");

    canvas->setTextSize(3);
    canvas->setTextColor(UI_YELLOW);
    canvas->setCursor(172, boxY + 28);
    canvas->printf("%+.0f", curOff);

    // 3. Botão Inferior Grande (Y alterna entre GRAVAR NORTE=0 e REPOR OFFSET=0)
    int btnY = cardY + 95;
    if (_compassMenuIdx == 0) {
      canvas->fillRect(8, btnY, 304, 23, UI_GREEN);
      canvas->drawRect(8, btnY, 304, 23, UI_WHITE);
      canvas->setTextSize(2);
      canvas->setTextColor(UI_BLACK);
      canvas->setCursor(16, btnY + 4);
      canvas->print("GRAVAR NORTE = 0");
    } else {
      canvas->fillRect(8, btnY, 304, 23, UI_ORANGE);
      canvas->drawRect(8, btnY, 304, 23, UI_WHITE);
      canvas->setTextSize(2);
      canvas->setTextColor(UI_BLACK);
      canvas->setCursor(16, btnY + 4);
      canvas->print("REPOR OFFSET = 0");
    }

    renderFooter("BOOT/X->: Aplicar | Y: Opcao | <-X: Sair");
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
  uint8_t _anchorMenuIdx;
  uint8_t _configMenuIdx;
  uint8_t _wifiListIdx;
  uint8_t _wifiScanIdx;
  uint8_t _wifiKbRow;
  uint8_t _wifiKbCol;
  uint8_t _wifiKbMode;
  uint8_t _wifiTextCursorPos;
  uint8_t _battMenuIdx;
  uint8_t _battCellsCursorIdx;
  uint8_t _battMahKbRow;
  uint8_t _battMahKbCol;
  char _battMahBuf[8];
  uint8_t _joyMenuIdx;
  bool _joyExitPromptActive;
  uint8_t _joyExitCursorIdx;
  uint8_t _joyCalStep;
  int _joyCalCenterX;
  int _joyCalCenterY;
  int _joyCalXLeft;
  int _joyCalXRight;
  int _joyCalYUp;
  int _joyCalYDown;
  int _joyCalPeakDist;
  uint8_t _bootHoldPct;
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
  uint8_t _motorMenuIdx;
  uint8_t _compassMenuIdx;

  uint8_t _navWizStep;
  uint32_t _navWizStartMs;
  uint32_t _navWizLastSampleMs;
  NavWizardStepSample _navWizSteps[5];
  NavWizardTrackPoint _navWizTrack[NAV_WIZ_MAX_TRACK_PTS];
  uint8_t _navWizTrackCount;
  uint16_t _navWizRunId;
  bool _navWizSdSaved;
  float _navWizRecommendedOffset;
  bool _navWizOverallPass;
  char _navWizDiagLine1[56];
  char _navWizDiagLine2[56];
  char _navWizSdPath[32];
  String _navWizLastReport;
};
