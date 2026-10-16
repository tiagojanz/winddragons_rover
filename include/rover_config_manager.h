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
    uint8_t screenBrightness;   // 0 - 100%
    uint8_t batteryCells;       // 3, 4, or 6
    uint16_t batteryCapacityMah; // e.g. 2650 mAh
    float anchorDrumDiamMm;     // Diâmetro do tambor do guincho da âncora (mm, ex: 30.0)
    uint8_t anchorPulsesPerRev; // Número de pulsos/ímanes por volta do tambor (ex: 1)
};

/**
 * RoverConfigManager - Gere as configurações principais do Rover (ID, SSID, Password AP, Brilho Ecrã, Bateria, Âncora)
 * Sincroniza bidirecionalmente entre o Cartão MicroSD (/config.json) e a memória NVS.
 */
class RoverConfigManager {
public:
    RoverConfigManager() : _sdCard(nullptr) {
        _config.roverId = 1;
        _config.apSsid = "WindDragons-Rover-01";
        _config.apPassword = "password123";
        _config.screenBrightness = 85;
        _config.batteryCells = 4;
        _config.batteryCapacityMah = 2650;
        _config.anchorDrumDiamMm = 30.0f;
        _config.anchorPulsesPerRev = 1;
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
    uint8_t getScreenBrightness() const { return _config.screenBrightness; }
    uint8_t getBatteryCells() const { return _config.batteryCells; }
    uint16_t getBatteryCapacityMah() const { return _config.batteryCapacityMah; }
    float getAnchorDrumDiamMm() const { return _config.anchorDrumDiamMm; }
    uint8_t getAnchorPulsesPerRev() const { return _config.anchorPulsesPerRev; }

    void setRoverId(uint8_t id) {
        _config.roverId = id;
    }

    void setApSsid(const String &ssid) {
        _config.apSsid = ssid;
    }

    void setApPassword(const String &password) {
        _config.apPassword = password;
    }

    void setScreenBrightness(uint8_t brightness) {
        if (brightness > 100) brightness = 100;
        _config.screenBrightness = brightness;
    }

    void setBatteryCells(uint8_t cells) {
        if (cells == 3 || cells == 4 || cells == 6) {
            _config.batteryCells = cells;
        }
    }

    void setBatteryCapacityMah(uint16_t mah) {
        if (mah >= 500 && mah <= 50000) {
            _config.batteryCapacityMah = mah;
        }
    }

    void setAnchorCalibration(float diamMm, uint8_t pulsesPerRev) {
        if (diamMm >= 5.0f && diamMm <= 500.0f) _config.anchorDrumDiamMm = diamMm;
        if (pulsesPerRev >= 1 && pulsesPerRev <= 24) _config.anchorPulsesPerRev = pulsesPerRev;
        saveConfig();
    }

    void updateConfig(uint8_t id, const String &ssid, const String &password, uint8_t brightness = 255, uint8_t cells = 0, uint16_t capacityMah = 0) {
        _config.roverId = (id > 0) ? id : 1;
        if (ssid.length() > 0) _config.apSsid = ssid;
        if (password.length() >= 8) _config.apPassword = password;
        if (brightness <= 100) {
            _config.screenBrightness = brightness;
        }
        if (cells == 3 || cells == 4 || cells == 6) {
            _config.batteryCells = cells;
        }
        if (capacityMah >= 500 && capacityMah <= 50000) {
            _config.batteryCapacityMah = capacityMah;
        }
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
            _config.screenBrightness = prefs.getUChar("bright", 85);
            _config.batteryCells = prefs.getUChar("cells", 4);
            if (_config.batteryCells != 3 && _config.batteryCells != 4 && _config.batteryCells != 6) {
                _config.batteryCells = 4;
            }
            _config.batteryCapacityMah = prefs.getUShort("cap_mah", 2650);
            if (_config.batteryCapacityMah < 500 || _config.batteryCapacityMah > 50000) {
                _config.batteryCapacityMah = 2650;
            }
            _config.anchorDrumDiamMm = prefs.getFloat("anc_diam", 30.0f);
            if (_config.anchorDrumDiamMm < 5.0f || _config.anchorDrumDiamMm > 500.0f) {
                _config.anchorDrumDiamMm = 30.0f;
            }
            _config.anchorPulsesPerRev = prefs.getUChar("anc_pulses", 1);
            if (_config.anchorPulsesPerRev < 1 || _config.anchorPulsesPerRev > 24) {
                _config.anchorPulsesPerRev = 1;
            }
            prefs.end();
            Serial.printf("[CONFIG] Carregado da memoria NVS -> ID: %u | SSID AP: %s | Brilho: %u%% | Bateria: %uS (%u mAh) | Âncora: Tambor=%.1fmm (%u p/rev)\n", 
                          _config.roverId, _config.apSsid.c_str(), _config.screenBrightness, _config.batteryCells, _config.batteryCapacityMah,
                          _config.anchorDrumDiamMm, _config.anchorPulsesPerRev);
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
        prefs.putUChar("bright", _config.screenBrightness);
        prefs.putUChar("cells", _config.batteryCells);
        prefs.putUShort("cap_mah", _config.batteryCapacityMah);
        prefs.putFloat("anc_diam", _config.anchorDrumDiamMm);
        prefs.putUChar("anc_pulses", _config.anchorPulsesPerRev);
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
        if (doc["screen_brightness"].is<uint8_t>()) {
            _config.screenBrightness = doc["screen_brightness"].as<uint8_t>();
        } else if (doc["brightness"].is<uint8_t>()) {
            _config.screenBrightness = doc["brightness"].as<uint8_t>();
        }
        if (_config.screenBrightness > 100) _config.screenBrightness = 100;

        if (doc["battery_cells"].is<uint8_t>()) {
            uint8_t c = doc["battery_cells"].as<uint8_t>();
            if (c == 3 || c == 4 || c == 6) _config.batteryCells = c;
        } else if (doc["cells"].is<uint8_t>()) {
            uint8_t c = doc["cells"].as<uint8_t>();
            if (c == 3 || c == 4 || c == 6) _config.batteryCells = c;
        }

        if (doc["battery_capacity_mah"].is<uint16_t>()) {
            uint16_t m = doc["battery_capacity_mah"].as<uint16_t>();
            if (m >= 500 && m <= 50000) _config.batteryCapacityMah = m;
        } else if (doc["capacity_mah"].is<uint16_t>()) {
            uint16_t m = doc["capacity_mah"].as<uint16_t>();
            if (m >= 500 && m <= 50000) _config.batteryCapacityMah = m;
        }

        if (doc["anchor_drum_diam_mm"].is<float>()) {
            float d = doc["anchor_drum_diam_mm"].as<float>();
            if (d >= 5.0f && d <= 500.0f) _config.anchorDrumDiamMm = d;
        }
        if (doc["anchor_pulses_per_rev"].is<uint8_t>()) {
            uint8_t p = doc["anchor_pulses_per_rev"].as<uint8_t>();
            if (p >= 1 && p <= 24) _config.anchorPulsesPerRev = p;
        }

        Serial.printf("[CONFIG] Carregado de /config.json (MicroSD) -> ID: %u | SSID AP: %s | Brilho: %u%% | Bateria: %uS (%u mAh) | Âncora: Tambor=%.1fmm (%u p/rev)\n", 
                      _config.roverId, _config.apSsid.c_str(), _config.screenBrightness, _config.batteryCells, _config.batteryCapacityMah,
                      _config.anchorDrumDiamMm, _config.anchorPulsesPerRev);
        return true;
    }

    bool saveConfigToSD() {
        if (!_sdCard || !_sdCard->isReady()) return false;

        JsonDocument doc;
        doc["rover_id"] = _config.roverId;
        doc["ap_ssid"] = _config.apSsid;
        doc["ap_password"] = _config.apPassword;
        doc["screen_brightness"] = _config.screenBrightness;
        doc["battery_cells"] = _config.batteryCells;
        doc["battery_capacity_mah"] = _config.batteryCapacityMah;
        doc["anchor_drum_diam_mm"] = _config.anchorDrumDiamMm;
        doc["anchor_pulses_per_rev"] = _config.anchorPulsesPerRev;

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
