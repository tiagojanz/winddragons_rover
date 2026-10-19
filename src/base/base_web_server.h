#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <Update.h>
#include <ArduinoJson.h>
#include "wifi_config_manager.h"
#include "fleet_manager.h"
#include "sd_card_manager.h"
#include "lora_protocol.h"
#include "network_config.h"
#include "base_network.h"
#include "qrcode_helper.h"
#include <esp_mac.h>
#include <Preferences.h>
#include "base_display.h"
#include "base_version.h"

class BaseWebServer {
public:
    BaseWebServer(WiFiConfigManager &wifiMgr, FleetManager &fleet, SDCardManager *sdCard = nullptr, BaseNetwork *network = nullptr)
        : _server(80), _wifi(wifiMgr), _fleet(fleet), _sd(sdCard), _network(network),
          _webControlActive(false), _webThrottle(0), _webRudder(0), 
          _webAnchorJog(0), _lastWebControlTime(0), _nextCmdId(2000),
          _isBusy(false), _uploadSuccess(false), _uploadCurrentPath(""),
          _display(nullptr) {}

    String getMacAddress() {
        String mac = WiFi.macAddress();
        if (mac.isEmpty() || mac == "00:00:00:00:00:00") {
            uint8_t baseMac[6];
            esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
            char macStr[18];
            snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                     baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
            mac = String(macStr);
        }
        return mac;
    }

    void begin() {
        setupRoutes();
        _server.begin();
        Serial.println("[BASE HTTP] Servidor Web ativo na porta 80");
    }

    void handleClient() {
        _server.handleClient();

        // Timeout de seguranca para controle web manual (2 segundos sem novo comando -> neutro)
        if (_webControlActive && (millis() - _lastWebControlTime > 2000)) {
            _webThrottle = 0;
            _webRudder = 0;
            _webAnchorJog = 0;
            _webControlActive = false;
        }

        // Timeout de seguranca para OTA do C6 (35 segundos sem progresso -> cancelar estado e restaurar ecrã)
        bool isOtaInProgress = (_c6OtaState != "IDLE" && 
                                _c6OtaState != "SUCCESS" && 
                                _c6OtaState != "ERROR" && 
                                _c6OtaState != "FAIL_TIMEOUT" &&
                                strncmp(_c6OtaState.c_str(), "FAIL", 4) != 0);
        if (isOtaInProgress && _c6OtaTimestamp > 0 && (millis() - _c6OtaTimestamp > 35000)) {
            Serial.println("[BASE OTA C6] Timeout: Sem resposta do Coprocessador C6 por 35s. A restaurar operacao normal.");
            _c6OtaState = "FAIL_TIMEOUT";
            _c6OtaDetail = "Timeout: Coprocessador C6 nao respondeu (desligado ou inacessivel)";
            _c6OtaTimestamp = 0;
            if (_display) {
                _display->showOtaScreen("COPROCESSADOR C6", "ERRO: TIMEOUT C6", 0, "C6 nao respondeu. Ecra restaurado.", UI_RED);
            }
            _otaAutoClearMs = millis() + 3000;
        }

        if (_otaAutoClearMs > 0 && millis() >= _otaAutoClearMs) {
            _otaAutoClearMs = 0;
            _isBusy = false;
            if (_display) _display->setOtaActive(false);
            _c6OtaState = "IDLE";
        }

        if (_restartScheduled > 0 && millis() >= _restartScheduled) {
            _restartScheduled = 0;
            delay(300);
            ESP.restart();
        }
    }

    typedef std::function<void(const char*)> CommandQueueCallback;
    void setCommandQueueCallback(CommandQueueCallback cb) {
        _commandQueueCb = cb;
    }

    void setC6OtaStatus(const char* status) {
        if (!status) return;
        _c6OtaTimestamp = millis();
        Serial.printf("[BASE HTTP] C6 OTA Estado: %s\n", status);

        if (strncmp(status, "PROGRESS:", 9) == 0) {
            int pct = atoi(status + 9);
            _c6OtaProgress = pct;
            _c6OtaState = "FLASHING";
            char detail[48];
            snprintf(detail, sizeof(detail), "A gravar Flash C6 (%d%%)...", pct);
            _c6OtaDetail = String(detail);
            if (_display) {
                _display->showOtaScreen("COPROCESSADOR C6", "PASSO 1/2: A GRAVAR FLASH", pct, detail, UI_BLUE);
            }
        } else if (strcmp(status, "STARTING") == 0) {
            _c6OtaState = "STARTING";
            _c6OtaProgress = 20;
            _c6OtaDetail = "C6 a inicializar modo OTA...";
            if (_display) {
                _display->showOtaScreen("COPROCESSADOR C6", "PASSO 1/2: PREPARACAO C6", 20, _c6OtaDetail.c_str(), UI_MAGENTA);
            }
        } else if (strcmp(status, "WIFI_CONNECTING") == 0) {
            _c6OtaState = "WIFI_CONNECTING";
            _c6OtaProgress = 25;
            _c6OtaDetail = "C6 a ligar ao WiFi...";
            if (_display) {
                _display->showOtaScreen("COPROCESSADOR C6", "PASSO 1/2: LIGAR AO WIFI", 25, _c6OtaDetail.c_str(), UI_ORANGE);
            }
        } else if (strcmp(status, "DOWNLOADING") == 0 || strcmp(status, "FLASH_START") == 0) {
            _c6OtaState = "DOWNLOADING";
            _c6OtaProgress = 30;
            _c6OtaDetail = "C6 a descarregar e a gravar Flash...";
            if (_display) {
                _display->showOtaScreen("COPROCESSADOR C6", "PASSO 1/2: A DESCARREGAR", 30, _c6OtaDetail.c_str(), UI_CYAN);
            }
        } else if (strcmp(status, "SUCCESS") == 0) {
            _c6OtaState = "SUCCESS";
            _c6OtaProgress = 100;
            _c6OtaDetail = "Coprocessador C6 atualizado com 100% de sucesso!";
            if (_display) {
                _display->showOtaScreen("COPROCESSADOR C6", "PASSO 1/2: SUCESSO!", 100, "C6 OK! A aguardar Master Lafvin...", UI_GREEN);
            }
        } else if (strncmp(status, "FAIL", 4) == 0) {
            _c6OtaState = String(status);
            _c6OtaDetail = "Falha na gravacao do Coprocessador C6";
            if (_display) {
                _display->showOtaScreen("COPROCESSADOR C6", "PASSO 1/2: FALHA NO C6", 0, status, UI_RED);
            }
            _otaAutoClearMs = millis() + 4000;
        } else {
            _c6OtaState = String(status);
            if (_display) {
                _display->showOtaScreen("COPROCESSADOR C6", "PASSO 1/2: ESTADO", _c6OtaProgress, status, UI_CYAN);
            }
        }
    }

    void setC6Version(const char* ver) {
        if (ver && strlen(ver) > 0) {
            _c6Version = String(ver);
        }
    }

    String getC6Version() const {
        return _c6Version;
    }

    void setSD(SDCardManager *sd) { _sd = sd; }
    void setDisplay(BaseDisplay *display) { _display = display; }
    void setLoraReadyPtr(bool *ready) { _loraReady = ready; }
    bool isBusy() const { return _isBusy; }
    bool isWebControlActive() const { return _webControlActive; }
    int8_t getWebThrottle() const { return _webThrottle; }
    int8_t getWebRudder() const { return _webRudder; }
    int8_t getWebAnchorJog() const { return _webAnchorJog; }

private:
    WebServer _server;
    WiFiConfigManager &_wifi;
    FleetManager &_fleet;
    SDCardManager *_sd;
    BaseNetwork *_network;
    BaseDisplay *_display;
    bool *_loraReady = nullptr;

    bool _webControlActive;
    int8_t _webThrottle;
    int8_t _webRudder;
    int8_t _webAnchorJog;
    uint32_t _lastWebControlTime;
    uint16_t _nextCmdId;

    File _uploadFile;
    bool _isBusy;
    bool _uploadSuccess;
    String _uploadCurrentPath;

    CommandQueueCallback _commandQueueCb = nullptr;
    String _c6Version = "Desconhecido";
    String _c6OtaState = "IDLE";
    String _c6OtaDetail = "";
    uint8_t _c6OtaProgress = 0;
    uint32_t _c6OtaTimestamp = 0;
    uint32_t _otaAutoClearMs = 0;
    size_t _lafvinOtaBytes = 0;
    uint32_t _restartScheduled = 0;

    bool handleStaticFile(String path = "") {
        if (!_sd || !_sd->isReady()) return false;

        if (path.isEmpty()) {
            path = _server.uri();
        }
        path = WebServer::urlDecode(path);
        int qIdx = path.indexOf('?');
        if (qIdx != -1) {
            path = path.substring(0, qIdx);
        }
        if (!path.startsWith("/")) path = "/" + path;

        if (!_sd->fileExists(path.c_str())) {
            return false;
        }

        _isBusy = true;
        _sd->prepareBus();
        File f = _sd->openFile(path.c_str(), FILE_READ);
        if (!f || f.isDirectory()) {
            if (f) f.close();
            _isBusy = false;
            return false;
        }

        String contentType = "application/octet-stream";
        if (path.endsWith(".html") || path.endsWith(".htm")) contentType = "text/html; charset=utf-8";
        else if (path.endsWith(".otf")) contentType = "font/otf";
        else if (path.endsWith(".ttf")) contentType = "font/ttf";
        else if (path.endsWith(".woff")) contentType = "font/woff";
        else if (path.endsWith(".woff2")) contentType = "font/woff2";
        else if (path.endsWith(".css")) contentType = "text/css; charset=utf-8";
        else if (path.endsWith(".js")) contentType = "application/javascript; charset=utf-8";
        else if (path.endsWith(".png")) contentType = "image/png";
        else if (path.endsWith(".jpg") || path.endsWith(".jpeg")) contentType = "image/jpeg";
        else if (path.endsWith(".svg")) contentType = "image/svg+xml";
        else if (path.endsWith(".ico")) contentType = "image/x-icon";
        else if (path.endsWith(".json")) contentType = "application/json";

        _server.sendHeader("Access-Control-Allow-Origin", "*");
        if (path.endsWith(".otf") || path.endsWith(".ttf") || path.endsWith(".woff") || path.endsWith(".woff2")) {
            _server.sendHeader("Cache-Control", "public, max-age=31536000, immutable");
        } else if (path.endsWith(".html") || path.endsWith(".htm") || path.endsWith(".css") || path.endsWith(".js")) {
            _server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
            _server.sendHeader("Pragma", "no-cache");
            _server.sendHeader("Expires", "0");
        }

        _server.client().setTimeout(30000);
        _server.streamFile(f, contentType);
        f.close();
        _isBusy = false;
        return true;
    }

    bool tryServeSdFile(const String &path) {
        if (!_sd || !_sd->isReady()) return false;

        // Na Base Station, páginas HTML devem vir estritamente de /www/base/
        if (path.startsWith("/www/") && !path.startsWith("/www/base/") && !path.startsWith("/www/rover/")) {
            String basePath = "/www/base/" + path.substring(5);
            if (_sd->fileExists(basePath.c_str())) {
                return handleStaticFile(basePath);
            }
            // Se for ficheiro HTML (.html), NÃO permitir fallback para /www/ raiz para evitar servir páginas do Rover
            if (path.endsWith(".html") || path.endsWith(".htm")) {
                return false;
            }
        }

        if (!_sd->fileExists(path.c_str())) {
            return false;
        }
        return handleStaticFile(path);
    }

