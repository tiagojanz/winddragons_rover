#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_NeoPixel.h>
#include <Preferences.h>

#include "config_common.h"
#include "lora_protocol.h"
#include "fleet_manager.h"
#include "base_joystick.h"
#include "base_network.h"
#include "base_display.h"
#include "sd_card_manager.h"
#include "wifi_config_manager.h"
#include "base_web_server.h"
#include "base_version.h"
#include "web_console.h"

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
FleetManager      fleet;
BaseJoystick      joystick;
WiFiConfigManager wifiConfig;
BaseNetwork       network(fleet, wifiConfig);
BaseDisplay       display;
SDCardManager     sdCard;
BaseWebServer     webServer(wifiConfig, fleet, &sdCard, &network);
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

// Modo de navegação e menu UI
uint8_t currentNavMode = 0; // 0 = MANUAL, 1 = HOLD STATION
uint16_t localCmdId = 1000;

enum UiScreen {
    SCREEN_PAGES,
    SCREEN_MENU
};
UiScreen currentScreen = SCREEN_PAGES;

const char* const MENU_ITEMS[] = {
    "1. MODO MANUAL",
    "2. HOLD STATION",
    "3. LANCAR ANCORA",
    "4. RECOLHER ANCORA",
    "5. PARAR MOTORES",
    "6. ESTADO BASE",
    "7. TESTE JOYSTICK"
};
const uint8_t MENU_COUNT = sizeof(MENU_ITEMS) / sizeof(MENU_ITEMS[0]);
uint8_t menuSelectedIndex = 0;
uint32_t lastMenuActivityMs = 0;
const char* menuFeedback = nullptr;
uint32_t menuFeedbackUntilMs = 0;

// Botão BOOT e navegação de ecrãs
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
    rgbLed.setPixelColor(0, rgbLed.Color(r, g, b));
    rgbLed.show();
}

