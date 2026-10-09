#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include <Adafruit_NeoPixel.h>

#include "config_common.h"
#include "lora_protocol.h"
#include "gps_tracker.h"
#include "actuators.h"
#include "battery_monitor.h"
#include "rover_display.h"
#include "sd_card_manager.h"
#include "wifi_config_manager.h"
#include "rover_config_manager.h"
#include "rover_web_server.h"

// Define default Rover ID fallback
#ifndef ROVER_ID
#define ROVER_ID 1
#endif

// Peripherals
GPSTracker gps;
ActuatorController actuators;
BatteryMonitor battery;
RoverDisplay roverDisplay;
SDCardManager sdCard;
Adafruit_NeoPixel rgbLed(1, PIN_RGB_LED, NEO_GRB + NEO_KHZ800);
WiFiConfigManager wifiConfig;
RoverConfigManager roverConfig;

// State tracking forward declaration
extern uint8_t currentMotorStatus;
extern uint32_t lastControlReceivedTime;

RoverWebServer webServer(wifiConfig, gps, battery, actuators, currentMotorStatus, lastControlReceivedTime, roverConfig, sdCard, &roverDisplay);

#if ENABLE_LORA
// Dedicated SPI bus for LoRa SX1278 on header pins (avoids display conflict)
SPIClass loraSPI(FSPI);
Module loraModule(PIN_LORA_NSS, PIN_LORA_DIO0, PIN_LORA_RST, RADIOLIB_NC, loraSPI);
SX1278 radio(&loraModule);
#endif

// State tracking
uint16_t telemetrySeq = 0;
uint32_t lastTelemetryTime = 0;
uint32_t lastControlReceivedTime = 0;
const uint32_t FAILSAFE_TIMEOUT_MS = 2500;
uint8_t currentMotorStatus = MOTOR_IDLE;
uint16_t lastExecutedCommandId = 0;
bool loraReady = false;

// Autonomous Navigation & Station Keeping Coordinates
double homeLat = 0.0;
double homeLng = 0.0;
bool hasHomeSet = false;

double stationTargetLat = 0.0;
double stationTargetLng = 0.0;

double navTargetLat = 0.0;
double navTargetLng = 0.0;

const float STATION_KEEPING_TOLERANCE_M = 8.0f; // Station keeping radius in meters
const float WAYPOINT_ARRIVAL_RADIUS_M    = 4.0f; // Target arrival radius in meters

// Button Navigation Interrupt
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

void sendTelemetry() {
    PacketTelemetry packet;
    memset(&packet, 0, sizeof(packet));

    packet.magic          = LORA_MAGIC_BYTE;
    packet.msg_type       = MSG_TELEMETRY;
    packet.rover_id       = roverConfig.getRoverId();
    packet.seq_num        = telemetrySeq++;
    
    packet.lat_deg7       = gps.getLatDeg7();
    packet.lng_deg7       = gps.getLngDeg7();
    packet.speed_cm_s     = static_cast<uint16_t>(gps.getSpeedKnots() * 51.4444f); // knots to cm/s
    packet.heading_deg10  = static_cast<uint16_t>(gps.getHeading() * 10.0f);
    packet.satellites     = gps.getSatellites();
    packet.fix_quality    = gps.getFixQuality();

    // Real LiPo 4S telemetry from APM Power Module (28V / 90A)
    battery.update();
    packet.battery_mv     = battery.getVoltageMv(); 
    packet.battery_pct    = battery.getPercentage();

    Serial.printf("[BATT 4S] %u mV (%u%%) | I: %.2f A | Consumo: %.1f mAh | Avg Cell: %.2f V\n",
                  battery.getVoltageMv(), battery.getPercentage(), 
                  battery.getCurrentAmps(), battery.getConsumedMah(), battery.getCellAverageV());

    packet.motor_status   = currentMotorStatus;
    packet.anchor_status  = actuators.getAnchorStatus();
    packet.anchor_depth_cm= actuators.getAnchorDepthCm();
    packet.last_cmd_id    = lastExecutedCommandId;

    packet.checksum = calculate_checksum(reinterpret_cast<const uint8_t*>(&packet), 
                                         sizeof(packet) - sizeof(packet.checksum));

    // Transmit telemetry
#if ENABLE_LORA
    if (loraReady) {
        radio.standby();
        radio.transmit(reinterpret_cast<uint8_t*>(&packet), sizeof(packet));
        radio.startReceive();
    }
#endif
}

