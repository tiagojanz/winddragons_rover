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
#include "web_console.h"
#include "wifi_config_manager.h"

class BaseNetwork {
public:
    BaseNetwork(FleetManager &fleet, WiFiConfigManager &wifi) 
        : fleetManager(fleet), wifiConfig(wifi), lastSyncTime(0), lastHttpStatus(0), 
          syncSuccess(false), successCount(0), failCount(0), consecutiveFails(0) {}

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
        // Em caso de falha de rede/cloud, aguarda 10s para não bloquear o servidor Web nem os botões
        uint32_t syncInterval = (consecutiveFails > 0) ? 10000 : TELEMETRY_SYNC_INTERVAL_MS;
        if (now - lastSyncTime < syncInterval) {
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
        // Obter MAC Address único de hardware
        char macStr[18] = {0};
        String wifiMac = WiFi.macAddress();
        if (wifiMac.isEmpty() || wifiMac == "00:00:00:00:00:00") {
            uint8_t baseMac[6];
            esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
            snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                     baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
        } else {
            strncpy(macStr, wifiMac.c_str(), sizeof(macStr) - 1);
        }
        wifiMac = String(); // Libertar String temporária antes do SSL

        // 1. Construir requestBody num bloco isolado para libertar o JsonDocument antes do handshake TLS
        String requestBody;
        requestBody.reserve(512);
        {
            JsonDocument doc;
            doc["station_id"] = macStr;
            doc["mac_address"] = macStr;
            doc["station_name"] = fleetManager.getStationName();
            doc["operation_state"] = fleetManager.getOperationStateStr();
            doc["operation_state_id"] = static_cast<uint8_t>(fleetManager.getOperationState());
            doc["circuit_id"] = fleetManager.getCircuitId();
            doc["circuit_name"] = fleetManager.getMissionName();
            doc["race_id"] = fleetManager.getRaceId();
            doc["mission_name"] = fleetManager.getMissionName();

            JsonArray roversArray = doc["rovers"].to<JsonArray>();

            // Only send active/online rovers
            for (const auto &r : fleetManager.getRovers()) {
                if (!r.is_online) continue;

                JsonObject rObj = roversArray.add<JsonObject>();
                rObj["rover_id"] = r.id;
                rObj["rover_code"] = r.code;
                rObj["name"] = r.name;
                rObj["mac_address"] = r.mac;
                rObj["role"] = r.role;
                if (r.target_lat != 0.0 || r.target_lng != 0.0) {
                    rObj["target_lat"] = serialized(String(r.target_lat, 6));
                    rObj["target_lng"] = serialized(String(r.target_lng, 6));
                }
                rObj["lat"] = serialized(String(r.lat, 6));
                rObj["lng"] = serialized(String(r.lng, 6));
                rObj["speed"] = serialized(String(r.speed_knots, 2));
                rObj["heading"] = serialized(String(r.heading_deg, 1));
                rObj["compass_active"] = r.compass_active;
                rObj["heading_source"] = r.compass_active ? "BNO055" : "GPS_COG";
                rObj["roll"] = serialized(String(r.roll_deg, 1));
                rObj["pitch"] = serialized(String(r.pitch_deg, 1));
                rObj["compass_cal_sys"] = r.compass_cal_sys;
                rObj["compass_cal_mag"] = r.compass_cal_mag;
                rObj["battery_pct"] = r.battery_pct;
                rObj["battery_voltage"] = serialized(String(r.battery_voltage, 2));
                rObj["motor_status"] = get_motor_status_str(r.motor_status);
                rObj["anchor_status"] = get_anchor_status_str(r.anchor_status);
                rObj["anchor_depth_m"] = serialized(String(r.anchor_depth_m, 2));
                rObj["rssi"] = r.rssi;
            }

            serializeJson(doc, requestBody);
        }

        // 2. Executar pedido HTTPS num escopo dedicado para que os ~42KB de buffers mbedTLS
        // sejam totalmente libertados ANTES de processar a resposta e gravar no MicroSD/NVS!
        String responsePayload;
        int httpResponseCode = -1;
        {
            WiFiClientSecure client;
            client.setInsecure(); // Skip certificate verification for flexible operation
            client.setTimeout(3000);

            HTTPClient https;
            https.setTimeout(3000);
            https.setConnectTimeout(2500);
            https.useHTTP10(true); // Evita chunked encoding e reduz alocações intermédias
            if (!https.begin(client, WINDDRAGONS_API_URL)) {
                Serial.println("[HTTP] Failed to connect to WindDragons endpoint");
                syncSuccess = false;
                failCount++;
                consecutiveFails++;
                return;
            }

            https.addHeader("Content-Type", "application/json");
            https.addHeader("Connection", "close");

            httpResponseCode = https.POST(requestBody);
            requestBody = String(); // Libertar imediatamente o body de envio
            lastHttpStatus = httpResponseCode;

            if (httpResponseCode == HTTP_CODE_OK || httpResponseCode == 201) {
                responsePayload = https.getString();
            } else {
                Serial.printf("[HTTP] POST failed, error: %d - %s (falhas: %u | FreeHeap: %u | MaxAlloc: %u)\n", 
                              httpResponseCode, https.errorToString(httpResponseCode).c_str(), failCount + 1,
                              (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
            }

            https.end();
            client.stop();
        } // <-- Destrói WiFiClientSecure e liberta todos os buffers SSL aqui!

        // 3. Processar resposta já com os ~42KB de SSL livres na heap
        if (httpResponseCode == HTTP_CODE_OK || httpResponseCode == 201) {
            syncSuccess = true;
            successCount++;
            consecutiveFails = 0;
            printFullJsonToSerial("[CLOUD] Resposta Portal: ", responsePayload);
            parseCloudResponse(responsePayload);
            if (successCount % 5 == 1) {
                Serial.printf("[CLOUD] Sync #%u OK (HTTP %d | Station: %s | State: %s | FreeHeap: %u)\n", 
                              successCount, httpResponseCode, macStr, fleetManager.getOperationStateStr(),
                              (unsigned)ESP.getFreeHeap());
            }
        } else {
            syncSuccess = false;
            failCount++;
            consecutiveFails++;
        }
    }

    static void printFullJsonToSerial(const char *prefix, const String &payload) {
        if (prefix) {
            Serial.print(prefix);
        }
        const char *str = payload.c_str();
        size_t len = payload.length();
        size_t offset = 0;
        const size_t chunkSize = 128; // Evita truncagem de 256 bytes do buffer USB CDC não-bloqueante
        while (offset < len) {
            size_t toWrite = (len - offset > chunkSize) ? chunkSize : (len - offset);
            Serial.write(reinterpret_cast<const uint8_t *>(str + offset), toWrite);
            offset += toWrite;
            if (offset < len) {
                delay(2);
            }
        }
        Serial.print("\n");
    }

    void parseCloudResponse(const String &payload) {
        JsonDocument respDoc;
        DeserializationError err = deserializeJson(respDoc, payload);
        if (err) {
            Serial.printf("[JSON] Parse error: %s\n", err.c_str());
            return;
        }

        // 1. Processar Estado de Operação e Dados da Estação / Circuito se fornecidos
        const char *opState = nullptr;
        int circuitId = respDoc["circuit_id"] | 0;
        const char *circuitName = nullptr;
        if (respDoc["circuit_name"].is<const char*>()) {
            circuitName = respDoc["circuit_name"].as<const char*>();
        }

        // Verificar respDoc["station"]
        if (respDoc["station"].is<JsonObject>()) {
            JsonObject st = respDoc["station"].as<JsonObject>();
            if (st["station_name"].is<const char*>()) {
                const char *stName = st["station_name"].as<const char*>();
                if (stName && strlen(stName) > 0 && strcmp(fleetManager.getStationName(), stName) != 0) {
                    fleetManager.setStationName(stName);
                }
            }
            if (circuitId <= 0 && st["circuit_id"].is<int>()) {
                circuitId = st["circuit_id"].as<int>();
            }
            if ((!circuitName || strlen(circuitName) == 0) && st["circuit_name"].is<const char*>()) {
                circuitName = st["circuit_name"].as<const char*>();
            }
            if (st["operation_state"].is<const char*>()) opState = st["operation_state"].as<const char*>();
            else if (st["state"].is<const char*>()) opState = st["state"].as<const char*>();
            else if (st["op_state"].is<const char*>()) opState = st["op_state"].as<const char*>();
            else if (st["status"].is<const char*>() && strcmp(st["status"].as<const char*>(), "ok") != 0 && strcmp(st["status"].as<const char*>(), "success") != 0) {
                opState = st["status"].as<const char*>();
            }
        }

        // Verificar respDoc["mission"] para circuit_id / circuit_name / opState
        if (respDoc["mission"].is<JsonObject>()) {
            JsonObject ms = respDoc["mission"].as<JsonObject>();
            if (circuitId <= 0 && ms["circuit_id"].is<int>()) {
                circuitId = ms["circuit_id"].as<int>();
            }
            if ((!circuitName || strlen(circuitName) == 0)) {
                if (ms["circuit_name"].is<const char*>()) circuitName = ms["circuit_name"].as<const char*>();
                else if (ms["mission_name"].is<const char*>()) circuitName = ms["mission_name"].as<const char*>();
            }
            if (!opState) {
                if (ms["operation_state"].is<const char*>()) opState = ms["operation_state"].as<const char*>();
                else if (ms["state"].is<const char*>()) opState = ms["state"].as<const char*>();
                else if (ms["op_state"].is<const char*>()) opState = ms["op_state"].as<const char*>();
            }
        }

        // Atualizar ID e Nome do Circuito no FleetManager mesmo quando "mission": null
        if (circuitId > 0 || (circuitName && strlen(circuitName) > 0)) {
            fleetManager.setCircuitInfo(circuitId, circuitName);
        }

        // Verificar raiz respDoc
        if (!opState) {
            if (respDoc["operation_state"].is<const char*>()) opState = respDoc["operation_state"].as<const char*>();
            else if (respDoc["state"].is<const char*>()) opState = respDoc["state"].as<const char*>();
            else if (respDoc["op_state"].is<const char*>()) opState = respDoc["op_state"].as<const char*>();
        }

        // Verificar se veio como ID numérico inteiro (0: STORAGE, 1: WAITING, 2: IDLE, 3: GO)
        int opStateInt = -1;
        if (!opState) {
            if (respDoc["station"].is<JsonObject>() && respDoc["station"]["operation_state_id"].is<int>()) {
                opStateInt = respDoc["station"]["operation_state_id"].as<int>();
            } else if (respDoc["station"].is<JsonObject>() && respDoc["station"]["operation_state"].is<int>()) {
                opStateInt = respDoc["station"]["operation_state"].as<int>();
            } else if (respDoc["mission"].is<JsonObject>() && respDoc["mission"]["operation_state_id"].is<int>()) {
                opStateInt = respDoc["mission"]["operation_state_id"].as<int>();
            } else if (respDoc["mission"].is<JsonObject>() && respDoc["mission"]["operation_state"].is<int>()) {
                opStateInt = respDoc["mission"]["operation_state"].as<int>();
            } else if (respDoc["operation_state_id"].is<int>()) {
                opStateInt = respDoc["operation_state_id"].as<int>();
            } else if (respDoc["operation_state"].is<int>()) {
                opStateInt = respDoc["operation_state"].as<int>();
            }
        }

        if (opStateInt >= 0 && opStateInt <= 3) {
            fleetManager.setOperationState(static_cast<BaseOperationState>(opStateInt));
        }

        // 2. Processar Lista de Rovers e Missão de Regata se fornecida
        if (respDoc["mission"].is<JsonObject>()) {
            JsonObject mObj = respDoc["mission"].as<JsonObject>();
            fleetManager.updateFromPortalMission(mObj, opState);
        } else if (respDoc["rovers"].is<JsonArray>()) {
            fleetManager.updateFromPortalMission(respDoc.as<JsonObject>(), opState);
        } else if (opState) {
            fleetManager.setOperationStateFromStr(opState);
        }

        // 3. Processar comandos pendentes
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

        uint8_t roverId = c["rover_id"] | fleetManager.resolveRoverCodeToId(roverCode);
        uint8_t action = ACTION_NONE;

        if (strcmp(actionStr, "HOLD_STATION") == 0) {
            action = ACTION_HOLD_STATION;
        } else if (strcmp(actionStr, "STOP") == 0 || strcmp(actionStr, "STANDBY") == 0) {
            action = ACTION_STOP;
        } else if (strcmp(actionStr, "DROP_ANCHOR") == 0) {
            action = ACTION_DROP_ANCHOR;
        } else if (strcmp(actionStr, "RELEASE_ANCHOR") == 0 || strcmp(actionStr, "LIBERTAR") == 0 || strcmp(actionStr, "LIBERTAR_ANCORA") == 0 || strcmp(actionStr, "RELEASE_RACK") == 0) {
            action = ACTION_RELEASE_ANCHOR;
        } else if (strcmp(actionStr, "LOCK_ANCHOR") == 0 || strcmp(actionStr, "ENGAGE_RACK") == 0 || strcmp(actionStr, "TRANCAR_ANCORA") == 0) {
            action = ACTION_ENGAGE_RACK;
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
        } else if (strcmp(actionStr, "SET_RTL") == 0 || strcmp(actionStr, "SET_HOME") == 0) {
            action = ACTION_SET_HOME;
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
    uint32_t consecutiveFails;
    int lastHttpStatus;
    bool syncSuccess;
};
