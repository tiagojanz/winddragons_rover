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

// System components
FleetManager      fleet;
BaseJoystick      joystick;
WiFiConfigManager wifiConfig;
BaseNetwork       network(fleet, wifiConfig);
BaseDisplay       display;
SDCardManager     sdCard;
BaseWebServer     webServer(wifiConfig, fleet);
Adafruit_NeoPixel rgbLed(1, PIN_RGB_LED, NEO_GRB + NEO_KHZ800);

#if ENABLE_LORA
// Dedicated SPI bus for LoRa SX1278 on header pins (avoids display conflict)
SPIClass loraSPI(FSPI);
Module loraModule(PIN_LORA_NSS, PIN_LORA_DIO0, PIN_LORA_RST, RADIOLIB_NC, loraSPI);
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
    "6. PROXIMO ROVER",
    "7. INFO REDE / IP",
    "8. VOLTAR / SAIR"
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

    // 4. Initialize Joystick
    joystick.begin();
    Serial.println("[BASE] Joystick inputs initialized");
    display.bootLogf(UI_LIGHTGREY, 55, "[JOY ] Joystick calib. OK");

#if ENABLE_LORA
    // 5. Initialize LoRa
    Serial.print("[BASE] Initializing LoRa SX1278 (433MHz)... ");
    loraSPI.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_NSS);
    int state = radio.begin(LORA_FREQUENCY, LORA_BANDWIDTH, LORA_SPREAD_FACTOR,
                            LORA_CODING_RATE, LORA_SYNC_WORD, LORA_OUTPUT_POWER,
                            LORA_PREAMBLE_LEN);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("OK!");
        loraReady = true;
        setRgbColor(0, 30, 0); // Green
        radio.startReceive();
        display.bootLogf(UI_GREEN, 70, "[LORA] SX1278 433MHz OK");
    } else {
        Serial.printf("FAILED! (code: %d)\n", state);
        loraReady = false;
        loraSPI.end();
        setRgbColor(50, 0, 0); // Red
        display.bootLogf(UI_RED, 70, "[LORA] Falha SX1278 (%d)", state);
    }
#else
    Serial.println("[BASE] LoRa em repouso (barramento SPI dedicado ao Display ST7789 e MicroSD)");
    display.bootLogf(UI_LIGHTGREY, 70, "[LORA] SX1278 em repouso");
