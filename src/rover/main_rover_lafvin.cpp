#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>

#include "../base/base_joystick.h"
#include "actuators.h"
#include "battery_monitor.h"
#include "config_common.h"
#include "gps_tracker.h"
#include "lora_protocol.h"
#include "rover_config_manager.h"
#include "rover_display.h"
#include "rover_version.h"
#include "rover_web_server.h"
#include "sd_card_manager.h"
#include "web_console.h"
#include "wifi_config_manager.h"

// =========================================================================
// WINDDRAGONS ROVER - LAFVIN MASTER FIRMWARE
// Placa: Lafvin ESP32-C6 LCD 1.47" (IPS ST7789 320x172)
// Responsabilidade:
//  - Ecrã LCD ST7789 com interface gráfica e telemetria
//  - Leitor MicroSD (TF Card) para registo em ficheiro e configurações
//  - Ponto de Acesso WiFi e Servidor Web móvel
//  - Botão BOOT (GPIO 9) para navegação entre ecrãs
//  - Ponte UART com o Coprocessador C6 (TX=GPIO 2, RX=GPIO 1)
// =========================================================================

// Consola Web e Monitor de Logs em Tempo Real
WebConsoleClass webConsole;

#ifndef ROVER_ID
#define ROVER_ID 1
#endif

// Periféricos e módulos
GPSTracker gps;
ActuatorController actuators;
BatteryMonitor battery;
BaseJoystick joystick;
RoverDisplay roverDisplay;
SDCardManager sdCard;
Adafruit_NeoPixel rgbLed(1, PIN_RGB_LED, NEO_GRB + NEO_KHZ800);
WiFiConfigManager wifiConfig;
RoverConfigManager roverConfig;

uint8_t currentMotorStatus = MOTOR_IDLE;
uint32_t lastControlReceivedTime = 0;
int16_t currentLoraRssi = 0;
float currentLoraSnr = 0.0f;
char loraStatusStr[32] = "A aguardar...";
RoverLoraDiagnostics roverLoraDiag;

RoverWebServer webServer(wifiConfig, gps, battery, actuators,
                         currentMotorStatus, lastControlReceivedTime,
                         roverConfig, sdCard, &roverDisplay);

// UART Inter-ESP (Lafvin Master -> C6 Coprocessor)
HardwareSerial interSerial(1);
uint32_t uartSeq = 0;
uint32_t lastUartPingTime = 0;
uint32_t lastUartRxTime = 0;
uint32_t motorTestStopAtMs = 0;
char queuedWebCommand[256] = "NONE";

// Controlo do ecrã e botão de navegação
volatile bool flagPageChange = false;
volatile uint32_t lastBtnPressTime = 0;

// Calibração do Joystick Analógico
int joyCenterX = 2048;
int joyCenterY = 2048;
bool joyHwConnected = false;

static bool calibrateLocalJoystick(int samples, int delayMs) {
  long sumX = 0, sumY = 0;
  int minX = 4095, maxX = 0;
  int minY = 4095, maxY = 0;
  for (int i = 0; i < samples; i++) {
    int rx = analogRead(PIN_JOYSTICK_X);
    int ry = analogRead(PIN_JOYSTICK_Y);
    sumX += rx;
    sumY += ry;
    if (rx < minX)
      minX = rx;
    if (rx > maxX)
      maxX = rx;
    if (ry < minY)
      minY = ry;
    if (ry > maxY)
      maxY = ry;
    if (delayMs > 0)
      delay(delayMs);
  }
  joyCenterX = (int)(sumX / samples);
  joyCenterY = (int)(sumY / samples);
  int spreadX = maxX - minX;
  int spreadY = maxY - minY;
  // Um potenciómetro real em repouso fica centrado (~1200..2800) com baixa
  // oscilação (<120 counts). Pinos CMOS flutuantes (sem joystick soldado na
  // boia) apresentam deriva ou ruído elevado de acoplamento UART.
  bool xValid = (joyCenterX >= 1200 && joyCenterX <= 2800) && (spreadX <= 120);
  bool yValid = (joyCenterY >= 1200 && joyCenterY <= 2800) && (spreadY <= 120);
  joyHwConnected = (xValid && yValid);
  return joyHwConnected;
}

void printLog(const char *format, ...);

static void disarmRoverNav() { joystick.disarmNav(); }

