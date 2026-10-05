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

void setRgbColor(uint8_t r, uint8_t g, uint8_t b) {
    rgbLed.setPixelColor(0, rgbLed.Color(r, g, b));
    rgbLed.show();
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.printf("\n=== WINDDRAGONS ROVER-%02d BOOTING ===\n", ROVER_ID);

    rgbLed.begin();
    setRgbColor(20, 20, 0); // Yellow: Booting

    // Initialize Actuators (ESC, Rudder, Winch)
    actuators.begin();
    Serial.println("[ROVER] Actuators initialized (ESC, Rudder, Winch)");

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
    int txState = radio.transmit(reinterpret_cast<uint8_t*>(&packet), sizeof(packet));
    if (txState == RADIOLIB_ERR_NONE) {
        // Serial.printf("[ROVER] Telemetry #%u sent | Lat: %d, Lng: %d\n", packet.seq_num, packet.lat_deg7, packet.lng_deg7);
    }
    // Return to receiving
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
            // Checksum verification
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
                Serial.printf("[ROVER] Received Action Command #%u (Action: %u, Depth: %u cm)\n", 
                              cmd->command_id, cmd->action, cmd->target_depth_cm);

                switch (cmd->action) {
                    case ACTION_DROP_ANCHOR:
                        actuators.stopMotor();
                        actuators.commandDropAnchor(cmd->target_depth_cm);
                        currentMotorStatus = MOTOR_ANCHORED;
                        break;
                    case ACTION_RETRACT_ANCHOR:
                        actuators.commandRetractAnchor();
                        currentMotorStatus = MOTOR_IDLE;
                        break;
                    case ACTION_HOLD_STATION:
                        currentMotorStatus = MOTOR_HOLDING_STATION;
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

void loop() {
    uint32_t now = millis();

    // 1. Process GPS
    gps.update();

    // 2. Update Actuators (winch timing, etc.)
    actuators.update();

    // 3. Check for received LoRa packets
    int packetLen = radio.getPacketLength();
    if (packetLen > 0) {
        uint8_t buffer[64];
        int readState = radio.readData(buffer, packetLen);
        if (readState == RADIOLIB_ERR_NONE) {
            processIncomingPacket(buffer, packetLen);
        }
        radio.startReceive();
    }

    // 4. Failsafe Watchdog: In manual mode, stop if signal lost
    if (currentMotorStatus == MOTOR_MANUAL && (now - lastControlReceivedTime > FAILSAFE_TIMEOUT_MS)) {
        actuators.stopMotor();
        actuators.centerRudder();
        actuators.stopWinch();
        currentMotorStatus = MOTOR_FAILSAFE;
        setRgbColor(40, 10, 0); // Orange/Red warning
        Serial.println("[ROVER] FAILSAFE TRIGGERED! Signal lost -> Motor stopped.");
    }

    // 5. Send Telemetry periodically (every 1000ms)
    if (now - lastTelemetryTime >= 1000) {
        lastTelemetryTime = now;
        sendTelemetry();

        // Restore LED to green if GPS has fix
        if (gps.hasFix()) {
            setRgbColor(0, 25, 0);
        } else {
            setRgbColor(25, 25, 0); // Yellow: searching satellites
        }
    }
}
