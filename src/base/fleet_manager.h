#pragma once

#include <Arduino.h>
#include <vector>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "config_common.h"
#include "lora_protocol.h"
#include "network_config.h"
#include "sd_card_manager.h"

// ==========================================
// ESTADOS DE OPERAÇÃO DA ESTAÇÃO BASE
// ==========================================
enum BaseOperationState : uint8_t {
    BASE_OP_STORAGE        = 0, // Storage - Not in use
    BASE_OP_WAITING_ROVERS = 1, // Waiting Rover List - Connected and waiting
    BASE_OP_IDLE           = 2, // Idle - Rover list ready
    BASE_OP_GO             = 3  // Go - In mission
};

inline const char* get_base_op_state_str(BaseOperationState state) {
    switch (state) {
        case BASE_OP_STORAGE:        return "STORAGE";
        case BASE_OP_WAITING_ROVERS: return "WAITING ROVERS";
        case BASE_OP_IDLE:           return "IDLE";
        case BASE_OP_GO:             return "GO";
        default:                     return "STORAGE";
    }
}

inline const char* get_base_op_state_badge(BaseOperationState state) {
    switch (state) {
        case BASE_OP_STORAGE:        return "STOR";
        case BASE_OP_WAITING_ROVERS: return "WAIT";
        case BASE_OP_IDLE:           return "IDLE";
        case BASE_OP_GO:             return "GO";
        default:                     return "STOR";
    }
}

inline const char* get_base_op_state_desc(BaseOperationState state) {
    switch (state) {
        case BASE_OP_STORAGE:        return "Not in use";
        case BASE_OP_WAITING_ROVERS: return "Connected and waiting";
        case BASE_OP_IDLE:           return "Rover list ready";
        case BASE_OP_GO:             return "In mission";
        default:                     return "Not in use";
    }
}

inline bool parse_base_op_state(const char* str, BaseOperationState &outState) {
    if (!str) return false;
    while (*str == ' ' || *str == '\t' || *str == '\r' || *str == '\n') str++;
    if (*str == '\0') return false;

    // GO / IN MISSION
    if (strcasecmp(str, "GO") == 0 || 
        strcasecmp(str, "MISSION") == 0 || 
        strcasecmp(str, "IN_MISSION") == 0 || 
        strcasecmp(str, "IN MISSION") == 0 || 
        strcasecmp(str, "IN-MISSION") == 0 || 
        strcasecmp(str, "RUN") == 0 || 
        strcasecmp(str, "RUNNING") == 0 || 
        strcasecmp(str, "RACE") == 0 || 
        strcasecmp(str, "RACING") == 0 || 
        strcasecmp(str, "ACTIVE") == 0 || 
        strcasecmp(str, "REGATA") == 0 || 
        strcmp(str, "3") == 0) {
        outState = BASE_OP_GO;
        return true;
    }

    // IDLE / ROVER LIST READY
    if (strcasecmp(str, "IDLE") == 0 || 
        strcasecmp(str, "READY") == 0 || 
        strcasecmp(str, "ROVER_LIST_READY") == 0 || 
        strcasecmp(str, "ROVER LIST READY") == 0 || 
        strcasecmp(str, "ROVER-LIST-READY") == 0 || 
        strcasecmp(str, "LIST_READY") == 0 || 
        strcasecmp(str, "LIST READY") == 0 || 
        strcasecmp(str, "LIST-READY") == 0 || 
        strcmp(str, "2") == 0) {
        outState = BASE_OP_IDLE;
        return true;
    }

    // WAITING ROVERS / CONNECTED AND WAITING
    if (strcasecmp(str, "WAITING") == 0 || 
        strcasecmp(str, "WAITING_ROVERS") == 0 || 
        strcasecmp(str, "WAITING ROVERS") == 0 || 
        strcasecmp(str, "WAITING-ROVERS") == 0 || 
        strcasecmp(str, "WAIT") == 0 || 
        strcasecmp(str, "WAITING_ROVER_LIST") == 0 || 
        strcasecmp(str, "WAITING ROVER LIST") == 0 || 
        strcasecmp(str, "WAITING-ROVER-LIST") == 0 || 
        strcasecmp(str, "CONNECTED_AND_WAITING") == 0 || 
        strcasecmp(str, "CONNECTED AND WAITING") == 0 || 
        strcasecmp(str, "CONNECTED-AND-WAITING") == 0 || 
        strcasecmp(str, "WAITING_LIST") == 0 || 
        strcasecmp(str, "WAITING LIST") == 0 || 
        strcmp(str, "1") == 0) {
        outState = BASE_OP_WAITING_ROVERS;
        return true;
    }

    // STORAGE / NOT IN USE
    if (strcasecmp(str, "STORAGE") == 0 || 
        strcasecmp(str, "NOT_IN_USE") == 0 || 
        strcasecmp(str, "NOT IN USE") == 0 || 
        strcasecmp(str, "NOT-IN-USE") == 0 || 
        strcasecmp(str, "STANDBY") == 0 || 
        strcasecmp(str, "STOP") == 0 || 
        strcasecmp(str, "PAUSE") == 0 || 
        strcasecmp(str, "PAUSED") == 0 || 
        strcmp(str, "0") == 0) {
        outState = BASE_OP_STORAGE;
        return true;
    }

    return false;
}