    void setupRoutes() {
        // Rotas principais (Prioridade ao Cartão SD /www/base/... com fallback para /www/... e Flash)
        _server.on("/", [this]() {
            if (_wifi.isAPMode()) {
                if (tryServeSdFile("/www/wifi.html")) return;
                handleWifiPage();
            } else {
                if (tryServeSdFile("/www/mission.html") || tryServeSdFile("/www/index.html")) return;
                handleMissionPage(); // Na Base Station, a página principal de operação é a Missão & Regata
            }
        });
        _server.on("/index.html", [this]() {
            if (_wifi.isAPMode()) {
                if (tryServeSdFile("/www/wifi.html")) return;
                handleWifiPage();
            } else {
                if (tryServeSdFile("/www/mission.html") || tryServeSdFile("/www/index.html")) return;
                handleMissionPage();
            }
        });

        _server.on("/portal", [this]() {
            if (_wifi.isAPMode()) { _server.sendHeader("Location", "/wifi"); _server.send(302, "text/plain", ""); return; }
            if (tryServeSdFile("/www/portal.html")) return;
            handlePortalPage();
        });
        _server.on("/cloud", [this]() {
            if (_wifi.isAPMode()) { _server.sendHeader("Location", "/wifi"); _server.send(302, "text/plain", ""); return; }
            if (tryServeSdFile("/www/portal.html")) return;
            handlePortalPage();
        });
        _server.on("/wifi", [this]() {
            if (tryServeSdFile("/www/wifi.html")) return;
            handleWifiPage();
        });
        _server.on("/config", [this]() {
            if (tryServeSdFile("/www/config.html")) return;
            handleConfigPage();
        });
        _server.on("/settings", [this]() {
            if (tryServeSdFile("/www/config.html")) return;
            handleConfigPage();
        });
        _server.on("/gps", [this]() { 
            if (_wifi.isAPMode()) { _server.sendHeader("Location", "/wifi"); _server.send(302, "text/plain", ""); return; }
            if (tryServeSdFile("/www/gps.html") || tryServeSdFile("/www/index.html")) return;
            handleGpsPage(); 
        });
        _server.on("/battery", [this]() { 
            if (_wifi.isAPMode()) { _server.sendHeader("Location", "/wifi"); _server.send(302, "text/plain", ""); return; }
            if (tryServeSdFile("/www/battery.html")) return;
            handleBatteryPage(); 
        });
        _server.on("/control", [this]() { 
            if (_wifi.isAPMode()) { _server.sendHeader("Location", "/wifi"); _server.send(302, "text/plain", ""); return; }
            if (tryServeSdFile("/www/control.html")) return;
            handleControlPage(); 
        });
        _server.on("/mission", [this]() {
            if (_wifi.isAPMode()) { _server.sendHeader("Location", "/wifi"); _server.send(302, "text/plain", ""); return; }
            if (tryServeSdFile("/www/mission.html")) return;
            handleMissionPage();
        });
        _server.on("/regata", [this]() {
            if (_wifi.isAPMode()) { _server.sendHeader("Location", "/wifi"); _server.send(302, "text/plain", ""); return; }
            if (tryServeSdFile("/www/mission.html")) return;
            handleMissionPage();
        });
        _server.on("/sd", [this]() {
            if (tryServeSdFile("/www/sd.html")) return;
            handleSdPage();
        });

        // APIs JSON
        _server.on("/api/status", [this]() { handleApiStatus(); });
        _server.on("/api/scan", [this]() { handleApiScan(); });
        _server.on("/api/wifi/save", [this]() { handleApiWifiSave(); });
        _server.on("/api/wifi/setdefault", [this]() { handleApiWifiSetDefault(); });
        _server.on("/api/wifi/delete", [this]() { handleApiWifiDelete(); });
        _server.on("/api/wifi/connect", [this]() { handleApiWifiConnect(); });
        _server.on("/api/wifi/reconnect", [this]() { handleApiWifiReconnect(); });
        
        // Gestão de Missão e Lista de Rovers
        _server.on("/api/mission", HTTP_GET, [this]() { handleApiMission(); });
        _server.on("/api/mission/state", HTTP_POST, [this]() { handleApiMissionState(); });
        _server.on("/api/mission/rovers", HTTP_POST, [this]() { handleApiMissionRovers(); });
        _server.on("/api/mission/clear", HTTP_POST, [this]() { handleApiMissionClear(); });

        // Configuração do Ecrã LCD da Base
        _server.on("/api/display", HTTP_GET, [this]() { handleApiDisplayGet(); });
        _server.on("/api/display", HTTP_POST, [this]() { handleApiDisplayPost(); });
        _server.on("/api/display/brightness", [this]() { handleApiDisplayBrightness(); });

        // Controle de atuadores e comandos
        _server.on("/api/control/actuator", [this]() { handleApiActuator(); });
        _server.on("/api/control/action", [this]() { handleApiAction(); });
        _server.on("/api/rover/target", HTTP_POST, [this]() { handleApiAction(); });
        _server.on("/api/select_rover", [this]() { handleApiSelectRover(); });

        // Gestão do Cartão SD da Base Station
        _server.on("/api/sd/list", [this]() { handleApiSdList(); });
        _server.on("/api/sd/read", [this]() { handleApiSdRead(); });
        _server.on("/api/sd/download", [this]() { handleApiSdDownload(); });
        _server.on("/api/sd/save", [this]() { handleApiSdSave(); });
        _server.on("/api/sd/delete", [this]() { handleApiSdDelete(); });
        _server.on("/api/sd/delete_dir", [this]() { handleApiSdDeleteDir(); });
        _server.on("/api/sd/mkdir", [this]() { handleApiSdMkdir(); });

        // Upload de ficheiros para o SD da Base
        _server.on("/api/sd/upload", HTTP_POST, 
            [this]() { handleApiSdUploadFinish(); },
            [this]() { handleApiSdUploadData(); }
        );

        // Servir binário c6.bin para o Coprocessador C6 via HTTP
        _server.on("/firmware/c6.bin", HTTP_GET, [this]() {
            if (_sd && _sd->isReady() && _sd->fileExists("/firmware/c6.bin")) {
                File f = _sd->openFile("/firmware/c6.bin", FILE_READ);
                if (f) {
                    _server.streamFile(f, "application/octet-stream");
                    f.close();
                    return;
                }
            }
            _server.send(404, "text/plain", "Firmware c6.bin nao encontrado no cartao SD");
        });

        // Endpoints de atualizacao Dual OTA da Base (C6 + Lafvin)
        _server.on("/api/ota/c6", HTTP_POST, 
            [this]() { handleOtaC6UploadFinish(); },
            [this]() { handleOtaC6UploadData(); }
        );
        _server.on("/api/ota/c6/status", HTTP_GET, [this]() { handleApiOtaC6Status(); });
        _server.on("/api/ota/cancel", HTTP_ANY, [this]() { handleApiOtaCancel(); });
        _server.on("/api/system/reboot", HTTP_ANY, [this]() {
            _server.send(200, "application/json", "{\"success\":true,\"msg\":\"A reiniciar Base Station em 1 segundo...\"}");
            _restartScheduled = millis() + 1000;
        });
        _server.on("/api/ota/lafvin", HTTP_POST, 
            [this]() { handleApiOtaLafvinFinish(); },
            [this]() { handleApiOtaLafvinData(); }
        );

        // Servir fontes Font Awesome do SD se disponíveis
        _server.on("/system/fonts/Font%20Awesome%206%20Pro-Solid-900.woff2", HTTP_GET, [this]() {
            if (!handleStaticFile("/system/fonts/Font Awesome 6 Pro-Solid-900.woff2")) {
                _server.send(404, "text/plain", "Fonte woff2 nao encontrada no SD");
            }
        });
        _server.on("/system/fonts/Font Awesome 6 Pro-Solid-900.woff2", HTTP_GET, [this]() {
            if (!handleStaticFile("/system/fonts/Font Awesome 6 Pro-Solid-900.woff2")) {
                _server.send(404, "text/plain", "Fonte woff2 nao encontrada no SD");
            }
        });
        _server.on("/system/fonts/Font%20Awesome%206%20Pro-Solid-900.otf", HTTP_GET, [this]() {
            if (!handleStaticFile("/system/fonts/Font Awesome 6 Pro-Solid-900.otf")) {
                _server.send(404, "text/plain", "Fonte otf nao encontrada no SD");
            }
        });
        _server.on("/system/fonts/Font Awesome 6 Pro-Solid-900.otf", HTTP_GET, [this]() {
            if (!handleStaticFile("/system/fonts/Font Awesome 6 Pro-Solid-900.otf")) {
                _server.send(404, "text/plain", "Fonte otf nao encontrada no SD");
            }
        });

        _server.onNotFound([this]() {
            if (handleStaticFile(_server.uri())) {
                return;
            }
            if (_server.uri().startsWith("/") && !_server.uri().startsWith("/www/")) {
                if (handleStaticFile("/www/base" + _server.uri())) {
                    return;
                }
                // Na raiz /www/ apenas servir ficheiros estáticos (css, js, imagens, fontes), nunca HTML
                String uri = _server.uri();
                if (!uri.endsWith(".html") && !uri.endsWith(".htm")) {
                    if (handleStaticFile("/www" + uri)) {
                        return;
                    }
                }
            }
            // Em modo AP redirecionar para /wifi
            if (_wifi.isAPMode()) {
                _server.sendHeader("Location", "/wifi");
                _server.send(302, "text/plain", "");
            } else {
                _server.send(404, "text/plain", "Pagina nao encontrada");
            }
        });
    }

    // -------------------------------------------------------------
    // CSS & HTML TEMPLATES
    // -------------------------------------------------------------
    String getHtmlHeader(const String &title, const String &activeTab) {
        String html = "<!DOCTYPE html><html lang='pt'><head><meta charset='UTF-8'>";
        html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
        html += "<title>" + title + " - WindDragons</title>";
        html += "<style>";
        html += ":root{--bg:#0b1329;--card:#16203c;--card-border:#233258;--primary:#06b6d4;--primary-hover:#0891b2;--text:#f8fafc;--muted:#94a3b8;--green:#10b981;--yellow:#f59e0b;--red:#ef4444;}";
        html += "*{box-sizing:border-box;margin:0;padding:0;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Helvetica,Arial,sans-serif;}";
        html += "body{background:var(--bg);color:var(--text);padding-bottom:40px;line-height:1.5;}";
        html += ".navbar{background:linear-gradient(135deg,#0e1a38,#162348);border-bottom:1px solid var(--card-border);padding:14px 20px;display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:10px;}";
        html += ".brand{font-size:1.25rem;font-weight:700;color:var(--text);display:flex;align-items:center;gap:8px;}";
        html += ".brand span{color:var(--primary);}";
        html += ".badge{padding:4px 10px;border-radius:999px;font-size:0.75rem;font-weight:600;text-transform:uppercase;}";
        html += ".badge-ap{background:#78350f;color:#fde68a;}";
        html += ".badge-sta{background:#064e3b;color:#a7f3d0;}";
        html += ".nav-links{display:flex;gap:8px;background:rgba(0,0,0,0.25);padding:4px;border-radius:10px;border:1px solid var(--card-border);overflow-x:auto;}";
        html += ".nav-item{padding:8px 14px;border-radius:8px;color:var(--muted);text-decoration:none;font-size:0.875rem;font-weight:600;white-space:nowrap;transition:0.2s;}";
        html += ".nav-item:hover{color:var(--text);background:rgba(255,255,255,0.05);}";
        html += ".nav-item.active{background:var(--primary);color:#0f172a;}";
        html += ".container{max-width:960px;margin:24px auto;padding:0 16px;}";
        html += ".card{background:var(--card);border:1px solid var(--card-border);border-radius:14px;padding:20px;margin-bottom:20px;box-shadow:0 10px 15px -3px rgba(0,0,0,0.3);}";
        html += ".card-title{font-size:1.1rem;font-weight:700;margin-bottom:14px;display:flex;align-items:center;justify-content:space-between;color:var(--text);}";
        html += ".grid-2{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:16px;}";
        html += ".grid-4{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:12px;}";
        html += ".stat-box{background:rgba(0,0,0,0.2);padding:14px;border-radius:10px;border:1px solid rgba(255,255,255,0.05);}";
        html += ".stat-label{font-size:0.75rem;color:var(--muted);text-transform:uppercase;font-weight:600;}";
        html += ".stat-value{font-size:1.35rem;font-weight:700;margin-top:4px;color:var(--text);}";
        html += ".btn{display:inline-flex;align-items:center;justify-content:center;padding:10px 18px;border-radius:8px;font-weight:600;font-size:0.875rem;cursor:pointer;border:none;transition:0.2s;text-decoration:none;gap:6px;}";
        html += ".btn-primary{background:var(--primary);color:#0f172a;}";
        html += ".btn-primary:hover{background:var(--primary-hover);}";
        html += ".btn-danger{background:var(--red);color:#fff;}";
        html += ".btn-secondary{background:#334155;color:#fff;}";
        html += ".btn-secondary:hover{background:#475569;}";
        html += ".btn-group{display:flex;gap:8px;flex-wrap:wrap;}";
        html += "table{width:100%;border-collapse:collapse;margin-top:10px;}";
        html += "th,td{padding:10px 12px;text-align:left;border-bottom:1px solid var(--card-border);font-size:0.875rem;}";
        html += "th{color:var(--muted);font-weight:600;}";
        html += "input,select{width:100%;padding:10px 12px;background:#0f172a;border:1px solid var(--card-border);color:#fff;border-radius:8px;margin-top:6px;font-size:0.9rem;}";
        html += ".form-group{margin-bottom:14px;}";
        html += ".progress-bg{width:100%;height:16px;background:#0f172a;border-radius:8px;overflow:hidden;margin-top:8px;}";
        html += ".progress-bar{height:100%;transition:width 0.3s;}";
        html += ".slider{width:100%;-webkit-appearance:none;height:10px;background:#0f172a;border-radius:5px;outline:none;}";
        html += ".slider::-webkit-slider-thumb{-webkit-appearance:none;width:24px;height:24px;border-radius:50%;background:var(--primary);cursor:pointer;}";
        html += "</style></head><body>";

        // Navbar
        html += "<header class='navbar'>";
        html += "<div class='brand'>🌊 WindDragons <span>Base Station</span></div>";
        html += "<div class='nav-links'>";
        if (_wifi.isAPMode()) {
            html += "<a href='/wifi' class='nav-item " + String(activeTab == "wifi" ? "active" : "") + "'>Configuração WiFi (Modo AP)</a>";
            html += "<a href='/config' class='nav-item " + String(activeTab == "config" ? "active" : "") + "'>⚙️ Configuração</a>";
            html += "<a href='/mission' class='nav-item " + String(activeTab == "mission" ? "active" : "") + "'>🏁 Missão</a>";
        } else {
            html += "<a href='/mission' class='nav-item " + String(activeTab == "mission" ? "active" : "") + "'>🏁 Missão</a>";
            html += "<a href='/portal' class='nav-item " + String(activeTab == "portal" ? "active" : "") + "'>☁️ Portal</a>";
            html += "<a href='/config' class='nav-item " + String(activeTab == "config" ? "active" : "") + "'>⚙️ Configuração</a>";
            html += "<a href='/wifi' class='nav-item " + String(activeTab == "wifi" ? "active" : "") + "'>📡 WiFi</a>";
            html += "<a href='/gps' class='nav-item " + String(activeTab == "gps" ? "active" : "") + "'>📍 GPS Frota</a>";
            html += "<a href='/battery' class='nav-item " + String(activeTab == "battery" ? "active" : "") + "'>🔋 Bateria</a>";
            html += "<a href='/control' class='nav-item " + String(activeTab == "control" ? "active" : "") + "'>🎮 Comandos</a>";
            html += "<a href='/sd' class='nav-item " + String(activeTab == "sd" ? "active" : "") + "'>💾 Cartão SD</a>";
        }
        html += "</div>";
        html += "<div style='display:flex;align-items:center;gap:8px;'>";
        String opColor = "#64748b";
        if (_fleet.getOperationState() == BASE_OP_WAITING_ROVERS) opColor = "#f59e0b";
        else if (_fleet.getOperationState() == BASE_OP_IDLE) opColor = "#06b6d4";
        else if (_fleet.getOperationState() == BASE_OP_GO) opColor = "#10b981";
        html += "<span class='badge' style='background:" + opColor + ";color:#fff;' title='" + String(_fleet.getOperationStateDesc()) + "'>● " + String(_fleet.getOperationStateStr()) + "</span>";

        if (_wifi.isAPMode()) {
            html += "<span class='badge badge-ap'>MODO AP: " + _wifi.getSSID() + "</span>";
        } else {
            html += "<span class='badge badge-sta'>WIFI: " + _wifi.getIPAddress() + "</span>";
        }
        html += "</div></header><main class='container'>";
        return html;
    }

    String getHtmlFooter() {
        return "</main><footer style='text-align:center;color:var(--muted);font-size:0.75rem;margin-top:20px;'>WindDragons Telemetry System &copy; 2026</footer></body></html>";
    }

