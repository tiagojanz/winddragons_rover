#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "sd_card_manager.h"
#include "config_common.h"

/**
 * Estrutura de configuração geral do Rover
 */
struct RoverConfig {
    uint8_t roverId;
    String apSsid;
    String apPassword;
};

/**
 * RoverConfigManager - Gere as configurações principais do Rover (ID, SSID, Password AP)
 * Sincroniza bidirecionalmente entre o Cartão MicroSD (/config.json) e a memória NVS.
 */
class RoverConfigManager {
public:
    RoverConfigManager() : _sdCard(nullptr) {
        _config.roverId = 1;
        _config.apSsid = "WindDragons-Rover-01";
        _config.apPassword = "password123";
    }

    void begin(SDCardManager *sd = nullptr) {
        _sdCard = sd;
        loadConfig();
    }

    void setSDCardManager(SDCardManager *sd) {
        _sdCard = sd;
        if (_sdCard && _sdCard->isReady()) {
            if (_sdCard->fileExists("/config.json")) {
                loadConfigFromSD();
            } else {
                saveConfigToSD();
            }
        }
    }

    RoverConfig getConfig() const { return _config; }
    uint8_t getRoverId() const { return _config.roverId; }
    String getApSsid() const { return _config.apSsid; }
    String getApPassword() const { return _config.apPassword; }

    void setRoverId(uint8_t id) {
        _config.roverId = id;
    }

    void setApSsid(const String &ssid) {
        _config.apSsid = ssid;
    }

    void setApPassword(const String &password) {
        _config.apPassword = password;
    }

    void updateConfig(uint8_t id, const String &ssid, const String &password) {
        _config.roverId = (id > 0) ? id : 1;
        if (ssid.length() > 0) _config.apSsid = ssid;
        if (password.length() >= 8) _config.apPassword = password;
        saveConfig();
    }

    bool loadConfig() {
        bool loadedFromSD = false;

        // 1. Tentar ler primeiro do MicroSD (/config.json)
        if (_sdCard && _sdCard->isReady() && _sdCard->fileExists("/config.json")) {
            loadedFromSD = loadConfigFromSD();
        }

        // 2. Se não carregou do SD, carregar da memória não-volátil NVS
        if (!loadedFromSD) {
            Preferences prefs;
            prefs.begin("rover_cfg", true);
            _config.roverId = prefs.getUChar("id", 1);
            _config.apSsid = prefs.getString("ssid", "WindDragons-Rover-01");
            _config.apPassword = prefs.getString("pass", "password123");
            prefs.end();
            Serial.printf("[CONFIG] Carregado da memoria NVS -> ID: %u | SSID AP: %s\n", 
                          _config.roverId, _config.apSsid.c_str());
        }

        // 3. Se o cartão SD estiver presente mas ainda não possuir o /config.json, criar com valores padrão
        if (_sdCard && _sdCard->isReady() && !_sdCard->fileExists("/config.json")) {
            saveConfigToSD();
        }

        return true;
    }

    bool saveConfig() {
        // Guarda na memória Flash (NVS)
        Preferences prefs;
        prefs.begin("rover_cfg", false);
        prefs.putUChar("id", _config.roverId);
        prefs.putString("ssid", _config.apSsid);
        prefs.putString("pass", _config.apPassword);
        prefs.end();

        // Guarda no Cartão MicroSD
        if (_sdCard && _sdCard->isReady()) {
            saveConfigToSD();
        }
        return true;
    }

    bool loadConfigFromSD() {
        if (!_sdCard || !_sdCard->isReady() || !_sdCard->fileExists("/config.json")) return false;

        String jsonStr = _sdCard->readFile("/config.json", 2048);
        if (jsonStr.length() == 0) return false;

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, jsonStr);
        if (err) {
            Serial.printf("[CONFIG] Erro ao interpretar /config.json: %s\n", err.c_str());
            return false;
        }

        if (doc["rover_id"].is<uint8_t>()) {
            _config.roverId = doc["rover_id"].as<uint8_t>();
        }
        if (doc["ap_ssid"].is<const char*>()) {
            _config.apSsid = doc["ap_ssid"].as<String>();
        } else if (doc["ssid"].is<const char*>()) {
            _config.apSsid = doc["ssid"].as<String>();
        }
        if (doc["ap_password"].is<const char*>()) {
            _config.apPassword = doc["ap_password"].as<String>();
        } else if (doc["password"].is<const char*>()) {
            _config.apPassword = doc["password"].as<String>();
        }

        Serial.printf("[CONFIG] Carregado de /config.json (MicroSD) -> ID: %u | SSID AP: %s\n", 
                      _config.roverId, _config.apSsid.c_str());
        return true;
    }

    bool saveConfigToSD() {
        if (!_sdCard || !_sdCard->isReady()) return false;

        JsonDocument doc;
        doc["rover_id"] = _config.roverId;
        doc["ap_ssid"] = _config.apSsid;
        doc["ap_password"] = _config.apPassword;

        String jsonStr;
        serializeJsonPretty(doc, jsonStr);
        jsonStr += "\n";

        bool ok = _sdCard->writeFile("/config.json", jsonStr.c_str());
        if (ok) {
            Serial.println("[CONFIG] Ficheiro /config.json gravado com sucesso no MicroSD!");
        } else {
            Serial.println("[CONFIG] Erro ao gravar /config.json no MicroSD");
        }
        return ok;
    }

private:
    SDCardManager *_sdCard;
    RoverConfig _config;
};