void processIncomingPacket(uint8_t *buffer, size_t length) {
    if (length < 2 || buffer[0] != LORA_MAGIC_BYTE) {
        return; // Invalid packet
    }

    uint8_t msgType = buffer[1];

    if (msgType == MSG_MANUAL_CONTROL && length == sizeof(PacketManualControl)) {
        PacketManualControl *cmd = reinterpret_cast<PacketManualControl*>(buffer);
        if (cmd->target_rover_id == roverConfig.getRoverId() || cmd->target_rover_id == BROADCAST_ROVER_ID) {
            uint16_t calc = calculate_checksum(buffer, sizeof(PacketManualControl) - sizeof(cmd->checksum));
            if (calc == cmd->checksum) {
                lastControlReceivedTime = millis();

                if (cmd->mode_request == 1) { // HOLD STATION (Virtual Anchor)
                    if (currentMotorStatus != MOTOR_HOLDING_STATION) {
                        if (gps.hasFix()) {
                            stationTargetLat = gps.getLatitude();
                            stationTargetLng = gps.getLongitude();
                        }
                        currentMotorStatus = MOTOR_HOLDING_STATION;
                        Serial.printf("[ROVER] Switched to HOLD_STATION at (%.6f, %.6f)\n", 
                                      stationTargetLat, stationTargetLng);
                    }
                } else { // 0 = MANUAL
                    currentMotorStatus = MOTOR_MANUAL;
                    actuators.setThrottle(cmd->throttle);
                    actuators.setRudder(cmd->rudder);
                    actuators.jogWinch(cmd->anchor_jog);
                }

                // LED flash Blue
                setRgbColor(0, 0, 30);
            }
        }
    } 
    else if (msgType == MSG_ACTION_COMMAND && length == sizeof(PacketActionCommand)) {
        PacketActionCommand *cmd = reinterpret_cast<PacketActionCommand*>(buffer);
        if (cmd->target_rover_id == roverConfig.getRoverId() || cmd->target_rover_id == BROADCAST_ROVER_ID) {
            uint16_t calc = calculate_checksum(buffer, sizeof(PacketActionCommand) - sizeof(cmd->checksum));
            if (calc == cmd->checksum) {
                lastExecutedCommandId = cmd->command_id;
                Serial.printf("[ROVER] Received Command #%u: %s\n", 
                              cmd->command_id, get_action_str(cmd->action));

                switch (cmd->action) {
                    case ACTION_HOLD_STATION:
                        if (cmd->target_lat_deg7 != 0 && cmd->target_lng_deg7 != 0) {
                            stationTargetLat = cmd->target_lat_deg7 / 10000000.0;
                            stationTargetLng = cmd->target_lng_deg7 / 10000000.0;
                        } else if (gps.hasFix()) {
                            stationTargetLat = gps.getLatitude();
                            stationTargetLng = gps.getLongitude();
                        }
                        currentMotorStatus = MOTOR_HOLDING_STATION;
                        Serial.printf("[ROVER] HOLD_STATION target: %.6f, %.6f\n", 
                                      stationTargetLat, stationTargetLng);
                        break;

                    case ACTION_STOP:
                        actuators.stopMotor();
                        actuators.centerRudder();
                        currentMotorStatus = MOTOR_IDLE;
                        Serial.println("[ROVER] STOP/STANDBY: Motors neutral");
                        break;

                    case ACTION_DROP_ANCHOR:
                        actuators.stopMotor();
                        actuators.centerRudder();
                        actuators.commandDropAnchor(cmd->target_depth_cm);
                        currentMotorStatus = MOTOR_ANCHORED;
                        Serial.printf("[ROVER] DROP_ANCHOR: Cremalheira levantada -> queda livre por gravidade ate %u cm\n", cmd->target_depth_cm);
                        break;

                    case ACTION_RETRIEVE_ANCHOR:
                        actuators.commandRetractAnchor();
                        currentMotorStatus = MOTOR_IDLE;
                        Serial.println("[ROVER] RETRIEVE_ANCHOR: Retracting anchor to boat");
                        break;

                    case ACTION_STOP_ANCHOR:
                        actuators.stopWinch();
                        Serial.println("[ROVER] STOP_ANCHOR: Emergency stop winch");
                        break;

                    case ACTION_NAVIGATE:
                        if (cmd->target_lat_deg7 != 0 && cmd->target_lng_deg7 != 0) {
                            navTargetLat = cmd->target_lat_deg7 / 10000000.0;
                            navTargetLng = cmd->target_lng_deg7 / 10000000.0;
                            currentMotorStatus = MOTOR_MOVING_TO_TARGET;
                            Serial.printf("[ROVER] NAVIGATE to: %.6f, %.6f\n", navTargetLat, navTargetLng);
                        }
                        break;

                    case ACTION_RTL:
                        if (hasHomeSet) {
                            navTargetLat = homeLat;
                            navTargetLng = homeLng;
                            currentMotorStatus = MOTOR_RTL;
                            Serial.printf("[ROVER] RTL: Returning to Home (%.6f, %.6f)\n", homeLat, homeLng);
                        } else {
                            Serial.println("[ROVER] RTL ignored: Home position not yet established");
                        }
                        break;

                    case ACTION_ALARM: {
                        uint16_t dur = (cmd->duration_s > 0) ? cmd->duration_s : 10;
                        actuators.triggerAlarm(dur);
                        Serial.printf("[ROVER] ALARM triggered for %u seconds\n", dur);
                        break;
                    }

                    case ACTION_PING:
                        Serial.println("[ROVER] PING: Sending immediate telemetry packet");
                        sendTelemetry();
                        break;

                    case ACTION_MANUAL:
                        currentMotorStatus = MOTOR_MANUAL;
                        break;

                    default:
                        break;
                }
            }
        }
    }
}