inline BaseOperationState parse_base_op_state(const char* str) {
    BaseOperationState s = BASE_OP_STORAGE;
    parse_base_op_state(str, s);
    return s;
}

// ==========================================
// ESTRUTURA DO ROVER NA FROTA DE REGATA / MISSÃO
// ==========================================
struct FleetRover {
    uint8_t   id;
    char      code[16];
    char      name[32];
    char      mac[18];
    char      role[24]; // "largada CR", "largada", "boia 1", "boia 2", "chegada CR", "chegada"
    double    target_lat;
    double    target_lng;

    // Telemetria em tempo real
    double    lat;
    double    lng;
    float     speed_knots;
    float     heading_deg;
    uint8_t   battery_pct;
    float     battery_voltage;
    uint8_t   motor_status;
    uint8_t   anchor_status;
    float     anchor_depth_m; // Profundidade real atingida (medida por encoder)
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
    FleetManager() 
        : _sd(nullptr), _opState(BASE_OP_STORAGE), selectedRoverIndex(-1), _roversFromPortal(false) {
        memset(_raceId, 0, sizeof(_raceId));
        memset(_missionName, 0, sizeof(_missionName));
        strncpy(_raceId, "REGATA-DEFAULT", sizeof(_raceId) - 1);
        strncpy(_missionName, "Percurso de Regata", sizeof(_missionName) - 1);
        rovers.clear();
    }

    void begin(SDCardManager *sd = nullptr) {
        _sd = sd;
        loadMissionConfig();
    }

    void setSDCardManager(SDCardManager *sd) {
        _sd = sd;
        if (_sd && _sd->isReady()) {
            loadMissionConfig();
        }
    }

    void initDefaultSlots() {
        rovers.clear();
        _roversFromPortal = false;
        for (uint8_t i = 1; i <= MAX_FLEET_ROVERS; ++i) {
            FleetRover r;
            memset(&r, 0, sizeof(r));
            r.id = i;
            snprintf(r.code, sizeof(r.code), "ROVER-%02d", i);
            snprintf(r.name, sizeof(r.name), "Rover %02d", i);
            snprintf(r.mac, sizeof(r.mac), "00:00:00:00:00:%02X", i);
            r.battery_voltage = 12.0f;
            r.is_online = false;
            
            // Sugestão de papéis iniciais para teste
            if (i == 1) strncpy(r.role, "largada CR", sizeof(r.role) - 1);
            else if (i == 2) strncpy(r.role, "largada", sizeof(r.role) - 1);
            else if (i == 3) strncpy(r.role, "boia 1", sizeof(r.role) - 1);
            else if (i == 4) strncpy(r.role, "boia 2", sizeof(r.role) - 1);
            else if (i == 5) strncpy(r.role, "chegada CR", sizeof(r.role) - 1);
            else if (i == 6) strncpy(r.role, "chegada", sizeof(r.role) - 1);
            else snprintf(r.role, sizeof(r.role), "boia %d", i - 2);

            rovers.push_back(r);
        }
    }

