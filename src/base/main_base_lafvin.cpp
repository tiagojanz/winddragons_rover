#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <Preferences.h>
#include <SPI.h>

#include "base_display.h"
#include "base_joystick.h"
#include "base_network.h"
#include "base_version.h"
#include "base_web_server.h"
#include "config_common.h"
#include "fleet_manager.h"
#include "lora_protocol.h"
#include "sd_card_manager.h"
#include "web_console.h"
#include "wifi_config_manager.h"

// =========================================================================
// WINDDRAGONS BASE STATION - LAFVIN MASTER FIRMWARE
// Placa: Lafvin ESP32-C6 LCD 1.47" (IPS ST7789 320x172)
// Arquitetura Universal Dual ESP32-C6:
//  - Master Lafvin: LCD ST7789, MicroSD TF, WiFi STA/AP, WebServer, Joystick
//  - Coprocessador C6: LoRa SX1278 (433 MHz) em barramento de hardware FSPI
//  - Inter-ESP Link: UART (Lafvin TX=GPIO 2, RX=GPIO 1 @ 115200 baud)
// =========================================================================

// Consola Web e Monitor de Logs em Tempo Real
WebConsoleClass webConsole;

// Componentes do Sistema
FleetManager fleet;
BaseJoystick joystick;
WiFiConfigManager wifiConfig;
BaseNetwork network(fleet, wifiConfig);
BaseDisplay display;
SDCardManager sdCard;
BaseWebServer webServer(wifiConfig, fleet, &sdCard, &network);
Adafruit_NeoPixel rgbLed(1, PIN_RGB_LED, NEO_GRB + NEO_KHZ800);

// UART Inter-ESP (Lafvin Master <-> C6 Coprocessor)
HardwareSerial interSerial(1);
uint32_t uartSeq = 0;
uint32_t lastUartPingTime = 0;
uint32_t lastUartRxTime = 0;
char queuedWebCommand[256] = "NONE";
bool loraReady = false;
BaseLoraDiagnostics baseLoraDiag;

// Temporizadores e controlo
uint32_t lastControlTxTime = 0;
const uint32_t CONTROL_TX_INTERVAL_MS = 100; // 10Hz stream de controlo

// Modo de navegação e controlo UI
uint8_t currentNavMode = 0; // 0 = MANUAL, 1 = HOLD STATION, 2 = RETURN HOME
uint16_t localCmdId = 1000;

// Botão BOOT
volatile bool flagPageChange = false;
volatile uint32_t lastBtnPressTime = 0;

void IRAM_ATTR isrBootButton() {
  uint32_t now = millis();
  if (now - lastBtnPressTime > 200) {
    lastBtnPressTime = now;
    flagPageChange = true;
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
  Serial.setTxTimeoutMs(0); // Não-bloqueante USB CDC
  delay(200);

  printLog("\n=======================================================\n");
  printLog("  WINDDRAGONS BASE STATION: LAFVIN MASTER (LCD+SD+WIFI)\n");
  printLog("  Firmware: Master Lafvin %s (%s)\n", BASE_LAFVIN_VERSION,
           BASE_BUILD_DATE);
  printLog("=======================================================\n");

  // Desativar CS de periféricos SPI antes do arranque
  pinMode(PIN_LCD_CS, OUTPUT);
  digitalWrite(PIN_LCD_CS, HIGH);
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);

  // 1. Inicializar Display ST7789
  display.begin();
  display.showBootScreen();
  display.bootLogf(UI_CYAN, 10, "[BOOT] WindDragons Base %s",
                   BASE_LAFVIN_VERSION);

  // 2. LED RGB NeoPixel
  rgbLed.begin();
  setRgbColor(20, 20, 0); // Amarelo: arranque
  display.bootLogf(UI_LIGHTGREY, 20, "[LED ] NeoPixel RGB ativo");

  // 3. Inicializar MicroSD Card
  printLog("[BASE] A inicializar cartao MicroSD... ");
  if (sdCard.begin()) {
    printLog("OK (%llu MB)!\n", sdCard.getCardSizeMB());
    display.bootLogf(UI_GREEN, 35, "[SD  ] MicroSD FAT32 pronto");
  } else {
    printLog("Sem cartao presente.\n");
    display.bootLogf(UI_YELLOW, 35, "[SD  ] Sem cartao presente");
  }

  // 3.1 Inicializar Frota & Missão (SD / NVS)
  fleet.begin(&sdCard);
  {
    uint8_t r = 0, g = 0, b = 0;
    fleet.getScaledModeColorRgb(static_cast<uint8_t>(fleet.getOperationState()),
                                r, g, b);
    setRgbColor(r, g, b);
  }
  display.bootLogf(UI_GREEN, 48, "[FROTA] %u rovers | %s",
                   (uint32_t)fleet.getRovers().size(),
                   fleet.getOperationStateBadge());

  // 4. Inicializar Joystick
  joystick.begin();
  printLog(
      "[JOY ] Auto-calibracao concluida: Y(IO0)=%d [%s] | X(IO3)=%d [%s]\n",
      joystick.getCenterY(),
      joystick.isAxisYConnected() ? "CONECTADO" : "FLUTUANTE/ABERTO",
      joystick.getCenterX(),
      joystick.isAxisXConnected() ? "CONECTADO" : "FLUTUANTE/ABERTO");
  display.bootLogf(UI_LIGHTGREY, 55, "[JOY ] IO0:%d IO3:%d",
                   joystick.getCenterY(), joystick.getCenterX());

  // 5. Configurar UART Inter-ESP (TX=GPIO 2, RX=GPIO 1) com o Coprocessador C6
  interSerial.setRxBufferSize(512);
  interSerial.begin(INTER_ESP_BAUDRATE, SERIAL_8N1, PIN_INTER_ESP_LAFVIN_RX,
                    PIN_INTER_ESP_LAFVIN_TX);
  display.bootLogf(UI_GREEN, 65, "[UART] Inter-ESP C6 115200");
  printLog("[BASE] UART com Coprocessador C6 iniciada (TX=GPIO%d, RX=GPIO%d @ "
           "%d baud)\n",
           PIN_INTER_ESP_LAFVIN_TX, PIN_INTER_ESP_LAFVIN_RX,
           INTER_ESP_BAUDRATE);

  // Consulta imediata de versão e sincronização de LED ao Coprocessador C6
  interSerial.println("CMD:GET_VER");
  interSerial.println(fleet.buildC6LedCfgCommand());
  interSerial.flush();

  // 6. Configurar WiFi e Gestor de Redes
  display.bootLogf(UI_YELLOW, 75, "[NET ] A ligar WiFi (STA)...");
  wifiConfig.begin(&sdCard);
  bool wifiOk = wifiConfig.autoConnect(8000, [](const char *ssid) {
    display.bootLogf(UI_YELLOW, 80, "[NET ] A testar '%s'...", ssid);
  });

  if (!wifiOk) {
    printLog("[BASE] Sem rede STA -> A ativar Modo AP (WindDragons-Base)\n");
    wifiConfig.startAccessPoint("WindDragons-Base", "12345678");
    display.bootLogf(UI_YELLOW, 85, "[NET ] Falha STA -> Modo AP");
    display.bootLogf(UI_CYAN, 88, "[NET ] AP: WindDragons-Base");
    display.bootLogf(UI_WHITE, 90, "[NET ] IP: 192.168.4.1");
  } else {
    printLog("[BASE] WiFi Conectado! SSID: %s | IP: %s\n",
             wifiConfig.getSSID().c_str(), wifiConfig.getIPAddress().c_str());
    display.bootLogf(UI_GREEN, 90, "[NET ] IP: %s",
                     wifiConfig.getIPAddress().c_str());
  }

  // 7. Configurar Callback de Comandos Web e Iniciar Servidor Web
  webServer.setLoraReadyPtr(&loraReady);
  webServer.setLoraDiagnosticsPtr(&baseLoraDiag);
  webServer.setCommandQueueCallback([](const char *cmd) {
    if (cmd && strlen(cmd) > 0) {
      if (strncmp(cmd, "CMD:OTA:", 8) == 0) {
        // Envio imediato repetido (x3) para garantir receção do comando de OTA
        // pelo C6
        for (int i = 0; i < 3; i++) {
          interSerial.println(cmd);
          interSerial.flush();
          delay(25);
        }
        printLog("[BASE LAFVIN] >>> DISPARO OTA DIRETO UART C6 (x3): %s\n",
                 cmd);
      } else {
        interSerial.println(cmd);
        interSerial.flush();
        printLog("[BASE LAFVIN] >>> COMANDO UART DIRETO: %s\n", cmd);
      }
      strncpy(queuedWebCommand, cmd, sizeof(queuedWebCommand) - 1);
      queuedWebCommand[sizeof(queuedWebCommand) - 1] = '\0';
    }
  });
  webServer.begin();
  display.bootLogf(UI_GREEN, 95, "[HTTP] Servidor Web OK");

  // 8. Iniciar sincronização Cloud
  network.begin();
  display.bootLogf(UI_CYAN, 98, "[SYNC] Sincronizacao Cloud");

  // 9. Configurações de display
  Preferences dispPrefs;
  if (dispPrefs.begin("base_display", true)) {
    bool joyScr = dispPrefs.getBool("joy_scr", true);
    display.setJoystickScreenEnabled(joyScr);
    uint8_t bright = dispPrefs.getUChar("brightness", display.getBrightness());
    display.setBrightness(bright);
    dispPrefs.end();
  } else {
    display.setJoystickScreenEnabled(true);
  }
  webServer.setDisplay(&display);
  display.setWiFiConfigManager(&wifiConfig);

  // Finalizar ecrã de arranque -> vai para o Home Menu
  display.endBootScreen(1200);
  display.setPage(BASE_PAGE_HOME);

  // Botão BOOT / Clique Joystick (GPIO 9) gerido com debounce curto/longo em
  // BaseJoystick
  pinMode(PIN_BTN_BOOT, INPUT_PULLUP);
  flagPageChange = false;
}