// Navigation & Course correction controller
void updateAutonomousNavigation() {
    // Only run if an autonomous navigation mode is active
    if (currentMotorStatus != MOTOR_HOLDING_STATION && 
        currentMotorStatus != MOTOR_MOVING_TO_TARGET && 
        currentMotorStatus != MOTOR_RTL) {
        return;
    }

    if (!gps.hasFix()) {
        actuators.stopMotor();
        actuators.centerRudder();
        return;
    }

    // Set initial Home position once GPS fix is acquired
    if (!hasHomeSet) {
        homeLat = gps.getLatitude();
        homeLng = gps.getLongitude();
        hasHomeSet = true;
        Serial.printf("[ROVER] Home GPS position recorded: %.6f, %.6f\n", homeLat, homeLng);
    }

    // 1. Station Keeping Mode (Virtual Anchor)
    if (currentMotorStatus == MOTOR_HOLDING_STATION) {
        double distM = gps.distanceTo(stationTargetLat, stationTargetLng);
        if (distM > STATION_KEEPING_TOLERANCE_M) {
            double targetCourse = gps.courseTo(stationTargetLat, stationTargetLng);
            double headingError = targetCourse - gps.getHeading();
            while (headingError > 180.0) headingError -= 360.0;
            while (headingError < -180.0) headingError += 360.0;

            int8_t rudder = constrain(static_cast<int>(headingError * 1.5f), -100, 100);
            actuators.setRudder(rudder);

            // Gentle throttle to nudge rover back inside station radius
            int8_t throttle = (abs(headingError) > 50.0) ? 20 : 35;
            actuators.setThrottle(throttle);
        } else {
            // Inside station circle: stop thrusters
            actuators.stopMotor();
            actuators.centerRudder();
        }
    }
    // 2. Waypoint Navigation or RTL Mode
    else if (currentMotorStatus == MOTOR_MOVING_TO_TARGET || currentMotorStatus == MOTOR_RTL) {
        double distM = gps.distanceTo(navTargetLat, navTargetLng);
        if (distM <= WAYPOINT_ARRIVAL_RADIUS_M) {
            // Target coordinates reached
            actuators.stopMotor();
            actuators.centerRudder();
            Serial.printf("[ROVER] Arrived at destination (dist: %.1fm)!\n", distM);

            if (currentMotorStatus == MOTOR_MOVING_TO_TARGET) {
                // Hold station at new waypoint
                stationTargetLat = navTargetLat;
                stationTargetLng = navTargetLng;
                currentMotorStatus = MOTOR_HOLDING_STATION;
            } else {
                // Arrived Home on RTL -> Stop and idle
                currentMotorStatus = MOTOR_IDLE;
            }
        } else {
            double targetCourse = gps.courseTo(navTargetLat, navTargetLng);
            double headingError = targetCourse - gps.getHeading();
            while (headingError > 180.0) headingError -= 360.0;
            while (headingError < -180.0) headingError += 360.0;

            int8_t rudder = constrain(static_cast<int>(headingError * 1.8f), -100, 100);
            actuators.setRudder(rudder);

            // Forward speed: slow down for sharp turns
            int8_t throttle = (abs(headingError) > 60.0) ? 25 : 50;
            actuators.setThrottle(throttle);
        }
    }
}