    void clearRovers() {
        rovers.clear();
        selectedRoverIndex = -1;
        _roversFromPortal = false;
        saveMissionConfig();
    }

    bool hasReceivedRoversFromPortal() const { return _roversFromPortal; }
    void setRoversFromPortal(bool v) { _roversFromPortal = v; }

    // --- Gestão de Estados de Operação ---
    BaseOperationState getOperationState() const { return _opState; }
    const char* getOperationStateStr() const { return get_base_op_state_str(_opState); }
    const char* getOperationStateBadge() const { return get_base_op_state_badge(_opState); }
    const char* getOperationStateDesc() const { return get_base_op_state_desc(_opState); }

    void setOperationState(BaseOperationState state, bool save = true) {
        if (_opState == state) {
            return; // Estado não mudou. Evitar escrita repetida em Flash/SD e ruído de logs
        }
        _opState = state;
        Serial.printf("[MISSION] Estado da Estacao Base alterado para: %s (%s)\n", 
                      get_base_op_state_str(_opState), get_base_op_state_desc(_opState));
        if (save) {
            saveMissionConfig();
        }
    }

    bool setOperationStateFromStr(const char* stateStr, bool save = true) {
        if (!stateStr) return false;
        BaseOperationState s;
        if (parse_base_op_state(stateStr, s)) {
            setOperationState(s, save);
            return true;
        } else {
            Serial.printf("[MISSION] Aviso: Estado desconhecido recebido do Portal: '%s'\n", stateStr);
            return false;
        }
    }

    const char* getRaceId() const { return _raceId; }
    const char* getMissionName() const { return _missionName; }

    void setMissionInfo(const char* raceId, const char* missionName, bool save = true) {
        if (raceId && strlen(raceId) > 0) {
            strncpy(_raceId, raceId, sizeof(_raceId) - 1);
            _raceId[sizeof(_raceId) - 1] = '\0';
        }
        if (missionName && strlen(missionName) > 0) {
            strncpy(_missionName, missionName, sizeof(_missionName) - 1);
            _missionName[sizeof(_missionName) - 1] = '\0';
        }
        if (save) {
            saveMissionConfig();
        }
    }

    // --- Telemetria e Status ---
    void updateTelemetry(const PacketTelemetry &pkt, int16_t rssi) {
        if (pkt.rover_id == 0 || pkt.rover_id == BROADCAST_ROVER_ID) return;

        for (size_t i = 0; i < rovers.size(); ++i) {
            auto &r = rovers[i];
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
                if (!hasSelectedRover()) {
                    selectedRoverIndex = (int16_t)i;
                }
                return;
            }
        }