void transmitControlOrCommand() {
  // Prioridade 1: Comandos de Ação pendentes (da Cloud, Web ou Menu LCD, ex:
  // DROP_ANCHOR)
  if (fleet.hasPendingCommand()) {
    QueuedCommand qCmd = fleet.popPendingCommand();
    uint8_t targetId = (qCmd.target_rover_id > 0) ? qCmd.target_rover_id
                                                  : fleet.getSelectedRoverId();
    if (targetId == 0)
      targetId = 1;
    // Encaminhar para o Coprocessador C6 via UART:
    // CMD:ACTION:<target_id>:<action>:<cmd_id>:<depth>:<dur>:<lat7>:<lng7>
    char cmdBuf[128];
    snprintf(cmdBuf, sizeof(cmdBuf), "CMD:ACTION:%u:%u:%u:%u:%u:%ld:%ld\n",
             targetId, qCmd.action, qCmd.command_id, qCmd.depth_cm,
             qCmd.duration_s, (long)qCmd.target_lat, (long)qCmd.target_lng);
    interSerial.print(cmdBuf);
    interSerial.flush();
    printLog("[BASE LAFVIN] >>> Disparado Comando de Acao #%u (%s) -> Rover "
             "#%u via UART C6\n",
             qCmd.command_id, get_action_str(qCmd.action), targetId);
    return;
  }

  // Prioridade 2: Transmissão de controlo manual ao rover selecionado
  // Apenas conduz o rover por joystick quando no ecrã de Comando Direto
  // (BASE_PAGE_ROVER_CONTROL)
  uint8_t targetRoverId = fleet.getSelectedRoverId();
  if (targetRoverId == 0)
    targetRoverId = 1;

  bool inManualControlPage =
      (display.getCurrentPage() == BASE_PAGE_ROVER_CONTROL &&
       !display.isControlExitPromptActive());
  int8_t joyThr = inManualControlPage ? joystick.getThrottle() : 0;
  int8_t joyRud = inManualControlPage ? joystick.getRudder() : 0;
  int8_t joyJog = inManualControlPage ? joystick.getAnchorJog() : 0;
  bool joyDeflected = (joyThr != 0 || joyRud != 0 || joyJog != 0);

  static uint8_t joyNeutralStopCount = 0;
  int8_t thr = 0;
  int8_t rud = 0;
  int8_t jog = 0;

  if (joyDeflected) {
    webServer.clearWebControl();
    joyNeutralStopCount =
        3; // Garante envio de 3 pacotes neutros ao largar o joystick físico
    thr = joyThr;
    rud = joyRud;
    jog = joyJog;
  } else if (joyNeutralStopCount > 0) {
    joyNeutralStopCount--;
    thr = 0;
    rud = 0;
    jog = 0;
  } else if (webServer.consumeWebControlBurst()) {
    thr = webServer.getWebThrottle();
    rud = webServer.getWebRudder();
    jog = webServer.getWebAnchorJog();
  } else {
    // Em repouso: não inunda o canal LoRa nem sobrescreve o ângulo de
    // leme/estados do Rover
    return;
  }

  // Encaminhar para o Coprocessador C6 via UART:
  // CMD:CTRL:<target_id>:<thr>:<rud>:<jog>:<mode>
  char ctrlBuf[64];
  snprintf(ctrlBuf, sizeof(ctrlBuf), "CMD:CTRL:%u:%d:%d:%d:%d\n", targetRoverId,
           thr, rud, jog, currentNavMode);
  interSerial.print(ctrlBuf);
}