// Process Serial CLI Commands for direct debugging and actuator testing
void handleSerialCommands() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd.length() == 0) return;

        Serial.printf("[CLI] Comando recebido: '%s'\n", cmd.c_str());

        if (cmd.equalsIgnoreCase("largar") || cmd.equalsIgnoreCase("soltar") || cmd == "90") {
            actuators.liftRack();
        } else if (cmd.equalsIgnoreCase("travar") || cmd == "0") {
            actuators.engageRack();
        } else if (cmd.equalsIgnoreCase("relax") || cmd.equalsIgnoreCase("desligar") || cmd.equalsIgnoreCase("solto")) {
            actuators.relaxRack();
        } else if (cmd.startsWith("leme ") || cmd.startsWith("l ")) {
            int pos = cmd.indexOf(' ');
            int val = cmd.substring(pos + 1).toInt();
            actuators.setRudder(val);
        } else if (cmd.startsWith("motor ") || cmd.startsWith("m ")) {
            int pos = cmd.indexOf(' ');
            int val = cmd.substring(pos + 1).toInt();
            actuators.setThrottle(val);
        } else if (cmd.startsWith("guincho ") || cmd.startsWith("g ")) {
            int pos = cmd.indexOf(' ');
            String arg = cmd.substring(pos + 1);
            arg.trim();
            if (arg.equalsIgnoreCase("sobe") || arg.equalsIgnoreCase("enrolar") || arg.equalsIgnoreCase("puxar") || arg.equalsIgnoreCase("on") || arg == "1") {
                actuators.jogWinch(1);
            } else if (arg.equalsIgnoreCase("desce") || arg.equalsIgnoreCase("desenrolar") || arg.equalsIgnoreCase("largar") || arg == "-1") {
                actuators.jogWinch(-1);
            } else if (arg.equalsIgnoreCase("stop") || arg.equalsIgnoreCase("parar") || arg.equalsIgnoreCase("off") || arg.equalsIgnoreCase("corte") || arg == "0") {
                actuators.stopWinch();
            } else {
                int val = arg.toInt();
                if (val >= 800 && val <= 2200) {
                    actuators.setWinchMicroseconds(val);
                } else if (val > 0) {
                    actuators.jogWinch(1);
                } else if (val < 0) {
                    actuators.jogWinch(-1);
                } else {
                    actuators.stopWinch();
                }
            }
        } else if (cmd.equalsIgnoreCase("off") || cmd.equalsIgnoreCase("corte") || cmd.equalsIgnoreCase("desligar guincho")) {
            actuators.stopWinch();
        } else if (cmd.startsWith("neutro ") || cmd.startsWith("n ")) {
            int pos = cmd.indexOf(' ');
            int val = cmd.substring(pos + 1).toInt();
            actuators.setWinchNeutral(val);
        } else if (cmd.startsWith("ancora ") || cmd.startsWith("a ")) {
            int pos = cmd.indexOf(' ');
            int val = cmd.substring(pos + 1).toInt();
            actuators.setRackAngle(val);
        } else if (cmd.equalsIgnoreCase("status")) {
            Serial.printf("[CLI] Trinco: %s | Leme: %d%% | Motor: %d%% | Guincho: %d us (Neutro: %d us) | Âncora: %s (%d cm)\n", 
                          actuators.isRackReleased() ? "SOLTO" : "TRAVADO",
                          actuators.getRudder(), actuators.getThrottle(),
                          actuators.getWinchPulseUs(), actuators.getWinchNeutral(),
                          get_anchor_status_str(actuators.getAnchorStatus()),
                          actuators.getAnchorDepthCm());
        } else if (cmd.equalsIgnoreCase("help") || cmd == "?") {
            Serial.println("[CLI] Comandos Disponíveis:");
            Serial.println("  - leme < -100 a 100 >   : Move o leme (ex: leme -50, leme 0, leme 75)");
            Serial.println("  - motor < -100 a 100 >  : Aciona o motor ESC");
            Serial.println("  - guincho <1000 a 2000> : Pulso direto em us (ex: guincho 1515)");
            Serial.println("  - neutro <1000 a 2000>  : Memoriza novo ponto neutro calibrado (ex: neutro 1515)");
            Serial.println("  - guincho sobe / desce / parar : Controlo contínuo do guincho (GPIO 19)");
            Serial.println("  - ancora < 0 a 180 >    : Define ângulo do trinco da âncora (GPIO 23)");
            Serial.println("  - largar / soltar / 90  : Levanta o trinco da âncora (90°)");
            Serial.println("  - travar / 0            : Trava o trinco da âncora (0°)");
            Serial.println("  - relax / desligar      : Desativa o pulso PWM (evita aquecimento)");
            Serial.println("  - status                : Mostra o estado de todos os atuadores");
        } else {
            int val = cmd.toInt();
            if (val > 0 && val <= 180) {
                actuators.setRackAngle(val);
            } else {
                Serial.println("[CLI] Comando não reconhecido. Escreva 'help' para ver lista.");
            }
        }
    }
}

