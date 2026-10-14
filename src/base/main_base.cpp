#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include <Adafruit_NeoPixel.h>

#include "config_common.h"
#include "lora_protocol.h"
#include "fleet_manager.h"
#include "base_joystick.h"
#include "base_network.h"
#include "base_display.h"
#include "sd_card_manager.h"
#include "wifi_config_manager.h"
#include "base_web_server.h"

#include "soft_spi_hal.h"

// System components
FleetManager      fleet;
BaseJoystick      joystick;
WiFiConfigManager wifiConfig;
BaseNetwork       network(fleet, wifiConfig);
BaseDisplay       display;
SDCardManager     sdCard;
BaseWebServer     webServer(wifiConfig, fleet, &sdCard, &network);
Adafruit_NeoPixel rgbLed(1, PIN_RGB_LED, NEO_GRB + NEO_KHZ800);

#if ENABLE_LORA
// Dedicated SoftSpiHal isolates LoRa completely from hardware FSPI (LCD & MicroSD)
SoftSpiHal loraHal(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI);
Module loraModule(&loraHal, PIN_LORA_NSS, PIN_LORA_DIO0, PIN_LORA_RST, RADIOLIB_NC);
SX1278 radio(&loraModule);
bool loraReady = false;
#endif

// Loop timers
uint32_t lastControlTxTime = 0;
const uint32_t CONTROL_TX_INTERVAL_MS = 100; // 10Hz manual control stream

// Navigation Mode & UI Menu state
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

// BOOT Button Navigation Interrupt (Same as Rover)
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