    // -------------------------------------------------------------
    // PÁGINA 1: CONFIGURAÇÃO DE WIFI
    // -------------------------------------------------------------
    void handleWifiPage() {
        String html = getHtmlHeader("Configuração WiFi", "wifi");

        String mac = getMacAddress();
        String qrSvg = generateQrCodeSvg(mac.c_str(), 140);

        // Card 1: Estado Atual da Conexão & Identificação com QR Code
        html += "<div class='card'>";
        html += "<div class='card-title'><span>Estado da Ligação de Rede & Identificação</span><span class='badge' style='background:#065f46;color:#6ee7b7;'>● QR Code Ativo</span></div>";
        html += "<div style='display:flex;align-items:center;gap:24px;flex-wrap:wrap;margin-bottom:16px;'>";
        html += "<div style='text-align:center;'>";
        html += qrSvg;
        html += "<div style='font-size:0.75rem;color:var(--muted);margin-top:6px;'>Digitalizar MAC</div>";
        html += "</div>";
        html += "<div style='flex:1;min-width:240px;'>";
        html += "<div class='grid-2'>";
        html += "<div class='stat-box'><div class='stat-label'>ID Único / MAC Hardware (Portal)</div><div class='stat-value' style='color:var(--primary);font-family:monospace;font-size:1.15rem;word-break:break-all;'>" + mac + "</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Nome da Estação Base</div><div class='stat-value' style='color:var(--yellow);'>" + String(BASE_STATION_ID) + "</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Modo de Operação</div><div class='stat-value' style='color:" + String(_wifi.isAPMode() ? "var(--yellow)" : "var(--green)") + "'>";
        html += _wifi.isAPMode() ? "Ponto de Acesso (AP)" : "Conectado à Rede (STA)";
        html += "</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Endereço IP</div><div class='stat-value' style='color:var(--primary);'>" + _wifi.getIPAddress() + "</div></div>";
        html += "</div></div></div>";
        html += "<div class='grid-2'>";
        html += "<div class='stat-box'><div class='stat-label'>SSID Ativo</div><div class='stat-value' style='word-break:break-all;'>" + _wifi.getSSID() + "</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>" + String(_wifi.isAPMode() ? "Dispositivos Conectados" : "Sinal WiFi (RSSI)") + "</div>";
        html += "<div class='stat-value'>" + String(_wifi.isAPMode() ? String(_wifi.getAPStationCount()) : String(_wifi.getRSSI()) + " dBm") + "</div></div>";
        html += "</div>";

        if (_wifi.isAPMode()) {
            html += "<div style='margin-top:16px;padding:12px;background:#451a03;border-left:4px solid var(--yellow);border-radius:6px;font-size:0.875rem;'>";
            html += "<strong>Modo Ponto de Acesso:</strong> Conecte-se à rede WiFi gerada pela Base e configure abaixo a sua rede de casa/porto com marcação de Default.";
            html += "</div>";
        }
        html += "</div>";

        // Card 2: Lista de Redes Guardadas
        html += "<div class='card'>";
        html += "<div class='card-title'><span>Redes WiFi Guardadas</span> <span style='font-size:0.8rem;color:var(--muted);'>Persistência NVS</span></div>";
        const auto &nets = _wifi.getNetworks();
        if (nets.empty()) {
            html += "<p style='color:var(--muted);font-size:0.875rem;'>Nenhuma rede guardada no momento.</p>";
        } else {
            html += "<table><thead><tr><th>SSID</th><th>Estado</th><th>Ações</th></tr></thead><tbody>";
            for (const auto &n : nets) {
                bool isCurrent = _wifi.isConnected() && _wifi.getSSID().equalsIgnoreCase(n.ssid);
                html += "<tr><td><strong>" + n.ssid + "</strong></td>";
                html += "<td>";
                if (isCurrent) {
                    html += "<span class='badge' style='background:#059669;color:#fff;margin-right:4px;'>● LIGADA</span> ";
                }
                if (n.is_default) {
                    html += "<span class='badge' style='background:#065f46;color:#6ee7b7;'>★ PADRÃO</span>";
                } else if (!isCurrent) {
                    html += "<span style='color:var(--muted);font-size:0.8rem;'>Secundária</span>";
                }
                html += "</td>";
                html += "<td><div class='btn-group'>";
                if (isCurrent) {
                    html += "<button disabled class='btn' style='padding:6px 10px;background:#334155;color:#94a3b8;cursor:default;'>Ativa</button>";
                } else {
                    html += "<button onclick='connectNetwork(\"" + n.ssid + "\")' class='btn btn-primary' style='padding:6px 10px;background:#2563eb;'>Ligar</button>";
                }
                if (!n.is_default) {
                    html += "<button onclick='setDefault(\"" + n.ssid + "\")' class='btn btn-secondary' style='padding:6px 10px;'>Definir Padrão</button>";
                }
                html += "<button onclick='deleteNetwork(\"" + n.ssid + "\")' class='btn btn-danger' style='padding:6px 10px;'>Eliminar</button>";
                html += "</div></td></tr>";
            }
            html += "</tbody></table>";
        }
        html += "</div>";

        // Card 3: Adicionar / Atualizar Rede
        html += "<div class='card'>";
        html += "<div class='card-title'>Adicionar Nova Ligação WiFi</div>";
        html += "<form id='wifiForm' onsubmit='saveWifi(event)'>";
        html += "<div class='form-group'><label class='stat-label'>Nome da Rede (SSID):</label><input type='text' id='ssid' name='ssid' required placeholder='ex: MeuRoteador'></div>";
        html += "<div class='form-group'><label class='stat-label'>Palavra-passe (Password):</label><input type='password' id='pass' name='pass' placeholder='Password da rede WiFi'></div>";
        html += "<div class='form-group' style='display:flex;align-items:center;gap:10px;margin-top:10px;'>";
        html += "<input type='checkbox' id='is_default' name='is_default' style='width:auto;margin:0;'>";
        html += "<label for='is_default' style='cursor:pointer;font-size:0.875rem;'>Marcar esta rede como <strong>Padrão (Default)</strong> no arranque</label>";
        html += "</div>";
        html += "<div class='btn-group' style='margin-top:14px;'>";
        html += "<button type='submit' class='btn btn-primary'>Guardar Ligação</button>";
        html += "<button type='button' onclick='scanNetworks()' class='btn btn-secondary'>Procurar Redes Próximas</button>";
        if (_wifi.isAPMode()) {
            html += "<button type='button' onclick='reconnectSTA()' class='btn' style='background:var(--green);color:#fff;'>Tentar Ligar e Sair do AP</button>";
        }
        html += "</div></form>";
        html += "<div id='scanResults' style='margin-top:16px;'></div>";
        html += "</div>";

        // Script interativo WiFi
        html += "<script>";
        html += "function saveWifi(e){e.preventDefault();const s=document.getElementById('ssid').value;const p=document.getElementById('pass').value;const d=document.getElementById('is_default').checked;";
        html += "fetch('/api/wifi/save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:s,password:p,is_default:d})})";
        html += ".then(r=>r.json()).then(res=>{alert(res.msg);location.reload();});}";
        html += "function connectNetwork(s){if(confirm('Ligar à rede WiFi \"'+s+'\" agora?')){fetch('/api/wifi/connect',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:s})}).then(r=>r.json()).then(res=>{alert(res.msg||('A ligar à rede '+s+'...'));setTimeout(()=>location.reload(),6000);}).catch(()=>{alert('Comando enviado. A recarregar em 6 segundos...');setTimeout(()=>location.reload(),6000);});}}";
        html += "function setDefault(s){fetch('/api/wifi/setdefault',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:s})}).then(()=>location.reload());}";
        html += "function deleteNetwork(s){if(confirm('Eliminar rede '+s+'?')){fetch('/api/wifi/delete',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:s})}).then(()=>location.reload());}}";
        html += "function reconnectSTA(){if(confirm('Reiniciar o WiFi para conectar à rede padrão?')){fetch('/api/wifi/reconnect',{method:'POST'}).then(r=>r.json()).then(res=>{alert(res.msg);setTimeout(()=>location.reload(),5000);});}}";
        html += "function scanNetworks(){const d=document.getElementById('scanResults');d.innerHTML='<p style=\"color:var(--primary);\">A pesquisar redes WiFi ao alcance...</p>';";
        html += "fetch('/api/scan').then(r=>r.json()).then(list=>{if(!list||!list.length){d.innerHTML='<p>Nenhuma rede encontrada.</p>';return;}";
        html += "let h='<table><thead><tr><th>SSID</th><th>Sinal</th><th>Segurança</th><th>Ação</th></tr></thead><tbody>';";
        html += "list.forEach(item=>{h+='<tr><td><strong>'+item.ssid+'</strong></td><td>'+item.rssi+' dBm</td><td>'+(item.secure?'Protegida':'Aberta')+'</td><td><button onclick=\"document.getElementById(\\'ssid\\').value=\\''+item.ssid+'\\';document.getElementById(\\'pass\\').focus();\" class=\"btn btn-secondary\" style=\"padding:4px 8px;\">Selecionar</button></td></tr>';});";
        html += "h+='</tbody></table>';d.innerHTML=h;});}";
        html += "</script>";

        html += getHtmlFooter();
        _server.send(200, "text/html", html);
    }

    // -------------------------------------------------------------
    // PÁGINA: CONFIGURAÇÃO DA BASE STATION (LCD, VERSÕES & OTA)
    // -------------------------------------------------------------
    void handleConfigPage() {
        String html = getHtmlHeader("Configuração", "config");

        // Card 1: Brilho do Ecrã LCD (Display LCD ST7789)
        uint8_t curBright = _display ? _display->getBrightness() : 85;
        html += "<div class='card'>";
        html += "<div class='card-title'><span><i class='fa-solid fa-sun'></i> Brilho do Ecrã (Display LCD ST7789)</span><span id='brightValBadge' class='badge' style='background:#1e293b;color:var(--primary);font-size:0.9rem;font-weight:700;'>" + String(curBright) + "%</span></div>";
        html += "<p style='color:var(--muted);font-size:0.875rem;margin-bottom:16px;'>Ajuste o brilho da retroiluminação por PWM (LEDC GPIO 22). Ao mover o cursor, o ecrã atualiza em tempo real.</p>";
        html += "<div style='background:rgba(0,0,0,0.25);padding:18px;border-radius:12px;border:1px solid var(--card-border);'>";
        html += "<div style='display:flex;align-items:center;justify-content:space-between;margin-bottom:10px;'>";
        html += "<span style='font-size:0.85rem;color:var(--muted);'><i class='fa-solid fa-moon'></i> 5% Mínimo</span>";
        html += "<span id='brightDisplayVal' style='font-size:1.8rem;font-weight:700;color:var(--primary);'>" + String(curBright) + "%</span>";
        html += "<span style='font-size:0.85rem;color:var(--muted);'><i class='fa-solid fa-sun'></i> 100% Máximo</span>";
        html += "</div>";
        html += "<input type='range' id='cfg_bright' min='5' max='100' step='1' value='" + String(curBright) + "' class='slider' oninput='onBrightSlide(this.value)' onchange='onBrightChange(this.value)'>";
        html += "<div style='display:flex;gap:8px;margin-top:16px;flex-wrap:wrap;'>";
        html += "<button type='button' onclick='setPreset(25)' class='btn btn-secondary' style='padding:6px 12px;font-size:0.8rem;'><i class='fa-solid fa-moon'></i> 25% (Poupança)</button>";
        html += "<button type='button' onclick='setPreset(50)' class='btn btn-secondary' style='padding:6px 12px;font-size:0.8rem;'><i class='fa-solid fa-cloud-sun'></i> 50% (Normal)</button>";
        html += "<button type='button' onclick='setPreset(75)' class='btn btn-secondary' style='padding:6px 12px;font-size:0.8rem;'><i class='fa-solid fa-sun'></i> 75% (Dia)</button>";
        html += "<button type='button' onclick='setPreset(100)' class='btn btn-secondary' style='padding:6px 12px;font-size:0.8rem;'><i class='fa-solid fa-bolt'></i> 100% (Sol Máximo)</button>";
        html += "</div></div></div>";

        // Card 3: Configuração dos Ecrãs LCD da Base Station
        html += "<div class='card'>";
        html += "<div class='card-title'><span><i class='fa-solid fa-tv'></i> Páginas do Ecrã LCD (Base Station)</span><span class='badge' style='background:#1e3a8a;color:#93c5fd;'>ST7789 320x172</span></div>";
        html += "<p style='color:var(--muted);font-size:0.875rem;margin-bottom:14px;'>O ecrã da Estação Base possui 3 ecrãs principais essenciais: <strong>1. Estação (ID, MAC, Estado)</strong>, <strong>2. Conectividade WiFi</strong> e <strong>3. Rover Selecionado</strong>.</p>";
        html += "<div class='form-group' style='display:flex;align-items:center;gap:12px;padding:12px;background:rgba(255,255,255,0.03);border:1px solid var(--card-border);border-radius:8px;'>";
        html += "<input type='checkbox' id='joyScreenCheck' onchange='toggleJoyScreen(this.checked)' style='width:20px;height:20px;cursor:pointer;'>";
        html += "<label for='joyScreenCheck' style='cursor:pointer;font-size:0.95rem;font-weight:600;'>";
        html += "Ativar 4º Ecrã: Teste do Joystick";
        html += "<div style='font-size:0.75rem;font-weight:normal;color:var(--muted);margin-top:2px;'>Adiciona a mira analógica 2D e diagnóstico dos eixos ADC ao carrossel de navegação do ecrã físico.</div>";
        html += "</label></div></div>";

        // Card 4: Versão das Placas & Atualização de Firmware Over-The-Air (OTA)
        html += "<div class='card' style='border-color:var(--primary);'>";
        html += "<div class='card-title'><span><i class='fa-solid fa-cloud-arrow-up'></i> Versão das Placas & Atualização Over-The-Air (OTA)</span><span class='badge' style='background:#1e293b;color:var(--primary);font-size:0.85rem;font-weight:700;'>Dual ESP32-C6</span></div>";
        html += "<p style='color:var(--muted);font-size:0.875rem;margin-bottom:16px;'>Envie um único pacote <code>base_firmware.zip</code> contendo os binários de ambos os processadores (ou um ficheiro <code>.bin</code> individual). A atualização é sequencial e à prova de falhas: o Coprocessador C6 (LoRa 433MHz) é atualizado primeiro, seguido pelo Master Lafvin.</p>";

        html += "<div style='display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-bottom:16px;'>";
        html += "<div style='background:rgba(15,23,42,0.7);padding:14px;border-radius:8px;border:1px solid var(--card-border);'>";
        html += "<div style='color:var(--muted);font-size:0.75rem;text-transform:uppercase;font-weight:600;margin-bottom:4px;'><i class='fa-solid fa-microchip' style='color:var(--primary);margin-right:6px;'></i>Master Lafvin (LCD+SD+WiFi)</div>";
        html += "<div id='fwLafvinVer' style='font-size:1.3rem;font-weight:700;color:var(--green);'>" + String(BASE_LAFVIN_VERSION) + "</div>";
        html += "<div style='color:var(--muted);font-size:0.7rem;margin-top:2px;'>ST7789 Display, MicroSD, Web & Gestão</div>";
        html += "</div>";
        html += "<div style='background:rgba(15,23,42,0.7);padding:14px;border-radius:8px;border:1px solid var(--card-border);'>";
        html += "<div style='color:var(--muted);font-size:0.75rem;text-transform:uppercase;font-weight:600;margin-bottom:4px;'><i class='fa-solid fa-satellite-dish' style='color:var(--yellow);margin-right:6px;'></i>Coprocessador C6 (LoRa 433MHz)</div>";
        html += "<div id='fwC6Ver' style='font-size:1.3rem;font-weight:700;color:var(--yellow);'>" + (_c6Version.isEmpty() ? "A detetar..." : _c6Version) + "</div>";
        html += "<div style='color:var(--muted);font-size:0.7rem;margin-top:2px;'>LoRa SX1278 (Hardware FSPI dedicado)</div>";
        html += "</div></div>";

        // Form OTA fallback
        html += "<form method='POST' action='/api/ota/lafvin' enctype='multipart/form-data' style='display:flex;gap:10px;align-items:center;flex-wrap:wrap;'>";
        html += "<input type='file' name='file' accept='.bin' required style='flex:1;min-width:200px;'>";
        html += "<button type='submit' class='btn btn-primary' onclick='return confirm(\"Gravar firmware no Master Lafvin agora?\")'><i class='fa-solid fa-upload'></i> Atualizar Master (.bin)</button>";
        html += "</form>";
        html += "</div>";

        // Script interativo
        html += "<script>";
        html += "let slideTimer=null;";
        html += "function onBrightSlide(v){const elVal=document.getElementById('brightDisplayVal');if(elVal)elVal.innerText=v+'%';const elBadge=document.getElementById('brightValBadge');if(elBadge)elBadge.innerText=v+'%';if(slideTimer)clearTimeout(slideTimer);slideTimer=setTimeout(()=>{fetch('/api/display/brightness?val='+v,{method:'POST'}).catch(()=>{});},60);}";
        html += "function onBrightChange(v){fetch('/api/display/brightness?val='+v,{method:'POST'}).catch(()=>{});}";
        html += "function setPreset(v){const el=document.getElementById('cfg_bright');if(el)el.value=v;onBrightSlide(v);onBrightChange(v);}";
        html += "fetch('/api/display').then(r=>r.json()).then(d=>{const c=document.getElementById('joyScreenCheck');if(c)c.checked=!!d.joy_screen;const b=d.screen_brightness||d.brightness||85;const el=document.getElementById('cfg_bright');if(el)el.value=b;const elVal=document.getElementById('brightDisplayVal');if(elVal)elVal.innerText=b+'%';const elBadge=document.getElementById('brightValBadge');if(elBadge)elBadge.innerText=b+'%';}).catch(()=>{});";
        html += "function toggleJoyScreen(enabled){fetch('/api/display',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({joy_screen:enabled})}).then(r=>r.json()).then(res=>{alert(res.joy_screen?'4º Ecrã (Teste Joystick) ativado no LCD!':'4º Ecrã desativado. LCD em modo simplificado de 3 ecrãs.');}).catch(e=>{alert('Erro ao guardar configuração do ecrã');});}";
        html += "</script>";

        html += getHtmlFooter();
        _server.send(200, "text/html", html);
    }