void setup() {
    Serial.begin(115200);
    Serial.setTxTimeoutMs(0); // Non-blocking USB CDC TX (prevents 2000ms stalls)
    delay(200);
    Serial.printf("\n=== WINDDRAGONS ROVER-%02d BOOTING ===\n", ROVER_ID);

    // De-assert all SPI Chip Selects before bus init
    pinMode(PIN_LCD_CS, OUTPUT);
    digitalWrite(PIN_LCD_CS, HIGH);
    pinMode(PIN_LORA_NSS, OUTPUT);
    digitalWrite(PIN_LORA_NSS, HIGH);
    pinMode(PIN_SD_CS, OUTPUT);
    digitalWrite(PIN_SD_CS, HIGH); // MicroSD CS

    // 1. Initialize Display ST7789 1.47" and show Boot Screen IMMEDIATELY!
    roverDisplay.begin();
    roverDisplay.showBootScreen(ROVER_ID);
    roverDisplay.bootLogf(UI_CYAN, 10, "[BOOT] WindDragons OS v2.0");

    // 2. Status RGB NeoPixel LED
    rgbLed.begin();
    setRgbColor(20, 20, 0); // Yellow: Booting
    roverDisplay.bootLogf(UI_LIGHTGREY, 20, "[LED ] NeoPixel RGB ativo");

    // 3. Initialize Battery Monitor (APM 28V 90A Module for LiPo 4S)
    battery.begin();
    battery.update();
    Serial.println("[ROVER] APM Battery Monitor initialized (ADC Pins V=0, I=3)");
    roverDisplay.bootLogf(UI_GREEN, 30, "[PWR ] LiPo 4S: %.2fV (%u%%)", 
                          battery.getVoltageMv() / 1000.0f, battery.getPercentage());

    // 4. Initialize Actuators (ESC, Leme, Guincho, Cremalheira, Alarme)
    actuators.begin();
    Serial.println("[ROVER] Actuators initialized (ESC + 3 Servos: Leme, Guincho, Cremalheira)");
    roverDisplay.bootLogf(UI_LIGHTGREY, 40, "[ACT ] ESC + 3 Servos calib.");

    // 5. Initialize GPS
    gps.begin();
    Serial.println("[ROVER] Quectel LC29H GPS UART initialized");
    roverDisplay.bootLogf(UI_LIGHTGREY, 50, "[GPS ] Quectel LC29H pronto");

    // 6. Initialize MicroSD Card
    Serial.print("[ROVER] A inicializar cartao MicroSD... ");
    if (sdCard.begin()) {
        Serial.println("OK!");
        roverDisplay.bootLogf(UI_GREEN, 60, "[SD  ] MicroSD FAT32 pronto");
        sdCard.printCardInfo();
        sdCard.printDirectory("/", 1);

        if (sdCard.fileExists("/waypoints.txt")) {
            Serial.println("[ROVER] Encontrado /waypoints.txt no cartao SD:");
            Serial.println(sdCard.readFile("/waypoints.txt"));
            roverDisplay.bootLogf(UI_CYAN, 65, "[SD  ] /waypoints.txt lido");
        }

        if (!sdCard.fileExists("/telemetry.csv")) {
            sdCard.writeFile("/telemetry.csv", "timestamp_ms,lat,lng,speed_kn,heading_deg,batt_pct,batt_mv,motor_status\n");
            roverDisplay.bootLogf(UI_LIGHTGREY, 68, "[SD  ] /telemetry.csv criado");
        }
    } else {
        Serial.println("Nenhum cartao detetado (ou formato nao suportado).");
        roverDisplay.bootLogf(UI_YELLOW, 60, "[SD  ] Sem cartao presente");
    }

    // 7. Carregar configurações principais (/config.json no SD ou NVS)
    roverConfig.begin(&sdCard);
    roverDisplay.setBrightness(roverConfig.getScreenBrightness());
    webServer.setRoverId(roverConfig.getRoverId());
    Serial.printf("[ROVER] Configuracao Ativa: ID=%u | SSID AP=%s | Brilho=%u%%\n", 
                  roverConfig.getRoverId(), roverConfig.getApSsid().c_str(), roverConfig.getScreenBrightness());
    roverDisplay.bootLogf(UI_CYAN, 72, "[CFG ] ID=%u | Brilho=%u%%", 
                          roverConfig.getRoverId(), roverConfig.getScreenBrightness());

#if ENABLE_LORA
    Serial.print("[ROVER] Initializing LoRa SX1278 (433MHz)... ");
    loraSPI.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_NSS);
    int state = radio.begin(LORA_FREQUENCY, LORA_BANDWIDTH, LORA_SPREAD_FACTOR, 
                            LORA_CODING_RATE, LORA_SYNC_WORD, LORA_OUTPUT_POWER, 
                            LORA_PREAMBLE_LEN);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("OK!");
        loraReady = true;
        setRgbColor(0, 30, 0); // Green
        radio.startReceive();
        roverDisplay.bootLogf(UI_GREEN, 78, "[LORA] SX1278 433MHz pronto");
    } else {
        Serial.printf("FAILED! (code: %d)\n", state);
        loraReady = false;
        loraSPI.end();
        setRgbColor(50, 0, 0); // Red
        roverDisplay.bootLogf(UI_RED, 78, "[LORA] Falha SX1278 (%d)", state);
    }