static void handleRoverMenuInputs(uint32_t now, bool &forceDisplay) {
  (void)now;
  joystick.update();

  static bool lastModeRaw = false;
  bool curModeRaw = (digitalRead(PIN_BTN_MODE) == LOW);
  bool btnModeEdge = (curModeRaw && !lastModeRaw);
  lastModeRaw = curModeRaw;

  RoverScreenPage page = roverDisplay.getCurrentPage();

  // Ecrãs Interativos com Joystick (Atuadores e Teste do Joystick):
  // Retenção de 3 segundos no botão BOOT abre modal SAIR | CONTINUAR
  if (page == ROVER_PAGE_CONFIG_JOY_TEST || page == ROVER_PAGE_ACTUATORS) {
    RoverScreenPage parentPage = (page == ROVER_PAGE_ACTUATORS)
                                     ? ROVER_PAGE_OP_MENU
                                     : ROVER_PAGE_CONFIG_JOY;
    if (!roverDisplay.isJoyExitPromptActive()) {
      uint8_t holdPct = joystick.getBootHoldProgressPct(3000);
      if (holdPct != roverDisplay.getBootHoldProgressPct()) {
        roverDisplay.setBootHoldProgressPct(holdPct);
        forceDisplay = true;
      }
      joystick.wasShortClicked();
      joystick.wasLongClicked();
      joystick.getNavX();
      joystick.getNavY();

      if (joystick.wasBootHeld3s()) {
        disarmRoverNav();
        roverDisplay.setJoyExitPromptActive(true);
        roverDisplay.setJoyExitCursorIdx(0); // 0 = SAIR
        roverDisplay.setBootHoldProgressPct(0);
        forceDisplay = true;
        printLog("[UI] Hold BOOT 3s -> Modal Sair/Continuar\n");
      }
    } else {
      int8_t navY = joystick.getNavY();
      int8_t navX = joystick.getNavX();
      bool confirm = joystick.wasShortClicked() || joystick.wasLongClicked();
      joystick.wasBootHeld3s();

      if (navY != 0 || navX != 0) {
        roverDisplay.setJoyExitCursorIdx(
            (roverDisplay.getJoyExitCursorIdx() + 1) % 2);
        forceDisplay = true;
      }
      if (confirm) {
        if (roverDisplay.getJoyExitCursorIdx() == 0) {
          disarmRoverNav();
          roverDisplay.setJoyExitPromptActive(false);
          roverDisplay.setPage(parentPage);
          forceDisplay = true;
          printLog("[UI] Confirmou SAIR -> Menu Pai\n");
        } else {
          disarmRoverNav();
          roverDisplay.setJoyExitPromptActive(false);
          forceDisplay = true;
          printLog("[UI] Confirmou CONTINUAR\n");
        }
      }
    }
    return;
  }

  // Wizard de Calibração do Joystick em 5 Passos (BOOT avança, MODE cancela)
  if (page == ROVER_PAGE_CONFIG_JOY_CAL) {
    roverDisplay.updateJoyCalSample(joystick.getRawX(), joystick.getRawY());
    joystick.getNavX();
    joystick.getNavY();
    joystick.wasLongClicked();
    joystick.wasBootHeld3s();
    bool advance = joystick.wasShortClicked();

    if (btnModeEdge) {
      disarmRoverNav();
      roverDisplay.setPage(ROVER_PAGE_CONFIG_JOY);
      forceDisplay = true;
      printLog("[UI] Wizard Calibracao Joystick cancelado\n");
    } else if (advance) {
      if (!roverDisplay.isJoyCalStepReady()) {
        roverDisplay.showFeedback("ENCOSTE O JOYSTICK AO FIM!", 1400);
        forceDisplay = true;
      } else {
        uint8_t step = roverDisplay.getJoyCalStep();
        if (step < 4) {
          roverDisplay.setJoyCalStep(step + 1);
          forceDisplay = true;
          printLog("[UI] Wizard Calibracao Joystick -> Passo %u/5\n",
                   (unsigned)(step + 2));
        } else {
          uint16_t cx = roverDisplay.getJoyCalCenterX();
          uint16_t cy = roverDisplay.getJoyCalCenterY();
          uint16_t xl = roverDisplay.getJoyCalXLeft();
          uint16_t xr = roverDisplay.getJoyCalXRight();
          uint16_t yu = roverDisplay.getJoyCalYUp();
          uint16_t yd = roverDisplay.getJoyCalYDown();

          roverConfig.setJoystickCalibration(cx, cy, xl, xr, yu, yd, true);
          joystick.setCalibration(cx, cy, xl, xr, yu, yd);
          joyCenterX = cx;
          joyCenterY = cy;
          joyHwConnected = true;

          roverDisplay.showFeedback("CALIBRACAO GUARDADA!", 2000);
          disarmRoverNav();
          roverDisplay.setPage(ROVER_PAGE_CONFIG_JOY);
          forceDisplay = true;
          printLog("[UI] ★ Calibracao do Joystick guardada: CX=%u CY=%u | "
                   "XL=%u XR=%u | YU=%u YD=%u\n",
                   cx, cy, xl, xr, yu, yd);
        }
      }
    }
    return;
  }

  // Wizard de Teste de Navegação / Bússola / RTL em 5 Passos + Gravação no SD
  if (page == ROVER_PAGE_CONFIG_NAV_WIZARD) {
    roverDisplay.updateNavWizardLiveSample(gps);
    int8_t navX = joystick.getNavX();
    int8_t navY = joystick.getNavY();
    bool bootShort = joystick.wasShortClicked();
    bool bootLong = joystick.wasLongClicked();
    joystick.wasBootHeld3s();

    uint8_t step = roverDisplay.getNavWizStep();
    if (navX == -1 || btnModeEdge || bootLong) {
      disarmRoverNav();
      roverDisplay.setPage(ROVER_PAGE_CONFIG_MENU);
      forceDisplay = true;
      printLog("[UI] Wizard Teste Nav -> Configuracoes\n");
    } else if (step >= 5 && navY == -1) {
      disarmRoverNav();
      roverDisplay.beginNavWizard();
      roverDisplay.showFeedback("WIZARD REINICIADO!", 1500);
      forceDisplay = true;
      printLog("[UI] ★ Wizard Teste Nav reiniciado (Passo 1/5)\n");
    } else if (bootShort || navX == 1) {
      disarmRoverNav();
      if (step < 5) {
        if (step == 0) {
          // Passo 1/5: Grava também o ponto HOME atual no GPS e envia para a C6
          if (gps.setHomeToCurrent()) {
            char cmd[64];
            snprintf(cmd, sizeof(cmd), "CMD:SET_HOME:%ld:%ld",
                     (long)gps.getHomeLatDeg7(), (long)gps.getHomeLngDeg7());
            interSerial.println(cmd);
            interSerial.flush();
            strncpy(queuedWebCommand, cmd, sizeof(queuedWebCommand) - 1);
            queuedWebCommand[sizeof(queuedWebCommand) - 1] = '\0';
          } else {
            interSerial.println("CMD:SET_HOME");
            interSerial.flush();
          }
        }

        roverDisplay.confirmNavWizardStep(step, gps);
        printLog(
            "[NAV WIZ] ★ Passo %u/5 registado (Proa=%.1f Raw=%.1f Off=%+.1f "
            "Lat=%.6f Lng=%.6f)\n",
            (unsigned)(step + 1), gps.getHeading(), gps.getCompassRawHeading(),
            gps.getCompassOffset(), gps.getLatitude(), gps.getLongitude());

        if (step < 4) {
          roverDisplay.setNavWizStep(step + 1);
          char fb[40];
          snprintf(fb, sizeof(fb), "PASSO %u/5 GRAVADO!", (unsigned)(step + 1));
          roverDisplay.showFeedback(fb, 1200);
        } else {
          // Concluiu o Passo 5/5 -> Gerar Relatório Completo e Gravar no
          // MicroSD
          String report = roverDisplay.buildNavWizardReport(
              roverConfig.getRoverId(), roverConfig.isMotorInverted());
          String csvRow =
              roverDisplay.buildNavWizardCsvRow(roverConfig.getRoverId());

          Serial.println("\n" + report);
          webConsole.log("[NAV WIZARD] Relatorio completo gerado!\n");

          bool sdSaved = false;
          if (sdCard.isReady()) {
            sdSaved = sdCard.writeFile("/nav_test_wizard.txt", report.c_str());
            if (!sdCard.fileExists("/nav_test_history.csv")) {
              sdCard.writeFile(
                  "/nav_test_history.csv",
                  "uptime_s,rover,run_id,home_lat,home_lng,end_lat,end_lng,"
                  "dist_m,p1_raw_deg,p1_off_deg,p1_fused_deg,p2_dyaw_deg,"
                  "p3_dyaw_deg,p4_walk_gps_az_deg,p4_fused_deg,"
                  "p5_course_home_deg,p5_fused_deg,p5_rtl_err_deg,"
                  "p5_sim_rud_pct,p5_sim_thr_pct,result,diagnosis\n");
            }
            sdCard.appendFile("/nav_test_history.csv", csvRow.c_str());
            printLog(
                "[NAV WIZ] ★ Relatorio gravado no SD: /nav_test_wizard.txt "
                "(%s)\n",
                sdSaved ? "OK" : "FALHA");
          } else {
            printLog(
                "[NAV WIZ] ⚠ Cartao SD indisponivel — relatorio disponivel "
                "em /api/nav_wizard\n");
          }

          roverDisplay.setNavWizSdSaved(sdSaved, "/nav_test_wizard.txt");
          roverDisplay.setNavWizStep(5);
          roverDisplay.showFeedback(
              sdSaved ? "GUARDADO NO SD: OK!" : "CONCLUIDO (VER WEB)!", 2200);
        }
        forceDisplay = true;
      } else {
        // No ecrã de Resumo (step == 5), BOOT sai para o menu Configurações
        roverDisplay.setPage(ROVER_PAGE_CONFIG_MENU);
        forceDisplay = true;
        printLog("[UI] Resumo Teste Nav -> Configuracoes\n");
      }
    }
    return;
  }

  // Editor Numérico de Capacidade da Bateria (mAh)
  if (page == ROVER_PAGE_CONFIG_BATT_MAH) {
    int8_t dx = joystick.getNavXRepeat();
    int8_t dy = joystick.getNavY();
    bool keyClick = joystick.wasShortClicked();
    bool longCancel = joystick.wasLongClicked();
    joystick.wasBootHeld3s();

    if (dx != 0 || dy != 0) {
      roverDisplay.moveBattMahKbCursor(dx, dy);
      forceDisplay = true;
    } else if (btnModeEdge) {
      roverDisplay.backspaceBattMah();
      forceDisplay = true;
    } else if (longCancel) {
      disarmRoverNav();
      roverDisplay.setPage(ROVER_PAGE_CONFIG_BATT);
      forceDisplay = true;
    } else if (keyClick) {
      int8_t res = roverDisplay.pressBattMahKbKey();
      forceDisplay = true;
      if (res == -1) {
        disarmRoverNav();
        roverDisplay.setPage(ROVER_PAGE_CONFIG_BATT);
      } else if (res == 1) {
        uint16_t mah = roverDisplay.getBattMahValue();
        roverConfig.setBatteryCapacityMah(mah);
        roverConfig.saveConfig();
        battery.setCapacityMah(mah);
        char fb[32];
        snprintf(fb, sizeof(fb), "CAPACIDADE %u mAh OK!", mah);
        roverDisplay.showFeedback(fb, 1800);
        disarmRoverNav();
        roverDisplay.setPage(ROVER_PAGE_CONFIG_BATT);
        printLog("[UI] ★ Capacidade da bateria guardada: %u mAh\n", mah);
      }
    }
    return;
  }

  // Ecrã QWERTY para password WiFi (idêntico à Estação Base)
  if (page == ROVER_PAGE_CONFIG_WIFI_PASS) {
    int8_t dx = joystick.getNavXRepeat();
    int8_t dy = joystick.getNavY();
    bool keyClick = joystick.wasShortClicked();
    bool longCancel = joystick.wasLongClicked();
    joystick.wasBootHeld3s();

    if (dx != 0 || dy != 0) {
      roverDisplay.moveWifiKbCursor(dx, dy);
      forceDisplay = true;
    } else if (btnModeEdge) {
      roverDisplay.backspaceWifiPass();
      forceDisplay = true;
    } else if (longCancel) {
      disarmRoverNav();
      roverDisplay.setPage(ROVER_PAGE_CONFIG_WIFI_SCAN);
      forceDisplay = true;
    } else if (keyClick) {
      int8_t res = roverDisplay.pressWifiKbKey();
      forceDisplay = true;
      if (res == -1) {
        disarmRoverNav();
        roverDisplay.setPage(ROVER_PAGE_CONFIG_WIFI_SCAN);
      } else if (res == 1) {
        String targetSsid = roverDisplay.getWifiTargetSsid();
        String pass = roverDisplay.getWifiPassBuffer();
        roverDisplay.showWifiConnectingScreen(targetSsid.c_str());
        wifiConfig.addOrUpdateNetwork(targetSsid, pass, true);
        bool ok = wifiConfig.connectTo(targetSsid, 8000);
        disarmRoverNav();
        if (ok) {
          roverDisplay.showFeedback("WIFI CONECTADO!", 2000);
        } else {
          char apName[32];
          snprintf(apName, sizeof(apName), "WINDDRAGONS-ROVER-%02d",
                   roverConfig.getRoverId());
          wifiConfig.startAccessPoint(apName, "winddragon");
          roverDisplay.showFeedback("FALHA! MODO AP ATIVO", 2000);
        }
        roverDisplay.setPage(ROVER_PAGE_CONFIG_WIFI);
      }
    }
    return;
  }

  // Navegação padrão por Joystick (idêntica à Base):
  // Eixo Y: itera por cada menu (-1 = Cima, +1 = Baixo)
  // Eixo X Esquerda (-1): sai da página para o menu/página pai
  // Eixo X Direita (+1): entra na página ou confirma seleção
  int8_t navY = joystick.getNavY();
  int8_t navX = joystick.getNavX();
  bool bootShort = joystick.wasShortClicked();
  bool bootLong = joystick.wasLongClicked();
  bool clickEnter = bootShort;
  bool clickBack = btnModeEdge || bootLong;

  switch (page) {
  case ROVER_PAGE_HOME:
    if (navY != 0) {
      roverDisplay.setHomeMenuIdx((roverDisplay.getHomeMenuIdx() + 2 + navY) %
                                  2);
      forceDisplay = true;
    }
    if (navX == 1 || clickEnter) {
      disarmRoverNav();
      if (roverDisplay.getHomeMenuIdx() == 0) {
        roverDisplay.setPage(ROVER_PAGE_OP_MENU);
        printLog("[UI] Home -> Operacao\n");
      } else {
        roverDisplay.setPage(ROVER_PAGE_CONFIG_MENU);
        printLog("[UI] Home -> Configuracoes\n");
      }
      forceDisplay = true;
    }
    break;

  case ROVER_PAGE_OP_MENU:
    if (navY != 0) {
      roverDisplay.setOpMenuIdx((roverDisplay.getOpMenuIdx() + 5 + navY) % 5);
      forceDisplay = true;
    }
    if (navX == -1 || clickBack) {
      disarmRoverNav();
      roverDisplay.setPage(ROVER_PAGE_HOME);
      forceDisplay = true;
      printLog("[UI] Operacao -> Home\n");
    } else if (navX == 1 || clickEnter) {
      disarmRoverNav();
      uint8_t idx = roverDisplay.getOpMenuIdx();
      if (idx == 0) {
        roverDisplay.setPage(ROVER_PAGE_NAV);
        printLog("[UI] Operacao -> Navegacao\n");
      } else if (idx == 1) {
        roverDisplay.setPage(ROVER_PAGE_POWER);
        printLog("[UI] Operacao -> Bateria\n");
      } else if (idx == 2) {
        joystick.wasBootHeld3s();
        roverDisplay.setJoyExitPromptActive(false);
        roverDisplay.setBootHoldProgressPct(0);
        roverDisplay.setPage(ROVER_PAGE_ACTUATORS);
        if (joyHwConnected) {
          interSerial.println("CMD:JOY_EN:1");
          interSerial.flush();
        }
        printLog("[UI] Operacao -> Atuadores\n");
      } else if (idx == 3) {
        roverDisplay.setAnchorMenuIdx(0);
        roverDisplay.setPage(ROVER_PAGE_ANCHOR);
        printLog("[UI] Operacao -> Ancora\n");
      } else {
        if (gps.setHomeToCurrent()) {
          char cmd[64];
          snprintf(cmd, sizeof(cmd), "CMD:SET_HOME:%ld:%ld",
                   (long)gps.getHomeLatDeg7(), (long)gps.getHomeLngDeg7());
          interSerial.println(cmd);
          interSerial.flush();
          strncpy(queuedWebCommand, cmd, sizeof(queuedWebCommand) - 1);
          queuedWebCommand[sizeof(queuedWebCommand) - 1] = '\0';
          roverDisplay.showFeedback("HOME RTL DEFINIDO!", 2000);
          printLog("[UI] ★ Operacao -> SET RTL gravado em RAM (%.6f, %.6f)\n",
                   gps.getHomeLatitude(), gps.getHomeLongitude());
        } else {
          interSerial.println("CMD:SET_HOME");
          interSerial.flush();
          roverDisplay.showFeedback("SEM FIX GPS P/ RTL!", 2000);
          printLog("[UI] ⚠ Operacao -> SET RTL sem fix GPS local!\n");
        }
      }
      forceDisplay = true;
    }
    break;

  case ROVER_PAGE_ANCHOR:
    if (navY != 0) {
      roverDisplay.setAnchorMenuIdx(
          (roverDisplay.getAnchorMenuIdx() + 5 + navY) % 5);
      forceDisplay = true;
    }
    if (navX == -1 || clickBack) {
      disarmRoverNav();
      roverDisplay.setPage(ROVER_PAGE_OP_MENU);
      forceDisplay = true;
      printLog("[UI] Ancora -> Operacao\n");
    } else if (navX == 1 || clickEnter) {
      disarmRoverNav();
      uint8_t aIdx = roverDisplay.getAnchorMenuIdx();
      if (aIdx == 0) {
        // 1. Lançar Âncora (com sensor Hall auto-trancar ao parar)
        actuators.commandDropAnchor(0);
        currentMotorStatus = MOTOR_ANCHORED;
        interSerial.println("CMD:DROP_ANCHOR");
        interSerial.flush();
        strncpy(queuedWebCommand, "CMD:DROP_ANCHOR",
                sizeof(queuedWebCommand) - 1);
        queuedWebCommand[sizeof(queuedWebCommand) - 1] = '\0';
        roverDisplay.showFeedback("A LANCAR (AUTO-LOCK)!", 1800);
        printLog("[UI] ★ Ancora -> LANCAR ANCORA (auto-trancar por sensor)\n");
      } else if (aIdx == 1) {
        // 2. Libertar Cremalheira (livre sem sensor)
        actuators.releaseRack();
        interSerial.println("CMD:RACK:RELEASE");
        interSerial.flush();
        strncpy(queuedWebCommand, "CMD:RACK:RELEASE",
                sizeof(queuedWebCommand) - 1);
        queuedWebCommand[sizeof(queuedWebCommand) - 1] = '\0';
        roverDisplay.showFeedback("CREMALHEIRA LIBERTADA!", 1800);
        printLog("[UI] ★ Ancora -> LIBERTAR (sem sensor)\n");
      } else if (aIdx == 2) {
        // 3. Trancar Cremalheira
        actuators.engageRack();
        interSerial.println("CMD:RACK:ENGAGE");
        interSerial.flush();
        strncpy(queuedWebCommand, "CMD:RACK:ENGAGE",
                sizeof(queuedWebCommand) - 1);
        queuedWebCommand[sizeof(queuedWebCommand) - 1] = '\0';
        roverDisplay.showFeedback("CREMALHEIRA TRANCADA!", 1800);
        printLog("[UI] ★ Ancora -> TRANCAR CREMALHEIRA\n");
      } else if (aIdx == 3) {
        // 4. Recolher Âncora
        actuators.commandRetractAnchor();
        currentMotorStatus = MOTOR_IDLE;
        interSerial.println("CMD:RETRACT_ANCHOR");
        interSerial.flush();
        strncpy(queuedWebCommand, "CMD:RETRACT_ANCHOR",
                sizeof(queuedWebCommand) - 1);
        queuedWebCommand[sizeof(queuedWebCommand) - 1] = '\0';
        roverDisplay.showFeedback("A RECOLHER ANCORA...", 1800);
        printLog("[UI] ★ Ancora -> RECOLHER ANCORA\n");
      } else {
        // 5. Parar Guincho
        actuators.stopWinch();
        interSerial.println("CMD:STOP_ANCHOR");
        interSerial.flush();
        strncpy(queuedWebCommand, "CMD:STOP_ANCHOR",
                sizeof(queuedWebCommand) - 1);
        queuedWebCommand[sizeof(queuedWebCommand) - 1] = '\0';
        roverDisplay.showFeedback("GUINCHO PARADO!", 1800);
        printLog("[UI] ★ Ancora -> PARAR GUINCHO\n");
      }
      forceDisplay = true;
    }
    break;

  case ROVER_PAGE_NAV:
  case ROVER_PAGE_POWER:
    if (navX == -1 || clickBack || clickEnter) {
      disarmRoverNav();
      roverDisplay.setPage(ROVER_PAGE_OP_MENU);
      forceDisplay = true;
      printLog("[UI] Voltou para Operacao\n");
    }
    break;

  case ROVER_PAGE_CONFIG_MENU:
    if (navY != 0) {
      roverDisplay.setConfigMenuIdx(
          (roverDisplay.getConfigMenuIdx() + 9 + navY) % 9);
      forceDisplay = true;
    }
    if (navX == -1 || clickBack) {
      disarmRoverNav();
      roverDisplay.setPage(ROVER_PAGE_HOME);
      forceDisplay = true;
      printLog("[UI] Configuracoes -> Home\n");
    } else if (navX == 1 || clickEnter) {
      disarmRoverNav();
      uint8_t idx = roverDisplay.getConfigMenuIdx();
      if (idx == 0) {
        roverDisplay.setPage(ROVER_PAGE_CONFIG_WIFI);
        printLog("[UI] Configuracoes -> WiFi\n");
      } else if (idx == 1) {
        roverDisplay.setPage(ROVER_PAGE_CONFIG_BATT);
        printLog("[UI] Configuracoes -> Bateria\n");
      } else if (idx == 2) {
        roverDisplay.setPage(ROVER_PAGE_CONFIG_JOY);
        printLog("[UI] Configuracoes -> JoyStick\n");
      } else if (idx == 3) {
        roverDisplay.setMotorMenuIdx(0);
        roverDisplay.setPage(ROVER_PAGE_CONFIG_MOTOR);
        printLog("[UI] Configuracoes -> Motor (Sentido ESC)\n");
      } else if (idx == 4) {
        roverDisplay.setCompassMenuIdx(0);
        roverDisplay.setPage(ROVER_PAGE_CONFIG_COMPASS);
        printLog("[UI] Configuracoes -> Calibrar Norte (0 deg)\n");
      } else if (idx == 5) {
        joystick.wasShortClicked();
        joystick.wasLongClicked();
        joystick.wasBootHeld3s();
        roverDisplay.beginNavWizard();
        roverDisplay.setPage(ROVER_PAGE_CONFIG_NAV_WIZARD);
        printLog("[UI] Configuracoes -> Teste Nav (Wizard 1/5)\n");
      } else if (idx == 6) {
        roverDisplay.setPage(ROVER_PAGE_CONFIG_ID);
        printLog("[UI] Configuracoes -> ID\n");
      } else if (idx == 7) {
        roverDisplay.setPage(ROVER_PAGE_CONFIG_FW);
        printLog("[UI] Configuracoes -> Firmware\n");
      } else {
        roverDisplay.setPage(ROVER_PAGE_CONFIG_LORA);
        printLog("[UI] Configuracoes -> LoRa\n");
      }
      forceDisplay = true;
    }
    break;

  case ROVER_PAGE_CONFIG_COMPASS:
    if (navY != 0) {
      roverDisplay.setCompassMenuIdx(
          (roverDisplay.getCompassMenuIdx() + 2 + navY) % 2);
      forceDisplay = true;
    }
    if (navX == -1 || clickBack) {
      disarmRoverNav();
      roverDisplay.setPage(ROVER_PAGE_CONFIG_MENU);
      forceDisplay = true;
      printLog("[UI] Calibrar Norte -> Configuracoes\n");
    } else if (navX == 1 || clickEnter) {
      disarmRoverNav();
      if (roverDisplay.getCompassMenuIdx() == 0) {
        // Gravar Norte = 0° (alinhado com Garmin / referência externa)
        float rawH = gps.getCompassRawHeading();
        float newOff = -rawH;
        while (newOff > 180.0f)
          newOff -= 360.0f;
        while (newOff <= -180.0f)
          newOff += 360.0f;
        gps.setCompassData(gps.hasCompass(), 0.0f, gps.getRoll(),
                           gps.getPitch(), gps.getCompassCalSys(),
                           gps.getCompassCalMag(), newOff,
                           gps.getCompassCalGyro(), gps.getCompassCalAccel(),
                           rawH, gps.getCompassI2cAddr());

        interSerial.println("CMD:COMPASS_ZERO");
        interSerial.flush();
        strncpy(queuedWebCommand, "CMD:COMPASS_ZERO",
                sizeof(queuedWebCommand) - 1);
        queuedWebCommand[sizeof(queuedWebCommand) - 1] = '\0';

        roverDisplay.showFeedback("NORTE GRAVADO (0 DEG)!", 2000);
        printLog(
            "[UI] ★ Calibrar Norte: BNO055 alinhado a 0.0° (Offset=%+.1f°)\n",
            newOff);
      } else {
        // Repor Offset = 0°
        float rawH = gps.getCompassRawHeading();
        gps.setCompassData(gps.hasCompass(), rawH, gps.getRoll(),
                           gps.getPitch(), gps.getCompassCalSys(),
                           gps.getCompassCalMag(), 0.0f,
                           gps.getCompassCalGyro(), gps.getCompassCalAccel(),
                           rawH, gps.getCompassI2cAddr());

        interSerial.println("CMD:COMPASS_OFFSET:0");
        interSerial.flush();
        strncpy(queuedWebCommand, "CMD:COMPASS_OFFSET:0",
                sizeof(queuedWebCommand) - 1);
        queuedWebCommand[sizeof(queuedWebCommand) - 1] = '\0';

        roverDisplay.showFeedback("OFFSET REPOSTO (0 DEG)", 2000);
        printLog("[UI] ★ Calibrar Norte: Offset reposto para 0.0°\n");
      }
      forceDisplay = true;
    }
    break;

  case ROVER_PAGE_CONFIG_MOTOR:
    if (navY != 0) {
      roverDisplay.setMotorMenuIdx((roverDisplay.getMotorMenuIdx() + 2 + navY) %
                                   2);
      forceDisplay = true;
    }
    if (navX == -1 || clickBack) {
      if (motorTestStopAtMs > 0) {
        motorTestStopAtMs = 0;
        actuators.stopMotor();
        interSerial.println("CMD:MOTOR:0");
        interSerial.flush();
      }
      disarmRoverNav();
      roverDisplay.setPage(ROVER_PAGE_CONFIG_MENU);
      forceDisplay = true;
      printLog("[UI] Motor -> Configuracoes\n");
    } else if (navX == 1 || clickEnter) {
      disarmRoverNav();
      if (roverDisplay.getMotorMenuIdx() == 0) {
        bool inv = !roverConfig.isMotorInverted();
        roverConfig.setMotorInverted(inv, true);
        actuators.setMotorInverted(inv);

        char cmd[32];
        snprintf(cmd, sizeof(cmd), "CMD:MOTOR_INV:%d", inv ? 1 : 0);
        interSerial.println(cmd);
        interSerial.flush();
        strncpy(queuedWebCommand, cmd, sizeof(queuedWebCommand) - 1);
        queuedWebCommand[sizeof(queuedWebCommand) - 1] = '\0';

        roverDisplay.showFeedback(
            inv ? "MOTOR: INVERTIDO (REV)" : "MOTOR: NORMAL (PADRAO)", 1800);
        printLog("[UI] ★ Sentido do Motor ESC alterado para: %s\n",
                 inv ? "INVERTIDO (REVERSO)" : "NORMAL (PADRAO)");
      } else {
        // Pulso de teste de avanço (+25% durante 1.0 segundo)
        actuators.setThrottle(25);
        interSerial.println("CMD:MOTOR:25");
        interSerial.flush();
        strncpy(queuedWebCommand, "CMD:MOTOR:25", sizeof(queuedWebCommand) - 1);
        queuedWebCommand[sizeof(queuedWebCommand) - 1] = '\0';
        motorTestStopAtMs = millis() + 1000UL;
        roverDisplay.showFeedback("TESTE AVANCO +25% (1s)...", 1200);
        printLog("[UI] ★ Teste de Avanco do Motor ESC (+25%% por 1s, Inv=%d)\n",
                 roverConfig.isMotorInverted() ? 1 : 0);
      }
      forceDisplay = true;
    }
    break;

  case ROVER_PAGE_CONFIG_BATT:
    if (navY != 0) {
      roverDisplay.setBattMenuIdx((roverDisplay.getBattMenuIdx() + 2 + navY) %
                                  2);
      forceDisplay = true;
    }
    if (navX == -1 || clickBack) {
      disarmRoverNav();
      roverDisplay.setPage(ROVER_PAGE_CONFIG_MENU);
      forceDisplay = true;
      printLog("[UI] Bateria -> Configuracoes\n");
    } else if (navX == 1 || clickEnter) {
      disarmRoverNav();
      if (roverDisplay.getBattMenuIdx() == 0) {
        uint8_t curCells = roverConfig.getBatteryCells();
        uint8_t curIdx = (curCells == 3) ? 0 : ((curCells == 4) ? 1 : 2);
        roverDisplay.setBattCellsCursorIdx(curIdx);
        roverDisplay.setPage(ROVER_PAGE_CONFIG_BATT_CELLS);
        printLog("[UI] Bateria -> Opcoes (3S/4S/6S)\n");
      } else {
        roverDisplay.beginBattMahEntry(roverConfig.getBatteryCapacityMah());
        roverDisplay.setPage(ROVER_PAGE_CONFIG_BATT_MAH);
        printLog("[UI] Bateria -> Capacidade mAh\n");
      }
      forceDisplay = true;
    }
    break;

  case ROVER_PAGE_CONFIG_BATT_CELLS:
    if (navY != 0) {
      roverDisplay.setBattCellsCursorIdx(
          (roverDisplay.getBattCellsCursorIdx() + 3 + navY) % 3);
      forceDisplay = true;
    }
    if (navX == -1 || clickBack) {
      disarmRoverNav();
      roverDisplay.setPage(ROVER_PAGE_CONFIG_BATT);
      forceDisplay = true;
      printLog("[UI] Opcoes Bateria -> Bateria\n");
    } else if (navX == 1 || clickEnter) {
      disarmRoverNav();
      const uint8_t cellMap[3] = {3, 4, 6};
      uint8_t cells = cellMap[roverDisplay.getBattCellsCursorIdx() % 3];
      roverConfig.setBatteryCells(cells);
      roverConfig.saveConfig();
      battery.setCellCount(cells);

      char cmd[32];
      snprintf(cmd, sizeof(cmd), "CMD:BATT:%u", cells);
      interSerial.println(cmd);
      interSerial.flush();
      strncpy(queuedWebCommand, cmd, sizeof(queuedWebCommand) - 1);
      queuedWebCommand[sizeof(queuedWebCommand) - 1] = '\0';

      char fb[32];
      snprintf(fb, sizeof(fb), "BATERIA %uS GUARDADA!", cells);
      roverDisplay.showFeedback(fb, 1800);
      roverDisplay.setPage(ROVER_PAGE_CONFIG_BATT);
      forceDisplay = true;
      printLog("[UI] ★ Celulas da bateria alteradas para %uS\n", cells);
    }
    break;

  case ROVER_PAGE_CONFIG_JOY:
    if (navY != 0) {
      roverDisplay.setJoyMenuIdx((roverDisplay.getJoyMenuIdx() + 4 + navY) % 4);
      forceDisplay = true;
    }
    if (navX == -1 || clickBack) {
      disarmRoverNav();
      roverDisplay.setPage(ROVER_PAGE_CONFIG_MENU);
      forceDisplay = true;
      printLog("[UI] JoyStick -> Configuracoes\n");
    } else if (navX == 1 || clickEnter) {
      disarmRoverNav();
      uint8_t idx = roverDisplay.getJoyMenuIdx();
      if (idx == 0) {
        bool inv = !roverConfig.isJoystickInvertX();
        roverConfig.setJoystickInvertX(inv, true);
        joystick.setInvertX(inv);
        roverDisplay.showFeedback(inv ? "EIXO X: INVERTIDO" : "EIXO X: NORMAL",
                                  1500);
        printLog("[UI] ★ JoyStick Eixo X Invertido = %d\n", inv ? 1 : 0);
      } else if (idx == 1) {
        bool inv = !roverConfig.isJoystickInvertY();
        roverConfig.setJoystickInvertY(inv, true);
        joystick.setInvertY(inv);
        roverDisplay.showFeedback(inv ? "EIXO Y: INVERTIDO" : "EIXO Y: NORMAL",
                                  1500);
        printLog("[UI] ★ JoyStick Eixo Y Invertido = %d\n", inv ? 1 : 0);
      } else if (idx == 2) {
        joystick.wasShortClicked();
        joystick.wasLongClicked();
        joystick.wasBootHeld3s();
        roverDisplay.beginJoyCalWizard(joystick.getRawX(), joystick.getRawY());
        roverDisplay.setPage(ROVER_PAGE_CONFIG_JOY_CAL);
        printLog("[UI] JoyStick -> Calibrar (Wizard 1/5)\n");
      } else {
        joystick.wasBootHeld3s();
        roverDisplay.setJoyExitPromptActive(false);
        roverDisplay.setBootHoldProgressPct(0);
        roverDisplay.setPage(ROVER_PAGE_CONFIG_JOY_TEST);
        printLog("[UI] JoyStick -> Testar\n");
      }
      forceDisplay = true;
    }
    break;

  case ROVER_PAGE_CONFIG_WIFI:
    if (navX == -1 || clickBack) {
      disarmRoverNav();
      roverDisplay.setPage(ROVER_PAGE_CONFIG_MENU);
      forceDisplay = true;
      printLog("[UI] WiFi -> Configuracoes\n");
    } else if (navX == 1 || clickEnter) {
      disarmRoverNav();
      size_t totalItems = 1 + wifiConfig.getNetworks().size();
      roverDisplay.setWifiListIdx(0, totalItems);
      roverDisplay.setPage(ROVER_PAGE_CONFIG_WIFI_LIST);
      forceDisplay = true;
      printLog("[UI] WiFi -> Lista\n");
    }
    break;

  case ROVER_PAGE_CONFIG_WIFI_LIST: {
    const auto &nets = wifiConfig.getNetworks();
    size_t total = 1 + nets.size();
    if (navY != 0 && total > 0) {
      uint8_t nextIdx = (roverDisplay.getWifiListIdx() + total + navY) % total;
      roverDisplay.setWifiListIdx(nextIdx, total);
      forceDisplay = true;
    }
    if (navX == -1 || clickBack) {
      disarmRoverNav();
      roverDisplay.setPage(ROVER_PAGE_CONFIG_WIFI);
      forceDisplay = true;
      printLog("[UI] Lista -> WiFi\n");
    } else if (navX == 1 || clickEnter) {
      uint8_t idx = roverDisplay.getWifiListIdx();
      if (idx == 0) {
        roverDisplay.showWifiScanningScreen();
        const auto &scanned = wifiConfig.scanNetworks();
        disarmRoverNav();
        roverDisplay.setWifiScanIdx(0, scanned.size());
        roverDisplay.setPage(ROVER_PAGE_CONFIG_WIFI_SCAN);
        forceDisplay = true;
      } else if ((idx - 1) < nets.size()) {
        String targetSsid = nets[idx - 1].ssid;
        roverDisplay.showWifiConnectingScreen(targetSsid.c_str());
        wifiConfig.setDefault(targetSsid);
        bool ok = wifiConfig.connectTo(targetSsid, 8000);
        disarmRoverNav();
        if (ok) {
          roverDisplay.showFeedback("WIFI CONECTADO!", 2000);
        } else {
          char apName[32];
          snprintf(apName, sizeof(apName), "WINDDRAGONS-ROVER-%02d",
                   roverConfig.getRoverId());
          wifiConfig.startAccessPoint(apName, "winddragon");
          roverDisplay.showFeedback("FALHA! MODO AP ATIVO", 2000);
        }
        roverDisplay.setPage(ROVER_PAGE_CONFIG_WIFI);
        forceDisplay = true;
      }
    }
    break;
  }

  case ROVER_PAGE_CONFIG_WIFI_SCAN: {
    const auto &scanned = wifiConfig.getScannedNetworks();
    size_t total = scanned.size();
    if (navY != 0 && total > 0) {
      uint8_t nextIdx = (roverDisplay.getWifiScanIdx() + total + navY) % total;
      roverDisplay.setWifiScanIdx(nextIdx, total);
      forceDisplay = true;
    }
    if (navX == -1 || clickBack) {
      disarmRoverNav();
      roverDisplay.setPage(ROVER_PAGE_CONFIG_WIFI_LIST);
      forceDisplay = true;
      printLog("[UI] Scan -> Lista\n");
    } else if ((navX == 1 || clickEnter) && total > 0) {
      uint8_t idx = roverDisplay.getWifiScanIdx();
      if (idx < total) {
        const auto &sn = scanned[idx];
        if (wifiConfig.hasSavedNetwork(sn.ssid) || sn.open) {
          roverDisplay.showWifiConnectingScreen(sn.ssid.c_str());
          if (sn.open && !wifiConfig.hasSavedNetwork(sn.ssid)) {
            wifiConfig.addOrUpdateNetwork(sn.ssid, "", true);
          } else {
            wifiConfig.setDefault(sn.ssid);
          }
          bool ok = wifiConfig.connectTo(sn.ssid, 8000);
          disarmRoverNav();
          if (ok) {
            roverDisplay.showFeedback("WIFI CONECTADO!", 2000);
          } else {
            char apName[32];
            snprintf(apName, sizeof(apName), "WINDDRAGONS-ROVER-%02d",
                     roverConfig.getRoverId());
            wifiConfig.startAccessPoint(apName, "winddragon");
            roverDisplay.showFeedback("FALHA! MODO AP ATIVO", 2000);
          }
          roverDisplay.setPage(ROVER_PAGE_CONFIG_WIFI);
          forceDisplay = true;
        } else {
          roverDisplay.beginWifiPasswordEntry(sn.ssid.c_str());
          disarmRoverNav();
          roverDisplay.setPage(ROVER_PAGE_CONFIG_WIFI_PASS);
          forceDisplay = true;
        }
      }
    }
    break;
  }

  case ROVER_PAGE_CONFIG_ID:
  case ROVER_PAGE_CONFIG_FW:
  case ROVER_PAGE_CONFIG_LORA:
    if (navX == -1 || clickBack || clickEnter) {
      disarmRoverNav();
      roverDisplay.setPage(ROVER_PAGE_CONFIG_MENU);
      forceDisplay = true;
      printLog("[UI] Voltou para Configuracoes\n");
    }
    break;

  default:
    roverDisplay.setPage(ROVER_PAGE_HOME);
    forceDisplay = true;
    break;
  }
}