void setup() {
    Serial.begin(115200);
    Serial.setTxTimeoutMs(0); // Non-blocking USB CDC TX (prevents stalls)
    delay(200);
    Serial.println("\n=== WINDDRAGONS BASE STATION BOOTING ===");

    // De-assert all SPI Chip Selects before bus init
    pinMode(PIN_LCD_CS, OUTPUT);
    digitalWrite(PIN_LCD_CS, HIGH);
    pinMode(PIN_LORA_NSS, OUTPUT);
    digitalWrite(PIN_LORA_NSS, HIGH);
    pinMode(PIN_SD_CS, OUTPUT);
    digitalWrite(PIN_SD_CS, HIGH); // MicroSD CS

    // 1. Initialize Display & Boot Screen
    display.begin();
    display.showBootScreen();
    display.bootLogf(UI_CYAN, 10, "[BOOT] WindDragons Base v2");
    Serial.println("[BASE] LCD ST7789 initialized");

    // 2. Status RGB NeoPixel LED
    rgbLed.begin();
    setRgbColor(20, 20, 0); // Yellow: Booting
    display.bootLogf(UI_LIGHTGREY, 20, "[LED ] NeoPixel RGB ativo");

    // 3. Initialize MicroSD Card
    Serial.print("[BASE] A inicializar cartao MicroSD... ");
    if (sdCard.begin()) {
        Serial.println("OK!");
        display.bootLogf(UI_GREEN, 35, "[SD  ] MicroSD FAT32 pronto");
        sdCard.printCardInfo();
        sdCard.printDirectory("/", 1);

        if (sdCard.fileExists("/config.txt")) {
            Serial.println("[BASE] Leitura de /config.txt:");
            Serial.println(sdCard.readFile("/config.txt"));
            display.bootLogf(UI_CYAN, 45, "[SD  ] /config.txt lido");
        }
    } else {
        Serial.println("Nenhum cartao detetado (ou formato nao suportado).");
        display.bootLogf(UI_YELLOW, 35, "[SD  ] Sem cartao presente");
    }

    // 3.1 Inicializar Frota & Missão (SD / NVS)
    fleet.begin(&sdCard);
    display.bootLogf(UI_GREEN, 48, "[FROTA] %u rovers | %s", (uint32_t)fleet.getRovers().size(), fleet.getOperationStateBadge());

    // 4. Initialize Joystick
    joystick.begin();
    Serial.println("[BASE] Joystick inputs initialized");
    display.bootLogf(UI_LIGHTGREY, 55, "[JOY ] Joystick calib. OK");

#if ENABLE_LORA
    // 5. Initialize LoRa
    Serial.printf("[BASE] LoRa Pinos: SCK=IO%d, MOSI=IO%d, MISO=IO%d, NSS=IO%d, DIO0=IO%d\n",
                  PIN_LORA_SCK, PIN_LORA_MOSI, PIN_LORA_MISO, PIN_LORA_NSS, PIN_LORA_DIO0);
    loraHal.spiBegin();

    int state = RADIOLIB_ERR_CHIP_NOT_FOUND;
    for (int attempt = 1; attempt <= 10; attempt++) {
        uint8_t chipVersion = loraHal.readReg(0x42, PIN_LORA_NSS);
        uint8_t regOpMode   = loraHal.readReg(0x01, PIN_LORA_NSS);
        Serial.printf("[BASE] Tentativa #%d - RegVersion (0x42): 0x%02X | OpMode: 0x%02X\n", 
                      attempt, chipVersion, regOpMode);
        
        if (chipVersion == 0x12) {
            Serial.print("[BASE] SX1278 pronto! A inicializar RadioLib (433MHz)... ");
            state = radio.begin(LORA_FREQUENCY, LORA_BANDWIDTH, LORA_SPREAD_FACTOR,
                                LORA_CODING_RATE, LORA_SYNC_WORD, LORA_OUTPUT_POWER,
                                LORA_PREAMBLE_LEN);
            if (state == RADIOLIB_ERR_NONE) {
                Serial.println("OK!");
                loraReady = true;
                setRgbColor(0, 30, 0); // Green
                radio.startReceive();
                display.bootLogf(UI_GREEN, 70, "[LORA] SX1278 433MHz OK");
                break;
            } else {
                Serial.printf("RadioLib erro (%d)\n", state);
            }
        }
        delay(500);
    }

    if (!loraReady) {
        Serial.printf("[BASE] Initializing LoRa SX1278... FAILED! (code: %d)\n", state);
        setRgbColor(50, 0, 0); // Red
        display.bootLogf(UI_RED, 70, "[LORA] Falha SX1278 (%d)", state);
    }
#else
    Serial.println("[BASE] LoRa em repouso (barramento SPI dedicado ao Display ST7789 e MicroSD)");
    display.bootLogf(UI_LIGHTGREY, 70, "[LORA] SX1278 em repouso");
#endif

    // 6. Initialize Network (WiFi + AP Fallback + Web Server)
    Serial.println("[BASE] A inicializar gestor WiFi...");
    display.bootLogf(UI_YELLOW, 78, "[NET ] A ligar WiFi (STA)...");
    wifiConfig.begin(&sdCard);
    Serial.println("[BASE] A verificar ligacoes WiFi guardadas...");
    bool wifiOk = wifiConfig.autoConnect(8000, [](const char* ssid) {
        display.bootLogf(UI_YELLOW, 82, "[NET ] A testar '%s'...", ssid);
    });

    if (!wifiOk) {
        Serial.println("[BASE] Sem ligacao WiFi no arranque -> A ATIVAR MODO AP!");
        wifiConfig.startAccessPoint("WindDragons-Base", "12345678");
        setRgbColor(30, 15, 0); // Orange / AP mode
        display.bootLogf(UI_YELLOW, 84, "[NET ] Falha STA -> Modo AP");
        display.bootLogf(UI_CYAN, 87, "[NET ] AP: WindDragons-Base");
        display.bootLogf(UI_WHITE, 90, "[NET ] IP: 192.168.4.1");
    } else {
        Serial.printf("[BASE] Conectado ao WiFi: %s | IP: %s\n", wifiConfig.getSSID().c_str(), wifiConfig.getIPAddress().c_str());
        setRgbColor(0, 30, 0);  // Green / STA connected
        display.bootLogf(UI_GREEN, 90, "[NET ] IP: %s", wifiConfig.getIPAddress().c_str());
    }

    // 7. Iniciar Servidor HTTP Web
    webServer.setLoraReadyPtr(&loraReady);
    webServer.begin();
    display.bootLogf(UI_GREEN, 95, "[HTTP] Servidor Web OK");

    // 8. Iniciar sincronizacao Cloud
    network.begin();
    display.bootLogf(UI_CYAN, 98, "[SYNC] Sincronizacao Cloud");

    // Carregar configuração do ecrã LCD (Teste Joystick e Brilho)
    Preferences dispPrefs;
    if (dispPrefs.begin("base_display", true)) {
        bool joyScr = dispPrefs.getBool("joy_scr", false);
        display.setJoystickScreenEnabled(joyScr);
        uint8_t bright = dispPrefs.getUChar("brightness", display.getBrightness());
        display.setBrightness(bright);
        dispPrefs.end();
    }
    webServer.setDisplay(&display);

    // Finalizar ecrã de arranque
    display.endBootScreen(1200);

    // Arrancar no 1º Ecrã (Identificação da Estação, MAC e Estado)
    display.setPage(BASE_PAGE_STATION);

    // Configure BOOT button interrupt for runtime page cycling (Station -> WiFi -> Rover -> [Joy])
    pinMode(PIN_BTN_BOOT, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_BTN_BOOT), isrBootButton, FALLING);
