#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include <Adafruit_NeoPixel.h>

#include <Preferences.h>

#include "config_common.h"
#include "lora_protocol.h"
#include "gps_tracker.h"
#include "actuators.h"
#include "battery_monitor.h"

// =========================================================================
// WINDDRAGONS ROVER - C6 COPROCESSOR FIRMWARE
// Placa: ESP32-C6-DevKitC-1
// Responsabilidade:
//  - LoRa SX1278 (Ra-02 433MHz em barramento FSPI de hardware)
//  - Power Module (Bateria LiPo 6S nos pinos analógicos ADC)
//  - GPS Quectel LC29H (UART Hardware nos pinos 16/17)
//  - Atuadores & Servos (ESC=21, Leme=20, Guincho=19, Rack=18)
//  - Ponte UART de alta velocidade com o Lafvin Master (TX=13, RX=22)
// =========================================================================

// Configuração NVS para o Rover ID
Preferences c6Prefs;
uint8_t roverId = 1;

// Periféricos
GPSTracker gps;
ActuatorController actuators;
BatteryMonitor battery;
Adafruit_NeoPixel rgbLed(1, PIN_RGB_LED, NEO_GRB + NEO_KHZ800);

// Hardware SPI FSPI para LoRa SX1278
SPIClass loraSpiBus(FSPI);
SX1278 radio = new Module(PIN_C6_LORA_NSS, PIN_C6_LORA_DIO0, PIN_C6_LORA_RST, RADIOLIB_NC, loraSpiBus);
bool loraReady = false;
float lastRssi = 0.0f;
float lastSnr = 0.0f;

// UART Inter-ESP (DevKitC -> Lafvin)
HardwareSerial interSerial(1);

// Estado de controlo e navegação
uint16_t telemetrySeq = 0;
uint32_t lastTelemetryTime = 0;
uint32_t lastControlReceivedTime = 0;
const uint32_t FAILSAFE_TIMEOUT_MS = 2500;
uint8_t currentMotorStatus = MOTOR_IDLE;
uint16_t lastExecutedCommandId = 0;

// Coordenadas autónomas
double homeLat = 0.0;
double homeLng = 0.0;
bool hasHomeSet = false;
double stationTargetLat = 0.0;
double stationTargetLng = 0.0;
double navTargetLat = 0.0;
double navTargetLng = 0.0;
const float STATION_KEEPING_TOLERANCE_M = 8.0f;
const float WAYPOINT_ARRIVAL_RADIUS_M    = 4.0f;