    // -------------------------------------------------------------
    // PÁGINA: ESTADO DO PORTAL CLOUD
    // -------------------------------------------------------------
    void handlePortalPage() {
        String html = getHtmlHeader("Portal Cloud", "portal");
        String mac = getMacAddress();

        bool isConnected = _network && _network->isConnected();
        bool syncOk = _network && _network->isSyncSuccess();
        int httpCode = _network ? _network->getLastHttpStatus() : 0;
        uint32_t okCount = _network ? _network->getSuccessCount() : 0;
        uint32_t failCount = _network ? _network->getFailCount() : 0;

        html += "<div class='card'>";
        html += "<div class='card-title'><span>Estado da Ligação ao Portal Cloud</span>";
        if (syncOk) {
            html += "<span class='badge' style='background:#065f46;color:#6ee7b7;'>● SINCRONIZADO (ONLINE)</span>";
        } else if (isConnected) {
            html += "<span class='badge' style='background:#78350f;color:#fde68a;'>● A CONECTAR...</span>";
        } else {
            html += "<span class='badge' style='background:#7f1d1d;color:#fca5a5;'>● DESLIGADO</span>";
        }
        html += "</div>";

        html += "<div class='grid-4'>";
        html += "<div class='stat-box'><div class='stat-label'>Código HTTP</div><div class='stat-value' style='color:" + String(syncOk ? "var(--green)" : "var(--red)") + "'>" + (httpCode > 0 ? String(httpCode) : "--") + "</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Syncs OK</div><div class='stat-value' style='color:var(--green);'>" + String(okCount) + "</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Falhas</div><div class='stat-value' style='color:" + String(failCount == 0 ? "var(--muted)" : "var(--red)") + "'>" + String(failCount) + "</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Ciclo Envio</div><div class='stat-value' style='color:var(--primary);'>3.0s</div></div>";
        html += "</div>";

        String qrSvg = generateQrCodeSvg(mac.c_str(), 140);
        html += "<div style='margin-top:16px;display:flex;align-items:center;gap:20px;flex-wrap:wrap;'>";
        html += "<div style='text-align:center;'>";
        html += qrSvg;
        html += "<div style='font-size:0.75rem;color:var(--muted);margin-top:6px;'>Digitalizar MAC Station</div>";
        html += "</div>";
        html += "<div style='flex:1;min-width:240px;'>";
        html += "<div class='grid-2'>";
        html += "<div class='stat-box'><div class='stat-label'>Endereço MAC de Hardware (Station ID)</div><div class='stat-value' style='color:var(--primary);font-family:monospace;'>" + mac + "</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Nome da Estação Base</div><div class='stat-value' style='color:var(--yellow);'>" + String(BASE_STATION_ID) + "</div></div>";
        html += "</div>";
        html += "<div style='margin-top:12px;' class='stat-box'><div class='stat-label'>Endpoint Cloud API</div><div class='stat-value' style='font-size:0.95rem;font-family:monospace;color:var(--text);word-break:break-all;'>" + String(WINDDRAGONS_API_URL) + "</div></div>";
        html += "</div></div>";
        html += "</div>";

        // Tabela de Rovers do Portal
        html += "<div class='card'>";
        html += "<div class='card-title'><span>Rovers da Missão (Enviados pelo Portal)</span>";
        bool hasRovers = _fleet.hasReceivedRoversFromPortal() && !_fleet.getRovers().empty();
        if (hasRovers) {
            html += "<span class='badge' style='background:rgba(16,185,129,0.15);color:var(--green);'>" + String(_fleet.getRovers().size()) + " Rovers</span>";
        } else {
            html += "<span class='badge' style='background:rgba(255,255,255,0.06);color:var(--muted);'>0 Rovers</span>";
        }
        html += "</div>";

        if (!hasRovers) {
            html += "<div style='text-align:center;color:var(--muted);padding:28px 16px;'>";
            html += "<p style='margin:0;font-size:1rem;color:var(--text);'><strong>Nenhum rover enviado pelo Portal ainda.</strong></p>";
            html += "<p style='margin:6px 0 0 0;font-size:0.85rem;color:var(--muted);'>A lista será apresentada automaticamente assim que o portal responder com os rovers atribuídos à missão.</p>";
            html += "</div>";
        } else {
            html += "<div style='overflow-x:auto;'><table><thead><tr>";
            html += "<th>Código</th><th>Nome</th><th>Função (Role)</th><th>MAC</th><th>Estado</th><th>Bateria</th>";
            html += "</tr></thead><tbody>";
            for (const auto &rov : _fleet.getRovers()) {
                html += "<tr>";
                html += "<td><strong>" + String(rov.code) + "</strong></td>";
                html += "<td>" + String(rov.name) + "</td>";
                html += "<td><span class='badge' style='background:rgba(59,130,246,0.15);color:var(--primary);'>" + String(rov.role) + "</span></td>";
                html += "<td style='font-family:monospace;font-size:0.85rem;'>" + String(rov.mac) + "</td>";
                html += "<td><span class='badge' style='background:" + String(rov.is_online ? "#065f46" : "#7f1d1d") + ";color:" + String(rov.is_online ? "#6ee7b7" : "#fca5a5") + ";'>" + String(rov.is_online ? "ONLINE" : "OFFLINE") + "</span></td>";
                html += "<td>" + String(rov.battery_pct) + "%</td>";
                html += "</tr>";
            }
            html += "</tbody></table></div>";
        }
        html += "</div>";

        html += getHtmlFooter();
        _server.send(200, "text/html", html);
    }

    // -------------------------------------------------------------
    // PÁGINA 2: GPS E NAVEGAÇÃO
    // -------------------------------------------------------------
    void handleGpsPage() {
        FleetRover *r = _fleet.getSelectedRover();
        String html = getHtmlHeader("GPS & Telemetria", "gps");

        html += "<div class='card'>";
        html += "<div class='card-title'>";
        html += "<span>📍 Posicionamento e Rota Náutica</span>";
        html += "<select id='roverSelect' onchange='changeRover(this.value)' style='width:auto;display:inline-block;padding:4px 10px;'>";
        for (const auto &rov : _fleet.getRovers()) {
            html += "<option value='" + String(rov.id) + "' " + String((r && r->id == rov.id) ? "selected" : "") + ">" + String(rov.code) + (rov.is_online ? " [ONLINE]" : " [OFFLINE]") + "</option>";
        }
        html += "</select></div>";

        html += "<div class='grid-4'>";
        html += "<div class='stat-box'><div class='stat-label'>Estado Fix</div><div class='stat-value' id='fixStatus' style='color:" + String((r && r->is_online) ? "var(--green)" : "var(--red)") + "'>";
        html += (r && r->is_online) ? "ONLINE" : "OFFLINE";
        html += "</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Velocidade</div><div class='stat-value' id='speedVal'>" + String(r ? r->speed_knots : 0.0f, 1) + " kn</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Rumo Proa</div><div class='stat-value' id='headingVal'>" + String(r ? r->heading_deg : 0.0f, 0) + "&deg;</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Sinal LoRa</div><div class='stat-value' id='rssiVal'>" + String(r ? r->rssi : 0) + " dBm</div></div>";
        html += "</div>";

        // Coordenadas
        html += "<div class='grid-2' style='margin-top:16px;'>";
        html += "<div class='stat-box'><div class='stat-label'>Latitude</div><div class='stat-value' id='latVal' style='font-size:1.6rem;color:var(--primary);'>" + String(r ? r->lat : 0.0, 6) + "</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Longitude</div><div class='stat-value' id='lngVal' style='font-size:1.6rem;color:var(--primary);'>" + String(r ? r->lng : 0.0, 6) + "</div></div>";
        html += "</div>";

        // Bússola e Mapa
        html += "<div style='display:flex;gap:12px;margin-top:16px;flex-wrap:wrap;'>";
        String mapsUrl = "https://www.google.com/maps?q=" + String(r ? r->lat : 0.0, 6) + "," + String(r ? r->lng : 0.0, 6);
        html += "<a id='mapsLink' href='" + mapsUrl + "' target='_blank' class='btn btn-primary'>🗺️ Ver no Google Maps</a>";
        String osmUrl = "https://www.openstreetmap.org/?mlat=" + String(r ? r->lat : 0.0, 6) + "&mlon=" + String(r ? r->lng : 0.0, 6) + "#map=17/" + String(r ? r->lat : 0.0, 6) + "/" + String(r ? r->lng : 0.0, 6);
        html += "<a id='osmLink' href='" + osmUrl + "' target='_blank' class='btn btn-secondary'>🌐 Ver no OpenStreetMap</a>";
        html += "</div></div>";

        // Script de atualização automática
        html += "<script>";
        html += "function changeRover(id){fetch('/api/select_rover',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({rover_id:parseInt(id)})}).then(()=>location.reload());}";
        html += "setInterval(()=>{fetch('/api/status').then(r=>r.json()).then(data=>{const sel=data.selected;";
        html += "if(!sel)return;";
        html += "document.getElementById('latVal').innerText=sel.lat.toFixed(6);";
        html += "document.getElementById('lngVal').innerText=sel.lng.toFixed(6);";
        html += "document.getElementById('speedVal').innerText=sel.speed_knots.toFixed(1)+' kn';";
        html += "document.getElementById('headingVal').innerText=sel.heading_deg.toFixed(0)+'°';";
        html += "document.getElementById('rssiVal').innerText=sel.rssi+' dBm';";
        html += "const st=document.getElementById('fixStatus');if(sel.is_online){st.innerText='ONLINE';st.style.color='var(--green)';}else if(data.lora_ready){st.innerText='RÁDIO OK (ESCUTA)';st.style.color='var(--primary)';}else if(data.lora_enabled){st.innerText='FALHA RÁDIO';st.style.color='var(--red)';}else{st.innerText='REPOUSO';st.style.color='var(--muted)';}";
        html += "document.getElementById('mapsLink').href='https://www.google.com/maps?q='+sel.lat+','+sel.lng;";
        html += "document.getElementById('osmLink').href='https://www.openstreetmap.org/?mlat='+sel.lat+'&mlon='+sel.lng+'#map=17/'+sel.lat+'/'+sel.lng;";
        html += "});},1500);";
        html += "</script>";

        html += getHtmlFooter();
        _server.send(200, "text/html", html);
    }

    // -------------------------------------------------------------
    // PÁGINA 3: MONITOR DE BATERIA
    // -------------------------------------------------------------
    void handleBatteryPage() {
        FleetRover *r = _fleet.getSelectedRover();
        String html = getHtmlHeader("Monitor de Bateria", "battery");

        uint8_t pct = r ? r->battery_pct : 0;
        float vTot = r ? r->battery_voltage : 0.0f;
        float vCell = vTot / 4.0f;

        html += "<div class='card'>";
        html += "<div class='card-title'>";
        html += "<span>🔋 Telemetria de Bateria LiPo 4S</span>";
        html += "<select onchange='changeRover(this.value)' style='width:auto;display:inline-block;padding:4px 10px;'>";
        html += "<option value='0' " + String(!r ? "selected" : "") + ">-- Nenhum Rover Selecionado --</option>";
        for (const auto &rov : _fleet.getRovers()) {
            html += "<option value='" + String(rov.id) + "' " + String((r && r->id == rov.id) ? "selected" : "") + ">" + String(rov.code) + "</option>";
        }
        html += "</select></div>";

        // Indicador grande de percentagem
        String barColor = pct < 25 ? "var(--red)" : (pct < 50 ? "var(--yellow)" : "var(--green)");
        html += "<div style='text-align:center;padding:20px 0;'>";
        html += "<div id='batPct' style='font-size:3.5rem;font-weight:800;color:" + barColor + ";'>" + String(pct) + "%</div>";
        html += "<div class='progress-bg' style='max-width:400px;margin:0 auto;'><div id='batBar' class='progress-bar' style='width:" + String(pct) + "%;background:" + barColor + ";'></div></div>";
        html += "</div>";

        html += "<div class='grid-4'>";
        html += "<div class='stat-box'><div class='stat-label'>Tensão Total (Pack)</div><div class='stat-value' id='vTotVal' style='color:var(--text);'>" + String(vTot, 2) + " V</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Média por Célula</div><div class='stat-value' id='vCellVal' style='color:var(--primary);'>" + String(vCell, 2) + " V/cel</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Química / Tipo</div><div class='stat-value'>LiPo 4S</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Estado Pack</div><div class='stat-value' id='batHealth' style='color:" + barColor + ";'>";
        html += pct < 20 ? "CRÍTICO" : (pct < 40 ? "BAIXA" : "NORMAL");
        html += "</div></div>";
        html += "</div>";

        // Tabela representativa das 4 células
        html += "<div style='margin-top:20px;'>";
        html += "<div class='stat-label' style='margin-bottom:8px;'>Distribuição Estimada Células (4S):</div>";
        html += "<div class='grid-4'>";
        for (int c = 1; c <= 4; ++c) {
            html += "<div class='stat-box' style='text-align:center;'><div class='stat-label'>Célula " + String(c) + "</div><div class='stat-value cell-v' style='font-size:1.2rem;color:var(--primary);'>" + String(vCell, 2) + " V</div></div>";
        }
        html += "</div></div></div>";

        html += "<script>";
        html += "function changeRover(id){fetch('/api/select_rover',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({rover_id:parseInt(id)})}).then(()=>location.reload());}";
        html += "setInterval(()=>{fetch('/api/status').then(r=>r.json()).then(data=>{const sel=data.selected;";
        html += "if(!sel)return;";
        html += "const p=sel.battery_pct;const v=sel.battery_voltage;const vc=(v/4.0);";
        html += "const col=p<25?'var(--red)':(p<50?'var(--yellow)':'var(--green)');";
        html += "document.getElementById('batPct').innerText=p+'%';document.getElementById('batPct').style.color=col;";
        html += "document.getElementById('batBar').style.width=p+'%';document.getElementById('batBar').style.background=col;";
        html += "document.getElementById('vTotVal').innerText=v.toFixed(2)+' V';";
        html += "document.getElementById('vCellVal').innerText=vc.toFixed(2)+' V/cel';";
        html += "document.querySelectorAll('.cell-v').forEach(el=>el.innerText=vc.toFixed(2)+' V');";
        html += "const h=document.getElementById('batHealth');h.innerText=p<20?'CRÍTICO':(p<40?'BAIXA':'NORMAL');h.style.color=col;";
        html += "});},1500);";
        html += "</script>";

        html += getHtmlFooter();
        _server.send(200, "text/html", html);
    }

