#pragma once

#include <Arduino.h>
#include <vector>
#include "config_common.h"
#include "lora_protocol.h"
#include "network_config.h"

struct FleetRover {
    uint8_t   id;
    char      code[16];
    double    lat;
    double    lng;
    float     speed_knots;
    float     heading_deg;
    uint8_t   battery_pct;
    float     battery_voltage;
    uint8_t   motor_status;
    uint8_t   anchor_status;
    float     anchor_depth_m;
    int16_t   rssi;
    uint32_t  last_seen_ms;
    bool      is_online;
};

// Pending command queued for LoRa transmission
struct QueuedCommand {
    uint8_t  target_rover_id;
    uint16_t command_id;
    uint8_t  action;
    uint16_t depth_cm;
    uint16_t duration_s;
    int32_t  target_lat;
    int32_t  target_lng;
};

class FleetManager {
public:
    FleetManager() : selectedRoverIndex(0) {
        // Initialize default slots
        for (uint8_t i = 1; i <= MAX_FLEET_ROVERS; ++i) {
            FleetRover r;
            memset(&r, 0, sizeof(r));
            r.id = i;
            snprintf(r.code, sizeof(r.code), "ROVER-%02d", i);
            r.battery_voltage = 12.0f;
            r.is_online = false;
            rovers.push_back(r);
        }
    }

    void updateTelemetry(const PacketTelemetry &pkt, int16_t rssi) {
        for (auto &r : rovers) {
            if (r.id == pkt.rover_id) {
                r.lat = pkt.lat_deg7 / 10000000.0;
                r.lng = pkt.lng_deg7 / 10000000.0;
                r.speed_knots = (pkt.speed_cm_s / 51.4444f);
                r.heading_deg = pkt.heading_deg10 / 10.0f;
                r.battery_pct = pkt.battery_pct;
                r.battery_voltage = pkt.battery_mv / 1000.0f;
                r.motor_status = pkt.motor_status;
                r.anchor_status = pkt.anchor_status;
                r.anchor_depth_m = pkt.anchor_depth_cm / 100.0f;
                r.rssi = rssi;
                r.last_seen_ms = millis();
                r.is_online = true;
                return;
            }
        }
    }

    void checkOnlineStatus() {
        uint32_t now = millis();
        for (auto &r : rovers) {
            if (r.is_online && (now - r.last_seen_ms > ROVER_OFFLINE_TIMEOUT_MS)) {
                r.is_online = false;
            }
        }
    }

    const std::vector<FleetRover>& getRovers() const {
        return rovers;
    }

    FleetRover* getSelectedRover() {
        if (rovers.empty()) return nullptr;
        return &rovers[selectedRoverIndex % rovers.size()];
    }

    void selectNextRover() {
        if (!rovers.empty()) {
            selectedRoverIndex = (selectedRoverIndex + 1) % rovers.size();
        }
    }

    uint8_t getSelectedRoverId() const {
        if (rovers.empty()) return 1;
        return rovers[selectedRoverIndex % rovers.size()].id;
    }

    void queueCommand(uint8_t roverId, uint16_t cmdId, uint8_t action, 
                      uint16_t depthCm = 0, uint16_t durationS = 0, 
                      int32_t lat = 0, int32_t lng = 0) {
        QueuedCommand cmd;
        cmd.target_rover_id = roverId;
        cmd.command_id = cmdId;
        cmd.action = action;
        cmd.depth_cm = depthCm;
        cmd.duration_s = durationS;
        cmd.target_lat = lat;
        cmd.target_lng = lng;
        commandQueue.push_back(cmd);
    }

    bool hasPendingCommand() const {
        return !commandQueue.empty();
    }

    QueuedCommand popPendingCommand() {
        if (commandQueue.empty()) {
            return {0, 0, 0, 0, 0, 0, 0};
        }
        QueuedCommand cmd = commandQueue.front();
        commandQueue.erase(commandQueue.begin());
        return cmd;
    }

    uint8_t resolveRoverCodeToId(const char *code) const {
        for (const auto &r : rovers) {
            if (strcmp(r.code, code) == 0) {
                return r.id;
            }
        }
        // Try parsing ROVER-XX
        int id = 0;
        if (sscanf(code, "ROVER-%d", &id) == 1) {
            return static_cast<uint8_t>(id);
        }
        return 1;
    }

private:
    std::vector<FleetRover> rovers;
    std::vector<QueuedCommand> commandQueue;
    size_t selectedRoverIndex;
};