void printLog(const char* format, ...) {
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
    printLog("  Firmware: Master Lafvin %s (%s)\n", BASE_LAFVIN_VERSION, BASE_BUILD_DATE);
    printLog("=======================================================\n");

    // Desativar CS de periféricos SPI antes do arranque
    pinMode(PIN_LCD_CS, OUTPUT);
    digitalWrite(PIN_LCD_CS, HIGH);
    pinMode(PIN_SD_CS, OUTPUT);
    digitalWrite(PIN_SD_CS, HIGH);

    // 1. Inicializar Display ST7789
    display.begin();
    display.showBootScreen();
    display.bootLogf(UI_CYAN, 10, "[BOOT] WindDragons Base %s", BASE_LAFVIN_VERSION);

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
    display.bootLogf(UI_GREEN, 48, "[FROTA] %u rovers | %s", (uint32_t)fleet.getRovers().size(), fleet.getOperationStateBadge());

    // 4. Inicializar Joystick
    joystick.begin();
    printLog("[JOY ] Auto-calibracao concluida: Y(IO0)=%d [%s] | X(IO3)=%d [%s]\n",
             joystick.getCenterY(), joystick.isAxisYConnected() ? "CONECTADO" : "FLUTUANTE/ABERTO",
             joystick.getCenterX(), joystick.isAxisXConnected() ? "CONECTADO" : "FLUTUANTE/ABERTO");
    display.bootLogf(UI_LIGHTGREY, 55, "[JOY ] IO0:%d IO3:%d", joystick.getCenterY(), joystick.getCenterX());

    // 5. Configurar UART Inter-ESP (TX=GPIO 2, RX=GPIO 1) com o Coprocessador C6
    interSerial.setRxBufferSize(512);
    interSerial.begin(INTER_ESP_BAUDRATE, SERIAL_8N1, PIN_INTER_ESP_LAFVIN_RX, PIN_INTER_ESP_LAFVIN_TX);
    display.bootLogf(UI_GREEN, 65, "[UART] Inter-ESP C6 115200");
    printLog("[BASE] UART com Coprocessador C6 iniciada (TX=GPIO%d, RX=GPIO%d @ %d baud)\n",
             PIN_INTER_ESP_LAFVIN_TX, PIN_INTER_ESP_LAFVIN_RX, INTER_ESP_BAUDRATE);

    // Consulta imediata de versão ao Coprocessador C6
    interSerial.println("CMD:GET_VER");
    interSerial.flush();

    // 6. Configurar WiFi e Gestor de Redes
    display.bootLogf(UI_YELLOW, 75, "[NET ] A ligar WiFi (STA)...");
    wifiConfig.begin(&sdCard);
    bool wifiOk = wifiConfig.autoConnect(8000, [](const char* ssid) {
        display.bootLogf(UI_YELLOW, 80, "[NET ] A testar '%s'...", ssid);
    });

    if (!wifiOk) {
        printLog("[BASE] Sem rede STA -> A ativar Modo AP (WindDragons-Base)\n");
        wifiConfig.startAccessPoint("WindDragons-Base", "12345678");
        setRgbColor(30, 15, 0); // Laranja / Modo AP
        display.bootLogf(UI_YELLOW, 85, "[NET ] Falha STA -> Modo AP");
        display.bootLogf(UI_CYAN, 88, "[NET ] AP: WindDragons-Base");
        display.bootLogf(UI_WHITE, 90, "[NET ] IP: 192.168.4.1");
    } else {
        printLog("[BASE] WiFi Conectado! SSID: %s | IP: %s\n", wifiConfig.getSSID().c_str(), wifiConfig.getIPAddress().c_str());
        setRgbColor(0, 30, 0); // Verde / STA Conectado
        display.bootLogf(UI_GREEN, 90, "[NET ] IP: %s", wifiConfig.getIPAddress().c_str());
    }

    // 7. Configurar Callback de Comandos Web e Iniciar Servidor Web
    webServer.setLoraReadyPtr(&loraReady);
    webServer.setLoraDiagnosticsPtr(&baseLoraDiag);
    webServer.setCommandQueueCallback([](const char* cmd) {
        if (cmd && strlen(cmd) > 0) {
            if (strncmp(cmd, "CMD:OTA:", 8) == 0) {
                // Envio imediato repetido (x3) para garantir receção do comando de OTA pelo C6
                for (int i = 0; i < 3; i++) {
                    interSerial.println(cmd);
                    interSerial.flush();
                    delay(25);
                }
                printLog("[BASE LAFVIN] >>> DISPARO OTA DIRETO UART C6 (x3): %s\n", cmd);
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

    // Finalizar ecrã de arranque
    display.endBootScreen(1200);
    display.setPage(BASE_PAGE_JOYSTICK);

    // Botão BOOT / Clique Joystick (GPIO 9) gerido com debounce curto/longo em BaseJoystick
    pinMode(PIN_BTN_BOOT, INPUT_PULLUP);
    flagPageChange = false;
}

void transmitControlOrCommand() {
    // Prioridade 1: Comandos de Ação pendentes (da Cloud ou Web, ex: DROP_ANCHOR)
    if (fleet.hasPendingCommand()) {
        QueuedCommand qCmd = fleet.popPendingCommand();
        // Encaminhar para o Coprocessador C6 via UART:
        // CMD:ACTION:<target_id>:<action>:<cmd_id>:<depth>:<dur>:<lat7>:<lng7>
        char cmdBuf[128];
        snprintf(cmdBuf, sizeof(cmdBuf), "CMD:ACTION:%u:%u:%u:%u:%u:%ld:%ld\n",
                 qCmd.target_rover_id, qCmd.action, qCmd.command_id,
                 qCmd.depth_cm, qCmd.duration_s,
                 (long)qCmd.target_lat, (long)qCmd.target_lng);
        interSerial.print(cmdBuf);
        interSerial.flush();
        printLog("[BASE LAFVIN] >>> Disparado Comando de Acao #%u (%s) -> Rover #%u via UART C6\n",
                 qCmd.command_id, get_action_str(qCmd.action), qCmd.target_rover_id);
        return;
    }

    // Em modo STORAGE, não emite controlo contínuo de joystick para os rovers
    if (fleet.getOperationState() == BASE_OP_STORAGE && !webServer.isWebControlActive()) {
        return;
    }

    // Prioridade 2: Transmissão de controlo manual ao rover selecionado
    if (!fleet.hasSelectedRover()) {
        return;
    }

    int8_t thr = 0;
    int8_t rud = 0;
    int8_t jog = 0;

    // Em páginas de menu ou não-controlo, envia neutro
    if (currentScreen == SCREEN_MENU || display.getCurrentPage() != BASE_PAGE_ROVER) {
        thr = 0;
        rud = 0;
        jog = 0;
    } else if (webServer.isWebControlActive()) {
        thr = webServer.getWebThrottle();
        rud = webServer.getWebRudder();
        jog = webServer.getWebAnchorJog();
    } else {
        thr = joystick.getThrottle();
        rud = joystick.getRudder();
        jog = joystick.getAnchorJog();
    }

    // Encaminhar para o Coprocessador C6 via UART:
    // CMD:CTRL:<target_id>:<thr>:<rud>:<jog>:<mode>
    char ctrlBuf[64];
    snprintf(ctrlBuf, sizeof(ctrlBuf), "CMD:CTRL:%d:%d:%d:%d:%d\n",
             fleet.getSelectedRoverId(), thr, rud, jog, currentNavMode);
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
                    // Formato: TELEM:<id>:<voltMv>:<pct>:<lat7>:<lng7>:<spdCmS>:<head10>:<sats>:<fix>:<motSt>:<ancSt>:<depthCm>:<cmdId>:<rssi>:<snr>
                    uint32_t roverId = 0, batMv = 0, batPct = 0, spdCmS = 0, head10 = 0;
                    uint32_t sats = 0, fix = 0, motSt = 0, ancSt = 0, depthCm = 0, cmdId = 0;
                    long lat7 = 0, lng7 = 0;
                    float rssi = 0.0f, snr = 0.0f;

                    int parsed = sscanf(rxBuf + 6,
                                        "%u:%u:%u:%ld:%ld:%u:%u:%u:%u:%u:%u:%u:%u:%f:%f",
                                        &roverId, &batMv, &batPct,
                                        &lat7, &lng7, &spdCmS, &head10,
                                        &sats, &fix, &motSt, &ancSt,
                                        &depthCm, &cmdId, &rssi, &snr);
                    if (parsed >= 12 && roverId > 0) {
                        PacketTelemetry pkt;
                        memset(&pkt, 0, sizeof(pkt));
                        pkt.magic           = LORA_MAGIC_BYTE;
                        pkt.msg_type        = MSG_TELEMETRY;
                        pkt.rover_id        = (uint8_t)roverId;
                        pkt.battery_mv      = (uint16_t)batMv;
                        pkt.battery_pct     = (uint8_t)batPct;
                        pkt.lat_deg7        = lat7;
                        pkt.lng_deg7        = lng7;
                        pkt.speed_cm_s      = (uint16_t)spdCmS;
                        pkt.heading_deg10   = (uint16_t)head10;
                        pkt.satellites      = (uint8_t)sats;
                        pkt.fix_quality     = (uint8_t)fix;
                        pkt.motor_status    = (uint8_t)motSt;
                        pkt.anchor_status   = (uint8_t)ancSt;
                        pkt.anchor_depth_cm = (uint16_t)depthCm;
                        pkt.last_cmd_id     = (uint16_t)cmdId;

                        loraReady = true;
                        fleet.updateTelemetry(pkt, (int16_t)rssi);
                        printLog("[BASE LAFVIN] <<< Telemetria Rover #%u (Bat: %.2fV %u%% | RSSI: %.1f dBm)\n",
                                 roverId, batMv / 1000.0f, batPct, rssi);
                    }
                } else if (strncmp(rxBuf, "PONG:OTA:", 9) == 0) {
                    printLog("[BASE LAFVIN] ★ C6 OTA: %s\n", rxBuf + 9);
                    webServer.setC6OtaStatus(rxBuf + 9);
                } else if (strncmp(rxBuf, "PONG:VER:", 9) == 0) {
                    printLog("[BASE LAFVIN] ★ Versao Coprocessador C6: %s\n", rxBuf + 9);
                    webServer.setC6Version(rxBuf + 9);
                    display.setC6Version(rxBuf + 9);
                } else if (strncmp(rxBuf, "LORA_STAT:", 10) == 0) {
                    char st[16] = "OK";
                    uint32_t txP = 0, rxT = 0, ago = 999999;
                    float rssi = 0.0f, snr = 0.0f;
                    int isPoll = 1;
                    if (sscanf(rxBuf + 10, "%15[^:]:%u:%u:%lu:%f:%f:%d", st, &txP, &rxT, &ago, &rssi, &snr, &isPoll) >= 5) {
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
    joystick.update();
    static bool lastModeRaw = false;
    bool curModeRaw = (digitalRead(PIN_BTN_MODE) == LOW);
    bool btnModeEdge = (curModeRaw && !lastModeRaw);
    lastModeRaw = curModeRaw;

    if (currentScreen == SCREEN_PAGES) {
        BaseScreenPage page = display.getCurrentPage();

        // Clique curto no Joystick / BOOT (GPIO 9): avança para o próximo ecrã
        if (joystick.wasShortClicked()) {
            display.nextPage();
            forceDisplay = true;
            printLog("[BASE] Ecrã alterado para página %d\n", display.getCurrentPage());
            return;
        }

        // Pressão longa (>700ms) no Joystick / BOOT ou botão MODE (GPIO 19): abre Menu de Controlo
        if (joystick.wasLongClicked() || btnModeEdge) {
            if (page == BASE_PAGE_JOYSTICK && btnModeEdge) {
                display.setPage(BASE_PAGE_STATION);
                forceDisplay = true;
            } else {
                currentScreen = SCREEN_MENU;
                menuSelectedIndex = 0;
                lastMenuActivityMs = now;
                menuFeedback = nullptr;
                forceDisplay = true;
                printLog("[BASE] Abriu Menu de Controlo\n");
            }
        }
    } 
    else if (currentScreen == SCREEN_MENU) {
        int8_t step = joystick.getMenuNavStep();
        if (step != 0) {
            menuSelectedIndex = (menuSelectedIndex + step + MENU_COUNT) % MENU_COUNT;
            lastMenuActivityMs = now;
            forceDisplay = true;
        }

        if (joystick.wasLongClicked() || btnModeEdge) {
            currentScreen = SCREEN_PAGES;
            menuFeedback = nullptr;
            forceDisplay = true;
            return;
        }

        if (joystick.wasShortClicked()) {
            lastMenuActivityMs = now;
            forceDisplay = true;
            switch (menuSelectedIndex) {
                case 0: // MODO MANUAL
                    currentNavMode = 0;
                    menuFeedback = "MODO MANUAL OK";
                    break;
                case 1: // HOLD STATION (ANCORA VIRTUAL)
                    currentNavMode = 1;
                    menuFeedback = "HOLD STATION OK";
                    break;
                case 2: // LANCAR ANCORA
                    if (!fleet.hasSelectedRover()) {
                        menuFeedback = "SEM ROVER ATIVO";
                    } else {
                        fleet.queueCommand(fleet.getSelectedRoverId(), ++localCmdId, ACTION_DROP_ANCHOR, 500);
                        menuFeedback = "ANCORA A DESCER";
                    }
                    break;
                case 3: // RECOLHER ANCORA
                    if (!fleet.hasSelectedRover()) {
                        menuFeedback = "SEM ROVER ATIVO";
                    } else {
                        fleet.queueCommand(fleet.getSelectedRoverId(), ++localCmdId, ACTION_RETRIEVE_ANCHOR);
                        menuFeedback = "A RECOLHER...";
                    }
                    break;
                case 4: // PARAR MOTORES
                    if (!fleet.hasSelectedRover()) {
                        menuFeedback = "SEM ROVER ATIVO";
                    } else {
                        fleet.queueCommand(fleet.getSelectedRoverId(), ++localCmdId, ACTION_STOP);
                        currentNavMode = 0;
                        menuFeedback = "MOTORES PARADOS";
                    }
                    break;
                case 5: // ALTERNAR ESTADO BASE (STORAGE -> WAITING -> IDLE -> GO)
                    {
                        BaseOperationState cur = fleet.getOperationState();
                        BaseOperationState nxt = static_cast<BaseOperationState>((cur + 1) % 4);
                        fleet.setOperationState(nxt);
                        menuFeedback = fleet.getOperationStateStr();
                    }
                    break;
                case 6: // TESTE JOYSTICK
                    display.setJoystickScreenEnabled(true);
                    display.setPage(BASE_PAGE_JOYSTICK);
                    currentScreen = SCREEN_PAGES;
                    break;
                default:
                    currentScreen = SCREEN_PAGES;
                    break;
            }

            if (currentScreen == SCREEN_MENU) {
                menuFeedbackUntilMs = now + 800;
            }
        }

        if (menuFeedback && now > menuFeedbackUntilMs) {
            currentScreen = SCREEN_PAGES;
            menuFeedback = nullptr;
            forceDisplay = true;
        }

        if (now - lastMenuActivityMs > 8000) {
            currentScreen = SCREEN_PAGES;
            menuFeedback = nullptr;
            forceDisplay = true;
        }
    }
}

void loop() {
    uint32_t now = millis();
    bool forceDisplay = false;

    // 1. Processar entradas locais (Joystick, Clique Curto = Próx. Página, Longo/MODE = Menu)
    handleInputsAndMenu(now, forceDisplay);

    // 3. Processar clientes HTTP Web
    webServer.handleClient();

    // 4. Receber telemetria e respostas do Coprocessador C6 via UART
    handleUartFromC6();

    // Consulta periódica da versão do Coprocessador C6 se ainda desconhecida
    static uint32_t lastVerCheck = 0;
    if (strcmp(queuedWebCommand, "NONE") == 0 && strcmp(display.getC6Version(), "Desconhecido") == 0) {
        if (now - lastVerCheck >= 3000) {
            lastVerCheck = now;
            interSerial.println("CMD:GET_VER");
        }
    }

    // Ping periódico de heartbeat ao C6 a cada 200ms
    bool hasPendingCmd = (strcmp(queuedWebCommand, "NONE") != 0);
    if ((now - lastUartPingTime >= 200) || (hasPendingCmd && (now - lastUartPingTime >= 50))) {
        lastUartPingTime = now;
        uartSeq++;

        if (hasPendingCmd) {
            interSerial.println(queuedWebCommand);
            interSerial.flush();
            printLog("[BASE LAFVIN] >>> COMANDO PENDENTE UART: %s\n", queuedWebCommand);
            strncpy(queuedWebCommand, "NONE", sizeof(queuedWebCommand) - 1);
        }

        char pingMsg[48];
        snprintf(pingMsg, sizeof(pingMsg), "PING:%u:1\n", uartSeq);
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
    if (currentScreen == SCREEN_MENU) {
        display.renderMenu(MENU_ITEMS, MENU_COUNT, menuSelectedIndex, menuFeedback);
    } else {
        display.update(fleet, network.isConnected(), network.isSyncSuccess(), 
                       joystick.getThrottle(), joystick.getRudder(), currentNavMode, sdCard.isReady(),
                       wifiConfig.isAPMode(), wifiConfig.getIPAddress().c_str(),
                       wifiConfig.getSSID().c_str(), wifiConfig.getRSSI(),
                       wifiConfig.getAPStationCount(), wifiConfig.getAPPassword().c_str(),
                       forceDisplay,
                       network.getLastHttpStatus(), network.getSuccessCount(),
                       network.getFailCount(), network.getLastSyncTime(),
                       webServer.getMacAddress().c_str(),
                       joystick.getRawX(), joystick.getRawY(), joystick.getDeadband(),
                       joystick.isBootPressed(), joystick.isModePressed(),
                       joystick.isAnchorUpPressed(), joystick.isAnchorDownPressed(),
                       joystick.getJoyClickCount());
    }

    // 9. Atualizar LED NeoPixel conforme o estado operacional da Base
    static BaseOperationState lastOpLedState = (BaseOperationState)255;
    BaseOperationState curOp = fleet.getOperationState();
    if (curOp != lastOpLedState) {
        lastOpLedState = curOp;
        if (curOp == BASE_OP_STORAGE) {
            setRgbColor(8, 0, 16);  // Roxo / Standby
        } else if (curOp == BASE_OP_WAITING_ROVERS) {
            setRgbColor(30, 15, 0); // Laranja / À espera de lista
        } else if (curOp == BASE_OP_IDLE) {
            setRgbColor(0, 20, 25); // Ciano / Lista pronta
        } else if (curOp == BASE_OP_GO) {
            setRgbColor(0, 35, 0);  // Verde / Em missão
        }
    }

    // 10. Diagnóstico em Tempo Real do Pinout do Joystick via USB Serial
    static uint32_t lastPinDiagMs = 0;
    if (now - lastPinDiagMs >= 500) {
        lastPinDiagMs = now;
        int adc0 = joystick.getRawY();
        int adc3 = joystick.getRawX();
        printLog("[JOY PINOUT] Pad6(IO0)=%4d | Pad3(IO3)=%4d | Thr=%+4d%% Rud=%+4d%% | BTN[IO9=%d IO18=%d IO19=%d IO20=%d]\n",
                 adc0, adc3,
                 joystick.getThrottle(), joystick.getRudder(),
                 digitalRead(9), digitalRead(18), digitalRead(19), digitalRead(20));
    }
}