#ifdef PIN_BTN_SW1
    pinMode(PIN_BTN_SW1, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_BTN_SW1), isrBootButton, FALLING);
#endif
    flagPageChange = false; // ensure clean start on Page 0
}

void transmitControlOrCommand() {
#if ENABLE_LORA
    if (!loraReady) return;

    // Priority 1: Transmit any pending cloud or local action command (e.g. DROP_ANCHOR)
    if (fleet.hasPendingCommand()) {
        QueuedCommand qCmd = fleet.popPendingCommand();
        PacketActionCommand packet;
        memset(&packet, 0, sizeof(packet));

        packet.magic           = LORA_MAGIC_BYTE;
        packet.msg_type        = MSG_ACTION_COMMAND;
        packet.target_rover_id = qCmd.target_rover_id;
        packet.command_id      = qCmd.command_id;
        packet.action          = qCmd.action;
        packet.target_depth_cm = qCmd.depth_cm;
        packet.duration_s      = qCmd.duration_s;
        packet.target_lat_deg7 = qCmd.target_lat;
        packet.target_lng_deg7 = qCmd.target_lng;

        packet.checksum = calculate_checksum(reinterpret_cast<const uint8_t*>(&packet),
                                             sizeof(packet) - sizeof(packet.checksum));

        radio.standby();
        radio.transmit(reinterpret_cast<uint8_t*>(&packet), sizeof(packet));
        radio.startReceive();

        Serial.printf("[BASE] Dispatched LoRa Action Command #%u (%s) to Rover #%u\n", 
                      qCmd.command_id, get_action_str(qCmd.action), qCmd.target_rover_id);
        return;
    }

    // Em modo STORAGE, não emite controlo contínuo de joystick para os rovers (poupança de energia e canal rádio)
    if (fleet.getOperationState() == BASE_OP_STORAGE && !webServer.isWebControlActive()) {
        return;
    }

    // Priority 2: Transmit manual joystick control to selected active rover
    if (!fleet.hasSelectedRover()) {
        return; // Nenhum rover selecionado -> Não transmite comandos contínuos
    }

    PacketManualControl packet;
    memset(&packet, 0, sizeof(packet));

    packet.magic           = LORA_MAGIC_BYTE;
    packet.msg_type        = MSG_MANUAL_CONTROL;
    packet.target_rover_id = fleet.getSelectedRoverId();

    // In menu browsing or non-control pages, zero throttle & rudder to prevent boat movement
    if (currentScreen == SCREEN_MENU || display.getCurrentPage() != BASE_PAGE_ROVER) {
        packet.throttle = 0;
        packet.rudder   = 0;
        packet.anchor_jog = 0;
    } else if (webServer.isWebControlActive()) {
        // Priority to Web commands from browser
        packet.throttle = webServer.getWebThrottle();
        packet.rudder   = webServer.getWebRudder();
        packet.anchor_jog = webServer.getWebAnchorJog();
    } else {
        packet.throttle = joystick.getThrottle();
        packet.rudder   = joystick.getRudder();
        packet.anchor_jog = joystick.getAnchorJog();
    }
    packet.mode_request = currentNavMode;

    packet.checksum = calculate_checksum(reinterpret_cast<const uint8_t*>(&packet),
                                         sizeof(packet) - sizeof(packet.checksum));

    radio.standby();
    radio.transmit(reinterpret_cast<uint8_t*>(&packet), sizeof(packet));
    radio.startReceive();
#endif
}