void setRgbColor(uint8_t r, uint8_t g, uint8_t b) {
  if (r == 0 && g == 0 && b == 0) {
    rgbLed.clear();
  } else {
    rgbLed.setPixelColor(0, rgbLed.Color(r, g, b));
  }
  rgbLed.show();
}

void printLog(const char *format, ...) {
  char buf[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);
  Serial.print(buf);
  webConsole.log(buf);
}

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  delay(500);

  printLog("\n=======================================================\n");
  printLog("  WINDDRAGONS ROVER-%02d: LAFVIN MASTER (LCD+SD+WIFI)\n", ROVER_ID);
  printLog("  Firmware: Master Lafvin %s (%s)\n", ROVER_LAFVIN_VERSION,
           ROVER_BUILD_DATE);
  printLog("=======================================================\n");

  // Desativar CS do SD durante boot
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);

  // 1. Inicializar LCD ST7789
  roverDisplay.begin();
  roverDisplay.bootLogf(UI_CYAN, 10, "[SYS ] Master Lafvin %s",
                        ROVER_LAFVIN_VERSION);

  // 2. LED RGB NeoPixel
  rgbLed.begin();
  setRgbColor(20, 20, 0); // Yellow: Booting

  // 3. Inicializar MicroSD Card (Partilha SPI com o LCD)
  roverDisplay.bootLogf(UI_LIGHTGREY, 20, "[SD  ] A inicializar leitor TF...");
  if (sdCard.begin()) {
    roverDisplay.bootLogf(UI_GREEN, 35, "[SD  ] Cartao TF 100%% OK");
    printLog("[SD  ] Cartao TF detetado e pronto (%llu MB)!\n",
             sdCard.getCardSizeMB());
  } else {
    roverDisplay.bootLogf(UI_YELLOW, 35, "[SD  ] Nenhum cartao detetado");
  }

  // 4. Carregar configurações da memória NVS (Flash) e MicroSD
  roverConfig.begin(&sdCard);
  {
    uint8_t r = 0, g = 0, b = 0;
    roverConfig.getScaledC6ModeColorRgb(0, r, g, b);
    setRgbColor(r, g, b);
  }
  uint8_t currentRoverId = roverConfig.getRoverId();
  webServer.setRoverId(currentRoverId);
  battery.setCellCount(roverConfig.getBatteryCells());
  battery.setCapacityMah(roverConfig.getBatteryCapacityMah());
  actuators.setAnchorCalibration(roverConfig.getAnchorDrumDiamMm(),
                                 roverConfig.getAnchorPulsesPerRev());
  actuators.setRackConfig(roverConfig.getRackAngleEngaged(),
                          roverConfig.getRackAngleReleased(),
                          roverConfig.isRackInverted(), false);
  actuators.setMotorInverted(roverConfig.isMotorInverted());
  actuators.setLedConfigState(roverConfig.getLedCount(),
                              roverConfig.getLedBrightness());
  webServer.setCommandQueueCallback([](const char *cmd) {
    if (cmd && strlen(cmd) > 0) {
      if (strncmp(cmd, "CMD:OTA:", 8) == 0) {
        // Envio imediato repetido para máxima fiabilidade no início do OTA
        for (int i = 0; i < 3; i++) {
          interSerial.println(cmd);
          interSerial.flush();
          delay(25);
        }
        printLog("[LAFVIN] >>> DISPARO OTA DIRETO UART C6 (x3): %s\n", cmd);
      } else {
        interSerial.println(cmd);
        interSerial.flush();
        printLog("[LAFVIN] >>> COMANDO UART DIRETO: %s\n", cmd);
      }
    }
  });

  printLog("\n=======================================================\n");
  printLog("  WINDDRAGONS ROVER-%02d: LAFVIN MASTER (LCD+SD+WIFI)\n",
           currentRoverId);
  printLog("  Identificador NVS ativo: ROVER-%02d | Bateria: %uS (%u mAh)\n",
           currentRoverId, roverConfig.getBatteryCells(),
           roverConfig.getBatteryCapacityMah());
  printLog("=======================================================\n");

  roverDisplay.showBootScreen(currentRoverId);
  roverDisplay.bootLogf(UI_CYAN, 50,
                        "[BOOT] WindDragons Master (ID NVS: %u | %uS %umAh)",
                        currentRoverId, roverConfig.getBatteryCells(),
                        roverConfig.getBatteryCapacityMah());

  // 5. Configurar WiFi e Servidor Web
  char apName[32];
  snprintf(apName, sizeof(apName), "WINDDRAGONS-ROVER-%02d", currentRoverId);
  wifiConfig.begin(&sdCard);
  roverDisplay.setWiFiConfigManager(&wifiConfig);

  roverDisplay.bootLogf(UI_YELLOW, 65, "[WIFI] A procurar redes...");
  printLog("[WIFI] A procurar redes no cartao SD / NVS...\n");

  // Tentar conectar às redes configuradas em /wifi_networks.json ou NVS
  // (timeout de 6 segundos)
  bool wifiOk = wifiConfig.autoConnect(6000, [](const char *ssid) {
    roverDisplay.bootLogf(UI_YELLOW, 70, "[WIFI] A testar '%s'...", ssid);
    printLog("[WIFI] A testar ligacao a: %s\n", ssid);
  });

  if (wifiOk) {
    printLog("[WIFI] Ligado com sucesso! SSID: %s | IP: %s\n",
             wifiConfig.getSSID().c_str(), wifiConfig.getIPAddress().c_str());
    roverDisplay.bootLogf(UI_GREEN, 75, "[WIFI] STA: %s",
                          wifiConfig.getSSID().c_str());
    roverDisplay.bootLogf(UI_GREEN, 80, "[WIFI] IP: %s",
                          wifiConfig.getIPAddress().c_str());
  } else {
    // Fallback automático para Ponto de Acesso
    wifiConfig.startAccessPoint(apName, "winddragon");
    printLog("[WIFI] Sem rede STA. Modo Ponto de Acesso: %s (192.168.4.1)\n",
             apName);
    roverDisplay.bootLogf(UI_YELLOW, 75, "[WIFI] Sem rede -> Modo AP");
    roverDisplay.bootLogf(UI_CYAN, 80, "[WIFI] AP: %s (192.168.4.1)", apName);
  }

  webServer.setLoraDiagnosticsPtr(&roverLoraDiag);
  webServer.begin();

  // 6. Configurar UART Inter-ESP (TX=GPIO 2, RX=GPIO 1)
  interSerial.setRxBufferSize(512);
  interSerial.begin(INTER_ESP_BAUDRATE, SERIAL_8N1, PIN_INTER_ESP_LAFVIN_RX,
                    PIN_INTER_ESP_LAFVIN_TX);
  roverDisplay.bootLogf(UI_GREEN, 90, "[UART] Inter-ESP TX=2, RX=1");
  Serial.printf(
      "[UART] Canal serie com C6 iniciado nos pinos TX=%d, RX=%d (%d baud)\n",
      PIN_INTER_ESP_LAFVIN_TX, PIN_INTER_ESP_LAFVIN_RX, INTER_ESP_BAUDRATE);

  // 7. Inicializar Joystick e Botões (mesmo driver BaseJoystick usado na Base)
  joystick.begin();
  joystick.setInvertX(roverConfig.isJoystickInvertX());
  joystick.setInvertY(roverConfig.isJoystickInvertY());
  if (roverConfig.isJoystickCalibrated()) {
    joystick.setCalibration(
        roverConfig.getJoyCenterX(), roverConfig.getJoyCenterY(),
        roverConfig.getJoyXLeft(), roverConfig.getJoyXRight(),
        roverConfig.getJoyYUp(), roverConfig.getJoyYDown());
  }
  joyCenterX = joystick.getCenterX();
  joyCenterY = joystick.getCenterY();
  joyHwConnected = joystick.isAxisXConnected() || joystick.isAxisYConnected();
  printLog("[JOYSTICK] Inicializado (X0=%d [%s], Y0=%d [%s], InvX=%d, InvY=%d, "
           "Cal=%d)\n",
           joyCenterX, joystick.isAxisXConnected() ? "OK" : "ABERTO",
           joyCenterY, joystick.isAxisYConnected() ? "OK" : "ABERTO",
           roverConfig.isJoystickInvertX() ? 1 : 0,
           roverConfig.isJoystickInvertY() ? 1 : 0,
           roverConfig.isJoystickCalibrated() ? 1 : 0);

  roverDisplay.bootLogf(UI_GREEN, 100, "[BOOT] Sistema Pronto!");
  roverDisplay.endBootScreen(600);

  // 9. Configurar GPS Quectel LC29H no Header J2 (RX=GPIO 16, TX=GPIO 17)
  gps.begin(true, GPS_BAUDRATE, PIN_LAFVIN_GPS_RX, PIN_LAFVIN_GPS_TX);
  printLog("[GPS ] Quectel LC29H inicializado no Lafvin (RX=GPIO %d, TX=GPIO "
           "%d a %d baud)\n",
           PIN_LAFVIN_GPS_RX, PIN_LAFVIN_GPS_TX, GPS_BAUDRATE);
}