#else
    Serial.println("[ROVER] LoRa em repouso (barramento SPI dedicado ao Display ST7789 e MicroSD)");
    loraReady = false;
    roverDisplay.bootLogf(UI_LIGHTGREY, 78, "[LORA] SX1278 em repouso");
#endif

    // 8. Initialize WiFi with Auto-Connect / Fallback to AP Mode
    wifiConfig.begin(&sdCard);
    Serial.println("[ROVER] A ligar WiFi (STA)...");
    roverDisplay.bootLogf(UI_YELLOW, 82, "[NET ] A ligar WiFi (STA)...");

    bool wifiOk = wifiConfig.autoConnect(8000, [](const char* ssid) {
        roverDisplay.bootLogf(UI_YELLOW, 86, "[NET ] A testar '%s'...", ssid);
    });

    if (wifiOk) {
        roverDisplay.bootLogf(UI_GREEN, 92, "[NET ] IP: %s", wifiConfig.getIPAddress().c_str());
    } else {
        wifiConfig.startAccessPoint(roverConfig.getApSsid().c_str(), roverConfig.getApPassword().c_str());
        roverDisplay.bootLogf(UI_YELLOW, 88, "[NET ] Falhou STA -> Iniciar AP");
        roverDisplay.bootLogf(UI_CYAN, 92, "[NET ] AP: %s", roverConfig.getApSsid().c_str());
        roverDisplay.bootLogf(UI_WHITE, 94, "[NET ] IP: 192.168.4.1");
    }

    // 9. Initialize HTTP Web Server (port 80)
    webServer.begin();
    roverDisplay.bootLogf(UI_GREEN, 97, "[HTTP] Servidor Web porta 80");

    // Configure BOOT button interrupt for runtime page cycling
    pinMode(PIN_BTN_BOOT, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_BTN_BOOT), isrBootButton, FALLING);
    flagPageChange = false; // ensure clean start on Page 0

    // Finalize boot screen with pause so user can see complete boot log
    roverDisplay.endBootScreen(1600);
}

