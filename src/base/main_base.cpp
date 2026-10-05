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

// System components
FleetManager  fleet;
BaseJoystick  joystick;
BaseNetwork   network(fleet);
BaseDisplay   display;
Adafruit_NeoPixel rgbLed(1, PIN_RGB_LED, NEO_GRB + NEO_KHZ800);

// Hardware SPI for SX1278
SPIClass spiLoRa(FSPI);
Module loraModule(PIN_LORA_NSS, PIN_LORA_DIO0, PIN_LORA_RST, RADIOLIB_NC, spiLoRa);
SX1278 radio(&loraModule);

// Loop timers
uint32_t lastControlTxTime = 0;
const uint32_t CONTROL_TX_INTERVAL_MS = 100; // 10Hz manual control stream

// Button state
bool lastBootBtnState = HIGH;
uint32_t lastDebounceTime = 0;

void setRgbColor(uint8_t r, uint8_t g, uint8_t b) {
    rgbLed.setPixelColor(0, rgbLed.Color(r, g, b));
    rgbLed.show();
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== WINDDRAGONS BASE STATION BOOTING ===");

    rgbLed.begin();
    setRgbColor(20, 20, 0); // Yellow

    pinMode(PIN_BTN_BOOT, INPUT_PULLUP);

    // 1. Initialize Display
    display.begin();
    Serial.println("[BASE] LCD ST7789 initialized");

    // 2. Initialize Joystick
    joystick.begin();
    Serial.println("[BASE] Joystick inputs initialized");

    // 3. Initialize LoRa
    spiLoRa.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_NSS);
    Serial.print("[BASE] Initializing LoRa SX1278 (433MHz)... ");
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
    radio.startReceive();

    // 4. Initialize Network (WiFi + WindDragons API)
    network.begin();
}

void transmitControlOrCommand() {
    // Priority 1: Transmit any pending cloud action command (e.g. DROP_ANCHOR)
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
    packet.throttle        = joystick.getThrottle();
    packet.rudder          = joystick.getRudder();
    packet.anchor_jog      = joystick.getAnchorJog();
    packet.mode_request    = 0; // Manual

    packet.checksum = calculate_checksum(reinterpret_cast<const uint8_t*>(&packet),
                                         sizeof(packet) - sizeof(packet.checksum));

    radio.standby();
    radio.transmit(reinterpret_cast<uint8_t*>(&packet), sizeof(packet));
    radio.startReceive();
}

void checkIncomingTelemetry() {
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
}

void handleRoverCycleButton() {
    bool currentBtn = digitalRead(PIN_BTN_BOOT);
    if (currentBtn != lastBootBtnState) {
        lastDebounceTime = millis();
    }
    if ((millis() - lastDebounceTime) > 50) {
        if (lastBootBtnState == HIGH && currentBtn == LOW) {
            // Button pressed
            fleet.selectNextRover();
            Serial.printf("[BASE] Cycled to %s\n", fleet.getSelectedRover()->code);
        }
    }
    lastBootBtnState = currentBtn;
}

void loop() {
    uint32_t now = millis();

    // 1. Process local inputs
    joystick.update();
    handleRoverCycleButton();

    // 2. Receive LoRa telemetry from fleet rovers
    checkIncomingTelemetry();

    // 3. Send LoRa commands to Rover (10Hz)
    if (now - lastControlTxTime >= CONTROL_TX_INTERVAL_MS) {
        lastControlTxTime = now;
        transmitControlOrCommand();
    }

    // 4. Update WiFi & sync with Winddragons Cloud API
    network.update();

    // 5. Update Rover online/offline status
    fleet.checkOnlineStatus();

    // 6. Update LCD screen
    display.update(fleet, network.isConnected(), network.isSyncSuccess(), 
                   joystick.getThrottle(), joystick.getRudder());
}