void loop() {
  uint32_t now = millis();

  // 1. Atualizar GPS Quectel LC29H
  gps.update();

  // 1. Processar clientes HTTP Web
  webServer.handleClient();

  // 2. Navegação de Menus (Joystick + Botões, idêntico ao firmware da Base)
  if (motorTestStopAtMs > 0 && now >= motorTestStopAtMs) {
    motorTestStopAtMs = 0;
    actuators.stopMotor();
    interSerial.println("CMD:MOTOR:0");
    interSerial.flush();
    strncpy(queuedWebCommand, "CMD:MOTOR:0", sizeof(queuedWebCommand) - 1);
    queuedWebCommand[sizeof(queuedWebCommand) - 1] = '\0';
    printLog("[UI] ★ Fim do pulso de teste do Motor ESC (0%%)\n");
  }
  joystick.setInvertX(roverConfig.isJoystickInvertX());
  joystick.setInvertY(roverConfig.isJoystickInvertY());
  bool forceDisplay = false;
  handleRoverMenuInputs(now, forceDisplay);

  // 3. Leitura do Joystick Analógico com Curso Completo (100% no fim mecânico)
  static uint32_t lastJoyReadTime = 0;
  static int lastSentThr = 0;
  static int lastSentRud = 0;
  static bool joyActive = false;
  static float filtX = 0, filtY = 0;
  static bool filtInit = false;
  static bool wasJoyEnabled = roverConfig.isJoystickEnabled();

  bool joyEn = roverConfig.isJoystickEnabled();
  if (joyEn != wasJoyEnabled) {
    if (!joyEn) {
      if (joyActive) {
        joyActive = false;
        lastSentRud = 0;
        lastSentThr = 0;
        actuators.setRudder(0);
        actuators.setThrottle(0);
        interSerial.println("CMD:JOY:0:0");
        interSerial.flush();
        snprintf(queuedWebCommand, sizeof(queuedWebCommand), "CMD:JOY:0:0");
      }
      printLog("[JOYSTICK] Leitura do joystick fisico DESATIVADA\n");
    } else {
      calibrateLocalJoystick(20, 2);
      filtInit = false;
      printLog("[JOYSTICK] Leitura do joystick fisico ATIVADA (X0=%d, Y0=%d, "
               "HW=%s)\n",
               joyCenterX, joyCenterY, joyHwConnected ? "OK" : "FLUTUANTE");
    }
    wasJoyEnabled = joyEn;
  }

  // Apenas atua nos servos/motor quando estiver dentro da página "Atuadores"
  // (ROVER_PAGE_ACTUATORS) e sem o modal SAIR | CONTINUAR aberto
  bool inActuatorsPage =
      (roverDisplay.getCurrentPage() == ROVER_PAGE_ACTUATORS &&
       !roverDisplay.isJoyExitPromptActive());
  static bool joyActuatorsArmed = false;
  if (!inActuatorsPage) {
    joyActuatorsArmed = false;
  }

  // Apenas lê o joystick local para os atuadores se estiver na página Atuadores
  // (ou se precisar limpar joyActive ao sair) e o hardware físico estiver
  // ligado
  if ((inActuatorsPage || joyActive) && joyHwConnected &&
      (now - lastJoyReadTime >= 50)) {
    lastJoyReadTime = now;
    int rawX = analogRead(PIN_JOYSTICK_X);
    int rawY = analogRead(PIN_JOYSTICK_Y);

    if (!filtInit) {
      filtX = rawX;
      filtY = rawY;
      filtInit = true;
    }
    filtX = 0.75f * filtX + 0.25f * (float)rawX;
    filtY = 0.75f * filtY + 0.25f * (float)rawY;

    int deadband = 220; // Zona morta alinhada com BaseJoystick
    bool axisXValid = (joyCenterX >= 1100 && joyCenterX <= 2900);
    bool axisYValid = (joyCenterY >= 1100 && joyCenterY <= 2900);
    int diffX = axisXValid ? ((int)filtX - joyCenterX) : 0;
    int diffY = axisYValid ? ((int)filtY - joyCenterY) : 0;
    if (roverConfig.isJoystickInvertX())
      diffX = -diffX;
    if (roverConfig.isJoystickInvertY())
      diffY = -diffY;

    static int maxSeenRight = 1200;
    static int maxSeenLeft = 1200;
    static int maxSeenDown = 1200;
    static int maxSeenUp = 1200;

    int rud = 0;
    int thr = 0;
    if (joystick.isCalibrated()) {
      rud = joystick.getRudder();
      thr = joystick.getThrottle();
    } else {
      if (diffX > deadband) {
        // Estibordo (Right)
        int d = diffX - deadband;
        if (d > maxSeenRight)
          maxSeenRight = d;
        rud = constrain(d * 100 / maxSeenRight, 0, 100);
      } else if (diffX < -deadband) {
        // Bombordo (Left)
        int d = (-diffX) - deadband;
        if (d > maxSeenLeft)
          maxSeenLeft = d;
        rud = -constrain(d * 100 / maxSeenLeft, 0, 100);
      }

      if (diffY > deadband) {
        // Marcha-atrás / Ré (Pull back)
        int d = diffY - deadband;
        if (d > maxSeenDown)
          maxSeenDown = d;
        thr = -constrain(d * 100 / maxSeenDown, 0, 100);
      } else if (diffY < -deadband) {
        // Avanço / Frente (Push forward)
        int d = (-diffY) - deadband;
        if (d > maxSeenUp)
          maxSeenUp = d;
        thr = constrain(d * 100 / maxSeenUp, 0, 100);
      }
    }

    // Exige passagem pelo centro neutro ao entrar em "Atuadores" (para que o
    // movimento X-> de entrada no menu não dispare o leme) e bloqueia fora de
    // "Atuadores"
    if (!inActuatorsPage) {
      rud = 0;
      thr = 0;
    } else if (!joyActuatorsArmed) {
      if (rud == 0 && thr == 0) {
        joyActuatorsArmed = true;
      } else {
        rud = 0;
        thr = 0;
      }
    }

    if (rud != 0 || thr != 0) {
      joyActive = true;
      actuators.setRudder((int8_t)rud);
      actuators.setThrottle((int8_t)thr);
      bool hitFullScale = (abs(rud) == 100 && lastSentRud != rud) ||
                          (abs(thr) == 100 && lastSentThr != thr);
      if (abs(rud - lastSentRud) >= 3 || abs(thr - lastSentThr) >= 3 ||
          hitFullScale) {
        lastSentRud = rud;
        lastSentThr = thr;
        interSerial.printf("CMD:JOY:%d:%d\n", thr, rud);
        interSerial.flush();
        snprintf(queuedWebCommand, sizeof(queuedWebCommand), "CMD:JOY:%d:%d",
                 thr, rud);
      }
    } else if (joyActive) {
      joyActive = false;
      lastSentRud = 0;
      lastSentThr = 0;
      actuators.setRudder(0);
      actuators.setThrottle(0);
      interSerial.println("CMD:JOY:0:0");
      interSerial.flush();
      snprintf(queuedWebCommand, sizeof(queuedWebCommand), "CMD:JOY:0:0");
    }
  }

  // Consulta periódica da versão do Coprocessador C6 até obter resposta
  static uint32_t lastVerCheck = 0;
  if (strcmp(queuedWebCommand, "NONE") == 0 &&
      strcmp(roverDisplay.getC6Version(), "--") == 0) {
    if (now - lastVerCheck >= 2000) {
      lastVerCheck = now;
      strncpy(queuedWebCommand, "CMD:GET_VER", sizeof(queuedWebCommand) - 1);
    }
  }

  // 4. Comunicação Inter-ESP UART (Ping periódico a cada 200ms ou rápido a cada
  // 50ms se houver comando pendente)
  bool hasPendingCmd = (strcmp(queuedWebCommand, "NONE") != 0);
  if ((now - lastUartPingTime >= 200) ||
      (hasPendingCmd && (now - lastUartPingTime >= 50))) {
    lastUartPingTime = now;
    uartSeq++;

    // Se houver comando pendente, envia-o como linha direta dedicada CMD:...
    if (hasPendingCmd) {
      interSerial.println(queuedWebCommand);
      interSerial.flush();
      printLog("[LAFVIN] >>> COMANDO PENDENTE UART: %s\n", queuedWebCommand);
      strncpy(queuedWebCommand, "NONE", sizeof(queuedWebCommand) - 1);
    }

    // PING de telemetria periódico limpo (sem colidir colons em scansets)
    char pingMsg[160];
    snprintf(pingMsg, sizeof(pingMsg),
             "PING:%u:%u:NONE:%ld:%ld:%.1f:%.1f:%u:%u\n", uartSeq,
             roverConfig.getRoverId(), (long)gps.getLatDeg7(),
             (long)gps.getLngDeg7(), gps.getSpeedKnots(), gps.getGpsCourse(),
             gps.getSatellites(), gps.hasFix() ? 1 : 0);
    interSerial.print(pingMsg);
  }

  // 4. Receção e interpretação de respostas PONG do C6 Coprocessador
  while (interSerial.available()) {
    char c = (char)interSerial.read();
    static char rxBuf[256];
    static size_t rxIdx = 0;

    if (c == '\n' || c == '\r') {
      if (rxIdx > 0) {
        rxBuf[rxIdx] = '\0';

        if (strncmp(rxBuf, "PONG:OTA:", 9) == 0) {
          lastUartRxTime = now;
          printLog("[LAFVIN ROVER] ★ C6 OTA: %s\n", rxBuf + 9);
          webServer.setC6OtaStatus(rxBuf + 9);
        } else if (strncmp(rxBuf, "PONG:VER:", 9) == 0) {
          lastUartRxTime = now;
          printLog("[LAFVIN ROVER] ★ Versao Coprocessador C6: %s\n", rxBuf + 9);
          webServer.setC6Version(rxBuf + 9);
          roverDisplay.setC6Version(rxBuf + 9);
          // Sincronizar calibração da cremalheira, âncora, bateria e estado do
          // joystick com o Coprocessador C6
          interSerial.printf("CMD:BATT:%u\n", roverConfig.getBatteryCells());
          interSerial.printf("CMD:ANC_CFG:%.1f:%u\n",
                             roverConfig.getAnchorDrumDiamMm(),
                             roverConfig.getAnchorPulsesPerRev());
          interSerial.printf("CMD:RACK_CFG:%u:%u:%d\n",
                             roverConfig.getRackAngleEngaged(),
                             roverConfig.getRackAngleReleased(),
                             roverConfig.isRackInverted() ? 1 : 0);
          interSerial.printf("CMD:MOTOR_INV:%d\n",
                             roverConfig.isMotorInverted() ? 1 : 0);
          interSerial.printf("CMD:JOY_EN:%d\n",
                             roverConfig.isJoystickEnabled() ? 1 : 0);
          interSerial.printf("CMD:LED:CFG:%u:%u\n", roverConfig.getLedCount(),
                             roverConfig.getLedBrightness());
          interSerial.println(roverConfig.buildLedModeAnimsCommand());
          interSerial.println(roverConfig.buildC6LedCfgCommand());
          interSerial.flush();
        } else if (strncmp(rxBuf, "LED_MODES:", 10) == 0) {
          lastUartRxTime = now;
          int m[7] = {0, 0, 0, 0, 0, 0, 0};
          int n = sscanf(rxBuf + 10, "%d:%d:%d:%d:%d:%d:%d", &m[0], &m[1],
                         &m[2], &m[3], &m[4], &m[5], &m[6]);
          if (n >= 1) {
            for (int i = 0; i < 7 && i < n; i++) {
              roverConfig.setLedModeAnim((uint8_t)i, (uint8_t)m[i], false);
            }
            roverConfig.saveConfig();
            printLog("[LAFVIN ROVER] ★ Animacoes Nauticas sincronizadas da C6 "
                     "e gravadas no SD/NVS!\n");
          }
        } else if (strncmp(rxBuf, "PONG:", 5) == 0) {
          lastUartRxTime = now;
          uint32_t seq = 0, batMv = 0, batPct = 0, sats = 0, fix = 0, motSt = 0,
                   depth = 0, ancSt = 0;
          long lat7 = 0, lng7 = 0, hallPulses = -1;
          float batA = 0.0f, spd = 0.0f, head = 0.0f, rssi = 0.0f, snr = 0.0f;
          int rud = 0, thr = 0, winch = 0, rack = 0;
          char rfSt[32] = "OK";

          int parsed =
              sscanf(rxBuf + 5,
                     "%u:%u:%f:%u:%ld:%ld:%f:%f:%u:%u:%u:%d:%d:%d:%d:%"
                     "u:%31[^:]:%f:%f:%u:%ld",
                     &seq, &batMv, &batA, &batPct, &lat7, &lng7, &spd, &head,
                     &sats, &fix, &motSt, &rud, &thr, &winch, &rack, &depth,
                     rfSt, &rssi, &snr, &ancSt, &hallPulses);
          if (parsed >= 10) {
            battery.setValues(batMv, batA, (uint8_t)batPct);
            if (!gps.isHardwareLocked() && !gps.hasFix()) {
              gps.setValues((double)lat7 / 10000000.0,
                            (double)lng7 / 10000000.0, spd, head, (uint8_t)sats,
                            fix > 0);
            }
            if (parsed < 20) {
              ancSt = (motSt == MOTOR_ANCHORED) ? ANCHOR_DEPLOYED
                                                : ANCHOR_RETRACTED;
            }
            int8_t effThr = joyActive ? actuators.getThrottle() : (int8_t)thr;
            int8_t effRud = joyActive ? actuators.getRudder() : (int8_t)rud;
            actuators.setValues(effThr, effRud, winch, rack > 0, (uint8_t)ancSt,
                                (uint16_t)depth, hallPulses);
            currentMotorStatus = (uint8_t)motSt;
            currentLoraRssi = (int16_t)rssi;
            currentLoraSnr = snr;
            strncpy(loraStatusStr, rfSt, sizeof(loraStatusStr) - 1);
            printLog("[LAFVIN ROVER] <<< PONG #%u: Bat=%.2fV / %.2fA (%u%%) | "
                     "LoRa=%s (RSSI=%.1f) | Fix=%u Sats=%u\n",
                     seq, batMv / 1000.0f, batA, batPct, rfSt, rssi, fix, sats);
          }
        } else if (strncmp(rxBuf, "LORA_STAT:", 10) == 0) {
          lastUartRxTime = now;
          char st[16] = "OK";
          uint32_t txT = 0, rxP = 0, ago = 999999;
          float rssi = 0.0f, snr = 0.0f;
          int contact = 0;
          if (sscanf(rxBuf + 10, "%15[^:]:%u:%u:%lu:%f:%f:%d", st, &txT, &rxP,
                     &ago, &rssi, &snr, &contact) >= 5) {
            roverLoraDiag.hwReady = (strcmp(st, "OK") == 0);
            roverLoraDiag.txTelems = txT;
            roverLoraDiag.rxPolls = rxP;
            roverLoraDiag.lastRxSecAgo = ago;
            roverLoraDiag.rssi = rssi;
            roverLoraDiag.snr = snr;
            roverLoraDiag.hasContact = (contact != 0);
            roverDisplay.setLoraStats(txT, rxP, ago, snr, contact != 0);
          }
        } else if (strncmp(rxBuf, "COMPASS_STAT:", 13) == 0) {
          lastUartRxTime = now;
          int cmpReady = 0;
          float cmpHead = 0.0f, cmpRoll = 0.0f, cmpPitch = 0.0f, cmpOff = 0.0f,
                cmpRawHead = 0.0f;
          uint32_t calSys = 0, calMag = 0, calGyro = 0, calAccel = 0,
                   i2cAddr = 0x28;
          int n =
              sscanf(rxBuf + 13, "%d:%f:%f:%f:%u:%u:%f:%u:%u:%f:%u", &cmpReady,
                     &cmpHead, &cmpRoll, &cmpPitch, &calSys, &calMag, &cmpOff,
                     &calGyro, &calAccel, &cmpRawHead, &i2cAddr);
          if (n >= 6) {
            gps.setCompassData(cmpReady != 0, cmpHead, cmpRoll, cmpPitch,
                               (uint8_t)calSys, (uint8_t)calMag, cmpOff,
                               (uint8_t)calGyro, (uint8_t)calAccel, cmpRawHead,
                               (uint8_t)i2cAddr);
          }
        } else if (strncmp(rxBuf, "HOME_STAT:", 10) == 0) {
          lastUartRxTime = now;
          int hmSet = 0;
          long hmLat7 = 0, hmLng7 = 0;
          if (sscanf(rxBuf + 10, "%d:%ld:%ld", &hmSet, &hmLat7, &hmLng7) >= 1) {
            if (hmSet != 0 && (hmLat7 != 0 || hmLng7 != 0)) {
              gps.setHome((double)hmLat7 / 10000000.0,
                          (double)hmLng7 / 10000000.0);
            } else if (hmSet == 0) {
              gps.clearHome();
            }
          }
        } else {
          lastUartRxTime = now;
          webConsole.logf("[C6] %s", rxBuf);
        }
        rxIdx = 0;
      }
    } else {
      if (rxIdx < sizeof(rxBuf) - 1)
        rxBuf[rxIdx++] = c;
    }
  }

  // 5. Atualização do ecrã LCD ST7789 (a cada 200ms ou 60ms em Atuadores /
  // Teste / Calibração de Joystick, se barramento SPI estiver livre)
  static uint32_t lastLcdUpdate = 0;
  uint32_t lcdInterval =
      (roverDisplay.getCurrentPage() == ROVER_PAGE_CONFIG_JOY_TEST ||
       roverDisplay.getCurrentPage() == ROVER_PAGE_CONFIG_JOY_CAL ||
       roverDisplay.getCurrentPage() == ROVER_PAGE_CONFIG_NAV_WIZARD ||
       roverDisplay.getCurrentPage() == ROVER_PAGE_CONFIG_COMPASS ||
       roverDisplay.getCurrentPage() == ROVER_PAGE_ACTUATORS)
          ? 60
          : 200;
  if (!webServer.isBusy() &&
      (forceDisplay || (now - lastLcdUpdate >= lcdInterval))) {
    lastLcdUpdate = now;
    bool failsafeActive = (currentMotorStatus == MOTOR_FAILSAFE);

    roverDisplay.update(
        roverConfig.getRoverId(), gps, battery, actuators, currentMotorStatus,
        currentLoraRssi, failsafeActive, forceDisplay, sdCard.isReady(),
        wifiConfig.isAPMode(), wifiConfig.getIPAddress().c_str(),
        wifiConfig.getSSID().c_str(), wifiConfig.getRSSI(),
        wifiConfig.getAPStationCount(), wifiConfig.getAPPassword().c_str(),
        roverConfig.isJoystickInvertX(), roverConfig.isJoystickInvertY(),
        joystick.getRawX(), joystick.getRawY(), joystick.getThrottle(),
        joystick.getRudder(), joystick.getDeadband(), joystick.isBootPressed(),
        joystick.isModePressed(), joystick.isAnchorUpPressed(),
        joystick.isAnchorDownPressed(), joystick.getJoyClickCount());
  }

  // 5b. Atualizar LED NeoPixel onboard (GPIO 8) conforme modo e intensidade C6
  static uint32_t lastLafvinLedHash = 0xFFFFFFFFUL;
  uint8_t c6Brt = roverConfig.getC6LedBrightness();
  uint8_t mIdx = (currentMotorStatus < 7) ? currentMotorStatus : 0;
  bool blinkOff =
      (currentMotorStatus == MOTOR_FAILSAFE) && ((now % 500) >= 250);
  String mHex = roverConfig.getC6ModeColor(mIdx);
  uint32_t mHexNum = (mHex.length() >= 7)
                         ? (uint32_t)strtoul(mHex.c_str() + 1, nullptr, 16)
                         : 0;
  uint32_t curLedHash = ((uint32_t)mIdx << 28) ^ ((uint32_t)c6Brt << 20) ^
                        ((uint32_t)(blinkOff ? 1 : 0) << 19) ^ mHexNum;
  if (curLedHash != lastLafvinLedHash) {
    lastLafvinLedHash = curLedHash;
    if (c6Brt == 0 || blinkOff) {
      setRgbColor(0, 0, 0);
    } else {
      uint8_t r = 0, g = 0, b = 0;
      roverConfig.getScaledC6ModeColorRgb(mIdx, r, g, b);
      setRgbColor(r, g, b);
    }
  }

  // 6. Gravação de log no cartão MicroSD (a cada 5000ms se o servidor web não
  // estiver ocupado)
  static uint32_t lastSdLogTime = 0;
  if (sdCard.isReady() && !webServer.isBusy() &&
      (now - lastSdLogTime >= 5000)) {
    lastSdLogTime = now;
    char logBuf[220];
    snprintf(
        logBuf, sizeof(logBuf),
        "%lu,%.6f,%.6f,%.1f,%.1f,%u,%.2fV,%.2fA,%u,%s,%d,%s,%.1f,%.1f,%u,%u\n",
        now, gps.getLatitude(), gps.getLongitude(), gps.getSpeedKnots(),
        gps.getHeading(), battery.getPercentage(),
        battery.getVoltageMv() / 1000.0f, battery.getCurrentAmps(),
        currentMotorStatus, loraStatusStr, currentLoraRssi,
        gps.hasCompass() ? "BNO055" : "GPS_COG", gps.getRoll(), gps.getPitch(),
        gps.getCompassCalSys(), gps.getCompassCalMag());
    sdCard.appendFile("/telemetry.csv", logBuf);
  }

  // 7. Consola de comandos Serial (ex: SET_ID 2)
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.startsWith("SET_ID ") || cmd.startsWith("ID ")) {
      int newId = cmd.substring(cmd.indexOf(' ') + 1).toInt();
      if (newId >= 1 && newId <= 254) {
        roverConfig.setRoverId((uint8_t)newId);
        roverConfig.saveConfig();
        printLog("[LAFVIN] ★ ID guardado em NVS: %d. A reiniciar...\n", newId);
        delay(500);
        ESP.restart();
      }
    }
  }
}