void loop() {
    uint32_t now = millis();

    // 0. Handle HTTP Web Server requests
    webServer.handleClient();

    // 0.1 Process Serial CLI debugging commands
    handleSerialCommands();

    // 1. Screen Navigation Button (Instant Hardware Interrupt)
    bool forceDisplay = false;
    if (flagPageChange) {
        flagPageChange = false;
        roverDisplay.nextPage();
        forceDisplay = true;
        bool failsafeActive = (currentMotorStatus == MOTOR_FAILSAFE);
        int16_t currentRssi = 0;
#if ENABLE_LORA
        if (loraReady) currentRssi = static_cast<int16_t>(radio.getRSSI());
#endif
        roverDisplay.update(roverConfig.getRoverId(), gps, battery, actuators, currentMotorStatus, 
                            currentRssi, failsafeActive, true, sdCard.isReady(),
                            wifiConfig.isAPMode(), wifiConfig.getIPAddress().c_str(),
                            wifiConfig.getSSID().c_str(), wifiConfig.getRSSI(),
                            wifiConfig.getAPStationCount(),
                            wifiConfig.getAPPassword().c_str());
        Serial.printf("[ROVER] Display page switched to: %d (1:NAV, 2:BATT, 3:ACT, 4:NET)\n", roverDisplay.getCurrentPage() + 1);
    }

    // 2. Process GPS
    gps.update();

    // 3. Update Actuators (winch timing, alarm timing/strobe)
    actuators.update();

    // 4. Autonomous Navigation / Station Keeping updates (10Hz)
    static uint32_t lastNavUpdate = 0;
    if (now - lastNavUpdate >= 100) {
        lastNavUpdate = now;
        updateAutonomousNavigation();
    }

    // 5. Check for received LoRa packets
#if ENABLE_LORA
    if (loraReady) {
        int packetLen = radio.getPacketLength();
        if (packetLen > 0) {
            uint8_t buffer[64];
            int readState = radio.readData(buffer, packetLen);
            if (readState == RADIOLIB_ERR_NONE) {
                processIncomingPacket(buffer, packetLen);
            }
            radio.startReceive();
        }
    }
#endif

#if ENABLE_LORA
    // 6. Failsafe Watchdog: In manual mode, stop if signal lost
    if (currentMotorStatus == MOTOR_MANUAL && (now - lastControlReceivedTime > FAILSAFE_TIMEOUT_MS)) {
        actuators.stopMotor();
        actuators.centerRudder();
        actuators.stopWinch();
        currentMotorStatus = MOTOR_FAILSAFE;
        setRgbColor(40, 10, 0); // Orange/Red warning
        Serial.println("[ROVER] FAILSAFE TRIGGERED! Signal lost -> Motor stopped.");
    }
#endif

    // 7. Handle Visual LED Status / Strobe Alarm
    if (actuators.isAlarmActive()) {
        if (actuators.getAlarmStrobeState()) {
            setRgbColor(255, 255, 255); // High-intensity white strobe
        } else {
            setRgbColor(0, 0, 0);
        }
    }

    // 8. Send Telemetry periodically (every 1000ms)
    if (now - lastTelemetryTime >= 1000) {
        lastTelemetryTime = now;
        sendTelemetry();

        // Restore LED color according to operating state when alarm is not active
        if (!actuators.isAlarmActive()) {
            if (currentMotorStatus == MOTOR_HOLDING_STATION || currentMotorStatus == MOTOR_MOVING_TO_TARGET || currentMotorStatus == MOTOR_RTL) {
                setRgbColor(0, 20, 25); // Cyan: Autonomous / Station keeping
            } else if (gps.hasFix()) {
                setRgbColor(0, 25, 0);  // Green: GPS locked, ready
            } else {
                setRgbColor(25, 25, 0); // Yellow: Searching satellites
            }
        }
    }

    // 9. Log telemetry to SD card periodically (every 5000ms if SD card is ready and not busy)
    static uint32_t lastSdLogTime = 0;
    if (!webServer.isBusy() && sdCard.isReady() && (now - lastSdLogTime >= 5000)) {
        lastSdLogTime = now;
        char logBuf[128];
        snprintf(logBuf, sizeof(logBuf), "%lu,%.6f,%.6f,%.1f,%.1f,%u,%u,%u\n",
                 now, gps.getLatitude(), gps.getLongitude(), gps.getSpeedKnots(),
                 gps.getHeading(), battery.getPercentage(), battery.getVoltageMv(),
                 currentMotorStatus);
        sdCard.appendFile("/telemetry.csv", logBuf);
    }

    // 10. Update Rover Onboard LCD Display (Instant if forced, or 5Hz periodically - paused during SD transfers)
    if (!webServer.isBusy()) {
        bool failsafeActive = (currentMotorStatus == MOTOR_FAILSAFE);
        int16_t currentRssi = 0;
#if ENABLE_LORA
        if (loraReady) currentRssi = static_cast<int16_t>(radio.getRSSI());
#endif
        roverDisplay.update(roverConfig.getRoverId(), gps, battery, actuators, currentMotorStatus, 
                            currentRssi, failsafeActive, forceDisplay, sdCard.isReady(),
                            wifiConfig.isAPMode(), wifiConfig.getIPAddress().c_str(),
                            wifiConfig.getSSID().c_str(), wifiConfig.getRSSI(),
                            wifiConfig.getAPStationCount(),
                            wifiConfig.getAPPassword().c_str());
    }
}