#endif

    sdCard.prepareBus();
    digitalWrite(PIN_SD_CS, HIGH);

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
    webServer.begin();
    display.bootLogf(UI_GREEN, 95, "[HTTP] Servidor Web OK");

    // 8. Iniciar sincronizacao Cloud
    network.begin();
    display.bootLogf(UI_CYAN, 98, "[SYNC] Sincronizacao Cloud");

    // Finalizar ecrã de arranque
    display.endBootScreen(1200);

    // Ecrã a indicar o modo de rede no arranque (igual ao Rover)
    display.setPage(BASE_PAGE_NETWORK);
    display.update(fleet, network.isConnected(), network.isSyncSuccess(),
                   0, 0, currentNavMode, sdCard.isReady(),
                   wifiConfig.isAPMode(), wifiConfig.getIPAddress().c_str(),
                   wifiConfig.getSSID().c_str(), wifiConfig.getRSSI(),
                   wifiConfig.getAPStationCount(), wifiConfig.getAPPassword().c_str(),
                   true);
    delay(2000);
    display.setPage(BASE_PAGE_CONTROL);

    // Configure BOOT button interrupt for runtime page cycling (Control -> Fleet -> Network)
    pinMode(PIN_BTN_BOOT, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_BTN_BOOT), isrBootButton, FALLING);
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

    // Priority 2: Transmit manual joystick control to selected active rover
    PacketManualControl packet;
    memset(&packet, 0, sizeof(packet));

    packet.magic           = LORA_MAGIC_BYTE;
    packet.msg_type        = MSG_MANUAL_CONTROL;
    packet.target_rover_id = fleet.getSelectedRoverId();

    // In menu browsing or non-control pages, zero throttle & rudder to prevent boat movement
    if (currentScreen == SCREEN_MENU || display.getCurrentPage() != BASE_PAGE_CONTROL) {
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

        if (page == BASE_PAGE_CONTROL) {
            // Long Press (>700ms) or physical MODE button: Open Action Menu
            if (joystick.wasLongClicked() || btnModePressed) {
                currentScreen = SCREEN_MENU;
                menuSelectedIndex = 0;
                lastMenuActivityMs = now;
                menuFeedback = nullptr;
                Serial.println("[BASE] Abriu Menu de Controlo");
            }
        }
        else if (page == BASE_PAGE_FLEET) {
            // Navegar entre rovers da frota com o joystick
            int8_t step = joystick.getMenuNavStep();
            if (step != 0) {
                fleet.selectNextRover();
                Serial.printf("[BASE] Frota: Rover alterado para %s\n", fleet.getSelectedRover()->code);
            }

            if (btnModePressed) {
                display.setPage(BASE_PAGE_CONTROL);
                Serial.printf("[BASE] Rover %s selecionado para comando!\n", fleet.getSelectedRover()->code);
            }
            else if (joystick.wasLongClicked()) {
                currentScreen = SCREEN_MENU;
                menuSelectedIndex = 0;
                lastMenuActivityMs = now;
                menuFeedback = nullptr;
            }
        }
        else if (page == BASE_PAGE_NETWORK) {
            // Botão MODE: Voltar à página de comando
            if (btnModePressed) {
                display.setPage(BASE_PAGE_CONTROL);
            }
            else if (joystick.wasLongClicked()) {
                currentScreen = SCREEN_MENU;
                menuSelectedIndex = 0;
                lastMenuActivityMs = now;
                menuFeedback = nullptr;
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
                    fleet.queueCommand(fleet.getSelectedRoverId(), ++localCmdId, ACTION_DROP_ANCHOR, 500);
                    menuFeedback = "ANCORA A DESCER";
                    break;
                case 3: // RECOLHER ANCORA
                    fleet.queueCommand(fleet.getSelectedRoverId(), ++localCmdId, ACTION_RETRIEVE_ANCHOR);
                    menuFeedback = "A RECOLHER...";
                    break;
                case 4: // PARAR MOTORES
                    fleet.queueCommand(fleet.getSelectedRoverId(), ++localCmdId, ACTION_STOP);
                    currentNavMode = 0;
                    menuFeedback = "MOTORES PARADOS";
                    break;
                case 5: // PROXIMO ROVER
                    fleet.selectNextRover();
                    display.setPage(BASE_PAGE_CONTROL);
                    menuFeedback = "ROVER ALTERADO";
                    break;
                case 6: // INFO REDE / IP
                    display.setPage(BASE_PAGE_NETWORK);
                    currentScreen = SCREEN_PAGES;
                    break;
                case 7: // VOLTAR / SAIR
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

    // 1. Screen Navigation Button (Instant Hardware Interrupt on BOOT / GPIO 9)
    bool forceDisplay = false;
    if (flagPageChange) {
        flagPageChange = false;
        if (currentScreen == SCREEN_MENU) {
            currentScreen = SCREEN_PAGES;
        } else {
            display.nextPage();
        }
        forceDisplay = true;
        Serial.printf("[BASE] Ecrã alterado para página %d (0:CONTROL, 1:FLEET, 2:NETWORK)\n", display.getCurrentPage());
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

    // 8. Update LCD screen (Menu or Active Page: Control, Fleet, Network)
    if (currentScreen == SCREEN_MENU) {
        display.renderMenu(MENU_ITEMS, MENU_COUNT, menuSelectedIndex, menuFeedback);
    } else {
        display.update(fleet, network.isConnected(), network.isSyncSuccess(), 
                       joystick.getThrottle(), joystick.getRudder(), currentNavMode, sdCard.isReady(),
                       wifiConfig.isAPMode(), wifiConfig.getIPAddress().c_str(),
                       wifiConfig.getSSID().c_str(), wifiConfig.getRSSI(),
                       wifiConfig.getAPStationCount(), wifiConfig.getAPPassword().c_str(),
                       forceDisplay);
    }
}
