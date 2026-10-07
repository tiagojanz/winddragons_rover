#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <vector>
#include <functional>
#include <ArduinoJson.h>
#include "network_config.h"
#include "sd_card_manager.h"

struct SavedNetwork {
    String ssid;
    String password;
    bool is_default;
};

class WiFiConfigManager {
public:
    WiFiConfigManager() : _isApMode(false), _apSsid("WindDragons-Base"), _apPass("12345678"), _sdCard(nullptr) {}

    void begin(SDCardManager *sd = nullptr) {
        if (sd) _sdCard = sd;
        loadNetworks();
    }

    void setSDCardManager(SDCardManager *sd) {
        _sdCard = sd;
        // Se o cartao acabou de ser detetado, sincronizar redes
        if (_sdCard && _sdCard->isReady()) {
            if (_sdCard->fileExists("/wifi_networks.json")) {
                loadNetworksFromSD();
            } else if (!_networks.empty()) {
                saveNetworksToSD();
            }
        }
    }

    void loadNetworks() {
        _networks.clear();

        // 1. Tentar carregar primeiro do cartao SD (se disponivel e com ficheiro presente)
        bool loadedFromSD = false;
        if (_sdCard && _sdCard->isReady() && _sdCard->fileExists("/wifi_networks.json")) {
            loadedFromSD = loadNetworksFromSD();
        }

        // 2. Se nao carregou do SD, carregar da memoria NVS
        if (!loadedFromSD) {
            Preferences prefs;
            prefs.begin("wind_wifi", true); // read-only
            int count = prefs.getInt("count", 0);

            for (int i = 0; i < count; ++i) {
                String kSsid = "s" + String(i);
                String kPass = "p" + String(i);
                String kDef  = "d" + String(i);

                SavedNetwork net;
                net.ssid = prefs.getString(kSsid.c_str(), "");
                net.password = prefs.getString(kPass.c_str(), "");
                net.is_default = prefs.getBool(kDef.c_str(), false);

                if (net.ssid.length() > 0) {
                    _networks.push_back(net);
                }
            }
            prefs.end();
        }

        // 3. Se nenhuma rede existir, inicializar com a rede padrao do firmware
        if (_networks.empty()) {
            SavedNetwork defaultNet;
            defaultNet.ssid = WIFI_SSID;
            defaultNet.password = WIFI_PASSWORD;
            defaultNet.is_default = true;
            _networks.push_back(defaultNet);
            saveNetworks();
        } else if (loadedFromSD) {
            // Guardar tambem na NVS para redundancia
            saveNetworksToNVS();
        } else if (_sdCard && _sdCard->isReady()) {
            // Guardar no SD para sincronizar
            saveNetworksToSD();
        }
    }

    void saveNetworks() {
        saveNetworksToNVS();
        saveNetworksToSD();
    }

    void saveNetworksToNVS() {
        Preferences prefs;
        prefs.begin("wind_wifi", false); // read-write
        prefs.clear();
        prefs.putInt("count", _networks.size());

        for (size_t i = 0; i < _networks.size(); ++i) {
            String kSsid = "s" + String(i);
            String kPass = "p" + String(i);
            String kDef  = "d" + String(i);

            prefs.putString(kSsid.c_str(), _networks[i].ssid);
            prefs.putString(kPass.c_str(), _networks[i].password);
            prefs.putBool(kDef.c_str(), _networks[i].is_default);
        }
        prefs.end();
    }

    void saveNetworksToSD() {
        if (!_sdCard || !_sdCard->isReady()) return;

        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();

        for (const auto &n : _networks) {
            JsonObject obj = arr.add<JsonObject>();
            obj["ssid"] = n.ssid;
            obj["password"] = n.password;
            obj["is_default"] = n.is_default;
        }

        String jsonStr;
        serializeJsonPretty(doc, jsonStr);
        if (_sdCard->writeFile("/wifi_networks.json", jsonStr.c_str())) {
            Serial.println("[WIFI] Redes guardadas no cartao SD (/wifi_networks.json)");
        } else {
            Serial.println("[WIFI] Falha ao gravar redes no cartao SD");
        }
    }

    bool loadNetworksFromSD() {
        if (!_sdCard || !_sdCard->isReady()) return false;
        if (!_sdCard->fileExists("/wifi_networks.json")) return false;

        String content = _sdCard->readFile("/wifi_networks.json", 8192);
        if (content.length() == 0) return false;

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, content);
        if (err || !doc.is<JsonArray>()) {
            Serial.printf("[WIFI] Erro ao interpretar /wifi_networks.json: %s\n", err.c_str());
            return false;
        }