    // -------------------------------------------------------------
    // PÁGINA: GESTÃO DE MISSÃO, REGATA & ESTADO DE OPERAÇÃO
    // -------------------------------------------------------------
    void handleMissionPage() {
        String html = getHtmlHeader("Gestão de Missão & Regata", "mission");

        BaseOperationState currentState = _fleet.getOperationState();

        // 1. Card de Estado de Operação da Estação Base
        html += "<div class='card'>";
        html += "<div class='card-title'><span>🚩 Estado de Operação da Estação Base</span>";
        String curBadgeColor = "#64748b";
        if (currentState == BASE_OP_WAITING_ROVERS) curBadgeColor = "#f59e0b";
        else if (currentState == BASE_OP_IDLE) curBadgeColor = "#06b6d4";
        else if (currentState == BASE_OP_GO) curBadgeColor = "#10b981";
        html += "<span class='badge' id='badgeState' style='background:" + curBadgeColor + ";color:#fff;font-size:0.85rem;padding:6px 14px;'>";
        html += String(_fleet.getOperationStateStr()) + " - " + String(_fleet.getOperationStateDesc()) + "</span>";
        html += "</div>";

        html += "<div style='display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:12px;margin-top:14px;'>";

        // Botão Storage
        html += "<button onclick=\"setState('STORAGE')\" id='btnStateSTORAGE' class='btn' style='display:flex;flex-direction:column;align-items:flex-start;padding:14px;background:" + String(currentState == BASE_OP_STORAGE ? "#475569;border:2px solid #94a3b8;" : "rgba(255,255,255,0.05);") + "color:#fff;'>";
        html += "<span style='font-size:1.05rem;font-weight:700;'>📦 Storage</span>";
        html += "<span style='font-size:0.75rem;color:var(--muted);margin-top:4px;'>Not in use (Standby)</span>";
        html += "</button>";

        // Botão Waiting Rover List
        html += "<button onclick=\"setState('WAITING_ROVERS')\" id='btnStateWAITING_ROVERS' class='btn' style='display:flex;flex-direction:column;align-items:flex-start;padding:14px;background:" + String(currentState == BASE_OP_WAITING_ROVERS ? "#b45309;border:2px solid #f59e0b;" : "rgba(255,255,255,0.05);") + "color:#fff;'>";
        html += "<span style='font-size:1.05rem;font-weight:700;'>⏳ Waiting Rover List</span>";
        html += "<span style='font-size:0.75rem;color:var(--muted);margin-top:4px;'>Connected and waiting</span>";
        html += "</button>";

        // Botão Idle
        html += "<button onclick=\"setState('IDLE')\" id='btnStateIDLE' class='btn' style='display:flex;flex-direction:column;align-items:flex-start;padding:14px;background:" + String(currentState == BASE_OP_IDLE ? "#0e7490;border:2px solid #06b6d4;" : "rgba(255,255,255,0.05);") + "color:#fff;'>";
        html += "<span style='font-size:1.05rem;font-weight:700;'>⚓ Idle</span>";
        html += "<span style='font-size:0.75rem;color:var(--muted);margin-top:4px;'>Rover list ready</span>";
        html += "</button>";

        // Botão Go
        html += "<button onclick=\"setState('GO')\" id='btnStateGO' class='btn' style='display:flex;flex-direction:column;align-items:flex-start;padding:14px;background:" + String(currentState == BASE_OP_GO ? "#047857;border:2px solid #10b981;" : "rgba(255,255,255,0.05);") + "color:#fff;'>";
        html += "<span style='font-size:1.05rem;font-weight:700;'>🚀 Go</span>";
        html += "<span style='font-size:0.75rem;color:var(--muted);margin-top:4px;'>In mission (Ativo)</span>";
        html += "</button>";

        html += "</div></div>";

        // 2. Card de Informação da Regata / Prova
        html += "<div class='card'>";
        html += "<div class='card-title'>⛵ Dados do Percurso / Regata</div>";
        html += "<div class='grid-2'>";
        html += "<div class='form-group'><label class='stat-label'>ID da Regata / Prova</label>";
        html += "<input type='text' id='raceIdInput' value='" + String(_fleet.getRaceId()) + "' placeholder='ex: REGATA-TEJO-2026-R1'></div>";
        html += "<div class='form-group'><label class='stat-label'>Nome da Missão / Prova</label>";
        html += "<input type='text' id='missionNameInput' value='" + String(_fleet.getMissionName()) + "' placeholder='ex: Trofeu de Outono - Prova 1'></div>";
        html += "</div></div>";

        // 3. Card de Bóias / Rovers da Missão
        html += "<div class='card'>";
        html += "<div class='card-title'><span>🛰️ Lista de Bóias da Missão (Persistente no MicroSD)</span>";
        html += "<span style='font-size:0.8rem;color:var(--muted);font-weight:normal;'>/config/rovers.json</span></div>";

        html += "<div style='overflow-x:auto;'>";
        html += "<table id='roversTable'>";
        html += "<thead><tr>";
        html += "<th>ID</th><th>Nome</th><th>MAC Address</th><th>Papel / Role</th><th>Coord. Alvo (Lat, Lng)</th><th>Status LoRa</th><th>Ações</th>";
        html += "</tr></thead><tbody id='roversTbody'>";

        for (const auto &r : _fleet.getRovers()) {
            html += "<tr id='rRow_" + String(r.id) + "'>";
            html += "<td><strong>" + String(r.id) + "</strong></td>";
            html += "<td><input type='text' class='r-name' value='" + String(r.name) + "' style='width:120px;padding:6px;'></td>";
            html += "<td><input type='text' class='r-mac' value='" + String(r.mac) + "' style='width:140px;font-family:monospace;padding:6px;'></td>";
            html += "<td><select class='r-role' style='width:120px;padding:6px;'>";
            const char* roles[] = {"largada CR", "largada", "boia 1", "boia 2", "boia 3", "boia 4", "chegada CR", "chegada"};
            for (const char* ro : roles) {
                String sel = (strcmp(r.role, ro) == 0) ? "selected" : "";
                html += "<option value='" + String(ro) + "' " + sel + ">" + String(ro) + "</option>";
            }
            html += "</select></td>";
            html += "<td><div style='display:flex;gap:4px;'><input type='number' step='0.000001' class='r-lat' value='" + String(r.target_lat, 6) + "' placeholder='Lat' style='width:90px;padding:6px;'><input type='number' step='0.000001' class='r-lng' value='" + String(r.target_lng, 6) + "' placeholder='Lng' style='width:90px;padding:6px;'></div></td>";
            html += "<td>";
            if (r.is_online) {
                html += "<span class='badge' style='background:#064e3b;color:#a7f3d0;'>ONLINE (" + String(r.battery_pct) + "%)</span>";
                if (r.anchor_depth_m > 0.05f) {
                    html += "<div style='font-size:0.75rem;color:var(--yellow);margin-top:2px;'>Âncora: " + String(r.anchor_depth_m, 2) + "m</div>";
                }
            } else {
                html += "<span class='badge' style='background:#7f1d1d;color:#fecaca;'>OFFLINE</span>";
            }
            html += "</td>";
            bool isSelected = (_fleet.getSelectedRoverId() == r.id);
            html += "<td><div style='display:flex;gap:4px;'>";
            if (isSelected) {
                html += "<button onclick='changeRover(0)' class='btn' style='background:#059669;color:#fff;padding:4px 8px;font-size:0.75rem;' title='Clique para desmarcar'>★ Ativo no LCD</button>";
            } else {
                html += "<button onclick='changeRover(" + String(r.id) + ")' class='btn btn-primary' style='padding:4px 8px;font-size:0.75rem;'>Selecionar</button>";
            }
            html += "<button onclick='deleteRow(" + String(r.id) + ")' class='btn btn-danger' style='padding:4px 8px;font-size:0.75rem;'>🗑️</button>";
            html += "</div></td>";
            html += "</tr>";
        }

        html += "</tbody></table></div>";

        // Botões de ação da tabela
        html += "<div style='display:flex;gap:10px;margin-top:16px;flex-wrap:wrap;'>";
        html += "<button onclick='addRow()' class='btn btn-secondary'>➕ Adicionar Bóia</button>";
        html += "<button onclick='saveRovers()' class='btn btn-primary' style='padding:10px 24px;'>💾 Guardar Lista no MicroSD</button>";
        html += "<button onclick='clearRovers()' class='btn btn-danger' style='background:#78350f;'>🔄 Repor Pré-definições</button>";
        html += "</div>";

        html += "</div>";

        // Scripts da Página de Missão
        html += "<script>";
        html += "function setState(st){";
        html += "fetch('/api/mission/state',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({state:st})})";
        html += ".then(r=>r.json()).then(res=>{if(res.status==='ok'){location.reload();}});";
        html += "}";

        html += "function deleteRow(id){const row=document.getElementById('rRow_'+id);if(row)row.remove();}";
        html += "function changeRover(id){fetch('/api/select_rover',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({rover_id:parseInt(id)})}).then(()=>location.reload());}";

        html += "function addRow(){";
        html += "const tbody=document.getElementById('roversTbody');";
        html += "const id=tbody.children.length+1;";
        html += "const tr=document.createElement('tr');tr.id='rRow_'+id;";
        html += "tr.innerHTML=`<td><strong>${id}</strong></td>`+";
        html += "`<td><input type='text' class='r-name' value='WD-Boia-${id}' style='width:120px;padding:6px;'></td>`+";
        html += "`<td><input type='text' class='r-mac' value='00:00:00:00:00:0${id}' style='width:140px;font-family:monospace;padding:6px;'></td>`+";
        html += "`<td><select class='r-role' style='width:120px;padding:6px;'>`+";
        html += "`<option value='largada CR'>largada CR</option><option value='largada'>largada</option>`+";
        html += "`<option value='boia 1'>boia 1</option><option value='boia 2'>boia 2</option>`+";
        html += "`<option value='boia 3'>boia 3</option><option value='chegada CR'>chegada CR</option><option value='chegada'>chegada</option>`+";
        html += "`</select></td>`+";
        html += "`<td><div style='display:flex;gap:4px;'><input type='number' step='0.000001' class='r-lat' value='0.000000' style='width:90px;padding:6px;'><input type='number' step='0.000001' class='r-lng' value='0.000000' style='width:90px;padding:6px;'></div></td>`+";
        html += "`<td><span class='badge' style='background:#334155;color:#94a3b8;'>NOVO</span></td>`+";
        html += "`<td><button onclick='deleteRow(${id})' class='btn btn-danger' style='padding:4px 8px;font-size:0.75rem;'>🗑️</button></td>`;";
        html += "tbody.appendChild(tr);";
        html += "}";

        html += "function saveRovers(){";
        html += "const raceId=document.getElementById('raceIdInput').value;";
        html += "const missionName=document.getElementById('missionNameInput').value;";
        html += "const rows=document.querySelectorAll('#roversTbody tr');";
        html += "const rovers=[];";
        html += "rows.forEach((r,idx)=>{";
        html += "const name=r.querySelector('.r-name').value;";
        html += "const mac=r.querySelector('.r-mac').value;";
        html += "const role=r.querySelector('.r-role').value;";
        html += "const lat=parseFloat(r.querySelector('.r-lat').value)||0.0;";
        html += "const lng=parseFloat(r.querySelector('.r-lng').value)||0.0;";
        html += "rovers.push({id:idx+1,name:name,mac_address:mac,role:role,lat:lat,lng:lng});";
        html += "});";
        html += "fetch('/api/mission/rovers',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({race_id:raceId,mission_name:missionName,rovers:rovers})})";
        html += ".then(r=>r.json()).then(res=>{alert('Lista de '+res.rover_count+' boias guardada no MicroSD com sucesso!');location.reload();});";
        html += "}";

        html += "function clearRovers(){if(confirm('Tem a certeza que deseja repor a lista de boias?')){";
        html += "fetch('/api/mission/clear',{method:'POST'}).then(r=>r.json()).then(res=>{location.reload();});";
        html += "}}";

        html += "</script>";
        html += getHtmlFooter();
        _server.send(200, "text/html", html);
    }

    // -------------------------------------------------------------
    // PÁGINA 4: COMANDAR A BOIA (SERVOS, MOTOR, ÂNCORA, ETC.)
    // -------------------------------------------------------------
    void handleControlPage() {
        FleetRover *r = _fleet.getSelectedRover();
        String html = getHtmlHeader("Comandar Boia", "control");

        html += "<div class='card'>";
        html += "<div class='card-title'>";
        html += "<span>🎮 Centro de Comando Remoto</span>";
        html += "<select id='targetRoverSelect' onchange='changeRover(this.value)' style='width:auto;display:inline-block;padding:4px 10px;'>";
        html += "<option value='0' " + String(!r ? "selected" : "") + ">-- Nenhum Rover Selecionado --</option>";
        for (const auto &rov : _fleet.getRovers()) {
            html += "<option value='" + String(rov.id) + "' " + String((r && r->id == rov.id) ? "selected" : "") + ">" + String(rov.code) + "</option>";
        }
        html += "</select></div>";

        // Status atual do Rover Alvo
        html += "<div class='grid-4' style='margin-bottom:16px;'>";
        html += "<div class='stat-box'><div class='stat-label'>Modo Motor</div><div class='stat-value' id='curMotor' style='color:var(--yellow);font-size:1.1rem;'>" + String(r ? get_motor_status_str(r->motor_status) : "idle") + "</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Estado Âncora</div><div class='stat-value' id='curAnchor' style='color:var(--primary);font-size:1.1rem;'>" + String(r ? get_anchor_status_str(r->anchor_status) : "retracted") + "</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Profundidade</div><div class='stat-value' id='curDepth'>" + String(r ? r->anchor_depth_m : 0.0f, 1) + " m</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Ligação Rádio</div><div class='stat-value' id='curRssi' style='color:var(--green);'>" + String(r ? r->rssi : 0) + " dBm</div></div>";
        html += "</div></div>";

        // Card: Propulsão & Leme (Sliders & Presets)
        html += "<div class='card'>";
        html += "<div class='card-title'>Motor Principal (ESC) & Leme de Direção</div>";
        html += "<div class='grid-2'>";
        
        // Coluna Motor
        html += "<div>";
        html += "<div class='stat-label'>Aceleração ESC: <span id='thrVal' style='font-size:1.1rem;color:var(--primary);'>0%</span></div>";
        html += "<input type='range' id='throttleSlider' min='-100' max='100' value='0' class='slider' oninput='updateSliders()' onchange='sendActuators()'>";
        html += "<div class='btn-group' style='margin-top:12px;'>";
        html += "<button onclick='setThrottle(0)' class='btn btn-secondary'>0% (Stop)</button>";
        html += "<button onclick='setThrottle(25)' class='btn btn-secondary'>+25%</button>";
        html += "<button onclick='setThrottle(50)' class='btn btn-secondary'>+50%</button>";
        html += "<button onclick='setThrottle(100)' class='btn btn-primary'>+100%</button>";
        html += "<button onclick='setThrottle(-50)' class='btn btn-secondary'>-50% (Ré)</button>";
        html += "</div></div>";

        // Coluna Leme
        html += "<div>";
        html += "<div class='stat-label'>Ângulo do Leme: <span id='rudVal' style='font-size:1.1rem;color:var(--primary);'>0%</span></div>";
        html += "<input type='range' id='rudderSlider' min='-100' max='100' value='0' class='slider' oninput='updateSliders()' onchange='sendActuators()'>";
        html += "<div class='btn-group' style='margin-top:12px;'>";
        html += "<button onclick='setRudder(-75)' class='btn btn-secondary'>⬅ Bombordo (-75%)</button>";
        html += "<button onclick='setRudder(0)' class='btn btn-secondary'>Centro (0%)</button>";
        html += "<button onclick='setRudder(75)' class='btn btn-secondary'>Estibordo (+75%) ➡</button>";
        html += "</div></div>";

        html += "</div></div>";

        // Card: Guincho de Âncora & Cremalheira
        html += "<div class='card'>";
        html += "<div class='card-title'>Guincho da Âncora & Cremalheira de Travão</div>";
        html += "<div class='btn-group'>";
        html += "<button onclick='sendAction(3)' class='btn btn-primary'>⚓ Lançar Âncora (Soltar)</button>";
        html += "<button onclick='sendAction(4)' class='btn btn-primary'>⬆ Recolher Âncora (Guincho)</button>";
        html += "<button onclick='sendAction(5)' class='btn btn-secondary'>🛑 Parar Guincho</button>";
        html += "</div></div>";

        // Card: Modos Operacionais & Emergência
        html += "<div class='card'>";
        html += "<div class='card-title'>Modos de Navegação & Emergência</div>";
        html += "<div class='btn-group'>";
        html += "<button onclick='sendAction(1)' class='btn' style='background:#0284c7;color:#fff;'>📍 Hold Station (GPS)</button>";
        html += "<button onclick='sendAction(7)' class='btn' style='background:#7c3aed;color:#fff;'>🏠 Return to Launch (RTL)</button>";
        html += "<button onclick='sendAction(10)' class='btn btn-secondary'>🕹️ Modo Manual</button>";
        html += "<button onclick='sendAction(8)' class='btn' style='background:var(--yellow);color:#0f172a;'>🚨 Alarme Sonoro / Strobe</button>";
        html += "<button onclick='sendEmergencyStop()' class='btn btn-danger' style='font-size:1rem;padding:12px 24px;'>🛑 PARAGEM DE EMERGÊNCIA</button>";
        html += "</div></div>";

        html += "<script>";
        html += "function updateSliders(){document.getElementById('thrVal').innerText=document.getElementById('throttleSlider').value+'%';document.getElementById('rudVal').innerText=document.getElementById('rudderSlider').value+'%';}";
        html += "function setThrottle(v){document.getElementById('throttleSlider').value=v;updateSliders();sendActuators();}";
        html += "function setRudder(v){document.getElementById('rudderSlider').value=v;updateSliders();sendActuators();}";
        html += "function sendActuators(){const t=parseInt(document.getElementById('throttleSlider').value);const r=parseInt(document.getElementById('rudderSlider').value);";
        html += "fetch('/api/control/actuator',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({throttle:t,rudder:r})});}";
        html += "function sendAction(a){const rid=parseInt(document.getElementById('targetRoverSelect').value);";
        html += "fetch('/api/control/action',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action:a,rover_id:rid})})";
        html += ".then(r=>r.json()).then(res=>alert(res.msg));}";
        html += "function sendEmergencyStop(){setThrottle(0);setRudder(0);sendAction(2);}";
        html += "function changeRover(id){fetch('/api/select_rover',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({rover_id:parseInt(id)})}).then(()=>location.reload());}";
        html += "</script>";

        html += getHtmlFooter();
        _server.send(200, "text/html", html);
    }