void printLog(const char* format, ...) {
    char buf[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    Serial.print(buf);
    Serial0.print(buf);
}

void setRgbColor(uint8_t r, uint8_t g, uint8_t b) {
    rgbLed.setPixelColor(0, rgbLed.Color(r, g, b));
    rgbLed.show();
}

void sendLoRaTelemetry() {
    if (!loraReady) return;

    PacketTelemetry packet;
    memset(&packet, 0, sizeof(packet));

    packet.magic          = LORA_MAGIC_BYTE;
    packet.msg_type       = MSG_TELEMETRY;
    packet.rover_id       = roverId;
    packet.seq_num        = telemetrySeq++;
    
    packet.lat_deg7       = gps.getLatDeg7();
    packet.lng_deg7       = gps.getLngDeg7();
    packet.speed_cm_s     = static_cast<uint16_t>(gps.getSpeedKnots() * 51.4444f);
    packet.heading_deg10  = static_cast<uint16_t>(gps.getHeading() * 10.0f);
    packet.satellites     = gps.getSatellites();
    packet.fix_quality    = gps.getFixQuality();
    
    packet.battery_mv     = battery.getVoltageMv();
    packet.battery_pct    = battery.getPercentage();
    packet.motor_status   = currentMotorStatus;
    packet.anchor_status  = actuators.getAnchorStatus();
    packet.anchor_depth_cm= actuators.getAnchorDepthCm();
    packet.last_cmd_id    = lastExecutedCommandId;

    radio.transmit(reinterpret_cast<uint8_t*>(&packet), sizeof(packet));
    radio.startReceive();
}

void processIncomingLoRa(uint8_t* buffer, int len) {
    if (len < 4) return;
    if (buffer[0] != LORA_MAGIC_BYTE) return;

    uint8_t msgType = buffer[1];
    uint8_t targetRoverId = buffer[2];
    if (targetRoverId != 0xFF && targetRoverId != roverId) return;

    lastRssi = radio.getRSSI();
    lastSnr  = radio.getSNR();

    if (msgType == MSG_MANUAL_CONTROL && len >= (int)sizeof(PacketManualControl)) {
        PacketManualControl* ctrl = reinterpret_cast<PacketManualControl*>(buffer);
        actuators.setThrottle(ctrl->throttle);
        actuators.setRudder(ctrl->rudder);
        if (ctrl->anchor_jog != 0) actuators.jogWinch(ctrl->anchor_jog);
        currentMotorStatus = MOTOR_MANUAL;
        lastControlReceivedTime = millis();
        setRgbColor(0, 50, 0); // Green: Control active
    } 
    else if (msgType == MSG_ACTION_COMMAND && len >= (int)sizeof(PacketActionCommand)) {
        PacketActionCommand* cmd = reinterpret_cast<PacketActionCommand*>(buffer);
        lastExecutedCommandId = cmd->command_id;

        switch (cmd->action) {
            case ACTION_DROP_ANCHOR:
                actuators.commandDropAnchor(cmd->target_depth_cm);
                currentMotorStatus = MOTOR_ANCHORED;
                break;
            case ACTION_RETRIEVE_ANCHOR:
                actuators.commandRetractAnchor();
                currentMotorStatus = MOTOR_IDLE;
                break;
            case ACTION_STOP_ANCHOR:
                actuators.stopWinch();
                break;
            case ACTION_NAVIGATE:
                if (cmd->target_lat_deg7 != 0 && cmd->target_lng_deg7 != 0) {
                    navTargetLat = cmd->target_lat_deg7 / 10000000.0;
                    navTargetLng = cmd->target_lng_deg7 / 10000000.0;
                    currentMotorStatus = MOTOR_MOVING_TO_TARGET;
                }
                break;
            case ACTION_RTL:
                if (hasHomeSet) {
                    navTargetLat = homeLat;
                    navTargetLng = homeLng;
                    currentMotorStatus = MOTOR_RTL;
                }
                break;
            case ACTION_ALARM:
                actuators.triggerAlarm(cmd->duration_s > 0 ? cmd->duration_s : 10);
                break;
            case ACTION_PING:
                sendLoRaTelemetry();
                break;
            default:
                break;
        }
    }
}

void updateAutonomousNavigation() {
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

    if (!hasHomeSet) {
        homeLat = gps.getLatitude();
        homeLng = gps.getLongitude();
        hasHomeSet = true;
    }

    if (currentMotorStatus == MOTOR_HOLDING_STATION) {
        double distM = gps.distanceTo(stationTargetLat, stationTargetLng);
        if (distM > STATION_KEEPING_TOLERANCE_M) {
            double targetCourse = gps.courseTo(stationTargetLat, stationTargetLng);
            double headingError = targetCourse - gps.getHeading();
            while (headingError > 180.0) headingError -= 360.0;
            while (headingError < -180.0) headingError += 360.0;

            int8_t rudder = constrain(static_cast<int>(headingError * 1.5f), -100, 100);
            actuators.setRudder(rudder);
            int8_t throttle = (abs(headingError) > 50.0) ? 20 : 35;
            actuators.setThrottle(throttle);
        } else {
            actuators.stopMotor();
            actuators.centerRudder();
        }
    } else if (currentMotorStatus == MOTOR_MOVING_TO_TARGET || currentMotorStatus == MOTOR_RTL) {
        double distM = gps.distanceTo(navTargetLat, navTargetLng);
        if (distM <= WAYPOINT_ARRIVAL_RADIUS_M) {
            actuators.stopMotor();
            actuators.centerRudder();
            if (currentMotorStatus == MOTOR_MOVING_TO_TARGET) {
                stationTargetLat = navTargetLat;
                stationTargetLng = navTargetLng;
                currentMotorStatus = MOTOR_HOLDING_STATION;
            } else {
                currentMotorStatus = MOTOR_IDLE;
            }
        } else {
            double targetCourse = gps.courseTo(navTargetLat, navTargetLng);
            double headingError = targetCourse - gps.getHeading();
            while (headingError > 180.0) headingError -= 360.0;
            while (headingError < -180.0) headingError += 360.0;

            int8_t rudder = constrain(static_cast<int>(headingError * 1.8f), -100, 100);
            actuators.setRudder(rudder);
            int8_t throttle = (abs(headingError) > 60.0) ? 25 : 50;
            actuators.setThrottle(throttle);
        }
    }
}

void setup() {
    Serial.begin(115200);
    Serial0.begin(115200);
    Serial.setTxTimeoutMs(0);
    delay(500);

    // Carregar ID e Bateria da memória NVS
    c6Prefs.begin("rover_c6", false);
    roverId = c6Prefs.getUChar("id", 1);
    uint8_t c6Cells = c6Prefs.getUChar("cells", 4);
    if (c6Cells == 3 || c6Cells == 4 || c6Cells == 6) {
        battery.setCellCount(c6Cells);
    }

    printLog("\n=======================================================\n");
    printLog("  WINDDRAGONS ROVER-%02d: C6 COPROCESSOR (LORA+NAV+ACT)\n", roverId);
    printLog("  Identificador NVS ativo: ROVER-%02d | Bateria: %uS\n", roverId, battery.getCellCount());
    printLog("=======================================================\n");

    // 1. LED RGB NeoPixel
    rgbLed.begin();
    setRgbColor(30, 30, 0); // Yellow: Booting

    // 2. Inter-ESP UART
    interSerial.setRxBufferSize(512);
    interSerial.begin(INTER_ESP_BAUDRATE, SERIAL_8N1, PIN_INTER_ESP_C6_RX, PIN_INTER_ESP_C6_TX);
    Serial.printf("[UART] Canal serie com Lafvin iniciado nos pinos TX=%d, RX=%d (%d baud)\n",
                  PIN_INTER_ESP_C6_TX, PIN_INTER_ESP_C6_RX, INTER_ESP_BAUDRATE);

    // 3. Monitor de Bateria LiPo (3S, 4S ou 6S)
    battery.beginDirectAdc();
    Serial.printf("[BATT] Monitor %uS LiPo ativo nos pinos VOLT=GPIO%d, CURR=GPIO%d\n",
                  battery.getCellCount(), PIN_C6_ADC_VOLT, PIN_C6_ADC_CURR);

    // 4. GPS Quectel LC29H (em bancada desligado para não colidir com UART0 de debug)
    gps.begin(false);
    Serial.println("[GPS ] GPS Quectel preparado (modo bancada/simulado)");

    // 5. Atuadores (Direct PWM / PCA9685)
    actuators.begin();
    Serial.println("[ACT ] Atuadores inicializados (ESC=21, Leme=20, Guincho=19, Rack=18)");

    // 6. LoRa SX1278 (FSPI Hardware)
    pinMode(PIN_C6_LORA_NSS, OUTPUT);
    digitalWrite(PIN_C6_LORA_NSS, HIGH);
    pinMode(PIN_C6_LORA_DIO0, INPUT);

    loraSpiBus.begin(PIN_C6_LORA_SCK, PIN_C6_LORA_MISO, PIN_C6_LORA_MOSI, PIN_C6_LORA_NSS);
    delay(50);

    Serial.println("[LORA] A inicializar RadioLib SX1278 (433.0 MHz)...");
    int state = radio.begin(LORA_FREQUENCY, LORA_BANDWIDTH, LORA_SPREAD_FACTOR,
                            LORA_CODING_RATE, LORA_SYNC_WORD, LORA_OUTPUT_POWER,
                            LORA_PREAMBLE_LEN, 0);
    if (state == RADIOLIB_ERR_NONE) {
        loraReady = true;
        radio.startReceive();
        Serial.println("[LORA] ★★★ SX1278 PRONTO E EM ESCUTA RX! ★★★");
        setRgbColor(0, 40, 0); // Green
    } else {
        loraReady = false;
        Serial.printf("[LORA] Falha na inicialização (código: %d)\n", state);
        setRgbColor(40, 0, 0); // Red
    }
}

void loop() {
    uint32_t now = millis();

    // 1. Atualizar sensores
    gps.update();
    battery.update();
    actuators.update();

    // 2. Navegação autónoma (10 Hz)
    static uint32_t lastNavUpdate = 0;
    if (now - lastNavUpdate >= 100) {
        lastNavUpdate = now;
        updateAutonomousNavigation();
    }

    // 3. Receção LoRa SX1278
    if (loraReady && digitalRead(PIN_C6_LORA_DIO0) == HIGH) {
        uint8_t buffer[64];
        int len = radio.getPacketLength();
        if (len > 0) {
            int state = radio.readData(buffer, len);
            if (state == RADIOLIB_ERR_NONE) {
                processIncomingLoRa(buffer, len);
            }
        }
        radio.startReceive();
    }

    // 4. Failsafe Watchdog (Perda de sinal em modo manual)
    if (currentMotorStatus == MOTOR_MANUAL && (now - lastControlReceivedTime > FAILSAFE_TIMEOUT_MS)) {
        actuators.stopMotor();
        actuators.centerRudder();
        actuators.stopWinch();
        currentMotorStatus = MOTOR_FAILSAFE;
        setRgbColor(40, 10, 0);
    }

    // 5. Envio periódico de telemetria LoRa para a Base (a cada 1000ms)
    if (now - lastTelemetryTime >= 1000) {
        lastTelemetryTime = now;
        sendLoRaTelemetry();
        printLog("[C6 ROVER] >>> LoRa Telemetria #%u enviada pelo ar (433MHz) | Bat=%.2fV (%u%%)\n",
                 telemetrySeq, battery.getVoltageMv() / 1000.0f, battery.getPercentage());
    }

    // 6. Comunicação UART com o Lafvin Master
    while (interSerial.available()) {
        char c = (char)interSerial.read();
        static char rxBuf[128];
        static size_t rxIdx = 0;

        if (c == '\n' || c == '\r') {
            if (rxIdx > 0) {
                rxBuf[rxIdx] = '\0';
                
                if (strncmp(rxBuf, "PING:", 5) == 0) {
                    uint32_t seq = 0;
                    uint32_t incomingId = 0;
                    char cmdStr[64] = "NONE";
                    int parsed = sscanf(rxBuf + 5, "%u:%u:%63s", &seq, &incomingId, cmdStr);
                    if (parsed >= 2 && incomingId > 0 && incomingId != roverId) {
                        roverId = (uint8_t)incomingId;
                        c6Prefs.putUChar("id", roverId);
                        printLog("[C6 ROVER] ★ ID sincronizado com Lafvin e gravado em NVS: %u\n", roverId);
                    }

                    // Se vier um comando da interface Web do Lafvin:
                    if (strncmp(cmdStr, "CMD:LEME:", 9) == 0) {
                        int val = atoi(cmdStr + 9);
                        actuators.setRudder(val);
                    } else if (strncmp(cmdStr, "CMD:MOTOR:", 10) == 0) {
                        int val = atoi(cmdStr + 10);
                        actuators.setThrottle(val);
                        lastControlReceivedTime = now;
                    } else if (strcmp(cmdStr, "CMD:DROP_ANCHOR") == 0) {
                        actuators.commandDropAnchor(300);
                        currentMotorStatus = MOTOR_ANCHORED;
                    } else if (strcmp(cmdStr, "CMD:RETRACT_ANCHOR") == 0) {
                        actuators.commandRetractAnchor();
                        currentMotorStatus = MOTOR_IDLE;
                    } else if (strcmp(cmdStr, "CMD:STOP_ANCHOR") == 0) {
                        actuators.stopWinch();
                    } else if (strncmp(cmdStr, "CMD:BATT:", 9) == 0) {
                        int cells = atoi(cmdStr + 9);
                        if (cells == 3 || cells == 4 || cells == 6) {
                            battery.setCellCount((uint8_t)cells);
                            c6Prefs.putUChar("cells", (uint8_t)cells);
                            printLog("[C6 ROVER] ★ Bateria configurada para %uS e gravada em NVS\n", cells);
                        }
                    }

                    // Resposta rica de telemetria para o Lafvin Master:
                    // PONG:<seq>:<batMv>:<batA>:<batPct>:<lat7>:<lng7>:<spd>:<head>:<sats>:<fix>:<motSt>:<rud>:<thr>:<winch>:<rack>:<depth>:<loraSt>:<rssi>:<snr>
                    char pong[256];
                    snprintf(pong, sizeof(pong),
                             "PONG:%u:%u:%.2f:%u:%ld:%ld:%.1f:%.1f:%u:%u:%u:%d:%d:%d:%d:%u:%s:%.1f:%.1f\n",
                             seq,
                             battery.getVoltageMv(),
                             battery.getCurrentAmps(),
                             battery.getPercentage(),
                             (long)gps.getLatDeg7(),
                             (long)gps.getLngDeg7(),
                             gps.getSpeedKnots(),
                             gps.getHeading(),
                             gps.getSatellites(),
                             gps.getFixQuality(),
                             currentMotorStatus,
                             actuators.getRudder(),
                             actuators.getThrottle(),
                             actuators.getWinchPulseUs(),
                             actuators.isRackReleased() ? 1 : 0,
                             actuators.getAnchorDepthCm(),
                             loraReady ? "RF_OK" : "RF_ERR",
                             lastRssi,
                             lastSnr);
                    interSerial.print(pong);
                    printLog("[C6 ROVER] <<< %s | >>> PONG: Bat=%.2fV (%d%%) LoRa=%s\n",
                             rxBuf, battery.getVoltageMv() / 1000.0f, battery.getPercentage(), loraReady ? "RF_OK" : "RF_ERR");
                }
                rxIdx = 0;
            }
        } else {
            if (rxIdx < sizeof(rxBuf) - 1) rxBuf[rxIdx++] = c;
        }
    }

    // 7. Consola de comandos Serial (ex: SET_ID 2)
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd.startsWith("SET_ID ") || cmd.startsWith("ID ")) {
            int newId = cmd.substring(cmd.indexOf(' ') + 1).toInt();
            if (newId >= 1 && newId <= 254) {
                roverId = (uint8_t)newId;
                c6Prefs.putUChar("id", roverId);
                printLog("[C6] ★ ID guardado em NVS: %d\n", newId);
            }
        }
    }
}