        // Auto-registo dinâmico caso o Rover ainda não esteja na lista da Base
        if (rovers.size() < MAX_FLEET_ROVERS) {
            FleetRover r;
            memset(&r, 0, sizeof(r));
            r.id = pkt.rover_id;
            snprintf(r.code, sizeof(r.code), "ROVER-%02d", r.id);
            snprintf(r.name, sizeof(r.name), "Rover %02d", r.id);
            snprintf(r.mac, sizeof(r.mac), "00:00:00:00:00:%02X", r.id);
            strncpy(r.role, "boia 1", sizeof(r.role) - 1);
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
            rovers.push_back(r);
            if (!hasSelectedRover()) {
                selectedRoverIndex = (int16_t)(rovers.size() - 1);
            }
            Serial.printf("[MISSION] Auto-registado ROVER-%02d via telemetria LoRa (RSSI: %d dBm)\n",
                          r.id, rssi);
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

    std::vector<FleetRover>& getRoversMutable() {
        return rovers;
    }

    bool hasSelectedRover() const {
        return (selectedRoverIndex >= 0 && selectedRoverIndex < (int16_t)rovers.size());
    }

    FleetRover* getSelectedRover() {
        if (!hasSelectedRover()) return nullptr;
        return &rovers[selectedRoverIndex];
    }

    FleetRover* getRoverById(uint8_t id) {
        for (auto &r : rovers) {
            if (r.id == id) return &r;
        }
        return nullptr;
    }

    bool selectRoverById(uint8_t id) {
        if (id == 0) {
            selectedRoverIndex = -1;
            return true;
        }
        for (size_t i = 0; i < rovers.size(); ++i) {
            if (rovers[i].id == id) {
                selectedRoverIndex = (int16_t)i;
                return true;
            }
        }
        return false;
    }

    void clearSelectedRover() {
        selectedRoverIndex = -1;
    }

    void selectNextRover() {
        if (!rovers.empty()) {
            if (selectedRoverIndex < 0) selectedRoverIndex = 0;
            else selectedRoverIndex = (selectedRoverIndex + 1) % rovers.size();
        }
    }

    uint8_t getSelectedRoverId() const {
        if (hasSelectedRover()) {
            return rovers[selectedRoverIndex].id;
        }
        return rovers.empty() ? 1 : rovers[0].id;
    }

    // --- Fila de Comandos LoRa ---
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
        if (!code || strlen(code) == 0) return 1;

        // 1. Procurar por code exato (ex: "ROVER-01")
        for (const auto &r : rovers) {
            if (strcasecmp(r.code, code) == 0) {
                return r.id;
            }
        }
        // 2. Procurar por nome (ex: "WD-Largada-CR")
        for (const auto &r : rovers) {
            if (strcasecmp(r.name, code) == 0) {
                return r.id;
            }
        }
        // 3. Procurar por MAC Address
        for (const auto &r : rovers) {
            if (strcasecmp(r.mac, code) == 0) {
                return r.id;
            }
        }
        // 4. Procurar por Role (ex: "largada CR")
        for (const auto &r : rovers) {
            if (strcasecmp(r.role, code) == 0) {
                return r.id;
            }
        }
        // 5. Tentar parsing numérico ROVER-XX ou ID direto
        int id = 0;
        if (sscanf(code, "ROVER-%d", &id) == 1 && id > 0) {
            return static_cast<uint8_t>(id);
        }
        if (sscanf(code, "%d", &id) == 1 && id > 0) {
            return static_cast<uint8_t>(id);
        }
        return 1;
    }

    bool isRoverListDifferent(const std::vector<FleetRover> &newList) const {
        if (newList.size() != rovers.size()) return true;
        for (size_t i = 0; i < newList.size(); ++i) {
            if (newList[i].id != rovers[i].id) return true;
            if (strcmp(newList[i].name, rovers[i].name) != 0) return true;
            if (strcmp(newList[i].mac, rovers[i].mac) != 0) return true;
            if (strcmp(newList[i].role, rovers[i].role) != 0) return true;
            if (fabs(newList[i].target_lat - rovers[i].target_lat) > 0.000001) return true;
            if (fabs(newList[i].target_lng - rovers[i].target_lng) > 0.000001) return true;
        }
        return false;
    }