void checkIncomingTelemetry() {
#if ENABLE_LORA
    if (!loraReady) return;

    int packetLen = radio.getPacketLength();
    if (packetLen > 0) {
        uint8_t buffer[64];
        int readState = radio.readData(buffer, packetLen);
        if (readState == RADIOLIB_ERR_NONE) {
            if (packetLen == sizeof(PacketTelemetry) && buffer[0] == LORA_MAGIC_BYTE && buffer[1] == MSG_TELEMETRY) {
                PacketTelemetry *pkt = reinterpret_cast<PacketTelemetry*>(buffer);
                uint16_t calc = calculate_checksum(buffer, sizeof(PacketTelemetry) - sizeof(pkt->checksum));
                if (calc == pkt->checksum) {
                    int16_t rssi = static_cast<int16_t>(radio.getRSSI());
                    fleet.updateTelemetry(*pkt, rssi);
                }
            }
        }
        radio.startReceive();
    }
#endif
}

void handleInputsAndMenu(uint32_t now) {
    joystick.update();

    bool btnModePressed = (digitalRead(PIN_BTN_MODE) == LOW);

    if (currentScreen == SCREEN_PAGES) {
        BaseScreenPage page = display.getCurrentPage();

        if (page == BASE_PAGE_STATION) {
            // 1º Ecrã: Nome da Estação, MAC, Estado
            // Clique curto no joystick: avança o estado da operação (STORAGE -> WAITING -> IDLE -> GO)
            if (joystick.wasShortClicked()) {
                BaseOperationState cur = fleet.getOperationState();
                BaseOperationState nxt = static_cast<BaseOperationState>((cur + 1) % 4);
                fleet.setOperationState(nxt);
                Serial.printf("[BASE] Estado de operacao alterado para: %s\n", fleet.getOperationStateStr());
            }
            // Long Press (>700ms) ou botão MODE: Abre Menu de Controlo
            else if (joystick.wasLongClicked() || btnModePressed) {
                currentScreen = SCREEN_MENU;
                menuSelectedIndex = 0;
                lastMenuActivityMs = now;
                menuFeedback = nullptr;
                Serial.println("[BASE] Abriu Menu de Controlo");
            }
        }
        else if (page == BASE_PAGE_WIFI) {
            // 2º Ecrã: Conectividade WiFi
            // Long Press (>700ms): Abre Menu
            if (joystick.wasLongClicked()) {
                currentScreen = SCREEN_MENU;
                menuSelectedIndex = 0;
                lastMenuActivityMs = now;
                menuFeedback = nullptr;
            }
            // Botão MODE ou clique curto: Avança para o 3º ecrã (Rover)
            else if (btnModePressed || joystick.wasShortClicked()) {
                display.setPage(BASE_PAGE_ROVER);
            }
        }
        else if (page == BASE_PAGE_ROVER) {
            // 3º Ecrã: Rover Selecionado (Comando e Telemetria)
            // Nota: A seleção do rover é efetuada exclusivamente através do Website.
            if (fleet.hasSelectedRover()) {
                // Clique curto no joystick: Alterna modo MANUAL vs HOLD STATION
                if (joystick.wasShortClicked()) {
                    currentNavMode = (currentNavMode == 0) ? 1 : 0;
                    Serial.printf("[BASE] Modo alterado para: %s\n", (currentNavMode == 1) ? "HOLD STATION" : "MANUAL");
                }
            }

            // Long Press (>700ms) ou botão MODE: Abre Menu de Controlo
            if (joystick.wasLongClicked() || btnModePressed) {
                currentScreen = SCREEN_MENU;
                menuSelectedIndex = 0;
                lastMenuActivityMs = now;
                menuFeedback = nullptr;
                Serial.println("[BASE] Abriu Menu de Controlo");
            }
        }
        else if (page == BASE_PAGE_JOYSTICK) {
            // 4º Ecrã (Opcional): Teste do Joystick
            // Botão MODE ou clique longo: Volta ao 1º Ecrã (Estação)
            if (btnModePressed || joystick.wasLongClicked()) {
                display.setPage(BASE_PAGE_STATION);
            }
        }
    } 
    else if (currentScreen == SCREEN_MENU) {
        // Step UP / DOWN with joystick
        int8_t step = joystick.getMenuNavStep();
        if (step != 0) {
            menuSelectedIndex = (menuSelectedIndex + step + MENU_COUNT) % MENU_COUNT;
            lastMenuActivityMs = now;
        }

        // Joystick Click: CONFIRM selected item
        if (joystick.wasShortClicked() || joystick.wasLongClicked()) {
            lastMenuActivityMs = now;
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
                    display.setPage(BASE_PAGE_JOYSTICK);
                    currentScreen = SCREEN_PAGES;
                    break;
                default:
                    currentScreen = SCREEN_PAGES;
                    break;
            }

            if (currentScreen == SCREEN_MENU) {
                menuFeedbackUntilMs = now + 800; // Show confirmation banner for 800ms
            }
        }

        // Auto-dismiss confirmation banner
        if (menuFeedback && now > menuFeedbackUntilMs) {
            currentScreen = SCREEN_PAGES;
            menuFeedback = nullptr;
        }

        // Inactivity timeout: 8 seconds
        if (now - lastMenuActivityMs > 8000) {
            currentScreen = SCREEN_PAGES;
            menuFeedback = nullptr;
        }
    }
}

