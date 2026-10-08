#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <esp_mac.h>

#include "network_config.h"
#include "fleet_manager.h"
#include "lora_protocol.h"
#include "wifi_config_manager.h"

class BaseNetwork {
public:
    BaseNetwork(FleetManager &fleet, WiFiConfigManager &wifi) 
        : fleetManager(fleet), wifiConfig(wifi), lastSyncTime(0), lastHttpStatus(0), 
          syncSuccess(false), successCount(0), failCount(0) {}

    void begin() {
        // Conexão e fallback AP geridos pelo WiFiConfigManager
    }

    bool isConnected() const {
        return wifiConfig.isConnected();
    }

    int getLastHttpStatus() const { return lastHttpStatus; }
    bool isSyncSuccess() const { return syncSuccess; }
    uint32_t getLastSyncTime() const { return lastSyncTime; }
    uint32_t getSuccessCount() const { return successCount; }
    uint32_t getFailCount() const { return failCount; }

    void update() {
        uint32_t now = millis();
        if (now - lastSyncTime < TELEMETRY_SYNC_INTERVAL_MS) {
            return;
        }
        lastSyncTime = now;

        if (!isConnected()) {
            syncSuccess = false;
            return;
        }

        syncTelemetryToCloud();
    }

private:
    FleetManager &fleetManager;
    WiFiConfigManager &wifiConfig;
    void syncTelemetryToCloud() {
        WiFiClientSecure client;
        client.setInsecure(); // Skip certificate verification for flexible operation
        client.setTimeout(4);

        HTTPClient https;
        https.setTimeout(4000);
        if (!https.begin(client, WINDDRAGONS_API_URL)) {
            Serial.println("[HTTP] Failed to connect to WindDragons endpoint");
            syncSuccess = false;
            failCount++;
            return;
        }

        https.addHeader("Content-Type", "application/json");

        // Obter MAC Address único de hardware
        String mac = WiFi.macAddress();
        if (mac.isEmpty() || mac == "00:00:00:00:00:00") {
            uint8_t baseMac[6];
            esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
            char macStr[18];
            snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                     baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
            mac = String(macStr);
        }

        // Construct JSON document
        JsonDocument doc;
        doc["station_id"] = mac;
        doc["mac_address"] = mac;
        doc["station_name"] = BASE_STATION_ID;
        JsonArray roversArray = doc["rovers"].to<JsonArray>();

        // Only send active/online rovers
        for (const auto &r : fleetManager.getRovers()) {
            if (!r.is_online) continue;

            JsonObject rObj = roversArray.add<JsonObject>();
            rObj["rover_code"] = r.code;
            rObj["lat"] = serialized(String(r.lat, 6));
            rObj["lng"] = serialized(String(r.lng, 6));
            rObj["speed"] = serialized(String(r.speed_knots, 2));
            rObj["heading"] = serialized(String(r.heading_deg, 1));
            rObj["battery_pct"] = r.battery_pct;
            rObj["battery_voltage"] = serialized(String(r.battery_voltage, 2));
            rObj["motor_status"] = get_motor_status_str(r.motor_status);
            rObj["anchor_status"] = get_anchor_status_str(r.anchor_status);
            rObj["anchor_depth_m"] = serialized(String(r.anchor_depth_m, 2));
            rObj["rssi"] = r.rssi;
        }

        String requestBody;
        serializeJson(doc, requestBody);

        int httpResponseCode = https.POST(requestBody);
        lastHttpStatus = httpResponseCode;

        if (httpResponseCode == HTTP_CODE_OK || httpResponseCode == 201) {
            syncSuccess = true;
            successCount++;
            String responsePayload = https.getString();
            parseCloudResponse(responsePayload);
            if (successCount % 5 == 1) {
                Serial.printf("[CLOUD] Sync #%u OK (HTTP %d | Station: %s)\n", 
                              successCount, httpResponseCode, mac.c_str());
            }
        } else {
            syncSuccess = false;
            failCount++;
            Serial.printf("[HTTP] POST failed, error: %d - %s (falhas: %u)\n", 
                          httpResponseCode, https.errorToString(httpResponseCode).c_str(), failCount);
        }

        https.end();
    }

    void parseCloudResponse(const String &payload) {
        JsonDocument respDoc;
        DeserializationError err = deserializeJson(respDoc, payload);
        if (err) {
            Serial.printf("[JSON] Parse error: %s\n", err.c_str());
            return;
        }

        bool hasCommand = respDoc["has_command"] | false;
        if (!hasCommand) {
            // No command pending
            return;
        }

        // Single pending command
        if (respDoc["command"].is<JsonObject>()) {
            processCommandObject(respDoc["command"].as<JsonObject>());
        }

        // Array of commands (if multiple)
        if (respDoc["commands"].is<JsonArray>()) {
            JsonArray cmdList = respDoc["commands"].as<JsonArray>();
            for (JsonObject c : cmdList) {
                processCommandObject(c);
            }
        }
    }

    void processCommandObject(JsonObject c) {
        uint16_t cmdId = c["id"] | 0;
        const char *roverCode = c["rover_code"] | "";
        const char *actionStr = c["action"] | "";
        float depthM = c["depth_m"] | 0.0f;
        uint16_t durationS = c["duration_s"] | 10;
        double targetLat = c["target_lat"] | 0.0;
        double targetLng = c["target_lng"] | 0.0;

        uint8_t roverId = fleetManager.resolveRoverCodeToId(roverCode);
        uint8_t action = ACTION_NONE;

        if (strcmp(actionStr, "HOLD_STATION") == 0) {
            action = ACTION_HOLD_STATION;
        } else if (strcmp(actionStr, "STOP") == 0 || strcmp(actionStr, "STANDBY") == 0) {
            action = ACTION_STOP;
        } else if (strcmp(actionStr, "DROP_ANCHOR") == 0) {
            action = ACTION_DROP_ANCHOR;
        } else if (strcmp(actionStr, "RETRIEVE_ANCHOR") == 0 || strcmp(actionStr, "RETRACT_ANCHOR") == 0) {
            action = ACTION_RETRIEVE_ANCHOR;
        } else if (strcmp(actionStr, "STOP_ANCHOR") == 0) {
            action = ACTION_STOP_ANCHOR;
        } else if (strcmp(actionStr, "NAVIGATE") == 0 || strcmp(actionStr, "GOTO") == 0) {
            action = ACTION_NAVIGATE;
        } else if (strcmp(actionStr, "RTL") == 0) {
            action = ACTION_RTL;
        } else if (strcmp(actionStr, "ALARM") == 0) {
            action = ACTION_ALARM;
        } else if (strcmp(actionStr, "PING") == 0) {
            action = ACTION_PING;
        } else if (strcmp(actionStr, "MANUAL") == 0) {
            action = ACTION_MANUAL;
        }

        if (action != ACTION_NONE) {
            Serial.printf("[CLOUD] Command #%u for %s -> %s (depth: %.1fm, dur: %us)\n", 
                          cmdId, roverCode, actionStr, depthM, durationS);

            fleetManager.queueCommand(
                roverId, 
                cmdId, 
                action, 
                static_cast<uint16_t>(depthM * 100.0f),
                durationS,
                static_cast<int32_t>(targetLat * 10000000.0),
                static_cast<int32_t>(targetLng * 10000000.0)
            );
        }
    }

    uint32_t lastSyncTime;
    uint32_t successCount;
    uint32_t failCount;
    int lastHttpStatus;
    bool syncSuccess;
};