void handleUartFromC6() {
  uint32_t now = millis();
  while (interSerial.available()) {
    char c = (char)interSerial.read();
    static char rxBuf[256];
    static size_t rxIdx = 0;

    if (c == '\n' || c == '\r') {
      if (rxIdx > 0) {
        rxBuf[rxIdx] = '\0';
        lastUartRxTime = now;

        if (strncmp(rxBuf, "TELEM:", 6) == 0) {
          // Formato:
          // TELEM:<id>:<voltMv>:<pct>:<lat7>:<lng7>:<spdCmS>:<head10>:<sats>:<fix>:<motSt>:<ancSt>:<depthCm>:<cmdId>:<rssi>:<snr>[:<roll10>:<pitch10>:<cmpFlags>]
          uint32_t roverId = 0, batMv = 0, batPct = 0, spdCmS = 0, head10 = 0;
          uint32_t sats = 0, fix = 0, motSt = 0, ancSt = 0, depthCm = 0,
                   cmdId = 0, cmpFlags = 0;
          long lat7 = 0, lng7 = 0;
          float rssi = 0.0f, snr = 0.0f;
          int roll10 = 0, pitch10 = 0;

          int parsed =
              sscanf(rxBuf + 6,
                     "%u:%u:%u:%ld:%ld:%u:%u:%u:%u:%u:%u:%u:%u:%f:%f:%d:%d:%u",
                     &roverId, &batMv, &batPct, &lat7, &lng7, &spdCmS, &head10,
                     &sats, &fix, &motSt, &ancSt, &depthCm, &cmdId, &rssi, &snr,
                     &roll10, &pitch10, &cmpFlags);
          if (parsed >= 12 && roverId > 0) {
            PacketTelemetry pkt;
            memset(&pkt, 0, sizeof(pkt));
            pkt.magic = LORA_MAGIC_BYTE;
            pkt.msg_type = MSG_TELEMETRY;
            pkt.rover_id = (uint8_t)roverId;
            pkt.battery_mv = (uint16_t)batMv;
            pkt.battery_pct = (uint8_t)batPct;
            pkt.lat_deg7 = lat7;
            pkt.lng_deg7 = lng7;
            pkt.speed_cm_s = (uint16_t)spdCmS;
            pkt.heading_deg10 = (uint16_t)head10;
            pkt.satellites = (uint8_t)sats;
            pkt.fix_quality = (uint8_t)fix;
            pkt.motor_status = (uint8_t)motSt;
            pkt.anchor_status = (uint8_t)ancSt;
            pkt.anchor_depth_cm = (uint16_t)depthCm;
            pkt.last_cmd_id = (uint16_t)cmdId;
            pkt.roll_deg10 = (int16_t)roll10;
            pkt.pitch_deg10 = (int16_t)pitch10;
            pkt.compass_flags = (uint8_t)cmpFlags;

            loraReady = true;
            fleet.updateTelemetry(pkt, (int16_t)rssi);
            printLog(
                "[BASE LAFVIN] <<< Telemetria Rover #%u (Bat: %.2fV %u%% "
                "| Proa: %.1f° [%s] Roll:%.1f° Pitch:%.1f° | RSSI: %.1f dBm)\n",
                roverId, batMv / 1000.0f, batPct, head10 / 10.0f,
                unpack_compass_active((uint8_t)cmpFlags) ? "BNO055" : "GPS",
                roll10 / 10.0f, pitch10 / 10.0f, rssi);
          }
        } else if (strncmp(rxBuf, "PONG:OTA:", 9) == 0) {
          printLog("[BASE LAFVIN] ★ C6 OTA: %s\n", rxBuf + 9);
          webServer.setC6OtaStatus(rxBuf + 9);
        } else if (strncmp(rxBuf, "PONG:VER:", 9) == 0) {
          bool firstHandshake = (webServer.getC6Version().isEmpty() ||
                                 webServer.getC6Version() == "Desconhecido" ||
                                 webServer.getC6Version() != (rxBuf + 9));
          if (firstHandshake) {
            printLog("[BASE LAFVIN] ★ Versao Coprocessador C6: %s\n",
                     rxBuf + 9);
            webServer.setC6Version(rxBuf + 9);
            display.setC6Version(rxBuf + 9);
            String ledCfgCmd = fleet.buildC6LedCfgCommand();
            interSerial.println(ledCfgCmd);
            interSerial.flush();
          }
        } else if (strncmp(rxBuf, "LORA_STAT:", 10) == 0) {
          char st[16] = "OK";
          uint32_t txP = 0, rxT = 0, ago = 999999;
          float rssi = 0.0f, snr = 0.0f;
          int isPoll = 1;
          if (sscanf(rxBuf + 10, "%15[^:]:%u:%u:%lu:%f:%f:%d", st, &txP, &rxT,
                     &ago, &rssi, &snr, &isPoll) >= 5) {
            baseLoraDiag.hwReady = (strcmp(st, "OK") == 0);
            baseLoraDiag.txPolls = txP;
            baseLoraDiag.rxTelems = rxT;
            baseLoraDiag.lastRxSecAgo = ago;
            baseLoraDiag.rssi = rssi;
            baseLoraDiag.snr = snr;
            baseLoraDiag.isPolling = (isPoll != 0);
            loraReady = baseLoraDiag.hwReady;
          }
        } else if (strncmp(rxBuf, "PONG:", 5) == 0) {
          loraReady = true;
          webConsole.logf("[C6] %s", rxBuf);
        } else {
          webConsole.logf("[C6] %s", rxBuf);
        }
        rxIdx = 0;
      }
    } else {
      if (rxIdx < sizeof(rxBuf) - 1) {
        rxBuf[rxIdx++] = c;
      }
    }
  }
}