void loop() {
    uint32_t now = millis();

    // 0. Polling debounce para o botão BOOT (GPIO 9 e GPIO 18) como garantia extra
    static bool lastBootRaw = HIGH;
    bool currentBootRaw = (digitalRead(PIN_BTN_BOOT) == LOW);
#ifdef PIN_BTN_SW1
    if (digitalRead(PIN_BTN_SW1) == LOW) currentBootRaw = true;
#endif
    if (currentBootRaw && lastBootRaw == HIGH && (now - lastBtnPressTime > 250)) {
        lastBtnPressTime = now;
        flagPageChange = true;
    }
    lastBootRaw = currentBootRaw ? LOW : HIGH;

    // 1. Screen Navigation Button (Instant Hardware Interrupt on BOOT / GPIO 9)
    bool forceDisplay = false;
    static uint32_t joyHoldStartMs = 0;

    if (display.getCurrentPage() == BASE_PAGE_JOYSTICK && currentScreen == SCREEN_PAGES) {
        // No ecrã de teste do joystick, manter premido o botão BOOT / JOYSTICK SW (GPIO 9) por > 1s sai da página
        bool btnBootDown = (digitalRead(PIN_BTN_BOOT) == LOW);
        if (btnBootDown) {
            if (joyHoldStartMs == 0) {
                joyHoldStartMs = now;
            } else if (now - joyHoldStartMs >= 1000) {
                display.setPage(BASE_PAGE_STATION);
                forceDisplay = true;
                joyHoldStartMs = 0;
                flagPageChange = false;
                Serial.println("[BASE] Saiu do ecrã de teste do joystick por pressão longa (>1s)");
            }
        } else {
            joyHoldStartMs = 0;
        }

        // Se houve clique simples em GPIO 9 na página de teste, consome a flag e refresca o ecrã
        if (flagPageChange) {
            flagPageChange = false;
            forceDisplay = true;
        }
    } else {
        if (flagPageChange) {
            flagPageChange = false;
            if (currentScreen == SCREEN_MENU) {
                currentScreen = SCREEN_PAGES;
            } else {
                display.nextPage();
            }
            forceDisplay = true;
            Serial.printf("[BASE] Ecrã alterado para página %d (0:ESTAÇÃO, 1:WIFI, 2:ROVER, 3:TESTE JOYSTICK)\n", display.getCurrentPage());
        }
    }

    // 2. Process local inputs & menu state machine
    handleInputsAndMenu(now);

    // 3. Handle HTTP Web Server requests (WiFi / AP)
    webServer.handleClient();

    // 4. Receive LoRa telemetry from fleet rovers
    checkIncomingTelemetry();

    // 5. Send LoRa commands to Rover (10Hz)
    if (now - lastControlTxTime >= CONTROL_TX_INTERVAL_MS) {
        lastControlTxTime = now;
        transmitControlOrCommand();
    }

    // 6. Update WiFi & sync with Winddragons Cloud API (only in STA mode)
    network.update();

    // 7. Update Rover online/offline status
    fleet.checkOnlineStatus();

    // 8. Update LCD screen (Menu or Active Page: Control, Fleet, Network, Cloud, Joystick)
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

    // 9. Atualizar LED NeoPixel de acordo com o estado operacional da Base
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
}
