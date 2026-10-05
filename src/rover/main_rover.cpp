#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include <Adafruit_NeoPixel.h>

#include "config_common.h"
#include "lora_protocol.h"
#include "gps_tracker.h"
#include "actuators.h"

// Define this Rover's ID (e.g. 1 for ROVER-01, 2 for ROVER-02)
#ifndef ROVER_ID
#define ROVER_ID 1
#endif

// Peripherals
GPSTracker gps;
ActuatorController actuators;
Adafruit_NeoPixel rgbLed(1, PIN_RGB_LED, NEO_GRB + NEO_KHZ800);

// Hardware SPI for SX1278 Ra-02
SPIClass spiLoRa(FSPI);
// Module(CS, DIO0, RST, DIO1)
Module loraModule(PIN_LORA_NSS, PIN_LORA_DIO0, PIN_LORA_RST, RADIOLIB_NC, spiLoRa);
SX1278 radio(&loraModule);

// State tracking
uint16_t telemetrySeq = 0;
uint32_t lastTelemetryTime = 0;
uint32_t lastControlReceivedTime = 0;
const uint32_t FAILSAFE_TIMEOUT_MS = 2500;
uint8_t currentMotorStatus = MOTOR_IDLE;
uint16_t lastExecutedCommandId = 0;

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

void setRgbColor(uint8_t r, uint8_t g, uint8_t b) {
    rgbLed.setPixelColor(0, rgbLed.Color(r, g, b));
    rgbLed.show();
}

void sendTelemetry() {
    PacketTelemetry packet;
    memset(&packet, 0, sizeof(packet));

    packet.magic          = LORA_MAGIC_BYTE;
    packet.msg_type       = MSG_TELEMETRY;
    packet.rover_id       = ROVER_ID;
    packet.seq_num        = telemetrySeq++;
    
    packet.lat_deg7       = gps.getLatDeg7();
    packet.lng_deg7       = gps.getLngDeg7();
    packet.speed_cm_s     = static_cast<uint16_t>(gps.getSpeedKnots() * 51.4444f); // knots to cm/s
    packet.heading_deg10  = static_cast<uint16_t>(gps.getHeading() * 10.0f);
    packet.satellites     = gps.getSatellites();
    packet.fix_quality    = gps.getFixQuality();

    // Simulated 3S LiPo voltage (e.g. 12.4V) or from analog divider if implemented
    packet.battery_mv     = 12400; 
    packet.battery_pct    = 90;

    packet.motor_status   = currentMotorStatus;
    packet.anchor_status  = actuators.getAnchorStatus();
    packet.anchor_depth_cm= actuators.getAnchorDepthCm();
    packet.last_cmd_id    = lastExecutedCommandId;

    packet.checksum = calculate_checksum(reinterpret_cast<const uint8_t*>(&packet), 
                                         sizeof(packet) - sizeof(packet.checksum));

    // Transmit telemetry
    radio.standby();
    radio.transmit(reinterpret_cast<uint8_t*>(&packet), sizeof(packet));
    radio.startReceive();
}

void processIncomingPacket(uint8_t *buffer, size_t length) {
    if (length < 2 || buffer[0] != LORA_MAGIC_BYTE) {
        return; // Invalid packet
    }

    uint8_t msgType = buffer[1];

    if (msgType == MSG_MANUAL_CONTROL && length == sizeof(PacketManualControl)) {
        PacketManualControl *cmd = reinterpret_cast<PacketManualControl*>(buffer);
        if (cmd->target_rover_id == ROVER_ID || cmd->target_rover_id == BROADCAST_ROVER_ID) {
            uint16_t calc = calculate_checksum(buffer, sizeof(PacketManualControl) - sizeof(cmd->checksum));
            if (calc == cmd->checksum) {
                lastControlReceivedTime = millis();
                currentMotorStatus = MOTOR_MANUAL;

                actuators.setThrottle(cmd->throttle);
                actuators.setRudder(cmd->rudder);
                actuators.jogWinch(cmd->anchor_jog);

                // LED flash Blue
                setRgbColor(0, 0, 30);
            }
        }
    } 
    else if (msgType == MSG_ACTION_COMMAND && length == sizeof(PacketActionCommand)) {
        PacketActionCommand *cmd = reinterpret_cast<PacketActionCommand*>(buffer);
        if (cmd->target_rover_id == ROVER_ID || cmd->target_rover_id == BROADCAST_ROVER_ID) {
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

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.printf("\n=== WINDDRAGONS ROVER-%02d BOOTING ===\n", ROVER_ID);

    rgbLed.begin();
    setRgbColor(20, 20, 0); // Yellow: Booting

    // Initialize Actuators (ESC, Leme, Guincho, Cremalheira, Alarme)
    actuators.begin();
    Serial.println("[ROVER] Actuators initialized (ESC + 3 Servos: Leme, Guincho, Cremalheira)");

    // Initialize GPS
    gps.begin();
    Serial.println("[ROVER] Quectel LC29H GPS UART initialized");

    // Initialize SPI and LoRa
    spiLoRa.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_NSS);
    
    Serial.print("[ROVER] Initializing LoRa SX1278 (433MHz)... ");
    int state = radio.begin(LORA_FREQUENCY, LORA_BANDWIDTH, LORA_SPREAD_FACTOR, 
                            LORA_CODING_RATE, LORA_SYNC_WORD, LORA_OUTPUT_POWER, 
                            LORA_PREAMBLE_LEN);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("OK!");
        setRgbColor(0, 30, 0); // Green
    } else {
        Serial.printf("FAILED! (code: %d)\n", state);
        setRgbColor(50, 0, 0); // Red
    }

    // Set to continuous receive mode
    radio.startReceive();
}

void loop() {
    uint32_t now = millis();

    // 1. Process GPS
    gps.update();

    // 2. Update Actuators (winch timing, alarm timing/strobe)
    actuators.update();

    // 3. Autonomous Navigation / Station Keeping updates (10Hz)
    static uint32_t lastNavUpdate = 0;
    if (now - lastNavUpdate >= 100) {
        lastNavUpdate = now;
        updateAutonomousNavigation();
    }

    // 4. Check for received LoRa packets
    int packetLen = radio.getPacketLength();
    if (packetLen > 0) {
        uint8_t buffer[64];
        int readState = radio.readData(buffer, packetLen);
        if (readState == RADIOLIB_ERR_NONE) {
            processIncomingPacket(buffer, packetLen);
        }
        radio.startReceive();
    }

    // 5. Failsafe Watchdog: In manual mode, stop if signal lost
    if (currentMotorStatus == MOTOR_MANUAL && (now - lastControlReceivedTime > FAILSAFE_TIMEOUT_MS)) {
        actuators.stopMotor();
        actuators.centerRudder();
        actuators.stopWinch();
        currentMotorStatus = MOTOR_FAILSAFE;
        setRgbColor(40, 10, 0); // Orange/Red warning
        Serial.println("[ROVER] FAILSAFE TRIGGERED! Signal lost -> Motor stopped.");
    }

    // 6. Handle Visual LED Status / Strobe Alarm
    if (actuators.isAlarmActive()) {
        if (actuators.getAlarmStrobeState()) {
            setRgbColor(255, 255, 255); // High-intensity white strobe
        } else {
            setRgbColor(0, 0, 0);
        }
    }

    // 7. Send Telemetry periodically (every 1000ms)
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
}