    // -------------------------------------------------------------
    // APIS JSON
    // -------------------------------------------------------------
    void handleApiStatus() {
        JsonDocument doc;
        String mac = getMacAddress();

        doc["station_id"] = mac;
        doc["mac_address"] = mac;
        doc["station_name"] = BASE_STATION_ID;
        doc["operation_state"] = _fleet.getOperationStateStr();
        doc["operation_state_id"] = static_cast<uint8_t>(_fleet.getOperationState());
        doc["operation_state_desc"] = _fleet.getOperationStateDesc();
        doc["race_id"] = _fleet.getRaceId();
        doc["mission_name"] = _fleet.getMissionName();

        doc["wifi"]["mac"] = mac;
        doc["wifi"]["ap"] = _wifi.isAPMode();
        doc["wifi"]["ssid"] = _wifi.getSSID();
        doc["wifi"]["ip"] = _wifi.getIPAddress();
        doc["wifi"]["rssi"] = _wifi.getRSSI();
        doc["selected_rover_id"] = _fleet.getSelectedRoverId();
        doc["portal_rovers_received"] = _fleet.hasReceivedRoversFromPortal();
        doc["screen_brightness"] = _display ? _display->getBrightness() : 85;
        doc["display"]["brightness"] = _display ? _display->getBrightness() : 85;
        doc["display"]["screen_brightness"] = _display ? _display->getBrightness() : 85;
        doc["display"]["joy_screen"] = _display ? _display->isJoystickScreenEnabled() : false;
        doc["lora_ready"] = _loraReady ? *_loraReady : false;
        doc["lora_enabled"] = (bool)ENABLE_LORA;

        JsonObject ver = doc["version"].to<JsonObject>();
        ver["bundle"] = BASE_FIRMWARE_VERSION_STR;
        ver["lafvin"] = BASE_LAFVIN_VERSION;
        ver["c6"] = _c6Version.isEmpty() ? "Desconhecido" : _c6Version;
        ver["build_date"] = BASE_BUILD_DATE;

        if (_network) {
            JsonObject cObj = doc["cloud"].to<JsonObject>();
            cObj["connected"] = _network->isConnected();
            cObj["sync_success"] = _network->isSyncSuccess();
            cObj["last_http_status"] = _network->getLastHttpStatus();
            cObj["success_count"] = _network->getSuccessCount();
            cObj["fail_count"] = _network->getFailCount();
            cObj["last_sync_ms"] = _network->getLastSyncTime();
            cObj["endpoint"] = WINDDRAGONS_API_URL;
        }

        FleetRover *sel = _fleet.getSelectedRover();
        if (sel) {
            JsonObject sObj = doc["selected"].to<JsonObject>();
            sObj["id"] = sel->id;
            sObj["code"] = sel->code;
            sObj["name"] = sel->name;
            sObj["mac"] = sel->mac;
            sObj["role"] = sel->role;
            sObj["target_lat"] = sel->target_lat;
            sObj["target_lng"] = sel->target_lng;
            sObj["lat"] = sel->lat;
            sObj["lng"] = sel->lng;
            sObj["speed_knots"] = sel->speed_knots;
            sObj["heading_deg"] = sel->heading_deg;
            sObj["battery_pct"] = sel->battery_pct;
            sObj["battery_voltage"] = sel->battery_voltage;
            sObj["motor_status"] = sel->motor_status;
            sObj["anchor_status"] = sel->anchor_status;
            sObj["anchor_depth_m"] = sel->anchor_depth_m;
            sObj["rssi"] = sel->rssi;
            sObj["is_online"] = sel->is_online;
        }

        JsonArray roversArr = doc["rovers"].to<JsonArray>();
        for (const auto &r : _fleet.getRovers()) {
            JsonObject rObj = roversArr.add<JsonObject>();
            rObj["id"] = r.id;
            rObj["code"] = r.code;
            rObj["name"] = r.name;
            rObj["mac"] = r.mac;
            rObj["role"] = r.role;
            rObj["target_lat"] = r.target_lat;
            rObj["target_lng"] = r.target_lng;
            rObj["lat"] = r.lat;
            rObj["lng"] = r.lng;
            rObj["speed_knots"] = r.speed_knots;
            rObj["heading_deg"] = r.heading_deg;
            rObj["battery_pct"] = r.battery_pct;
            rObj["battery_voltage"] = r.battery_voltage;
            rObj["motor_status"] = r.motor_status;
            rObj["anchor_status"] = r.anchor_status;
            rObj["anchor_depth_m"] = r.anchor_depth_m;
            rObj["rssi"] = r.rssi;
            rObj["is_online"] = r.is_online;
        }

        String res;
        serializeJson(doc, res);
        _server.send(200, "application/json", res);
    }

    void handleApiMission() {
        JsonDocument doc;
        doc["status"] = "ok";
        doc["operation_state"] = _fleet.getOperationStateStr();
        doc["operation_state_id"] = static_cast<uint8_t>(_fleet.getOperationState());
        doc["operation_state_desc"] = _fleet.getOperationStateDesc();
        doc["race_id"] = _fleet.getRaceId();
        doc["mission_name"] = _fleet.getMissionName();
        doc["selected_rover_id"] = _fleet.getSelectedRoverId();
        doc["portal_rovers_received"] = _fleet.hasReceivedRoversFromPortal();

        JsonArray roversArr = doc["rovers"].to<JsonArray>();
        for (const auto &r : _fleet.getRovers()) {
            JsonObject rObj = roversArr.add<JsonObject>();
            rObj["id"] = r.id;
            rObj["code"] = r.code;
            rObj["name"] = r.name;
            rObj["mac"] = r.mac;
            rObj["mac_address"] = r.mac;
            rObj["role"] = r.role;
            rObj["target_lat"] = r.target_lat;
            rObj["target_lng"] = r.target_lng;
            rObj["lat"] = r.lat;
            rObj["lng"] = r.lng;
            rObj["speed_knots"] = r.speed_knots;
            rObj["heading_deg"] = r.heading_deg;
            rObj["battery_pct"] = r.battery_pct;
            rObj["battery_voltage"] = r.battery_voltage;
            rObj["motor_status"] = get_motor_status_str(r.motor_status);
            rObj["anchor_status"] = get_anchor_status_str(r.anchor_status);
            rObj["anchor_depth_m"] = r.anchor_depth_m;
            rObj["rssi"] = r.rssi;
            rObj["is_online"] = r.is_online;
        }

        String res;
        serializeJson(doc, res);
        _server.send(200, "application/json", res);
    }