    // --- Atualização a partir do Portal Cloud ---
    void updateFromPortalMission(JsonObject mObj, const char *opStateStr = nullptr) {
        bool metaChanged = false;
        if (mObj["race_id"].is<const char*>()) {
            const char* rId = mObj["race_id"].as<const char*>();
            if (strcmp(_raceId, rId) != 0) {
                strncpy(_raceId, rId, sizeof(_raceId) - 1);
                _raceId[sizeof(_raceId) - 1] = '\0';
                metaChanged = true;
            }
        }
        if (mObj["mission_name"].is<const char*>()) {
            const char* mName = mObj["mission_name"].as<const char*>();
            if (strcmp(_missionName, mName) != 0) {
                strncpy(_missionName, mName, sizeof(_missionName) - 1);
                _missionName[sizeof(_missionName) - 1] = '\0';
                metaChanged = true;
            }
        }

        // Se opStateStr não veio na raiz, tenta extrair de mObj
        if (!opStateStr) {
            if (mObj["operation_state"].is<const char*>()) opStateStr = mObj["operation_state"].as<const char*>();
            else if (mObj["state"].is<const char*>()) opStateStr = mObj["state"].as<const char*>();
            else if (mObj["status"].is<const char*>()) opStateStr = mObj["status"].as<const char*>();
            else if (mObj["op_state"].is<const char*>()) opStateStr = mObj["op_state"].as<const char*>();
        }

        bool updateList = mObj["update_rover_list"] | false;
        JsonArray roversArray = mObj["rovers"].as<JsonArray>();

        bool listChanged = false;
        if ((updateList || !roversArray.isNull()) && roversArray.size() > 0) {
            std::vector<FleetRover> newList;

            for (JsonObject rJson : roversArray) {
                FleetRover r;
                memset(&r, 0, sizeof(r));
                r.id = rJson["id"] | (newList.size() + 1);
                snprintf(r.code, sizeof(r.code), "ROVER-%02d", r.id);

                const char *name = rJson["name"] | "";
                if (strlen(name) > 0) {
                    strncpy(r.name, name, sizeof(r.name) - 1);
                } else {
                    snprintf(r.name, sizeof(r.name), "Rover %02d", r.id);
                }

                const char *mac = rJson["mac_address"] | (rJson["mac"] | "");
                if (strlen(mac) > 0) {
                    strncpy(r.mac, mac, sizeof(r.mac) - 1);
                }

                const char *role = rJson["role"] | "";
                if (strlen(role) > 0) {
                    strncpy(r.role, role, sizeof(r.role) - 1);
                }

                if (rJson["coordinates"].is<JsonObject>()) {
                    JsonObject coord = rJson["coordinates"].as<JsonObject>();
                    r.target_lat = coord["lat"] | 0.0;
                    r.target_lng = coord["lng"] | 0.0;
                } else {
                    r.target_lat = rJson["lat"] | 0.0;
                    r.target_lng = rJson["lng"] | 0.0;
                }

                r.battery_voltage = 12.0f;
                r.is_online = false;

                // Manter telemetria anterior se já existia
                FleetRover *prev = getRoverById(r.id);
                if (prev) {
                    r.lat = prev->lat;
                    r.lng = prev->lng;
                    r.speed_knots = prev->speed_knots;
                    r.heading_deg = prev->heading_deg;
                    r.battery_pct = prev->battery_pct;
                    r.battery_voltage = prev->battery_voltage;
                    r.motor_status = prev->motor_status;
                    r.anchor_status = prev->anchor_status;
                    r.anchor_depth_m = prev->anchor_depth_m;
                    r.rssi = prev->rssi;
                    r.last_seen_ms = prev->last_seen_ms;
                    r.is_online = prev->is_online;
                }

                newList.push_back(r);
            }

            if (!newList.empty()) {
                _roversFromPortal = true;
                if (isRoverListDifferent(newList)) {
                    rovers = newList;
                    if (!hasSelectedRover() && !rovers.empty()) {
                        selectedRoverIndex = 0;
                    }
                    listChanged = true;
                    Serial.printf("[MISSION] Lista de rovers atualizada pelo Portal (%u rovers)\n", rovers.size());

                    // Transição automática para IDLE se estava à espera de lista
                    if (_opState == BASE_OP_WAITING_ROVERS) {
                        setOperationState(BASE_OP_IDLE, false);
                        Serial.println("[MISSION] Transição automatica: WAITING_ROVERS -> IDLE (Lista pronta)");
                    }
                }
            }
        }

        // Se o portal instruiu uma mudança de estado explícita
        if (opStateStr && strlen(opStateStr) > 0) {
            setOperationStateFromStr(opStateStr, false);
        }

        // Só salvar em MicroSD / NVS se realmente mudou a lista ou metadados da missão
        if (listChanged || metaChanged) {
            saveMissionConfig();
        }
    }