void handleInputsAndMenu(uint32_t now, bool &forceDisplay) {
  (void)now;
  joystick.update();
  static bool lastModeRaw = false;
  bool curModeRaw = (digitalRead(PIN_BTN_MODE) == LOW);
  bool btnModeEdge = (curModeRaw && !lastModeRaw);
  lastModeRaw = curModeRaw;

  BaseScreenPage page = display.getCurrentPage();

  // Páginas interativas de Joystick (Teste de Joystick e Comando Manual
  // Direto): Para sair: manter BOOT premido durante 3 segundos -> abre modal
  // "SAIR | CONTINUAR"
  if (page == BASE_PAGE_CONFIG_JOY || page == BASE_PAGE_ROVER_CONTROL) {
    BaseScreenPage parentPage = (page == BASE_PAGE_CONFIG_JOY)
                                    ? BASE_PAGE_CONFIG_MENU
                                    : BASE_PAGE_ROVER_MENU;
    if (!display.isControlExitPromptActive()) {
      // Atualizar barra visual de hold BOOT (3 segundos)
      display.setBootHoldProgressPct(joystick.getBootHoldProgressPct(3000));
      // Consumir cliques curtos/longos normais para não sair acidentalmente
      joystick.wasShortClicked();
      joystick.wasLongClicked();

      if (joystick.wasBootHeld3s()) {
        display.setBootHoldProgressPct(0);
        display.setControlExitPromptActive(true);
        joystick.disarmNavX();
        forceDisplay = true;
        printLog("[UI] Hold BOOT 3s -> Mostrar opcao SAIR | CONTINUAR\n");
      }
    } else {
      // Prompt "SAIR | CONTINUAR" ativo:
      // Y ou X alterna entre SAIR (0) e CONTINUAR (1); Clique BOOT confirma
      int8_t navY = joystick.getNavY();
      int8_t navX = joystick.getNavX();
      bool bootClick = joystick.wasShortClicked() || joystick.wasLongClicked();
      joystick.wasBootHeld3s();

      if (bootClick) {
        if (display.getControlExitCursorIdx() == 0) {
          // SAIR selecionado -> Volta para o menu pai
          display.setControlExitPromptActive(false);
          joystick.disarmNav();
          display.setPage(parentPage);
          forceDisplay = true;
          printLog("[UI] Confirmou SAIR -> Menu Pai\n");
        } else {
          // CONTINUAR selecionado -> Fecha o popup e permanece na página
          display.setControlExitPromptActive(false);
          joystick.disarmNav();
          forceDisplay = true;
          printLog("[UI] Confirmou CONTINUAR\n");
        }
      } else if (navY != 0 || navX != 0) {
        display.setControlExitCursorIdx(
            (display.getControlExitCursorIdx() + 1) % 2);
        forceDisplay = true;
      }
    }
    return;
  }

  // Ecrã de inserção de texto com teclado QWERTY (Password WiFi ou Nome da
  // Estação Base):
  if (page == BASE_PAGE_CONFIG_WIFI_PASS || page == BASE_PAGE_CONFIG_NAME) {
    bool isNamePage = (page == BASE_PAGE_CONFIG_NAME);
    BaseScreenPage backPage =
        isNamePage ? BASE_PAGE_CONFIG_MENU : BASE_PAGE_CONFIG_WIFI_SCAN;
    int8_t dx = joystick.getNavXRepeat();
    int8_t dy = joystick.getNavY();
    bool keyClick = joystick.wasShortClicked();
    bool longCancel = joystick.wasLongClicked();
    joystick.wasBootHeld3s();

    if (dx != 0 || dy != 0) {
      display.moveWifiKbCursor(dx, dy);
      forceDisplay = true;
    }
    if (btnModeEdge) {
      display.backspaceWifiPass();
      forceDisplay = true;
    } else if (longCancel) {
      display.setPage(backPage);
      joystick.disarmNavX();
      forceDisplay = true;
      printLog("[UI] Teclado QWERTY cancelado -> Voltar\n");
    } else if (keyClick) {
      uint8_t act = display.pressWifiKbKey();
      forceDisplay = true;
      if (act == 1) {
        // SAIR
        display.setPage(backPage);
        joystick.disarmNavX();
        printLog("[UI] Teclado QWERTY: SAIR -> Voltar\n");
      } else if (act == 2) {
        if (isNamePage) {
          // GUARDAR Nome da Estação Base
          String newName = display.getWifiPassBuf();
          newName.trim();
          joystick.disarmNavX();
          if (newName.length() > 0) {
            fleet.setStationName(newName.c_str(), true);
            display.showFeedback("NOME DA BASE GUARDADO!", 2000);
            printLog("[UI] Nome da Estacao Base alterado para: %s\n",
                     fleet.getStationName());
          } else {
            display.showFeedback("NOME INVALIDO (VAZIO)", 2000);
          }
          display.setPage(BASE_PAGE_CONFIG_MENU);
          forceDisplay = true;
        } else {
          // LIGAR: guardar credenciais e conectar
          String targetSsid = display.getWifiTargetSsid();
          String targetPass = display.getWifiPassBuf();
          printLog("[UI] A guardar e ligar a rede WiFi: %s\n",
                   targetSsid.c_str());
          display.showWifiConnectingScreen(targetSsid.c_str());
          wifiConfig.addOrUpdateNetwork(targetSsid, targetPass, true);
          bool ok = wifiConfig.connectTo(targetSsid, 8000);
          joystick.disarmNavX();
          if (ok) {
            display.showFeedback("WIFI GUARDADO E LIGADO!", 2000);
            printLog("[UI] Conectado a nova rede WiFi: %s (%s)\n",
                     targetSsid.c_str(), wifiConfig.getIPAddress().c_str());
          } else {
            wifiConfig.startAccessPoint("WindDragons-Base", "12345678");
            display.showFeedback("FALHA! MODO AP ATIVO", 2000);
            printLog("[UI] Falha ao ligar a %s -> Fallback Modo AP\n",
                     targetSsid.c_str());
          }
          display.setPage(BASE_PAGE_CONFIG_WIFI);
          forceDisplay = true;
        }
      }
    }
    return;
  }

  // Navegação padrão por Joystick:
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
  case BASE_PAGE_HOME:
    if (navY != 0) {
      display.setHomeMenuIdx((display.getHomeMenuIdx() + 2 + navY) % 2);
      forceDisplay = true;
    }
    if (navX == 1 || clickEnter) {
      if (display.getHomeMenuIdx() == 0) {
        display.setPage(BASE_PAGE_MISSION_MENU);
        printLog("[UI] Home -> Missao\n");
      } else {
        display.setPage(BASE_PAGE_CONFIG_MENU);
        printLog("[UI] Home -> Configuracoes\n");
      }
      forceDisplay = true;
    }
    break;

  case BASE_PAGE_MISSION_MENU:
    if (navY != 0) {
      display.setMissionMenuIdx((display.getMissionMenuIdx() + 3 + navY) % 3);
      forceDisplay = true;
    }
    if (navX == -1 || clickBack) {
      display.setPage(BASE_PAGE_HOME);
      forceDisplay = true;
      printLog("[UI] Missao -> Home\n");
    } else if (navX == 1 || clickEnter) {
      if (display.getMissionMenuIdx() == 0) {
        display.setStateCursorIdx((uint8_t)fleet.getOperationState());
        display.setPage(BASE_PAGE_MISSION_STATE);
        printLog("[UI] Missao -> Estado\n");
      } else if (display.getMissionMenuIdx() == 1) {
        const auto &rovers = fleet.getRovers();
        uint8_t selId = fleet.getSelectedRoverId();
        for (size_t i = 0; i < rovers.size(); ++i) {
          if (rovers[i].id == selId) {
            display.setRoverListIdx((uint8_t)i, rovers.size());
            break;
          }
        }
        display.setPage(BASE_PAGE_ROVER_LIST);
        printLog("[UI] Missao -> Lista de Rovers\n");
      } else {
        joystick.disarmNav();
        joystick.wasShortClicked();
        joystick.wasLongClicked();
        display.setRtlConfirmIdx(0);
        display.setPage(BASE_PAGE_MISSION_SET_ALL_RTL);
        printLog("[UI] Missao -> Confirmar SET ALL RTL\n");
      }
      forceDisplay = true;
    }
    break;

  case BASE_PAGE_MISSION_SET_ALL_RTL:
    if (bootShort || bootLong) {
      if (display.getRtlConfirmIdx() == 0) {
        fleet.queueCommand(BROADCAST_ROVER_ID, ++localCmdId, ACTION_SET_HOME);
        for (const auto &rv : fleet.getRovers()) {
          fleet.queueCommand(rv.id, ++localCmdId, ACTION_SET_HOME);
        }
        display.showFeedback("SET ALL RTL ENVIADO!", 1500);
        printLog("[UI] ★ Missao -> SET ALL RTL confirmado e enviado (%u rovers + broadcast)\n",
                 (unsigned)fleet.getRovers().size());
      } else {
        printLog("[UI] SET ALL RTL: Cancelar -> Menu Missao\n");
      }
      joystick.disarmNav();
      display.setPage(BASE_PAGE_MISSION_MENU);
      forceDisplay = true;
    } else if (navY != 0 || navX != 0) {
      display.setRtlConfirmIdx((display.getRtlConfirmIdx() + 1) % 2);
      forceDisplay = true;
    } else if (btnModeEdge) {
      joystick.disarmNav();
      display.setPage(BASE_PAGE_MISSION_MENU);
      forceDisplay = true;
      printLog("[UI] SET ALL RTL cancelado -> Menu Missao\n");
    }
    break;

  case BASE_PAGE_MISSION_STATE:
    if (navY != 0) {
      display.setStateCursorIdx((display.getStateCursorIdx() + 4 + navY) % 4);
      forceDisplay = true;
    }
    if (navX == -1 || clickBack) {
      display.setPage(BASE_PAGE_MISSION_MENU);
      forceDisplay = true;
      printLog("[UI] Estado -> Missao\n");
    } else if (navX == 1 || clickEnter) {
      BaseOperationState newSt =
          static_cast<BaseOperationState>(display.getStateCursorIdx());
      fleet.setOperationState(newSt);
      char fb[40];
      snprintf(fb, sizeof(fb), "ESTADO: %s", fleet.getOperationStateStr());
      display.showFeedback(fb, 1500);
      forceDisplay = true;
      printLog("[UI] Estado da Missao alterado para: %s\n",
               fleet.getOperationStateStr());
    }
    break;

  case BASE_PAGE_ROVER_LIST: {
    const auto &rovers = fleet.getRovers();
    size_t total = rovers.size();
    if (navY != 0 && total > 0) {
      uint8_t nextIdx = (display.getRoverListIdx() + total + navY) % total;
      display.setRoverListIdx(nextIdx, total);
      fleet.selectRoverById(rovers[nextIdx].id);
      forceDisplay = true;
    }
    if (navX == -1 || clickBack) {
      display.setPage(BASE_PAGE_MISSION_MENU);
      forceDisplay = true;
      printLog("[UI] Lista de Rovers -> Missao\n");
    } else if ((navX == 1 || clickEnter) && total > 0) {
      uint8_t idx = display.getRoverListIdx();
      if (idx < total) {
        fleet.selectRoverById(rovers[idx].id);
      }
      display.setRoverMenuIdx(0);
      display.setPage(BASE_PAGE_ROVER_MENU);
      forceDisplay = true;
      printLog("[UI] Lista de Rovers -> Rover #%u\n",
               fleet.getSelectedRoverId());
    }
    break;
  }

  case BASE_PAGE_ROVER_MENU:
    if (navY != 0) {
      display.setRoverMenuIdx((display.getRoverMenuIdx() + 4 + navY) % 4);
      forceDisplay = true;
    }
    if (navX == -1 || clickBack) {
      display.setPage(BASE_PAGE_ROVER_LIST);
      forceDisplay = true;
      printLog("[UI] Rover Menu -> Lista de Rovers\n");
    } else if (navX == 1 || clickEnter) {
      if (display.getRoverMenuIdx() == 0) {
        display.setRoverModeIdx(currentNavMode < 3 ? currentNavMode : 0);
        display.setPage(BASE_PAGE_ROVER_MODE);
        printLog("[UI] Rover Menu -> Modo\n");
      } else if (display.getRoverMenuIdx() == 1) {
        joystick.wasBootHeld3s();
        display.setControlExitPromptActive(false);
        display.setBootHoldProgressPct(0);
        display.setPage(BASE_PAGE_ROVER_CONTROL);
        printLog("[UI] Rover Menu -> Comando\n");
      } else if (display.getRoverMenuIdx() == 2) {
        joystick.disarmNav();
        joystick.wasShortClicked();
        joystick.wasLongClicked();
        display.setRtlConfirmIdx(0);
        display.setPage(BASE_PAGE_ROVER_SET_RTL);
        printLog("[UI] Rover Menu -> Confirmar SET RTL\n");
      } else {
        joystick.disarmNav();
        joystick.wasShortClicked();
        joystick.wasLongClicked();
        display.setRoverRemoveIdx(0);
        display.setPage(BASE_PAGE_ROVER_REMOVE);
        printLog("[UI] Rover Menu -> Confirmar Remocao\n");
      }
      forceDisplay = true;
    }
    break;

  case BASE_PAGE_ROVER_SET_RTL:
    if (bootShort || bootLong) {
      if (display.getRtlConfirmIdx() == 0) {
        uint8_t rId = fleet.getSelectedRoverId();
        if (rId == 0 && !fleet.getRovers().empty()) {
          uint8_t idx = display.getRoverListIdx();
          rId = (idx < fleet.getRovers().size()) ? fleet.getRovers()[idx].id
                                                 : fleet.getRovers()[0].id;
        }
        if (rId != 0) {
          fleet.queueCommand(rId, ++localCmdId, ACTION_SET_HOME);
          display.showFeedback("SET RTL ENVIADO!", 1500);
          printLog("[UI] ★ Rover Menu -> SET RTL confirmado e enviado ao Rover "
                   "#%u\n",
                   rId);
        }
      } else {
        printLog("[UI] SET RTL Rover: Cancelar -> Rover Menu\n");
      }
      joystick.disarmNav();
      display.setPage(BASE_PAGE_ROVER_MENU);
      forceDisplay = true;
    } else if (navY != 0 || navX != 0) {
      display.setRtlConfirmIdx((display.getRtlConfirmIdx() + 1) % 2);
      forceDisplay = true;
    } else if (btnModeEdge) {
      joystick.disarmNav();
      display.setPage(BASE_PAGE_ROVER_MENU);
      forceDisplay = true;
      printLog("[UI] SET RTL Rover cancelado -> Rover Menu\n");
    }
    break;

  case BASE_PAGE_ROVER_REMOVE:
    if (bootShort || bootLong) {
      if (display.getRoverRemoveIdx() == 0) {
        uint8_t rId = fleet.getSelectedRoverId();
        if (rId == 0 && !fleet.getRovers().empty()) {
          uint8_t idx = display.getRoverListIdx();
          rId = (idx < fleet.getRovers().size()) ? fleet.getRovers()[idx].id
                                                 : fleet.getRovers()[0].id;
        }
        if (rId != 0 && fleet.removeRoverById(rId, true)) {
          size_t remTotal = fleet.getRovers().size();
          uint8_t nextIdx = display.getRoverListIdx();
          if (nextIdx >= remTotal && remTotal > 0) {
            nextIdx = (uint8_t)(remTotal - 1);
          }
          display.setRoverListIdx(nextIdx, remTotal);
          if (remTotal > 0) {
            fleet.selectRoverById(fleet.getRovers()[nextIdx].id);
          }
          display.showFeedback("ROVER REMOVIDO!", 2000);
          printLog("[UI] Rover #%u removido da frota -> Lista de Rovers\n",
                   rId);
        }
        joystick.disarmNav();
        display.setPage(BASE_PAGE_ROVER_LIST);
      } else {
        joystick.disarmNav();
        display.setPage(BASE_PAGE_ROVER_MENU);
        printLog("[UI] Remover Rover: Cancelar -> Rover Menu\n");
      }
      forceDisplay = true;
    } else if (navY != 0 || navX != 0) {
      display.setRoverRemoveIdx((display.getRoverRemoveIdx() + 1) % 2);
      forceDisplay = true;
    } else if (btnModeEdge) {
      joystick.disarmNav();
      display.setPage(BASE_PAGE_ROVER_MENU);
      forceDisplay = true;
      printLog("[UI] Remover Rover cancelado -> Rover Menu\n");
    }
    break;

  case BASE_PAGE_ROVER_MODE:
    if (navY != 0) {
      display.setRoverModeIdx((display.getRoverModeIdx() + 3 + navY) % 3);
      forceDisplay = true;
    }
    if (navX == -1 || clickBack) {
      display.setPage(BASE_PAGE_ROVER_MENU);
      forceDisplay = true;
      printLog("[UI] Modo -> Rover Menu\n");
    } else if (navX == 1 || clickEnter) {
      uint8_t rId = fleet.getSelectedRoverId();
      uint8_t mIdx = display.getRoverModeIdx();
      if (mIdx == 0) {
        currentNavMode = 0;
        fleet.queueCommand(rId, ++localCmdId, ACTION_MANUAL);
        display.showFeedback("MODO MANUAL ENVIADO!", 1500);
        printLog("[UI] Modo #0 enviado ao Rover #%u\n", rId);
      } else if (mIdx == 1) {
        currentNavMode = 1;
        fleet.queueCommand(rId, ++localCmdId, ACTION_HOLD_STATION);
        display.showFeedback("MODO HOLD ENVIADO!", 1500);
        printLog("[UI] Modo #1 enviado ao Rover #%u\n", rId);
      } else {
        joystick.disarmNav();
        joystick.wasShortClicked();
        joystick.wasLongClicked();
        display.setRtlConfirmIdx(0);
        display.setPage(BASE_PAGE_ROVER_RTL_CONFIRM);
        printLog("[UI] Modo -> Confirmar RETURN HOME (RTL) p/ Rover #%u\n",
                 rId);
      }
      forceDisplay = true;
    }
    break;

  case BASE_PAGE_ROVER_RTL_CONFIRM:
    if (bootShort || bootLong) {
      if (display.getRtlConfirmIdx() == 0) {
        uint8_t rId = fleet.getSelectedRoverId();
        currentNavMode = 2;
        fleet.queueCommand(rId, ++localCmdId, ACTION_RTL);
        display.showFeedback("RETURN HOME ENVIADO!", 1500);
        printLog(
            "[UI] ★ Modo RETURN HOME (RTL) confirmado e enviado ao Rover #%u\n",
            rId);
      } else {
        printLog("[UI] RETURN HOME (RTL): Cancelar -> Modo\n");
      }
      joystick.disarmNav();
      display.setPage(BASE_PAGE_ROVER_MODE);
      forceDisplay = true;
    } else if (navY != 0 || navX != 0) {
      display.setRtlConfirmIdx((display.getRtlConfirmIdx() + 1) % 2);
      forceDisplay = true;
    } else if (btnModeEdge) {
      joystick.disarmNav();
      display.setPage(BASE_PAGE_ROVER_MODE);
      forceDisplay = true;
      printLog("[UI] RETURN HOME (RTL) cancelado -> Modo\n");
    }
    break;

  case BASE_PAGE_CONFIG_MENU:
    if (navY != 0) {
      display.setConfigMenuIdx((display.getConfigMenuIdx() + 6 + navY) % 6);
      forceDisplay = true;
    }
    if (navX == -1 || clickBack) {
      display.setPage(BASE_PAGE_HOME);
      forceDisplay = true;
      printLog("[UI] Configuracoes -> Home\n");
    } else if (navX == 1 || clickEnter) {
      uint8_t cIdx = display.getConfigMenuIdx();
      if (cIdx == 0) {
        display.setPage(BASE_PAGE_CONFIG_WIFI);
        printLog("[UI] Configuracoes -> WiFi\n");
      } else if (cIdx == 1) {
        joystick.wasBootHeld3s();
        display.setControlExitPromptActive(false);
        display.setBootHoldProgressPct(0);
        display.setPage(BASE_PAGE_CONFIG_JOY);
        printLog("[UI] Configuracoes -> Joystick\n");
      } else if (cIdx == 2) {
        display.setPage(BASE_PAGE_CONFIG_ID);
        printLog("[UI] Configuracoes -> ID\n");
      } else if (cIdx == 3) {
        display.beginStationNameEntry(fleet.getStationName());
        joystick.disarmNavX();
        display.setPage(BASE_PAGE_CONFIG_NAME);
        printLog("[UI] Configuracoes -> Alterar Nome (%s)\n",
                 fleet.getStationName());
      } else if (cIdx == 4) {
        display.setPage(BASE_PAGE_CONFIG_FW);
        printLog("[UI] Configuracoes -> Firmware\n");
      } else {
        display.setPage(BASE_PAGE_CONFIG_PORTAL);
        printLog("[UI] Configuracoes -> Portal\n");
      }
      forceDisplay = true;
    }
    break;

  case BASE_PAGE_CONFIG_WIFI:
    if (navX == -1 || clickBack) {
      display.setPage(BASE_PAGE_CONFIG_MENU);
      forceDisplay = true;
      printLog("[UI] WiFi -> Configuracoes\n");
    } else if (navX == 1 || clickEnter) {
      const auto &nets = wifiConfig.getNetworks();
      size_t totalItems = 1 + nets.size(); // 0 = Scan, 1..N = Redes Guardadas
      display.setWifiListIdx(0, totalItems);
      display.setPage(BASE_PAGE_CONFIG_WIFI_LIST);
      forceDisplay = true;
      printLog("[UI] WiFi -> Lista (Scan + %u redes guardadas)\n",
               (unsigned)nets.size());
    }
    break;

  case BASE_PAGE_CONFIG_WIFI_LIST: {
    const auto &nets = wifiConfig.getNetworks();
    size_t total = 1 + nets.size(); // 0 = Scan, 1..N = Redes Guardadas
    if (navY != 0 && total > 0) {
      uint8_t nextIdx = (display.getWifiListIdx() + total + navY) % total;
      display.setWifiListIdx(nextIdx, total);
      forceDisplay = true;
    }
    if (navX == -1 || clickBack) {
      display.setPage(BASE_PAGE_CONFIG_WIFI);
      forceDisplay = true;
      printLog("[UI] Lista -> WiFi\n");
    } else if (navX == 1 || clickEnter) {
      uint8_t idx = display.getWifiListIdx();
      if (idx == 0) {
        // Item 0: "Scan" -> Varrimento de redes WiFi disponíveis
        printLog("[UI] A iniciar varrimento de redes WiFi (Scan)...\n");
        display.showWifiScanningScreen();
        const auto &scanned = wifiConfig.scanNetworks();
        joystick.disarmNavX();
        display.setWifiScanIdx(0, scanned.size());
        display.setPage(BASE_PAGE_CONFIG_WIFI_SCAN);
        forceDisplay = true;
        printLog("[UI] Scan concluido: %u redes encontradas\n",
                 (unsigned)scanned.size());
      } else if ((idx - 1) < nets.size()) {
        String targetSsid = nets[idx - 1].ssid;
        printLog("[UI] A ligar a rede WiFi guardada: %s\n", targetSsid.c_str());
        display.showWifiConnectingScreen(targetSsid.c_str());
        wifiConfig.setDefault(targetSsid);
        bool ok = wifiConfig.connectTo(targetSsid, 8000);
        joystick.disarmNavX();
        if (ok) {
          display.showFeedback("WIFI CONECTADO!", 2000);
          printLog("[UI] WiFi conectado com sucesso a: %s (%s)\n",
                   targetSsid.c_str(), wifiConfig.getIPAddress().c_str());
        } else {
          wifiConfig.startAccessPoint("WindDragons-Base", "12345678");
          display.showFeedback("FALHA! MODO AP ATIVO", 2000);
          printLog("[UI] Falha ao ligar a %s -> Fallback Modo AP\n",
                   targetSsid.c_str());
        }
        display.setPage(BASE_PAGE_CONFIG_WIFI);
        forceDisplay = true;
      }
    }
    break;
  }

  case BASE_PAGE_CONFIG_WIFI_SCAN: {
    const auto &scanned = wifiConfig.getScannedNetworks();
    size_t total = scanned.size();
    if (navY != 0 && total > 0) {
      uint8_t nextIdx = (display.getWifiScanIdx() + total + navY) % total;
      display.setWifiScanIdx(nextIdx, total);
      forceDisplay = true;
    }
    if (navX == -1 || clickBack) {
      display.setPage(BASE_PAGE_CONFIG_WIFI_LIST);
      forceDisplay = true;
      printLog("[UI] Scan -> Lista\n");
    } else if ((navX == 1 || clickEnter) && total > 0) {
      uint8_t idx = display.getWifiScanIdx();
      if (idx < total) {
        const auto &sn = scanned[idx];
        if (wifiConfig.hasSavedNetwork(sn.ssid)) {
          // Rede conhecida -> Liga-se diretamente com as credenciais guardadas
          printLog(
              "[UI] Rede conhecida selecionada no Scan: %s -> A ligar...\n",
              sn.ssid.c_str());
          display.showWifiConnectingScreen(sn.ssid.c_str());
          wifiConfig.setDefault(sn.ssid);
          bool ok = wifiConfig.connectTo(sn.ssid, 8000);
          joystick.disarmNavX();
          if (ok) {
            display.showFeedback("WIFI CONECTADO!", 2000);
          } else {
            wifiConfig.startAccessPoint("WindDragons-Base", "12345678");
            display.showFeedback("FALHA! MODO AP ATIVO", 2000);
          }
          display.setPage(BASE_PAGE_CONFIG_WIFI);
          forceDisplay = true;
        } else if (sn.open) {
          // Rede aberta sem password -> Guardar e ligar diretamente
          printLog("[UI] Rede aberta selecionada no Scan: %s -> A ligar...\n",
                   sn.ssid.c_str());
          display.showWifiConnectingScreen(sn.ssid.c_str());
          wifiConfig.addOrUpdateNetwork(sn.ssid, "", true);
          bool ok = wifiConfig.connectTo(sn.ssid, 8000);
          joystick.disarmNavX();
          if (ok) {
            display.showFeedback("WIFI CONECTADO!", 2000);
          } else {
            wifiConfig.startAccessPoint("WindDragons-Base", "12345678");
            display.showFeedback("FALHA! MODO AP ATIVO", 2000);
          }
          display.setPage(BASE_PAGE_CONFIG_WIFI);
          forceDisplay = true;
        } else {
          // Rede desconhecida protegida -> Abrir ecrã QWERTY para inserir
          // password
          printLog(
              "[UI] Rede desconhecida selecionada: %s -> Inserir Password\n",
              sn.ssid.c_str());
          display.beginWifiPasswordEntry(sn.ssid.c_str());
          joystick.disarmNavX();
          display.setPage(BASE_PAGE_CONFIG_WIFI_PASS);
          forceDisplay = true;
        }
      }
    }
    break;
  }

  case BASE_PAGE_CONFIG_ID:
  case BASE_PAGE_CONFIG_FW:
  case BASE_PAGE_CONFIG_PORTAL:
    if (navX == -1 || clickBack || clickEnter) {
      display.setPage(BASE_PAGE_CONFIG_MENU);
      forceDisplay = true;
      printLog("[UI] Voltou para Configuracoes\n");
    }
    break;

  default:
    display.setPage(BASE_PAGE_HOME);
    forceDisplay = true;
    break;
  }
}