        JsonArray arr = doc.as<JsonArray>();
        if (arr.size() == 0) return false;

        _networks.clear();
        for (JsonObject obj : arr) {
            SavedNetwork net;
            net.ssid = obj["ssid"] | "";
            net.password = obj["password"] | "";
            net.is_default = obj["is_default"] | false;
            if (net.ssid.length() > 0) {
                _networks.push_back(net);
            }
        }

        Serial.printf("[WIFI] %u redes carregadas com sucesso do cartao SD!\n", _networks.size());
        return true;
    }

    const std::vector<SavedNetwork>& getNetworks() const {
        return _networks;
    }

    bool addOrUpdateNetwork(const String &ssid, const String &password, bool makeDefault = false) {
        if (ssid.length() == 0) return false;

        // Se for para ser default, desmarcar as outras
        if (makeDefault) {
            for (auto &n : _networks) {
                n.is_default = false;
            }
        }

        bool found = false;
        for (auto &n : _networks) {
            if (n.ssid.equalsIgnoreCase(ssid)) {
                n.password = password;
                if (makeDefault) n.is_default = true;
                found = true;
                break;
            }
        }

        if (!found) {
            SavedNetwork net;
            net.ssid = ssid;
            net.password = password;
            net.is_default = makeDefault || (_networks.empty());
            _networks.push_back(net);
        }

        saveNetworks();
        return true;
    }

    bool setDefault(const String &ssid) {
        bool found = false;
        for (auto &n : _networks) {
            if (n.ssid.equalsIgnoreCase(ssid)) {
                n.is_default = true;
                found = true;
            } else {
                n.is_default = false;
            }
        }
        if (found) {
            saveNetworks();
        }
        return found;
    }

    bool removeNetwork(const String &ssid) {
        bool removed = false;
        for (auto it = _networks.begin(); it != _networks.end(); ) {
            if (it->ssid.equalsIgnoreCase(ssid)) {
                it = _networks.erase(it);
                removed = true;
            } else {
                ++it;
            }
        }
        if (removed) {
            // Se nenhuma ficou como default e a lista nao esta vazia, marcar a primeira
            bool hasDef = false;
            for (const auto &n : _networks) {
                if (n.is_default) { hasDef = true; break; }
            }
            if (!hasDef && !_networks.empty()) {
                _networks[0].is_default = true;
            }
            saveNetworks();
        }
        return removed;
    }

    /**
     * Tenta conectar-se ao WiFi: primeiro a rede default, e se falhar, as outras.
     * Retorna true se conectou, false se todas falharam.
     */
    bool autoConnect(uint32_t timeoutPerNetworkMs = 10000, std::function<void(const char* ssid)> onAttempt = nullptr) {
        WiFi.mode(WIFI_STA);
        delay(50);
        WiFi.disconnect(false);
        delay(100);

        // 1. Tentar primeiro a rede marcada como default
        for (const auto &n : _networks) {
            if (n.ssid == "WIFI_NETWORK_NAME" || n.ssid.isEmpty()) continue;
            if (n.is_default) {
                Serial.printf("[WIFI] A tentar ligar a rede DEFAULT: %s ...\n", n.ssid.c_str());
                if (onAttempt) onAttempt(n.ssid.c_str());
                if (attemptConnection(n.ssid, n.password, timeoutPerNetworkMs)) {
                    _isApMode = false;
                    _connectedSsid = n.ssid;
                    return true;
                }
                Serial.println("[WIFI] Falhou a ligacao a rede default.");
                break;
            }
        }

        // 2. Tentar as restantes redes guardadas
        for (const auto &n : _networks) {
            if (n.ssid == "WIFI_NETWORK_NAME" || n.ssid.isEmpty()) continue;
            if (n.is_default) continue; // ja tentada
            Serial.printf("[WIFI] A tentar ligar a rede alternativa: %s ...\n", n.ssid.c_str());
            if (onAttempt) onAttempt(n.ssid.c_str());
            if (attemptConnection(n.ssid, n.password, timeoutPerNetworkMs)) {
                _isApMode = false;
                _connectedSsid = n.ssid;
                return true;
            }
            Serial.println("[WIFI] Falhou ligacao.");
        }

        return false;
    }

    /**
     * Tenta conectar-se diretamente a uma rede guardada especificada pelo seu SSID.
     * Retorna true se conectou, false se falhou ou nao foi encontrada.
     */
    bool connectTo(const String &ssid, uint32_t timeoutMs = 10000) {
        String pass = "";
        bool found = false;
        for (const auto &n : _networks) {
            if (n.ssid.equalsIgnoreCase(ssid)) {
                pass = n.password;
                found = true;
                break;
            }
        }

        if (!found) {
            Serial.printf("[WIFI] Rede '%s' nao encontrada na lista de guardadas!\n", ssid.c_str());
            return false;
        }

        WiFi.mode(WIFI_STA);
        delay(50);
        WiFi.disconnect(false);
        delay(100);

        Serial.printf("[WIFI] A tentar ligar manualmente a: %s ...\n", ssid.c_str());
        if (attemptConnection(ssid, pass, timeoutMs)) {
            _isApMode = false;
            _connectedSsid = ssid;
            Serial.printf("[WIFI] Conectado com sucesso a %s! IP: %s\n", ssid.c_str(), WiFi.localIP().toString().c_str());
            return true;
        }

        Serial.printf("[WIFI] Falhou a ligacao a %s.\n", ssid.c_str());
        return false;
    }

    bool isConnectedTo(const String &ssid) const {
        return isConnected() && getSSID().equalsIgnoreCase(ssid);
    }

    /**
     * Inicia o modo Ponto de Acesso (Access Point)
     */
    void startAccessPoint(const char* ssid = nullptr, const char* pass = nullptr) {
        if (ssid) _apSsid = ssid;
        if (pass) _apPass = pass;

        WiFi.mode(WIFI_AP);
        delay(50);
        WiFi.disconnect(false);
        delay(50);

        // IP padrão 192.168.4.1
        IPAddress apIP(192, 168, 4, 1);
        IPAddress gateway(192, 168, 4, 1);
        IPAddress subnet(255, 255, 255, 0);
        WiFi.softAPConfig(apIP, gateway, subnet);

        bool ok = WiFi.softAP(_apSsid.c_str(), _apPass.length() > 0 ? _apPass.c_str() : nullptr);
        _isApMode = true;
        _connectedSsid = _apSsid;

        Serial.println("==========================================");
        Serial.printf("[WIFI] MODO ACCESS POINT INICIADO (%s)\n", ok ? "OK" : "ERRO");
        Serial.printf(" SSID: %s\n", _apSsid.c_str());
        Serial.printf(" Password: %s\n", _apPass.length() > 0 ? _apPass.c_str() : "(Aberta)");
        Serial.printf(" IP: %s\n", WiFi.softAPIP().toString().c_str());
        Serial.println("==========================================");
    }

    bool isAPMode() const { return _isApMode; }
    bool isConnected() const { return !_isApMode && (WiFi.status() == WL_CONNECTED); }

    String getIPAddress() const {
        if (_isApMode) {
            return WiFi.softAPIP().toString();
        } else if (WiFi.status() == WL_CONNECTED) {
            return WiFi.localIP().toString();
        }
        return "0.0.0.0";
    }

    String getSSID() const {
        if (_isApMode) {
            return _apSsid;
        } else if (WiFi.status() == WL_CONNECTED) {
            return _connectedSsid.length() > 0 ? _connectedSsid : WiFi.SSID();
        }
        return "Desconectado";
    }

    int8_t getRSSI() const {
        if (_isApMode) return 0;
        return WiFi.RSSI();
    }

    uint8_t getAPStationCount() const {
        if (!_isApMode) return 0;
        return WiFi.softAPgetStationNum();
    }

    String getAPPassword() const {
        return _apPass;
    }

private:
    bool _isApMode;
    String _apSsid;
    String _apPass;
    String _connectedSsid;
    std::vector<SavedNetwork> _networks;
    SDCardManager *_sdCard;

    bool attemptConnection(const String &ssid, const String &pass, uint32_t timeoutMs) {
        // Assegurar cancelamento de qualquer ligacao anterior pendente
        WiFi.disconnect(false);
        delay(100);

        WiFi.begin(ssid.c_str(), pass.c_str());
        uint32_t start = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - start < timeoutMs)) {
            delay(250);
            Serial.print(".");
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            return true;
        }

        // Interromper a tentativa pendente para libertar o estado do ESP-IDF antes da proxima rede
        WiFi.disconnect(false);
        delay(100);
        return false;
    }
};