    void handleApiMissionState() {
        if (!_server.hasArg("plain")) {
            _server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Sem payload JSON\"}");
            return;
        }
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, _server.arg("plain"));
        if (err || !doc["state"].is<const char*>()) {
            _server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Campo 'state' invalido\"}");
            return;
        }
        const char *stateStr = doc["state"].as<const char*>();
        _fleet.setOperationStateFromStr(stateStr, true);

        JsonDocument resp;
        resp["status"] = "ok";
        resp["operation_state"] = _fleet.getOperationStateStr();
        resp["operation_state_desc"] = _fleet.getOperationStateDesc();
        String out;
        serializeJson(resp, out);
        _server.send(200, "application/json", out);
    }

    void handleApiMissionRovers() {
        if (!_server.hasArg("plain")) {
            _server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Sem payload JSON\"}");
            return;
        }
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, _server.arg("plain"));
        if (err) {
            _server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"JSON invalido\"}");
            return;
        }

        const char *raceId = doc["race_id"] | "";
        const char *missionName = doc["mission_name"] | "";
        if (strlen(raceId) > 0 || strlen(missionName) > 0) {
            _fleet.setMissionInfo(raceId, missionName, false);
        }

        if (doc["rovers"].is<JsonArray>()) {
            JsonArray arr = doc["rovers"].as<JsonArray>();
            std::vector<FleetRover> newList;
            for (JsonObject rObj : arr) {
                FleetRover r;
                memset(&r, 0, sizeof(r));
                r.id = rObj["id"] | (newList.size() + 1);
                snprintf(r.code, sizeof(r.code), "ROVER-%02d", r.id);
                strncpy(r.name, rObj["name"] | "", sizeof(r.name) - 1);
                strncpy(r.mac, rObj["mac_address"] | (rObj["mac"] | ""), sizeof(r.mac) - 1);
                strncpy(r.role, rObj["role"] | "", sizeof(r.role) - 1);
                r.target_lat = rObj["lat"] | (rObj["target_lat"] | 0.0);
                r.target_lng = rObj["lng"] | (rObj["target_lng"] | 0.0);
                r.battery_voltage = 12.0f;
                r.is_online = false;

                FleetRover *prev = _fleet.getRoverById(r.id);
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
                _fleet.getRoversMutable() = newList;
                if (_fleet.getOperationState() == BASE_OP_WAITING_ROVERS) {
                    _fleet.setOperationState(BASE_OP_IDLE, false);
                }
            }
        }

        if (doc["operation_state"].is<const char*>()) {
            _fleet.setOperationStateFromStr(doc["operation_state"].as<const char*>(), false);
        }

        _fleet.saveMissionConfig();

        JsonDocument resp;
        resp["status"] = "ok";
        resp["rover_count"] = _fleet.getRovers().size();
        resp["operation_state"] = _fleet.getOperationStateStr();
        String out;
        serializeJson(resp, out);
        _server.send(200, "application/json", out);
    }

    void handleApiMissionClear() {
        _fleet.clearRovers();
        _fleet.setOperationState(BASE_OP_WAITING_ROVERS, true);
        _server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Missao reiniciada e lista em espera\"}");
    }

    void handleApiDisplayGet() {
        bool joyScr = _display ? _display->isJoystickScreenEnabled() : false;
        uint8_t bright = _display ? _display->getBrightness() : 85;
        Preferences prefs;
        if (prefs.begin("base_display", true)) {
            joyScr = prefs.getBool("joy_scr", joyScr);
            bright = prefs.getUChar("brightness", bright);
            prefs.end();
        }
        JsonDocument doc;
        doc["joy_screen"] = joyScr;
        doc["page_count"] = joyScr ? 4 : 3;
        doc["screen_brightness"] = bright;
        doc["brightness"] = bright;
        String out;
        serializeJson(doc, out);
        _server.send(200, "application/json", out);
    }

    void handleApiDisplayPost() {
        if (!_server.hasArg("plain")) {
            _server.send(400, "application/json", "{\"status\":\"error\",\"msg\":\"Sem payload\"}");
            return;
        }
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, _server.arg("plain"));
        if (err) {
            _server.send(400, "application/json", "{\"status\":\"error\",\"msg\":\"JSON invalido\"}");
            return;
        }
        bool joyScr = _display ? _display->isJoystickScreenEnabled() : false;
        uint8_t bright = _display ? _display->getBrightness() : 85;
        bool hasJoy = false;
        bool hasBright = false;

        Preferences prefs;
        if (prefs.begin("base_display", false)) {
            if (!doc["joy_screen"].isNull()) {
                joyScr = doc["joy_screen"].as<bool>();
                prefs.putBool("joy_scr", joyScr);
                hasJoy = true;
            }
            if (!doc["screen_brightness"].isNull() || !doc["brightness"].isNull()) {
                bright = doc["screen_brightness"] | doc["brightness"] | bright;
                if (bright > 100) bright = 100;
                prefs.putUChar("brightness", bright);
                hasBright = true;
            }
            prefs.end();
        }
        if (hasJoy && _display) {
            _display->setJoystickScreenEnabled(joyScr);
            Serial.printf("[BASE CONFIG] 4º Ecra (Teste Joystick): %s\n", joyScr ? "ATIVADO" : "DESATIVADO");
        }
        if (hasBright && _display) {
            _display->setBrightness(bright);
            Serial.printf("[BASE CONFIG] Brilho do Ecra: %u%%\n", bright);
        }

        JsonDocument res;
        res["status"] = "ok";
        res["joy_screen"] = joyScr;
        res["screen_brightness"] = bright;
        res["brightness"] = bright;
        String out;
        serializeJson(res, out);
        _server.send(200, "application/json", out);
    }

    void handleApiDisplayBrightness() {
        uint8_t val = _display ? _display->getBrightness() : 85;
        if (_server.hasArg("val")) {
            val = _server.arg("val").toInt();
        } else if (_server.hasArg("plain")) {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, _server.arg("plain"));
            if (!err) {
                val = doc["screen_brightness"] | doc["brightness"] | val;
            }
        }
        if (val > 100) val = 100;

        if (_display) {
            _display->saveBrightness(val);
        } else {
            Preferences prefs;
            if (prefs.begin("base_display", false)) {
                prefs.putUChar("brightness", val);
                prefs.end();
            }
        }

        JsonDocument res;
        res["success"] = true;
        res["screen_brightness"] = val;
        res["brightness"] = val;
        res["msg"] = "Brilho do ecrã ajustado para " + String(val) + "%";
        String out;
        serializeJson(res, out);
        _server.send(200, "application/json", out);
    }

    void handleApiScan() {
        int n = WiFi.scanNetworks();
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();

        for (int i = 0; i < n; ++i) {
            JsonObject item = arr.add<JsonObject>();
            item["ssid"] = WiFi.SSID(i);
            item["rssi"] = WiFi.RSSI(i);
            item["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
        }

        String res;
        serializeJson(doc, res);
        _server.send(200, "application/json", res);
    }

    void handleApiWifiSave() {
        if (!_server.hasArg("plain")) {
            _server.send(400, "application/json", "{\"error\":\"Missing body\"}");
            return;
        }
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, _server.arg("plain"));
        if (err) {
            _server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
            return;
        }

        String ssid = doc["ssid"] | "";
        String pass = doc["password"] | "";
        bool isDef = doc["is_default"] | false;

        if (ssid.length() == 0) {
            _server.send(400, "application/json", "{\"error\":\"SSID vazio\"}");
            return;
        }

        _wifi.addOrUpdateNetwork(ssid, pass, isDef);
        _server.send(200, "application/json", "{\"msg\":\"Rede WiFi guardada com sucesso!\"}");
    }

    void handleApiWifiSetDefault() {
        if (!_server.hasArg("plain")) {
            _server.send(400, "application/json", "{\"error\":\"Missing body\"}");
            return;
        }
        JsonDocument doc;
        deserializeJson(doc, _server.arg("plain"));
        String ssid = doc["ssid"] | "";
        _wifi.setDefault(ssid);
        _server.send(200, "application/json", "{\"msg\":\"Rede padrao atualizada!\"}");
    }

    void handleApiWifiDelete() {
        if (!_server.hasArg("plain")) {
            _server.send(400, "application/json", "{\"error\":\"Missing body\"}");
            return;
        }
        JsonDocument doc;
        deserializeJson(doc, _server.arg("plain"));
        String ssid = doc["ssid"] | "";
        _wifi.removeNetwork(ssid);
        _server.send(200, "application/json", "{\"msg\":\"Rede removida!\"}");
    }

    void handleApiWifiConnect() {
        if (!_server.hasArg("plain")) {
            _server.send(400, "application/json", "{\"error\":\"Missing body\"}");
            return;
        }
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, _server.arg("plain"));
        if (err) {
            _server.send(400, "application/json", "{\"error\":\"JSON invalido\"}");
            return;
        }
        String ssid = doc["ssid"] | "";
        if (ssid.length() == 0) {
            _server.send(400, "application/json", "{\"error\":\"SSID vazio\"}");
            return;
        }

        _server.send(200, "application/json", "{\"msg\":\"A iniciar ligacao a '" + ssid + "'... Verifique o visor LCD da Base.\"}");
        delay(400);

        bool ok = _wifi.connectTo(ssid, 10000);
        if (!ok) {
            Serial.println("[BASE] Ligacao manual falhou. A restaurar Ponto de Acesso...");
            _wifi.startAccessPoint("WindDragons-Base", "12345678");
        }
    }

    void handleApiWifiReconnect() {
        _server.send(200, "application/json", "{\"msg\":\"A reiniciar WiFi para tentar ligar em modo cliente...\"}");
        delay(500);
        _wifi.autoConnect();
    }

    void handleApiActuator() {
        if (!_server.hasArg("plain")) {
            _server.send(400, "application/json", "{\"error\":\"Missing body\"}");
            return;
        }
        JsonDocument doc;
        deserializeJson(doc, _server.arg("plain"));
        
        _webThrottle = constrain(doc["throttle"] | 0, -100, 100);
        _webRudder   = constrain(doc["rudder"] | 0, -100, 100);
        _webAnchorJog= constrain(doc["anchor_jog"] | 0, -1, 1);
        _lastWebControlTime = millis();
        _webControlActive = true;

        _server.send(200, "application/json", "{\"status\":\"ok\"}");
    }

    void handleApiAction() {
        if (!_server.hasArg("plain")) {
            _server.send(400, "application/json", "{\"error\":\"Missing body\"}");
            return;
        }
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, _server.arg("plain"));
        if (err) {
            _server.send(400, "application/json", "{\"error\":\"JSON invalido\"}");
            return;
        }

        uint8_t action = doc["action"] | 0;
        uint8_t targetRid = doc["rover_id"] | _fleet.getSelectedRoverId();
        uint16_t depth = doc["depth_cm"] | 0;
        uint16_t duration = doc["duration_s"] | 0;

        double lat = doc["lat"] | (doc["target_lat"] | 0.0);
        double lng = doc["lng"] | (doc["target_lng"] | 0.0);
        int32_t latDeg7 = static_cast<int32_t>(round(lat * 10000000.0));
        int32_t lngDeg7 = static_cast<int32_t>(round(lng * 10000000.0));

        // Se foram fornecidas coordenadas e nenhuma ação específica foi selecionada, define ACTION_NAVIGATE (6)
        if (action == 0 && (latDeg7 != 0 || lngDeg7 != 0)) {
            action = ACTION_NAVIGATE;
        }

        FleetRover *r = _fleet.getRoverById(targetRid);
        if (r && (lat != 0.0 || lng != 0.0)) {
            r->target_lat = lat;
            r->target_lng = lng;
        }

        _fleet.queueCommand(targetRid, ++_nextCmdId, action, depth, duration, latDeg7, lngDeg7);

        String msg = "Comando '" + String(get_action_str(action)) + "' colocado na fila LoRa para ROVER-" + String(targetRid);
        if (latDeg7 != 0 || lngDeg7 != 0) {
            msg += " (Alvo: " + String(lat, 6) + ", " + String(lng, 6) + ")";
        }
        _server.send(200, "application/json", "{\"status\":\"queued\",\"msg\":\"" + msg + "\",\"rover_id\":" + String(targetRid) + ",\"action\":" + String(action) + ",\"lat\":" + String(lat, 6) + ",\"lng\":" + String(lng, 6) + "}");
    }

    void handleApiSelectRover() {
        if (!_server.hasArg("plain")) {
            _server.send(400, "application/json", "{\"error\":\"Missing body\"}");
            return;
        }
        JsonDocument doc;
        deserializeJson(doc, _server.arg("plain"));
        uint8_t rid = doc["rover_id"] | 0;

        _fleet.selectRoverById(rid);
        if (rid == 0) {
            Serial.println("[BASE] Rover desselecionado via Web (Nenhum rover selecionado)");
        } else {
            Serial.printf("[BASE] Rover %u selecionado via Web\n", rid);
        }
        _server.send(200, "application/json", "{\"status\":\"ok\",\"selected_rover_id\":" + String(rid) + "}");
    }

    // -------------------------------------------------------------
    // PÁGINA 5: EXPLORADOR DO CARTÃO MICROSD (BASE STATION)
    // -------------------------------------------------------------
    void handleSdPage() {
        String html = getHtmlHeader("Explorador do Cartão MicroSD", "sd");

        bool sdOk = _sd && _sd->isReady();
        uint64_t totalMb = sdOk ? (_sd->getTotalBytes() / (1024ULL * 1024ULL)) : 0;
        uint64_t usedMb = sdOk ? (_sd->getUsedBytes() / (1024ULL * 1024ULL)) : 0;
        uint64_t freeMb = sdOk ? (_sd->getFreeBytes() / (1024ULL * 1024ULL)) : 0;
        uint8_t usedPct = (totalMb > 0) ? (uint8_t)((usedMb * 100ULL) / totalMb) : 0;

        html += "<div class='card'>";
        html += "<div class='card-title'><span>💾 Armazenamento MicroSD - Base Station</span>";
        if (sdOk) {
            html += "<span class='badge' style='background:#065f46;color:#6ee7b7;'>FAT32 PRONTO</span>";
        } else {
            html += "<span class='badge' style='background:#7f1d1d;color:#fca5a5;'>NÃO DETETADO</span>";
        }
        html += "</div>";

        if (!sdOk) {
            html += "<div style='padding:20px;text-align:center;'>";
            html += "<p style='color:var(--yellow);font-size:1.1rem;margin-bottom:8px;'>⚠️ Cartão MicroSD não detetado ou não montado na Base Station.</p>";
            html += "<p style='color:var(--muted);font-size:0.875rem;'>Insira um cartão formatado em FAT32 no slot MicroSD da Base Station e reinicie o dispositivo.</p>";
            html += "</div></div>";
            html += getHtmlFooter();
            _server.send(200, "text/html", html);
            return;
        }

        // Barra de progresso do Cartão
        html += "<div style='display:flex;justify-content:space-between;align-items:baseline;flex-wrap:wrap;gap:8px;font-size:0.875rem;margin-bottom:8px;'>";
        html += "<span style='color:var(--muted);'>Espaço Utilizado: <strong style='color:var(--text);font-size:0.95rem;'>" + String((uint32_t)usedMb) + " MB</strong> de " + String((uint32_t)totalMb) + " MB (" + String(usedPct) + "%)</span>";
        html += "<span style='color:var(--muted);'>Livre: <strong style='color:var(--green);font-size:0.95rem;'>" + String((uint32_t)freeMb) + " MB</strong></span>";
        html += "</div>";
        html += "<div class='progress-bg' style='height:10px;margin-top:0;'><div class='progress-bar' style='width:" + String(usedPct) + "%;background:var(--primary);'></div></div>";
        html += "</div>";

        // Card Explorador
        html += "<div class='card'>";
        html += "<div class='card-title' style='margin-bottom:10px;'><span>Explorador de Ficheiros (/www/base/ & Logs)</span></div>";
        
        // Barra de Ações & Breadcrumbs
        html += "<div style='display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-bottom:14px;'>";
        html += "<button onclick='loadDir(\"/\")' class='btn btn-secondary' style='padding:6px 12px;'>📁 Raiz (/)</button>";
        html += "<button onclick='loadDir(\"/www\")' class='btn btn-secondary' style='padding:6px 12px;'>🌐 /www</button>";
        html += "<button onclick='loadDir(\"/www/base\")' class='btn btn-primary' style='padding:6px 12px;'>🏠 /www/base</button>";
        html += "<button onclick='createNewDir()' class='btn btn-secondary' style='padding:6px 12px;'>➕ Nova Pasta</button>";
        html += "<span id='curDirLabel' style='color:var(--primary);font-weight:700;font-size:0.9rem;margin-left:auto;'>/</span>";
        html += "</div>";

        // Tabela de ficheiros
        html += "<div id='fileListWrap' style='background:rgba(0,0,0,0.25);border:1px solid var(--card-border);border-radius:10px;padding:8px;min-height:160px;'>";
        html += "<p style='color:var(--muted);text-align:center;padding:20px;'>A carregar ficheiros do cartão SD...</p>";
        html += "</div>";

        // Upload Form
        html += "<div style='margin-top:20px;padding-top:16px;border-top:1px solid var(--card-border);'>";
        html += "<div class='stat-label' style='margin-bottom:8px;'>Carregar Novo Ficheiro para a Pasta Atual:</div>";
        html += "<form id='upForm' method='POST' action='/api/sd/upload' enctype='multipart/form-data' style='display:flex;gap:10px;align-items:center;flex-wrap:wrap;'>";
        html += "<input type='hidden' id='upDir' name='dir' value='/'>";
        html += "<input type='file' id='upFile' name='file' required style='flex:1;min-width:200px;margin:0;'>";
        html += "<button type='submit' class='btn btn-primary'>⬆ Enviar Ficheiro</button>";
        html += "</form></div>";

        html += "</div>";

        // Script interativo
        html += "<script>";
        html += "let curPath='/';";
        html += "function loadDir(dir){";
        html += "curPath=dir;";
        html += "document.getElementById('curDirLabel').innerText=curPath;";
        html += "document.getElementById('upDir').value=curPath;";
        html += "const w=document.getElementById('fileListWrap');";
        html += "w.innerHTML='<p style=\"color:var(--muted);text-align:center;padding:20px;\">A ler diretoria...</p>';";
        html += "fetch('/api/sd/list?dir='+encodeURIComponent(curPath)).then(r=>r.json()).then(data=>{";
        html += "if(!data.files||!data.files.length){w.innerHTML='<p style=\"color:var(--muted);text-align:center;padding:20px;\">Pasta vazia.</p>';return;}";
        html += "let t='<table><thead><tr><th>Nome</th><th>Tipo</th><th>Tamanho</th><th>Ações</th></tr></thead><tbody>';";
        html += "if(curPath!=='/'){";
        html += "const upDir=curPath.substring(0,curPath.lastIndexOf('/'))||'/';";
        html += "t+='<tr><td><a href=\"#\" onclick=\"loadDir(\\''+upDir+'\\');return false;\" style=\"color:var(--primary);text-decoration:none;font-weight:700;\">⬅ .. (Pasta Acima)</a></td><td>PASTA</td><td>-</td><td>-</td></tr>';";
        html += "}";
        html += "data.files.forEach(f=>{";
        html += "t+='<tr>';";
        html += "if(f.is_dir){";
        html += "t+='<td><a href=\"#\" onclick=\"loadDir(\\''+f.path+'\\');return false;\" style=\"color:var(--primary);text-decoration:none;font-weight:700;\">📁 '+f.name+'/</a></td><td>PASTA</td><td>-</td>';";
        html += "t+='<td><div class=\"btn-group\"><button onclick=\"deleteItem(\\''+f.path+'\\',true)\" class=\"btn btn-danger\" style=\"padding:4px 8px;font-size:0.75rem;\">Eliminar</button></div></td>';";
        html += "}else{";
        html += "t+='<td>📄 '+f.name+'</td><td>FICHEIRO</td><td>'+f.size+' B</td>';";
        html += "t+='<td><div class=\"btn-group\"><a href=\"/api/sd/download?file='+encodeURIComponent(f.path)+'\" class=\"btn btn-secondary\" style=\"padding:4px 8px;font-size:0.75rem;\">Baixar</a><button onclick=\"deleteItem(\\''+f.path+'\\',false)\" class=\"btn btn-danger\" style=\"padding:4px 8px;font-size:0.75rem;\">Eliminar</button></div></td>';";
        html += "}";
        html += "t+='</tr>';";
        html += "});";
        html += "t+='</tbody></table>';";
        html += "w.innerHTML=t;";
        html += "}).catch(()=>{w.innerHTML='<p style=\"color:var(--red);text-align:center;padding:20px;\">Erro ao ler o cartão SD.</p>';});";
        html += "}";
        html += "function deleteItem(p,isDir){if(confirm('Eliminar \"'+p+'\" do cartão SD?')){fetch('/api/sd/delete',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({path:p,is_dir:isDir})}).then(r=>r.json()).then(res=>{alert(res.msg);loadDir(curPath);});}}";
        html += "function createNewDir(){const name=prompt('Nome da nova pasta:');if(!name)return;const full=(curPath==='/'?'/'+name:(curPath+'/'+name));fetch('/api/sd/mkdir',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({dir:full})}).then(r=>r.json()).then(res=>{alert(res.msg);loadDir(curPath);});}";
        html += "loadDir('/www/base');";
        html += "</script>";

        html += getHtmlFooter();
        _server.send(200, "text/html", html);
    }

    // -------------------------------------------------------------
    // APIS DO CARTÃO MICROSD
    // -------------------------------------------------------------
    void handleApiSdList() {
        String dir = "/";
        if (_server.hasArg("dir")) {
            dir = _server.arg("dir");
            if (!dir.startsWith("/")) dir = "/" + dir;
        }

        JsonDocument doc;
        doc["ready"] = (_sd && _sd->isReady());
        doc["current_dir"] = dir;
        doc["card_type"] = (_sd && _sd->isReady()) ? _sd->getCardTypeStr() : "NONE";
        doc["total_mb"] = (_sd && _sd->isReady()) ? (uint32_t)(_sd->getTotalBytes() / (1024ULL * 1024ULL)) : 0;
        doc["used_mb"] = (_sd && _sd->isReady()) ? (uint32_t)(_sd->getUsedBytes() / (1024ULL * 1024ULL)) : 0;
        doc["free_mb"] = (_sd && _sd->isReady()) ? (uint32_t)(_sd->getFreeBytes() / (1024ULL * 1024ULL)) : 0;

        JsonArray arr = doc["files"].to<JsonArray>();
        if (_sd && _sd->isReady()) {
            auto entries = _sd->listEntries(dir.c_str());
            for (const auto &e : entries) {
                JsonObject item = arr.add<JsonObject>();
                item["name"] = e.name;
                item["path"] = e.path.length() > 0 ? e.path : (dir == "/" ? ("/" + e.name) : (dir + "/" + e.name));
                item["size"] = (uint32_t)e.size;
                item["is_dir"] = e.isDirectory;
            }
        }

        String res;
        serializeJson(doc, res);
        _server.send(200, "application/json", res);
    }

    void handleApiSdRead() {
        String path = "";
        if (_server.hasArg("file")) {
            path = _server.arg("file");
        } else if (_server.hasArg("path")) {
            path = _server.arg("path");
        } else {
            _server.send(400, "text/plain", "Parâmetro 'file' ou 'path' ausente");
            return;
        }
        if (!path.startsWith("/")) path = "/" + path;

        if (!_sd || !_sd->isReady() || !_sd->fileExists(path.c_str())) {
            _server.send(404, "text/plain", "Ficheiro não encontrado no SD");
            return;
        }

        String content = _sd->readFile(path.c_str(), 16384);
        _server.send(200, "text/plain; charset=utf-8", content);
    }

    void handleApiSdDownload() {
        String path = "";
        if (_server.hasArg("file")) {
            path = _server.arg("file");
        } else if (_server.hasArg("path")) {
            path = _server.arg("path");
        } else {
            _server.send(400, "text/plain", "Parâmetro 'file' ou 'path' ausente");
            return;
        }
        if (!path.startsWith("/")) path = "/" + path;

        if (!_sd || !_sd->isReady() || !_sd->fileExists(path.c_str())) {
            _server.send(404, "text/plain", "Ficheiro não encontrado no cartão SD");
            return;
        }

        File f = _sd->openFile(path.c_str(), FILE_READ);
        if (!f || f.isDirectory()) {
            if (f) f.close();
            _server.send(400, "text/plain", "Ficheiro inválido ou é diretório");
            return;
        }

        String filename = path.substring(path.lastIndexOf('/') + 1);
        _server.sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
        _server.streamFile(f, "application/octet-stream");
        f.close();
    }

    void handleApiSdSave() {
        if (!_server.hasArg("plain")) {
            _server.send(400, "application/json", "{\"error\":\"Missing body\"}");
            return;
        }
        JsonDocument doc;
        deserializeJson(doc, _server.arg("plain"));

        String path = doc["file"] | (doc["path"] | "");
        String content = doc["content"] | "";
        if (!path.startsWith("/")) path = "/" + path;

        if (!_sd || !_sd->isReady()) {
            _server.send(500, "application/json", "{\"success\":false,\"msg\":\"Cartão SD não disponível\"}");
            return;
        }

        bool ok = _sd->writeFile(path.c_str(), content.c_str());

        // Se gravou redes wifi, atualizar wifiConfig
        if (ok && path == "/wifi_networks.json") {
            _wifi.loadNetworksFromSD();
        }

        JsonDocument res;
        res["success"] = ok;
        res["msg"] = ok ? "Ficheiro guardado com sucesso no cartão SD!" : "Erro ao gravar no cartão SD";
        String out;
        serializeJson(res, out);
        _server.send(200, "application/json", out);
    }

    void handleApiSdDelete() {
        if (!_server.hasArg("plain")) {
            _server.send(400, "application/json", "{\"error\":\"Missing body\"}");
            return;
        }
        JsonDocument doc;
        deserializeJson(doc, _server.arg("plain"));

        String path = doc["path"] | (doc["file"] | (doc["dir"] | ""));
        bool isDir = doc["is_dir"] | false;
        if (!path.startsWith("/")) path = "/" + path;

        while (path.length() > 1 && path.endsWith("/")) {
            path = path.substring(0, path.length() - 1);
        }

        if (path == "/" || path == "") {
            _server.send(400, "application/json", "{\"success\":false,\"msg\":\"Não é permitido apagar a raiz (/)\"}");
            return;
        }

        if (!_sd || !_sd->isReady()) {
            _server.send(500, "application/json", "{\"success\":false,\"msg\":\"Cartão SD não disponível\"}");
            return;
        }

        bool ok = false;
        if (isDir) {
            ok = _sd->deleteDir(path.c_str(), true);
        } else {
            ok = _sd->deleteFile(path.c_str());
            if (!ok) {
                ok = _sd->deleteDir(path.c_str(), true);
            }
        }

        JsonDocument res;
        res["success"] = ok;
        res["msg"] = ok ? (isDir ? "Diretório eliminado com sucesso!" : "Item eliminado do cartão SD!") : "Erro ao eliminar do cartão SD";
        String out;
        serializeJson(res, out);
        _server.send(200, "application/json", out);
    }

    void handleApiSdDeleteDir() {
        if (!_server.hasArg("plain")) {
            _server.send(400, "application/json", "{\"error\":\"Missing body\"}");
            return;
        }
        JsonDocument doc;
        deserializeJson(doc, _server.arg("plain"));

        String path = doc["path"] | (doc["dir"] | "");
        if (!path.startsWith("/")) path = "/" + path;

        while (path.length() > 1 && path.endsWith("/")) {
            path = path.substring(0, path.length() - 1);
        }

        if (path == "/" || path == "") {
            _server.send(400, "application/json", "{\"success\":false,\"msg\":\"Não é permitido apagar a raiz (/)\"}");
            return;
        }

        if (!_sd || !_sd->isReady()) {
            _server.send(500, "application/json", "{\"success\":false,\"msg\":\"Cartão SD não disponível\"}");
            return;
        }

        bool ok = _sd->deleteDir(path.c_str(), true);

        JsonDocument res;
        res["success"] = ok;
        res["msg"] = ok ? "Diretório eliminado com sucesso do cartão SD!" : "Erro ao eliminar diretório do cartão SD";
        String out;
        serializeJson(res, out);
        _server.send(200, "application/json", out);
    }

    void handleApiSdMkdir() {
        if (!_server.hasArg("plain")) {
            _server.send(400, "application/json", "{\"error\":\"Missing body\"}");
            return;
        }
        JsonDocument doc;
        deserializeJson(doc, _server.arg("plain"));

        String path = doc["path"] | (doc["dir"] | "");
        if (!path.startsWith("/")) path = "/" + path;

        while (path.length() > 1 && path.endsWith("/")) {
            path = path.substring(0, path.length() - 1);
        }

        if (path == "/" || path == "") {
            _server.send(400, "application/json", "{\"success\":false,\"msg\":\"Nome de diretório inválido\"}");
            return;
        }

        if (!_sd || !_sd->isReady()) {
            _server.send(500, "application/json", "{\"success\":false,\"msg\":\"Cartão SD não disponível\"}");
            return;
        }

        bool ok = _sd->createDir(path.c_str());

        JsonDocument res;
        res["success"] = ok;
        res["msg"] = ok ? "Diretório criado com sucesso no cartão SD!" : "Erro ao criar diretório no cartão SD";
        String out;
        serializeJson(res, out);
        _server.send(200, "application/json", out);
    }

    void handleApiSdUploadData() {
        if (!_sd || !_sd->isReady()) return;

        HTTPUpload& upload = _server.upload();
        if (upload.status == UPLOAD_FILE_START) {
            _server.client().setTimeout(30000);
            _uploadSuccess = false;
            _isBusy = true;
            String dir = _server.hasArg("dir") ? _server.arg("dir") : "/";
            if (!dir.startsWith("/")) dir = "/" + dir;
            while (dir.length() > 1 && dir.endsWith("/")) {
                dir = dir.substring(0, dir.length() - 1);
            }

            String filename = upload.filename;
            if (filename.startsWith("/")) filename = filename.substring(1);
            String fullPath = (dir == "/") ? ("/" + filename) : (dir + "/" + filename);
            _uploadCurrentPath = fullPath;
            Serial.printf("[BASE SD UPLOAD] A iniciar envio de: %s\n", fullPath.c_str());

            if (_sd->fileExists(fullPath.c_str())) {
                _sd->deleteFile(fullPath.c_str());
            }

            _uploadFile = _sd->openFile(fullPath.c_str(), FILE_WRITE);
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            yield();
            if (_uploadFile) {
                _sd->prepareBus();
                _uploadFile.write(upload.buf, upload.currentSize);
            }
        } else if (upload.status == UPLOAD_FILE_END) {
            if (_uploadFile) {
                _uploadFile.flush();
                _uploadFile.close();
                _uploadSuccess = true;
                Serial.printf("[BASE SD UPLOAD] Concluído com sucesso: %s (%u bytes)\n", upload.filename.c_str(), upload.totalSize);
            }
            _isBusy = false;
        } else if (upload.status == UPLOAD_FILE_ABORTED) {
            _uploadSuccess = false;
            if (_uploadFile) {
                _uploadFile.close();
                if (_sd->fileExists(_uploadCurrentPath.c_str())) {
                    _sd->deleteFile(_uploadCurrentPath.c_str());
                }
                Serial.printf("[BASE SD UPLOAD] Envio cancelado/abortado: %s\n", upload.filename.c_str());
            }
            _isBusy = false;
        }
    }

    void handleApiSdUploadFinish() {
        if (_server.hasArg("ajax")) {
            if (_uploadSuccess) {
                _server.send(200, "application/json", "{\"success\":true,\"msg\":\"Upload concluído com sucesso na Base\"}");
            } else {
                _server.send(500, "application/json", "{\"success\":false,\"msg\":\"Falha ou aborto durante o upload na Base\"}");
            }
            return;
        }
        _server.sendHeader("Location", "/sd");
        _server.send(303, "text/plain", _uploadSuccess ? "Upload concluído com sucesso" : "Erro no upload");
    }

    void handleOtaC6UploadData() {
        HTTPUpload& upload = _server.upload();
        if (upload.status == UPLOAD_FILE_START) {
            _server.client().setTimeout(30000);
            _uploadSuccess = false;
            _isBusy = true;
            _c6OtaProgress = 5;
            _c6OtaState = "UPLOADING";
            _c6OtaDetail = "A receber binario c6.bin no cartao SD da Base...";
            Serial.printf("[BASE OTA C6] Recebendo firmware: %s\n", upload.filename.c_str());
            if (_display) _display->showOtaScreen("COPROCESSADOR C6", "PASSO 1/2: RECEBER FICHEIRO", 5, _c6OtaDetail.c_str(), UI_CYAN);

            if (_sd && _sd->isReady()) {
                if (!_sd->fileExists("/firmware")) {
                    _sd->createDir("/firmware");
                }
                if (_sd->fileExists("/firmware/c6.bin")) {
                    _sd->deleteFile("/firmware/c6.bin");
                }
                _uploadFile = _sd->openFile("/firmware/c6.bin", FILE_WRITE);
            }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            yield();
            if (_uploadFile && _sd) {
                _sd->prepareBus();
                _uploadFile.write(upload.buf, upload.currentSize);
            }
        } else if (upload.status == UPLOAD_FILE_END) {
            if (_uploadFile) {
                _uploadFile.flush();
                _uploadFile.close();
                _uploadSuccess = true;
                _c6OtaProgress = 15;
                _c6OtaState = "SAVED";
                _c6OtaDetail = String("Ficheiro guardado no SD (") + (upload.totalSize / 1024) + " KB)";
                Serial.printf("[BASE OTA C6] Firmware gravado no SD: %u bytes\n", upload.totalSize);
                if (_display) _display->showOtaScreen("COPROCESSADOR C6", "PASSO 1/2: FICHEIRO GUARDADO", 15, _c6OtaDetail.c_str(), UI_YELLOW);
            }
            _isBusy = false;
        } else if (upload.status == UPLOAD_FILE_ABORTED) {
            _uploadSuccess = false;
            _c6OtaState = "ERROR";
            _c6OtaDetail = "Upload abortado";
            if (_uploadFile) _uploadFile.close();
            _isBusy = false;
            if (_display) _display->showOtaScreen("COPROCESSADOR C6", "PASSO 1/2: FALHA NO UPLOAD", 0, "Envio do ficheiro c6.bin abortado", UI_RED);
        }
    }

    void handleOtaC6UploadFinish() {
        if (!_uploadSuccess) {
            _server.send(500, "application/json", "{\"success\":false,\"error\":\"Falha ao receber c6.bin no cartao SD\"}");
            return;
        }

        // Obter credenciais de rede
        String ssid = _wifi.getSSID();
        String pass = _wifi.getConnectedPassword();
        String ip   = _wifi.getIPAddress();

        if (_wifi.isAPMode() || ip == "0.0.0.0" || ssid == "Desconectado" || ssid.isEmpty()) {
            ssid = (_wifi.getSSID() != "Desconectado" && !_wifi.getSSID().isEmpty()) ? _wifi.getSSID() : "WindDragons-Base";
            pass = _wifi.getAPPassword();
            if (pass.isEmpty()) pass = "12345678";
            ip = "192.168.4.1";
        }

        String otaUrl = "http://" + ip + "/firmware/c6.bin";
        String cmd = "CMD:OTA:" + ssid + "|" + pass + "|" + otaUrl;

        Serial.printf("[BASE OTA C6] Disparando OTA para C6: %s\n", cmd.c_str());
        if (_commandQueueCb) {
            _commandQueueCb(cmd.c_str());
        }

        _c6OtaProgress = 20;
        _c6OtaState = "STARTING";
        _c6OtaTimestamp = millis();
        _c6OtaDetail = "A aguardar ligacao do Coprocessador C6...";
        if (_display) _display->showOtaScreen("COPROCESSADOR C6", "PASSO 1/2: DOWNLOAD C6", 20, _c6OtaDetail.c_str(), UI_YELLOW);

        _server.send(200, "application/json", "{\"success\":true,\"state\":\"STARTING\",\"url\":\"" + otaUrl + "\"}");
    }

    void handleApiOtaC6Status() {
        JsonDocument doc;
        doc["state"] = _c6OtaState;
        doc["detail"] = _c6OtaDetail;
        doc["progress"] = _c6OtaProgress;
        doc["time"] = millis();
        String res;
        serializeJson(doc, res);
        _server.send(200, "application/json", res);
    }

    void handleApiOtaCancel() {
        Serial.println("[BASE OTA] Cancelamento manual de OTA solicitado via API.");
        _c6OtaState = "IDLE";
        _c6OtaDetail = "OTA cancelado";
        _c6OtaProgress = 0;
        _c6OtaTimestamp = 0;
        _otaAutoClearMs = 0;
        _isBusy = false;
        if (_display) _display->setOtaActive(false);
        _server.send(200, "application/json", "{\"success\":true,\"msg\":\"OTA cancelado. Operação e ecrã restaurados.\"}");
    }

    void handleApiOtaLafvinData() {
        HTTPUpload& upload = _server.upload();
        if (upload.status == UPLOAD_FILE_START) {
            _server.client().setTimeout(40000);
            _isBusy = true;
            _lafvinOtaBytes = 0;
            Serial.printf("[BASE OTA LAFVIN] A iniciar gravacao: %s\n", upload.filename.c_str());
            if (_display) _display->showOtaScreen("MASTER LAFVIN", "PASSO 2/2: INICIO GRAVACAO", 0, "A preparar particao Flash OTA...", UI_YELLOW);
            if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                Update.printError(Serial);
                if (_display) _display->showOtaScreen("MASTER LAFVIN", "FALHA NA FLASH", 0, "Erro ao iniciar Update OTA", UI_RED);
            }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            yield();
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                Update.printError(Serial);
                if (_display) _display->showOtaScreen("MASTER LAFVIN", "FALHA NA FLASH", 0, "Erro ao gravar bloco Flash", UI_RED);
            } else {
                _lafvinOtaBytes += upload.currentSize;
                size_t totalExpected = _server.header("Content-Length").toInt();
                uint8_t pct = (totalExpected > 0) ? (uint8_t)((_lafvinOtaBytes * 100) / totalExpected) : 50;
                char d[48];
                snprintf(d, sizeof(d), "%u KB gravados na particao Flash", (unsigned)(_lafvinOtaBytes / 1024));
                if (_display) _display->showOtaScreen("MASTER LAFVIN", "PASSO 2/2: A GRAVAR FLASH", pct, d, UI_CYAN);
            }
        } else if (upload.status == UPLOAD_FILE_END) {
            if (Update.end(true)) {
                Serial.printf("[BASE OTA LAFVIN] Gravacao concluida: %u bytes\n", upload.totalSize);
                if (_display) _display->showOtaScreen("MASTER LAFVIN", "CONCLUIDO COM SUCESSO", 100, "100% OK! A reiniciar Base...", UI_GREEN);
            } else {
                Update.printError(Serial);
                if (_display) _display->showOtaScreen("MASTER LAFVIN", "FALHA NA VERIFICACAO", 0, "Erro de integridade na Flash", UI_RED);
            }
            _isBusy = false;
        } else if (upload.status == UPLOAD_FILE_ABORTED) {
            Update.end();
            _isBusy = false;
            Serial.println("[BASE OTA LAFVIN] Gravacao abortada");
            if (_display) _display->showOtaScreen("MASTER LAFVIN", "FALHA NA GRAVACAO", 0, "Upload abortado", UI_RED);
        }
    }

    void handleApiOtaLafvinFinish() {
        _server.sendHeader("Connection", "close");
        if (Update.hasError()) {
            if (_display) _display->showOtaScreen("MASTER LAFVIN", "FALHA NA GRAVACAO", 0, "Erro ao gravar firmware", UI_RED);
            _server.send(500, "application/json", "{\"success\":false,\"error\":\"Erro ao gravar firmware do Master Lafvin\"}");
        } else {
            if (_display) _display->showOtaScreen("MASTER LAFVIN", "ATUALIZACAO CONCLUIDA", 100, "Master OK! A reiniciar em 2 segundos...", UI_GREEN);
            _server.send(200, "application/json", "{\"success\":true,\"message\":\"Master Lafvin atualizado com sucesso! A reiniciar em 2 segundos...\"}");
            _restartScheduled = millis() + 1500;
        }
    }
};