void loop() {
  uint32_t now = millis();
  bool forceDisplay = false;

  // 1. Processar entradas locais (Joystick, Clique Curto = Próx. Página,
  // Longo/MODE = Menu)
  handleInputsAndMenu(now, forceDisplay);

  // 3. Processar clientes HTTP Web
  webServer.handleClient();

  // 4. Receber telemetria e respostas do Coprocessador C6 via UART
  handleUartFromC6();

  // Consulta periódica da versão do Coprocessador C6 se ainda desconhecida
  static uint32_t lastVerCheck = 0;
  if (strcmp(queuedWebCommand, "NONE") == 0 &&
      strcmp(display.getC6Version(), "Desconhecido") == 0) {
    if (now - lastVerCheck >= 3000) {
      lastVerCheck = now;
      interSerial.println("CMD:GET_VER");
    }
  }

  // Ping periódico de heartbeat ao C6 a cada 200ms
  bool hasPendingCmd = (strcmp(queuedWebCommand, "NONE") != 0);
  if ((now - lastUartPingTime >= 200) ||
      (hasPendingCmd && (now - lastUartPingTime >= 50))) {
    lastUartPingTime = now;
    uartSeq++;

    if (hasPendingCmd) {
      interSerial.println(queuedWebCommand);
      interSerial.flush();
      printLog("[BASE LAFVIN] >>> COMANDO PENDENTE UART: %s\n",
               queuedWebCommand);
      strncpy(queuedWebCommand, "NONE", sizeof(queuedWebCommand) - 1);
    }

    char pingMsg[48];
    snprintf(pingMsg, sizeof(pingMsg), "PING:%u:%u\n", uartSeq,
             static_cast<unsigned>(fleet.getOperationState()));
    interSerial.print(pingMsg);
  }

  // 5. Transmitir comandos de controlo ao C6 via UART (10Hz)
  if (now - lastControlTxTime >= CONTROL_TX_INTERVAL_MS) {
    lastControlTxTime = now;
    transmitControlOrCommand();
  }

  // 6. Atualizar WiFi & Sincronização Cloud
  network.update();

  // 7. Atualizar estado online/offline dos rovers
  fleet.checkOnlineStatus();

  // 8. Atualizar ecrã LCD ST7789
  display.update(
      fleet, network.isConnected(), network.isSyncSuccess(),
      joystick.getThrottle(), joystick.getRudder(), currentNavMode,
      sdCard.isReady(), wifiConfig.isAPMode(),
      wifiConfig.getIPAddress().c_str(), wifiConfig.getSSID().c_str(),
      wifiConfig.getRSSI(), wifiConfig.getAPStationCount(),
      wifiConfig.getAPPassword().c_str(), forceDisplay,
      network.getLastHttpStatus(), network.getSuccessCount(),
      network.getFailCount(), network.getLastSyncTime(),
      webServer.getMacAddress().c_str(), joystick.getRawX(), joystick.getRawY(),
      joystick.getDeadband(), joystick.isBootPressed(),
      joystick.isModePressed(), joystick.isAnchorUpPressed(),
      joystick.isAnchorDownPressed(), joystick.getJoyClickCount());

  // 9. Atualizar LED NeoPixel conforme o estado operacional da Base e
  // configuração C6 LED
  static uint32_t lastLedSyncHash = 0xFFFFFFFF;
  BaseOperationState curOp = fleet.getOperationState();
  uint8_t brt = fleet.getC6LedBrightness();
  String curHex = fleet.getC6ModeColor(static_cast<uint8_t>(curOp));
  uint32_t hexNum = (curHex.length() >= 7)
                        ? (uint32_t)strtoul(curHex.c_str() + 1, nullptr, 16)
                        : 0;
  uint32_t curHash = (static_cast<uint32_t>(curOp) << 28) ^
                     (static_cast<uint32_t>(brt) << 20) ^ hexNum;
  if (curHash != lastLedSyncHash) {
    lastLedSyncHash = curHash;
    uint8_t r = 0, g = 0, b = 0;
    fleet.getScaledModeColorRgb(static_cast<uint8_t>(curOp), r, g, b);
    setRgbColor(r, g, b);
  }

  // 10. Diagnóstico em Tempo Real do Pinout do Joystick via USB Serial
  static uint32_t lastPinDiagMs = 0;
  if (now - lastPinDiagMs >= 500) {
    lastPinDiagMs = now;
    int adc0 = joystick.getRawY();
    int adc3 = joystick.getRawX();
    printLog("[JOY PINOUT] Pad6(IO0)=%4d | Pad3(IO3)=%4d | Thr=%+4d%% "
             "Rud=%+4d%% | BTN[IO9=%d IO18=%d IO19=%d IO20=%d]\n",
             adc0, adc3, joystick.getThrottle(), joystick.getRudder(),
             digitalRead(9), digitalRead(18), digitalRead(19), digitalRead(20));
  }
}