    // --- Persistência: MicroSD e Memória NVS ---
    bool loadMissionConfig() {
        bool loaded = false;

        // 1. Tentar ler do MicroSD (/config/rovers.json ou /rovers.json)
        if (_sd && _sd->isReady()) {
            const char* path = _sd->fileExists("/config/rovers.json") ? "/config/rovers.json" : 
                               (_sd->fileExists("/rovers.json") ? "/rovers.json" : nullptr);

            if (path) {
                String jsonStr = _sd->readFile(path, 8192);
                if (jsonStr.length() > 0) {
                    JsonDocument doc;
                    DeserializationError err = deserializeJson(doc, jsonStr);
                    if (!err) {
                        if (doc["operation_state"].is<const char*>()) {
                            _opState = parse_base_op_state(doc["operation_state"].as<const char*>());
                        }
                        if (doc["race_id"].is<const char*>()) {
                            strncpy(_raceId, doc["race_id"].as<const char*>(), sizeof(_raceId) - 1);
                        }
                        if (doc["mission_name"].is<const char*>()) {
                            strncpy(_missionName, doc["mission_name"].as<const char*>(), sizeof(_missionName) - 1);
                        }
                        if (doc["source"].is<const char*>() && strcmp(doc["source"].as<const char*>(), "portal") == 0) {
                            _roversFromPortal = true;
                        }

                        JsonArray arr = doc["rovers"].as<JsonArray>();
                        if (!arr.isNull() && arr.size() > 0) {
                            rovers.clear();
                            for (JsonObject rObj : arr) {
                                FleetRover r;
                                memset(&r, 0, sizeof(r));
                                r.id = rObj["id"] | (rovers.size() + 1);
                                snprintf(r.code, sizeof(r.code), "ROVER-%02d", r.id);
                                strncpy(r.name, rObj["name"] | "", sizeof(r.name) - 1);
                                strncpy(r.mac, rObj["mac_address"] | (rObj["mac"] | ""), sizeof(r.mac) - 1);
                                strncpy(r.role, rObj["role"] | "", sizeof(r.role) - 1);
                                r.target_lat = rObj["lat"] | 0.0;
                                r.target_lng = rObj["lng"] | 0.0;
                                r.battery_voltage = 12.0f;
                                r.is_online = false;
                                rovers.push_back(r);
                            }
                        }
                        loaded = true;
                        Serial.printf("[MISSION] Carregado de %s (SD): %u rovers | Estado: %s\n", 
                                      path, rovers.size(), get_base_op_state_str(_opState));
                    }
                }
            }
        }

        // 2. Se não carregou do SD, carregar da memória NVS
        if (!loaded) {
            Preferences prefs;
            if (prefs.begin("base_mission", true)) {
                _opState = static_cast<BaseOperationState>(prefs.getUChar("op_state", BASE_OP_STORAGE));
                String rId = prefs.getString("race_id", "REGATA-DEFAULT");
                String mName = prefs.getString("m_name", "Percurso de Regata");
                strncpy(_raceId, rId.c_str(), sizeof(_raceId) - 1);
                strncpy(_missionName, mName.c_str(), sizeof(_missionName) - 1);
                uint8_t count = prefs.getUChar("r_count", 0);
                if (count > 0 && count <= MAX_FLEET_ROVERS) {
                    rovers.clear();
                    for (uint8_t i = 0; i < count; ++i) {
                        char prefix[16];
                        snprintf(prefix, sizeof(prefix), "r%d_", i);
                        FleetRover r;
                        memset(&r, 0, sizeof(r));
                        r.id = prefs.getUChar((String(prefix) + "id").c_str(), i + 1);
                        snprintf(r.code, sizeof(r.code), "ROVER-%02d", r.id);
                        strncpy(r.name, prefs.getString((String(prefix) + "name").c_str(), "").c_str(), sizeof(r.name) - 1);
                        strncpy(r.mac, prefs.getString((String(prefix) + "mac").c_str(), "").c_str(), sizeof(r.mac) - 1);
                        strncpy(r.role, prefs.getString((String(prefix) + "role").c_str(), "").c_str(), sizeof(r.role) - 1);
                        r.battery_voltage = 12.0f;
                        r.is_online = false;
                        rovers.push_back(r);
                    }
                    loaded = true;
                    _roversFromPortal = prefs.getBool("from_portal", false);
                    Serial.printf("[MISSION] Carregado da memoria NVS: %u rovers | Estado: %s\n", 
                                  rovers.size(), get_base_op_state_str(_opState));
                }
                prefs.end();
            }
        }

        // Se não carregou nenhum rover (sem SD ou NVS vazia), inicializar slot padrão ROVER-01
        if (rovers.empty()) {
            FleetRover r;
            memset(&r, 0, sizeof(r));
            r.id = 1;
            snprintf(r.code, sizeof(r.code), "ROVER-01");
            snprintf(r.name, sizeof(r.name), "Rover 01");
            snprintf(r.mac, sizeof(r.mac), "00:00:00:00:00:01");
            strncpy(r.role, "largada CR", sizeof(r.role) - 1);
            r.battery_voltage = 12.0f;
            r.is_online = false;
            rovers.push_back(r);
        }

        // Selecionar automaticamente o primeiro rover caso nenhum esteja selecionado
        if (!hasSelectedRover() && !rovers.empty()) {
            selectedRoverIndex = 0;
        }

        // Se ainda não existia configuração e o SD está disponível, salvar inicial
        if (!loaded && _sd && _sd->isReady()) {
            saveMissionConfig();
        }

        return loaded;
    }

    bool saveMissionConfig() {
        // 1. Guardar na memória Flash NVS
        Preferences prefs;
        if (prefs.begin("base_mission", false)) {
            prefs.putUChar("op_state", static_cast<uint8_t>(_opState));
            prefs.putString("race_id", _raceId);
            prefs.putString("m_name", _missionName);
            prefs.putBool("from_portal", _roversFromPortal);
            prefs.putUChar("r_count", static_cast<uint8_t>(rovers.size()));
            for (size_t i = 0; i < rovers.size(); ++i) {
                char prefix[16];
                snprintf(prefix, sizeof(prefix), "r%d_", (int)i);
                prefs.putUChar((String(prefix) + "id").c_str(), rovers[i].id);
                prefs.putString((String(prefix) + "name").c_str(), rovers[i].name);
                prefs.putString((String(prefix) + "mac").c_str(), rovers[i].mac);
                prefs.putString((String(prefix) + "role").c_str(), rovers[i].role);
            }
            prefs.end();
        }

        // 2. Guardar no Cartão MicroSD (/config/rovers.json)
        if (_sd && _sd->isReady()) {
            // Garantir que a directoria /config existe
            if (!_sd->fileExists("/config")) {
                _sd->createDir("/config");
            }

            JsonDocument doc;
            doc["operation_state"] = get_base_op_state_str(_opState);
            doc["race_id"] = _raceId;
            doc["mission_name"] = _missionName;
            doc["source"] = _roversFromPortal ? "portal" : "manual";
            doc["updated_at"] = millis();

            JsonArray arr = doc["rovers"].to<JsonArray>();
            for (const auto &r : rovers) {
                JsonObject rObj = arr.add<JsonObject>();
                rObj["id"] = r.id;
                rObj["name"] = r.name;
                rObj["mac_address"] = r.mac;
                rObj["role"] = r.role;
                rObj["lat"] = r.target_lat;
                rObj["lng"] = r.target_lng;
            }

            String jsonStr;
            serializeJsonPretty(doc, jsonStr);
            jsonStr += "\n";

            bool ok = _sd->writeFile("/config/rovers.json", jsonStr.c_str());
            if (!ok) {
                // Tenta gravar na raiz caso /config falhe
                ok = _sd->writeFile("/rovers.json", jsonStr.c_str());
                if (ok) {
                    Serial.println("[MISSION] Ficheiro /rovers.json guardado na raiz do MicroSD");
                }
            } else {
                Serial.println("[MISSION] Ficheiro /config/rovers.json guardado com sucesso no MicroSD!");
            }
            return ok;
        }

        return true;
    }

private:
    SDCardManager *_sd;
    BaseOperationState _opState;
    char _raceId[32];
    char _missionName[48];

    std::vector<FleetRover> rovers;
    std::vector<QueuedCommand> commandQueue;
    int16_t selectedRoverIndex;
    bool _roversFromPortal;
};
