#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <Update.h>
#include <ArduinoJson.h>
#include "wifi_config_manager.h"
#include "rover_config_manager.h"
#include "sd_card_manager.h"
#include "lora_protocol.h"
#include "gps_tracker.h"
#include "battery_monitor.h"
#include "actuators.h"
#include "rover_display.h"
#include "qrcode_helper.h"
#include "rover_version.h"
#include "web_console.h"

struct RoverLoraDiagnostics {
    bool hwReady = false;
    uint32_t txTelems = 0;
    uint32_t rxPolls = 0;
    uint32_t lastRxSecAgo = 999999;
    float rssi = 0.0f;
    float snr = 0.0f;
    bool hasContact = false;
};

class RoverWebServer {
public:
    RoverWebServer(WiFiConfigManager &wifiMgr, GPSTracker &gps, BatteryMonitor &battery,
                   ActuatorController &actuators, uint8_t &motorStatus, uint32_t &lastControlTime,
                   RoverConfigManager &configMgr, SDCardManager &sdCard, RoverDisplay *display = nullptr)
        : _server(80), _wifi(wifiMgr), _gps(gps), _battery(battery),
          _actuators(actuators), _motorStatus(motorStatus), _lastControlTime(lastControlTime),
          _config(configMgr), _roverId(configMgr.getRoverId()), _sd(sdCard), _display(display) {}

    void begin() {
        setupRoutes();
        _server.begin();
        Serial.println("[ROVER HTTP] Servidor Web ativo na porta 80");
    }

    void handleClient() {
        _server.handleClient();
        if (_restartScheduled > 0 && millis() >= _restartScheduled) {
            _restartScheduled = 0;
            delay(300);
            ESP.restart();
        }
    }

    void setC6OtaStatus(const char* status) {
        if (!status) return;
        _c6OtaTimestamp = millis();
        Serial.printf("[ROVER HTTP] C6 OTA Estado: %s\n", status);

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

    void setRoverId(uint8_t id) {
        _roverId = id;
    }

    typedef std::function<void(const char*)> CommandQueueCallback;
    void setCommandQueueCallback(CommandQueueCallback cb) {
        _commandQueueCb = cb;
    }

    void setDisplay(RoverDisplay *display) {
        _display = display;
    }

    void setLoraDiagnosticsPtr(const RoverLoraDiagnostics *diag) {
        _loraDiag = diag;
    }

    bool isBusy() const {
        return _isBusy;
    }

private:
    WebServer _server;
    CommandQueueCallback _commandQueueCb = nullptr;
    const RoverLoraDiagnostics *_loraDiag = nullptr;
    WiFiConfigManager &_wifi;
    GPSTracker &_gps;
    BatteryMonitor &_battery;
    ActuatorController &_actuators;
    uint8_t &_motorStatus;
    uint32_t &_lastControlTime;
    RoverConfigManager &_config;
    uint8_t _roverId;
    SDCardManager &_sd;
    RoverDisplay *_display;
    File _uploadFile;
    bool _isBusy = false;
    bool _uploadSuccess = false;
    String _uploadCurrentPath = "";
    String _c6OtaState = "IDLE";
    String _c6OtaDetail = "";
    uint8_t _c6OtaProgress = 0;
    size_t _lafvinOtaBytes = 0;
    String _c6Version = "--";
    uint32_t _c6OtaTimestamp = 0;
    uint32_t _restartScheduled = 0;

    bool handleStaticFile(String path = "") {
        if (path.isEmpty()) {
            path = _server.uri();
        }
        path = WebServer::urlDecode(path);
        int qIdx = path.indexOf('?');
        if (qIdx != -1) {
            path = path.substring(0, qIdx);
        }
        if (!path.startsWith("/")) path = "/" + path;

        if (!_sd.isReady() || !_sd.fileExists(path.c_str())) {
            return false;
        }

        _isBusy = true;
        _sd.prepareBus();
        File f = _sd.openFile(path.c_str(), FILE_READ);
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

        size_t fileSize = f.size();
        _server.setContentLength(fileSize);
        _server.send(200, contentType, "");

        WiFiClient client = _server.client();
        client.setTimeout(5000);
        uint8_t buf[2048];
        while (f.available() && client.connected()) {
            size_t n = f.read(buf, sizeof(buf));
            if (n > 0) {
                client.write(buf, n);
            }
        }
        f.close();
        _isBusy = false;
        return true;
    }

    bool tryServeSdFile(const String &path) {
        if (!_sd.isReady()) {
            return false;
        }
        // Tentar prioritariamente o caminho especializado do Rover: /www/rover/xxx
        if (path.startsWith("/www/") && !path.startsWith("/www/rover/") && !path.startsWith("/www/base/")) {
            String roverPath = "/www/rover/" + path.substring(5);
            if (_sd.fileExists(roverPath.c_str())) {
                return handleStaticFile(roverPath);
            }
        }
        if (!_sd.fileExists(path.c_str())) {
            return false;
        }
        return handleStaticFile(path);
    }

    void setupRoutes() {
        const char* headerKeys[] = {"Content-Length"};
        _server.collectHeaders(headerKeys, 1);

        // Rotas principais (Prioridade ao Cartão SD /www/rover/... com fallback para /www/... e Flash)
        _server.on("/", [this]() {
            if (tryServeSdFile("/www/index.html") || tryServeSdFile("/www/gps.html")) return;
            handleGpsPage();
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
            if (tryServeSdFile("/www/gps.html") || tryServeSdFile("/www/index.html")) return;
            handleGpsPage(); 
        });
        _server.on("/battery", [this]() { 
            if (tryServeSdFile("/www/battery.html")) return;
            handleBatteryPage(); 
        });
        _server.on("/control", [this]() { 
            if (tryServeSdFile("/www/control.html")) return;
            handleControlPage(); 
        });
        _server.on("/www/control.html", [this]() { handleControlPage(); });
        _server.on("/www/rover/control.html", [this]() { handleControlPage(); });
        _server.on("/sd", [this]() {
            if (tryServeSdFile("/www/sd.html")) return;
            handleSdPage();
        });
        _server.on("/favicon.ico", [this]() { _server.send(204, "image/x-icon", ""); });

        // APIs JSON & Operações
        _server.on("/api/status", [this]() { handleApiStatus(); });
        _server.on("/api/lora/status", [this]() {
            JsonDocument doc;
            JsonObject lObj = doc.to<JsonObject>();
            lObj["hw_ready"] = _loraDiag ? _loraDiag->hwReady : true;
            lObj["role"] = "rover";
            lObj["freq_mhz"] = LORA_FREQUENCY;
            lObj["bw_khz"] = LORA_BANDWIDTH;
            lObj["sf"] = LORA_SPREAD_FACTOR;
            lObj["cr"] = "4/5";
            lObj["power_dbm"] = LORA_OUTPUT_POWER;
            lObj["tx_telemetry"] = _loraDiag ? _loraDiag->txTelems : 0;
            lObj["rx_polls"] = _loraDiag ? _loraDiag->rxPolls : 0;
            lObj["last_comm_seconds_ago"] = _loraDiag ? _loraDiag->lastRxSecAgo : 999999;
            lObj["has_contact"] = _loraDiag ? _loraDiag->hasContact : false;
            lObj["rssi"] = _loraDiag ? _loraDiag->rssi : 0.0f;
            lObj["snr"] = _loraDiag ? _loraDiag->snr : 0.0f;
            String res;
            serializeJson(doc, res);
            _server.send(200, "application/json", res);
        });
        _server.on("/api/lora/send_telem", HTTP_POST, [this]() {
            if (_commandQueueCb) {
                _commandQueueCb("CMD:TX_TELEM");
                _server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Telemetria disparada via LoRa 433MHz\"}");
            } else {
                _server.send(500, "application/json", "{\"status\":\"error\",\"message\":\"Callback UART indisponivel\"}");
            }
        });
        _server.on("/api/scan", [this]() { handleApiScan(); });
        _server.on("/api/wifi/save", [this]() { handleApiWifiSave(); });
        _server.on("/api/wifi/setdefault", [this]() { handleApiWifiSetDefault(); });
        _server.on("/api/wifi/delete", [this]() { handleApiWifiDelete(); });
        _server.on("/api/wifi/connect", [this]() { handleApiWifiConnect(); });
        _server.on("/api/wifi/reconnect", [this]() { handleApiWifiReconnect(); });
        
        // Configuração geral do Rover (ID, SSID AP, Password AP, Brilho do Ecrã)
        _server.on("/api/rover/config", HTTP_GET, [this]() { handleApiRoverConfigGet(); });
        _server.on("/api/rover/config", HTTP_POST, [this]() { handleApiRoverConfigSave(); });
        _server.on("/api/rover/config/reload", HTTP_POST, [this]() { handleApiRoverConfigReload(); });
        _server.on("/api/display/brightness", HTTP_POST, [this]() { handleApiDisplayBrightness(); });

        // Configuração da Bateria (3S, 4S ou 6S e Capacidade mAh)
        _server.on("/api/battery/config", HTTP_GET, [this]() { handleApiBatteryConfigGet(); });
        _server.on("/api/battery/config", HTTP_POST, [this]() { handleApiBatteryConfigSet(); });
        _server.on("/api/battery/reset", HTTP_POST, [this]() { handleApiBatteryReset(); });

        // Configuração do GPS Quectel LC29H (Frequência Hz e Comandos NMEA/PAIR)
        _server.on("/api/gps/rate", HTTP_POST, [this]() { handleApiGpsRate(); });
        _server.on("/api/gps/command", HTTP_POST, [this]() { handleApiGpsCommand(); });

        // Controle de atuadores
        _server.on("/api/control/actuator", [this]() { handleApiActuator(); });
        _server.on("/api/control/action", [this]() { handleApiAction(); });
        _server.on("/api/control/winch", [this]() { handleApiControlWinch(); });
        _server.on("/api/control/anchor/config", [this]() { handleApiAnchorConfig(); });
        _server.on("/api/control/rack/config", [this]() { handleApiRackConfig(); });

        // Gestão do Cartão SD
        _server.on("/api/sd/list", [this]() { handleApiSdList(); });
        _server.on("/api/sd/read", [this]() { handleApiSdRead(); });
        _server.on("/api/sd/download", [this]() { handleApiSdDownload(); });
        _server.on("/api/sd/save", [this]() { handleApiSdSave(); });
        _server.on("/api/sd/delete", [this]() { handleApiSdDelete(); });
        _server.on("/api/sd/delete_dir", [this]() { handleApiSdDeleteDir(); });
        _server.on("/api/sd/mkdir", [this]() { handleApiSdMkdir(); });

        // Upload de ficheiros para o SD
        _server.on("/api/sd/upload", HTTP_POST, 
            [this]() { handleApiSdUploadFinish(); },
            [this]() { handleApiSdUploadData(); }
        );

        // Servir o binario para o C6 descarregar via HTTPUpdate
        _server.on("/firmware/c6.bin", HTTP_GET, [this]() {
            if (_sd.isReady() && _sd.fileExists("/firmware/c6.bin")) {
                File f = _sd.openFile("/firmware/c6.bin", FILE_READ);
                if (f) {
                    _server.sendHeader("Access-Control-Allow-Origin", "*");
                    _server.client().setTimeout(30000);
                    _server.streamFile(f, "application/octet-stream");
                    f.close();
                    return;
                }
            }
            _server.send(404, "text/plain", "Firmware c6.bin nao encontrado no cartao SD");
        });

        // Endpoints de atualizacao OTA do Rover (Dual-Core C6 + Lafvin)
        _server.on("/api/ota/c6", HTTP_POST, 
            [this]() { handleOtaC6UploadFinish(); },
            [this]() { handleOtaC6UploadData(); }
        );
        _server.on("/api/ota/c6/status", HTTP_GET, [this]() { handleApiOtaC6Status(); });
        _server.on("/api/ota/lafvin", HTTP_POST, 
            [this]() { handleApiOtaLafvinFinish(); },
            [this]() { handleApiOtaLafvinData(); }
        );

        // Endpoints de Consola e Monitor de Logs em Tempo Real
        _server.on("/api/console", HTTP_GET, [this]() {
            uint32_t since = 0;
            if (_server.hasArg("since")) {
                since = (uint32_t)_server.arg("since").toInt();
            }
            _server.send(200, "application/json", webConsole.getJsonSince(since));
        });
        _server.on("/api/console/send", HTTP_POST, [this]() {
            String cmd = "";
            if (_server.hasArg("cmd")) {
                cmd = _server.arg("cmd");
            } else if (_server.hasArg("plain")) {
                cmd = _server.arg("plain");
            }
            cmd.trim();
            if (cmd.length() > 0) {
                if (cmd.equalsIgnoreCase("CLEAR")) {
                    webConsole.clear();
                    _server.send(200, "application/json", "{\"status\":\"ok\",\"action\":\"cleared\"}");
                    return;
                }
                if (_commandQueueCb) {
                    _commandQueueCb(cmd.c_str());
                }
                webConsole.logf("[CONSOLE TX] %s", cmd.c_str());
                _server.send(200, "application/json", "{\"status\":\"ok\"}");
                return;
            }
            _server.send(400, "application/json", "{\"status\":\"error\",\"msg\":\"Missing cmd\"}");
        });
        _server.on("/api/console/clear", HTTP_POST, [this]() {
            webConsole.clear();
            _server.send(200, "application/json", "{\"status\":\"ok\"}");
        });

        // Servir fontes ou outros ficheiros estáticos diretamente do Cartão SD
        _server.on("/system/fonts/Font%20Awesome%206%20Pro-Solid-900.woff2", HTTP_GET, [this]() {
            if (!handleStaticFile("/system/fonts/Font Awesome 6 Pro-Solid-900.woff2")) {
                _server.send(404, "text/plain", "Fonte Font Awesome woff2 nao encontrada no cartao SD");
            }
        });
        _server.on("/system/fonts/Font Awesome 6 Pro-Solid-900.woff2", HTTP_GET, [this]() {
            if (!handleStaticFile("/system/fonts/Font Awesome 6 Pro-Solid-900.woff2")) {
                _server.send(404, "text/plain", "Fonte Font Awesome woff2 nao encontrada no cartao SD");
            }
        });
        _server.on("/system/fonts/Font%20Awesome%206%20Pro-Solid-900.otf", HTTP_GET, [this]() {
            if (!handleStaticFile("/system/fonts/Font Awesome 6 Pro-Solid-900.otf")) {
                _server.send(404, "text/plain", "Fonte Font Awesome otf nao encontrada no cartao SD");
            }
        });
        _server.on("/system/fonts/Font Awesome 6 Pro-Solid-900.otf", HTTP_GET, [this]() {
            if (!handleStaticFile("/system/fonts/Font Awesome 6 Pro-Solid-900.otf")) {
                _server.send(404, "text/plain", "Fonte Font Awesome otf nao encontrada no cartao SD");
            }
        });

        _server.onNotFound([this]() {
            if (_server.uri().startsWith("/") && !_server.uri().startsWith("/www/")) {
                if (handleStaticFile("/www/rover" + _server.uri())) {
                    return;
                }
                if (handleStaticFile("/www" + _server.uri())) {
                    return;
                }
            }
            if (handleStaticFile(_server.uri())) {
                return;
            }
            _server.send(404, "text/plain", "Ficheiro ou pagina nao encontrada");
        });
    }

    // -------------------------------------------------------------
    // CSS & HTML TEMPLATES
    // -------------------------------------------------------------
    String getHtmlHeader(const String &title, const String &activeTab) {
        String html = "<!DOCTYPE html><html lang='pt'><head><meta charset='UTF-8'>";
        html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
        html += "<title>" + title + " - Rover-" + String(_roverId) + "</title>";
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
        html += ".nav-item{display:inline-flex;align-items:center;gap:6px;padding:8px 14px;border-radius:8px;color:var(--muted);text-decoration:none;font-size:0.875rem;font-weight:600;white-space:nowrap;transition:0.2s;}";
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
        html += "input,select,textarea{width:100%;padding:10px 12px;background:#0f172a;border:1px solid var(--card-border);color:#fff;border-radius:8px;margin-top:6px;font-size:0.9rem;}";
        html += ".form-group{margin-bottom:14px;}";
        html += ".progress-bg{width:100%;height:16px;background:#0f172a;border-radius:8px;overflow:hidden;margin-top:8px;}";
        html += ".progress-bar{height:100%;transition:width 0.3s;}";
        html += ".slider{width:100%;-webkit-appearance:none;height:10px;background:#0f172a;border-radius:5px;outline:none;}";
        html += ".slider::-webkit-slider-thumb{-webkit-appearance:none;width:24px;height:24px;border-radius:50%;background:var(--primary);cursor:pointer;}";
        // Font Awesome 6 Pro Solid (carregada do Cartão SD)
        html += "@font-face{font-family:'Font Awesome 6 Pro';src:url('/system/fonts/Font%20Awesome%206%20Pro-Solid-900.woff2') format('woff2'),url('/system/fonts/Font%20Awesome%206%20Pro-Solid-900.otf') format('opentype');font-weight:900;font-style:normal;font-display:block;}";
        html += ".fa,.fas,.fa-solid{font-family:'Font Awesome 6 Pro'!important;font-weight:900;font-style:normal;font-variant:normal;line-height:1;text-rendering:auto;display:inline-block;vertical-align:-0.125em;-webkit-font-smoothing:antialiased;-moz-osx-font-smoothing:grayscale;}";
        html += ".btn i,.nav-item i,.brand i{pointer-events:none;}";
        html += ".fa-robot:before{content:'\\f544';}";
        html += ".fa-wifi:before{content:'\\f1eb';}";
        html += ".fa-gear:before,.fa-cog:before{content:'\\f013';}";
        html += ".fa-folder:before{content:'\\f07b';}";
        html += ".fa-folder-open:before{content:'\\f07c';}";
        html += ".fa-folder-plus:before{content:'\\f65e';}";
        html += ".fa-location-dot:before,.fa-map-marker-alt:before{content:'\\f3c5';}";
        html += ".fa-battery-full:before{content:'\\f240';}";
        html += ".fa-battery-three-quarters:before{content:'\\f241';}";
        html += ".fa-battery-half:before{content:'\\f242';}";
        html += ".fa-battery-quarter:before{content:'\\f243';}";
        html += ".fa-battery-empty:before{content:'\\f244';}";
        html += ".fa-gamepad:before{content:'\\f11b';}";
        html += ".fa-floppy-disk:before,.fa-save:before{content:'\\f0c7';}";
        html += ".fa-sun:before{content:'\\f185';}";
        html += ".fa-moon:before{content:'\\f186';}";
        html += ".fa-cloud-sun:before{content:'\\f6c4';}";
        html += ".fa-bolt:before{content:'\\f0e7';}";
        html += ".fa-tower-broadcast:before,.fa-broadcast-tower:before{content:'\\f519';}";
        html += ".fa-eye:before{content:'\\f06e';}";
        html += ".fa-eye-slash:before{content:'\\f070';}";
        html += ".fa-rotate:before,.fa-sync:before{content:'\\f021';}";
        html += ".fa-arrow-right:before{content:'\\f061';}";
        html += ".fa-arrow-left:before{content:'\\f060';}";
        html += ".fa-arrow-up:before{content:'\\f062';}";
        html += ".fa-arrow-turn-up:before,.fa-level-up-alt:before{content:'\\f3bf';}";
        html += ".fa-circle-check:before{content:'\\f058';}";
        html += ".fa-circle-xmark:before{content:'\\f057';}";
        html += ".fa-circle:before{content:'\\f111';}";
        html += ".fa-star:before{content:'\\f005';}";
        html += ".fa-plug:before{content:'\\f1e6';}";
        html += ".fa-network-wired:before{content:'\\f6ff';}";
        html += ".fa-map:before{content:'\\f279';}";
        html += ".fa-compass:before{content:'\\f14e';}";
        html += ".fa-gauge-high:before,.fa-tachometer-alt:before{content:'\\f625';}";
        html += ".fa-anchor:before{content:'\\f13d';}";
        html += ".fa-stop:before{content:'\\f04d';}";
        html += ".fa-hand:before{content:'\\f256';}";
        html += ".fa-lock:before{content:'\\f023';}";
        html += ".fa-lock-open:before{content:'\\f3c1';}";
        html += ".fa-house:before,.fa-home:before{content:'\\f015';}";
        html += ".fa-bell:before{content:'\\f0f3';}";
        html += ".fa-triangle-exclamation:before{content:'\\f071';}";
        html += ".fa-trash-can:before,.fa-trash:before{content:'\\f2ed';}";
        html += ".fa-file:before{content:'\\f15b';}";
        html += ".fa-file-lines:before{content:'\\f15c';}";
        html += ".fa-file-pen:before{content:'\\f31c';}";
        html += ".fa-download:before{content:'\\f019';}";
        html += ".fa-upload:before{content:'\\f093';}";
        html += ".fa-cloud-arrow-up:before{content:'\\f0ee';}";
        html += ".fa-magnifying-glass:before,.fa-search:before{content:'\\f002';}";
        html += ".fa-microchip:before{content:'\\f2db';}";
        html += ".fa-sd-card:before{content:'\\f7c2';}";
        html += ".fa-signal:before{content:'\\f012';}";
        html += ".fa-satellite:before{content:'\\f7bf';}";
        html += ".fa-sliders:before{content:'\\f1de';}";
        html += ".fa-xmark:before{content:'\\f00d';}";
        html += ".fa-plus:before{content:'\\2b';}";
        html += ".fa-font:before{content:'\\f031';}";
        html += ".fa-file-code:before{content:'\\f1c9';}";
        html += ".fa-file-image:before{content:'\\f1c5';}";
        html += ".fa-file-zipper:before{content:'\\f1c6';}";
        html += ".fa-table-cells:before,.fa-th:before{content:'\\f00a';}";
        html += ".fa-list:before{content:'\\f03a';}";
        html += ".fa-copy:before{content:'\\f0c5';}";
        html += ".fa-filter:before{content:'\\f0b0';}";
        html += ".fa-qrcode:before{content:'\\f029';}";
        html += ".fa-camera:before{content:'\\f030';}";
        html += "</style></head><body>";

        // Navbar
        html += "<header class='navbar'>";
        html += "<div class='brand'><i class='fa-solid fa-robot'></i> WindDragons <span>Rover-" + String(_roverId) + "</span></div>";
        html += "<div class='nav-links'>";
        if (_wifi.isAPMode()) {
            html += "<a href='/wifi' class='nav-item " + String(activeTab == "wifi" ? "active" : "") + "'><i class='fa-solid fa-wifi'></i> Configuração WiFi</a>";
            html += "<a href='/config' class='nav-item " + String(activeTab == "config" ? "active" : "") + "'><i class='fa-solid fa-gear'></i> Configurações</a>";
            html += "<a href='/sd' class='nav-item " + String(activeTab == "sd" ? "active" : "") + "'><i class='fa-solid fa-folder'></i> Cartão SD</a>";
        } else {
            html += "<a href='/wifi' class='nav-item " + String(activeTab == "wifi" ? "active" : "") + "'><i class='fa-solid fa-wifi'></i> WiFi</a>";
            html += "<a href='/gps' class='nav-item " + String(activeTab == "gps" ? "active" : "") + "'><i class='fa-solid fa-location-dot'></i> GPS</a>";
            html += "<a href='/battery' class='nav-item " + String(activeTab == "battery" ? "active" : "") + "'><i class='fa-solid fa-battery-three-quarters'></i> Bateria</a>";
            html += "<a href='/control' class='nav-item " + String(activeTab == "control" ? "active" : "") + "'><i class='fa-solid fa-gamepad'></i> Comandos</a>";
            html += "<a href='/config' class='nav-item " + String(activeTab == "config" ? "active" : "") + "'><i class='fa-solid fa-gear'></i> Configurações</a>";
            html += "<a href='/sd' class='nav-item " + String(activeTab == "sd" ? "active" : "") + "'><i class='fa-solid fa-folder'></i> Cartão SD</a>";
        }
        html += "</div>";
        html += "<div style='display:flex;align-items:center;gap:8px;'>";
        if (_wifi.isAPMode()) {
            html += "<span class='badge badge-ap'><i class='fa-solid fa-tower-broadcast'></i> MODO AP: " + _wifi.getSSID() + "</span>";
        } else {
            html += "<span class='badge badge-sta'><i class='fa-solid fa-wifi'></i> WIFI: " + _wifi.getIPAddress() + "</span>";
        }
        html += "</div></header><main class='container'>";
        return html;
    }

    String getHtmlFooter() {
        return "</main><footer style='text-align:center;color:var(--muted);font-size:0.75rem;margin-top:20px;'>WindDragons Onboard Rover Telemetry &copy; 2026</footer></body></html>";
    }

    // -------------------------------------------------------------
    // PÁGINA 1: CONFIGURAÇÃO DE WIFI
    // -------------------------------------------------------------
    void handleWifiPage() {
        String html = getHtmlHeader("Configuração WiFi", "wifi");
        String mac = getDeviceMacAddress();
        String qrSvg = generateQrCodeSvg(mac.c_str(), 140);

        // Card 1: Estado Atual & Identificação com QR Code
        html += "<div class='card'>";
        html += "<div class='card-title'><span><i class='fa-solid fa-network-wired'></i> Estado da Ligação & Identificação</span><span class='badge' style='background:#065f46;color:#6ee7b7;'><i class='fa-solid fa-qrcode'></i> QR Code MAC</span></div>";
        html += "<div style='display:flex;align-items:center;gap:20px;flex-wrap:wrap;margin-bottom:14px;'>";
        html += "<div style='text-align:center;'>";
        html += qrSvg;
        html += "<div style='font-size:0.75rem;color:var(--muted);margin-top:6px;'><i class='fa-solid fa-camera'></i> Digitalizar MAC</div>";
        html += "</div>";
        html += "<div style='flex:1;min-width:240px;'>";
        html += "<div class='grid-2'>";
        html += "<div class='stat-box'><div class='stat-label'>Endereço MAC (Hardware)</div><div class='stat-value' style='color:var(--primary);font-family:monospace;font-size:1.15rem;word-break:break-all;'>" + mac + "</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Dispositivo / ID</div><div class='stat-value' style='color:var(--yellow);'>ROVER-" + String(_roverId < 10 ? "0" : "") + String(_roverId) + "</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Modo de Operação</div><div class='stat-value' style='color:" + String(_wifi.isAPMode() ? "var(--yellow)" : "var(--green)") + "'>";
        html += _wifi.isAPMode() ? "Ponto de Acesso (AP)" : "Conectado à Rede (STA)";
        html += "</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Endereço IP</div><div class='stat-value' style='color:var(--primary);'>" + _wifi.getIPAddress() + "</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>SSID Ativo</div><div class='stat-value' style='word-break:break-all;'>" + _wifi.getSSID() + "</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>" + String(_wifi.isAPMode() ? "Dispositivos Conectados" : "Sinal WiFi (RSSI)") + "</div>";
        html += "<div class='stat-value'>" + String(_wifi.isAPMode() ? String(_wifi.getAPStationCount()) : String(_wifi.getRSSI()) + " dBm") + "</div></div>";
        html += "</div></div></div>";

        if (_wifi.isAPMode()) {
            html += "<div style='margin-top:16px;padding:12px;background:#451a03;border-left:4px solid var(--yellow);border-radius:6px;font-size:0.875rem;'>";
            html += "<strong>Modo Ponto de Acesso:</strong> Conectado ao AP local da boia. As redes guardadas são gravadas simultaneamente na NVS e no Cartão SD (<code>/wifi_networks.json</code>).";
            html += "</div>";
        }
        html += "</div>";

        // Card 2: Lista de Redes Guardadas
        html += "<div class='card'>";
        html += "<div class='card-title'><span><i class='fa-solid fa-wifi'></i> Redes WiFi Guardadas</span> <span style='font-size:0.8rem;color:var(--muted);'>NVS + Cartão SD (/wifi_networks.json)</span></div>";
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
                    html += "<span class='badge' style='background:#059669;color:#fff;margin-right:4px;'><i class='fa-solid fa-circle'></i> LIGADA</span> ";
                }
                if (n.is_default) {
                    html += "<span class='badge' style='background:#065f46;color:#6ee7b7;'><i class='fa-solid fa-star'></i> PADRÃO</span>";
                } else if (!isCurrent) {
                    html += "<span style='color:var(--muted);font-size:0.8rem;'>Secundária</span>";
                }
                html += "</td>";
                html += "<td><div class='btn-group'>";
                if (isCurrent) {
                    html += "<button disabled class='btn' style='padding:6px 10px;background:#334155;color:#94a3b8;cursor:default;'><i class='fa-solid fa-circle-check'></i> Ativa</button>";
                } else {
                    html += "<button onclick='connectNetwork(\"" + n.ssid + "\")' class='btn btn-primary' style='padding:6px 10px;background:#2563eb;'><i class='fa-solid fa-plug'></i> Ligar</button>";
                }
                if (!n.is_default) {
                    html += "<button onclick='setDefault(\"" + n.ssid + "\")' class='btn btn-secondary' style='padding:6px 10px;'><i class='fa-solid fa-star'></i> Definir Padrão</button>";
                }
                html += "<button onclick='deleteNetwork(\"" + n.ssid + "\")' class='btn btn-danger' style='padding:6px 10px;'><i class='fa-solid fa-trash-can'></i> Eliminar</button>";
                html += "</div></td></tr>";
            }
            html += "</tbody></table>";
        }
        html += "</div>";

        // Card 3: Adicionar Nova Ligação
        html += "<div class='card'>";
        html += "<div class='card-title'><span><i class='fa-solid fa-plus'></i> Adicionar Nova Ligação WiFi</span></div>";
        html += "<form id='wifiForm' onsubmit='saveWifi(event)'>";
        html += "<div class='form-group'><label class='stat-label'>Nome da Rede (SSID):</label><input type='text' id='ssid' name='ssid' required placeholder='ex: WiFi_Porto'></div>";
        html += "<div class='form-group'><label class='stat-label'>Palavra-passe (Password):</label><input type='password' id='pass' name='pass' placeholder='Password da rede'></div>";
        html += "<div class='form-group' style='display:flex;align-items:center;gap:10px;margin-top:10px;'>";
        html += "<input type='checkbox' id='is_default' name='is_default' style='width:auto;margin:0;'>";
        html += "<label for='is_default' style='cursor:pointer;font-size:0.875rem;'>Marcar esta rede como <strong>Padrão (Default)</strong> no arranque</label>";
        html += "</div>";
        html += "<div class='btn-group' style='margin-top:14px;'>";
        html += "<button type='submit' class='btn btn-primary'><i class='fa-solid fa-floppy-disk'></i> Guardar Ligação (NVS + SD)</button>";
        html += "<button type='button' onclick='scanNetworks()' class='btn btn-secondary'><i class='fa-solid fa-magnifying-glass'></i> Procurar Redes Próximas</button>";
        if (_wifi.isAPMode()) {
            html += "<button type='button' onclick='reconnectSTA()' class='btn' style='background:var(--green);color:#fff;'><i class='fa-solid fa-rotate'></i> Tentar Ligar e Sair do AP</button>";
        }
        html += "</div></form>";
        html += "<div id='scanResults' style='margin-top:16px;'></div>";
        html += "</div>";

        // Card 4: Configuração Geral do Rover e AP (/config.json)
        html += "<div class='card'>";
        html += "<div class='card-title'><span><i class='fa-solid fa-robot'></i> Identificação do Rover & Ponto de Acesso (AP)</span> <span style='font-size:0.8rem;color:var(--muted);'>NVS + Cartão SD (/config.json)</span></div>";
        html += "<form id='cfgForm' onsubmit='saveRoverCfg(event)'>";
        html += "<div class='form-group'><label class='stat-label'>ID do Rover (1 - 254):</label><input type='number' id='cfg_id' min='1' max='254' required value='" + String(_config.getRoverId()) + "'></div>";
        html += "<div class='form-group'><label class='stat-label'>Nome da Rede AP (SSID):</label><input type='text' id='cfg_ssid' required value='" + _config.getApSsid() + "'></div>";
        html += "<div class='form-group'><label class='stat-label'>Palavra-passe do AP (mín. 8 caracteres):</label><input type='password' id='cfg_pass' minlength='8' required value='" + _config.getApPassword() + "'></div>";
        html += "<div class='btn-group' style='margin-top:14px;'>";
        html += "<button type='submit' class='btn btn-primary'><i class='fa-solid fa-floppy-disk'></i> Guardar Identificação (NVS + SD)</button>";
        html += "<a href='/config' class='btn btn-secondary'><i class='fa-solid fa-gear'></i> Abrir Página de Configurações Completa (Brilho do Ecrã) <i class='fa-solid fa-arrow-right'></i></a>";
        html += "</div></form></div>";

        // Script interativo
        html += "<script>";
        html += "function saveRoverCfg(e){e.preventDefault();const id=parseInt(document.getElementById('cfg_id').value);const s=document.getElementById('cfg_ssid').value;const p=document.getElementById('cfg_pass').value;";
        html += "fetch('/api/rover/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({rover_id:id,ap_ssid:s,ap_password:p})})";
        html += ".then(r=>r.json()).then(res=>{alert(res.msg);location.reload();});}";
        html += "function saveWifi(e){e.preventDefault();const s=document.getElementById('ssid').value;const p=document.getElementById('pass').value;const d=document.getElementById('is_default').checked;";
        html += "fetch('/api/wifi/save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:s,password:p,is_default:d})})";
        html += ".then(r=>r.json()).then(res=>{alert(res.msg);location.reload();});}";
        html += "function connectNetwork(s){if(confirm('Ligar à rede WiFi \"'+s+'\" agora?')){fetch('/api/wifi/connect',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:s})}).then(r=>r.json()).then(res=>{alert(res.msg||('A ligar à rede '+s+'...'));setTimeout(()=>location.reload(),6000);}).catch(()=>{alert('Comando enviado. A recarregar em 6 segundos...');setTimeout(()=>location.reload(),6000);});}}";
        html += "function setDefault(s){fetch('/api/wifi/setdefault',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:s})}).then(()=>location.reload());}";
        html += "function deleteNetwork(s){if(confirm('Eliminar rede '+s+'?')){fetch('/api/wifi/delete',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:s})}).then(()=>location.reload());}}";
        html += "function scanNetworks(){const d=document.getElementById('scanResults');d.innerHTML='<p style=\"color:var(--muted);\">A pesquisar redes WiFi próximas...</p>';";
        html += "fetch('/api/scan').then(r=>r.json()).then(nets=>{if(!nets.length){d.innerHTML='<p>Nenhuma rede encontrada.</p>';return;}";
        html += "let t='<table><thead><tr><th>SSID</th><th>Sinal (RSSI)</th><th>Ação</th></tr></thead><tbody>';";
        html += "nets.forEach(n=>{t+='<tr><td><strong>'+n.ssid+'</strong></td><td>'+n.rssi+' dBm</td><td><button class=\"btn btn-secondary\" style=\"padding:4px 8px;\" onclick=\"document.getElementById(\\'ssid\\').value=\\''+n.ssid+'\\'\">Selecionar</button></td></tr>';});";
        html += "t+='</tbody></table>';d.innerHTML=t;});}";
        html += "function reconnectSTA(){if(confirm('Tentar conectar às redes guardadas agora?')){fetch('/api/wifi/reconnect',{method:'POST'}).then(r=>r.json()).then(res=>{alert(res.msg);setTimeout(()=>location.reload(),4000);});}}";
        html += "</script>";

        html += getHtmlFooter();
        _server.send(200, "text/html", html);
    }

    // -------------------------------------------------------------
    // PÁGINA: CONFIGURAÇÕES GERAIS (/config.json & NVS)
    // -------------------------------------------------------------
    void handleConfigPage() {
        String html = getHtmlHeader("Configurações", "config");

        // Card 1: Estado do Armazenamento de Configurações
        bool hasSd = _sd.isReady();
        bool hasConfigFile = hasSd && _sd.fileExists("/config.json");
        
        html += "<div class='card'>";
        html += "<div class='card-title'><span><i class='fa-solid fa-floppy-disk'></i> Ficheiro de Configurações do Sistema</span><span class='badge' style='background:" + String(hasConfigFile ? "#064e3b;color:#a7f3d0;" : "#78350f;color:#fde68a;") + "'>" + String(hasConfigFile ? "<i class='fa-solid fa-sd-card'></i> MicroSD: /config.json" : "<i class='fa-solid fa-microchip'></i> Apenas Flash NVS") + "</span></div>";
        html += "<p style='color:var(--muted);font-size:0.875rem;margin-bottom:14px;'>As opções abaixo são gravadas sincronizadamente no ficheiro <code>/config.json</code> do Cartão MicroSD e na partição não-volátil NVS do ESP32.</p>";
        
        html += "<div class='grid-4'>";
        html += "<div class='stat-box'><div class='stat-label'>ID Ativo</div><div class='stat-value' style='color:var(--primary);'>Rover-" + String(_config.getRoverId()) + "</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Brilho do Ecrã</div><div class='stat-value' id='summaryBright' style='color:var(--yellow);'>" + String(_config.getScreenBrightness()) + "%</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>SSID AP Local</div><div class='stat-value' style='font-size:0.95rem;word-break:break-all;'>" + _config.getApSsid() + "</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Estado do MicroSD</div><div class='stat-value' style='font-size:0.95rem;color:" + String(hasSd ? "var(--green)" : "var(--yellow)") + "'>" + (hasSd ? "Cartão Pronto" : "Sem Cartão") + "</div></div>";
        html += "</div></div>";

        // Card: Identificação do Dispositivo & Endereço MAC (com QR Code)
        String cfgMac = getDeviceMacAddress();
        String cfgQrSvg = generateQrCodeSvg(cfgMac.c_str(), 140);

        html += "<div class='card' style='border-left:4px solid var(--primary);'>";
        html += "<div class='card-title'><span><i class='fa-solid fa-qrcode'></i> Endereço MAC & QR Code</span></div>";
        html += "<div style='display:flex;align-items:center;gap:24px;flex-wrap:wrap;'>";
        html += "<div style='text-align:center;'>";
        html += cfgQrSvg;
        html += "</div>";
        html += "<div style='flex:1;min-width:240px;'>";
        html += "<div style='color:var(--primary);font-family:monospace;letter-spacing:2px;font-size:2.6rem;font-weight:900;word-break:break-all;line-height:1.2;'>" + cfgMac + "</div>";
        html += "</div></div></div>";

        // Formulário Principal
        html += "<form id='configForm' onsubmit='saveAllConfig(event)'>";

        // Card 2: Brilho do Ecrã
        html += "<div class='card'>";
        html += "<div class='card-title'><span><i class='fa-solid fa-sun'></i> Brilho do Ecrã (Display LCD ST7789)</span><span id='brightValBadge' class='badge' style='background:#1e293b;color:var(--primary);font-size:0.9rem;font-weight:700;'>" + String(_config.getScreenBrightness()) + "%</span></div>";
        html += "<p style='color:var(--muted);font-size:0.875rem;margin-bottom:16px;'>Ajuste o brilho da retroiluminação por PWM (LEDC GPIO 22). Ao mover o cursor, o ecrã atualiza em tempo real.</p>";
        
        html += "<div style='background:rgba(0,0,0,0.25);padding:18px;border-radius:12px;border:1px solid var(--card-border);'>";
        html += "<div style='display:flex;align-items:center;justify-content:space-between;margin-bottom:10px;'>";
        html += "<span style='font-size:0.85rem;color:var(--muted);'><i class='fa-solid fa-moon'></i> 5% Mínimo</span>";
        html += "<span id='brightDisplayVal' style='font-size:1.8rem;font-weight:700;color:var(--primary);'>" + String(_config.getScreenBrightness()) + "%</span>";
        html += "<span style='font-size:0.85rem;color:var(--muted);'><i class='fa-solid fa-sun'></i> 100% Máximo</span>";
        html += "</div>";
        html += "<input type='range' id='cfg_bright' min='5' max='100' step='1' value='" + String(_config.getScreenBrightness()) + "' class='slider' oninput='onBrightSlide(this.value)' onchange='onBrightChange(this.value)'>";
        
        html += "<div style='display:flex;gap:8px;margin-top:16px;flex-wrap:wrap;'>";
        html += "<button type='button' onclick='setPreset(25)' class='btn btn-secondary' style='padding:6px 12px;font-size:0.8rem;'><i class='fa-solid fa-moon'></i> 25% (Poupança)</button>";
        html += "<button type='button' onclick='setPreset(50)' class='btn btn-secondary' style='padding:6px 12px;font-size:0.8rem;'><i class='fa-solid fa-cloud-sun'></i> 50% (Normal)</button>";
        html += "<button type='button' onclick='setPreset(75)' class='btn btn-secondary' style='padding:6px 12px;font-size:0.8rem;'><i class='fa-solid fa-sun'></i> 75% (Dia)</button>";
        html += "<button type='button' onclick='setPreset(100)' class='btn btn-secondary' style='padding:6px 12px;font-size:0.8rem;'><i class='fa-solid fa-bolt'></i> 100% (Sol Máximo)</button>";
        html += "</div>";
        html += "</div></div>";

        // Card 3: Identificação do Rover & Rádio
        html += "<div class='card'>";
        html += "<div class='card-title'><span><i class='fa-solid fa-robot'></i> Identificação do Rover & Rádio LoRa</span></div>";
        html += "<div class='grid-2'>";
        html += "<div class='form-group'><label class='stat-label'>ID do Rover (1 - 254):</label>";
        html += "<input type='number' id='cfg_id' min='1' max='254' required value='" + String(_config.getRoverId()) + "' oninput='updateIdPreview(this.value)'>";
        html += "<small style='color:var(--muted);font-size:0.75rem;margin-top:4px;display:block;'>Identificador único em mensagens LoRa SX1278 e telemetria: <strong id='roverTag' style='color:var(--primary);'>ROVER-" + String(_config.getRoverId() < 10 ? "0" : "") + String(_config.getRoverId()) + "</strong></small>";
        html += "</div>";
        html += "<div class='form-group'><label class='stat-label'>Tipo de Equipamento:</label>";
        html += "<input type='text' disabled value='WindDragons Autonomous Surface Rover / Buoy' style='background:#0b1120;color:var(--muted);cursor:not-allowed;'>";
        html += "<small style='color:var(--muted);font-size:0.75rem;margin-top:4px;display:block;'>Hardware: ESP32-C6 + ST7789 1.47\" + LoRa SX1278 + MicroSD</small>";
        html += "</div>";
        html += "</div></div>";

        // Card 4: Ponto de Acesso Local (WiFi AP)
        html += "<div class='card'>";
        html += "<div class='card-title'><span><i class='fa-solid fa-tower-broadcast'></i> Ponto de Acesso Local (WiFi AP)</span></div>";
        html += "<div class='grid-2'>";
        html += "<div class='form-group'><label class='stat-label'>Nome da Rede AP (SSID):</label>";
        html += "<input type='text' id='cfg_ssid' required value='" + _config.getApSsid() + "'>";
        html += "<small style='color:var(--muted);font-size:0.75rem;margin-top:4px;display:block;'>Nome do WiFi emitido quando não conectado a um router</small>";
        html += "</div>";
        html += "<div class='form-group'><label class='stat-label'>Palavra-passe do AP (mín. 8 caracteres):</label>";
        html += "<div style='display:flex;gap:6px;'>";
        html += "<input type='password' id='cfg_pass' minlength='8' required value='" + _config.getApPassword() + "' style='margin-top:0;'>";
        html += "<button type='button' onclick='togglePass()' class='btn btn-secondary' style='padding:0 12px;font-size:0.85rem;'><i id='passEye' class='fa-solid fa-eye'></i></button>";
        html += "</div>";
        html += "<small style='color:var(--muted);font-size:0.75rem;margin-top:4px;display:block;'>Segurança WPA2-PSK para acesso à telemetria local</small>";
        html += "</div>";
        html += "</div></div>";

        // Card 4.1: Calibração do Travão da Cremalheira (Sweet Spot de Encosto, Ponto Livre e Direção)
        html += "<div class='card' style='border:1px solid #10b981;'>";
        html += "<div class='card-title' style='display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:8px;'>";
        html += "<span><i class='fa-solid fa-sliders'></i> Calibração do Servo \"Travão da Cremalheira\" (GPIO 21)</span>";
        html += "<span id='rackDirBadge' class='badge' style='background:" + String(_config.isRackInverted() ? "#065f46;color:#6ee7b7;" : "#1e293b;color:var(--muted);") + "'>" + String(_config.isRackInverted() ? "DIREÇÃO: INVERTIDA" : "DIREÇÃO: NORMAL") + "</span>";
        html += "</div>";
        html += "<p style='color:var(--muted);font-size:0.85rem;margin-bottom:12px;'>Ajuste a direção de atuação do servo, o <b>sweet spot de encosto da cremalheira</b> (travada) e o <b>ponto onde fica livre</b> (solta).</p>";
        html += "<div style='display:flex;flex-wrap:wrap;gap:16px;align-items:flex-end;margin-bottom:14px;'>";
        html += "<div><label style='display:block;font-size:0.8rem;color:var(--muted);margin-bottom:4px;'>Sweet Spot Encosto / Travada (°):</label>";
        html += "<div style='display:flex;gap:6px;'><input type='number' id='cfg_rack_engaged' min='0' max='180' value='" + String(_config.getRackAngleEngaged()) + "' style='width:95px;margin:0;'>";
        html += "<button type='button' onclick='testRackPoint(\"engaged\")' class='btn btn-secondary' style='padding:8px 12px;'>Testar</button></div></div>";
        html += "<div><label style='display:block;font-size:0.8rem;color:var(--muted);margin-bottom:4px;'>Ponto Livre / Solta (°):</label>";
        html += "<div style='display:flex;gap:6px;'><input type='number' id='cfg_rack_released' min='0' max='180' value='" + String(_config.getRackAngleReleased()) + "' style='width:95px;margin:0;'>";
        html += "<button type='button' onclick='testRackPoint(\"released\")' class='btn btn-secondary' style='padding:8px 12px;'>Testar</button></div></div>";
        html += "<div style='display:flex;align-items:center;gap:8px;padding:8px 12px;background:rgba(15,23,42,0.6);border:1px solid #334155;border-radius:8px;'>";
        html += "<input type='checkbox' id='cfg_rack_inverted' " + String(_config.isRackInverted() ? "checked" : "") + " onchange='saveRackConfig()' style='width:18px;height:18px;margin:0;cursor:pointer;'>";
        html += "<label for='cfg_rack_inverted' style='cursor:pointer;font-size:0.88rem;font-weight:600;'>Inverter Direção do Servo</label></div>";
        html += "<button type='button' onclick='saveRackConfig()' class='btn btn-primary'><i class='fa-solid fa-floppy-disk'></i> Guardar Calibração Cremalheira</button>";
        html += "</div>";
        html += "<div style='background:rgba(15,23,42,0.6);border:1px solid #334155;border-radius:8px;padding:12px;'>";
        html += "<div style='display:flex;justify-content:space-between;align-items:center;margin-bottom:6px;'>";
        html += "<span class='stat-label'>Teste / Pesquisa de Sweet Spot Ao Vivo (0° a 180°):</span>";
        html += "<span id='rackLiveAngleVal' style='font-weight:700;color:var(--primary);'>" + String(_actuators.getCurrentRackAngle()) + "° (" + String(_actuators.getCurrentRackPulseUs()) + " µs)</span></div>";
        html += "<input type='range' id='rackTestSlider' min='0' max='180' step='1' value='" + String(_actuators.getCurrentRackAngle()) + "' class='slider' oninput='onRackTestSlide(this.value)'>";
        html += "<div class='btn-group' style='margin-top:10px;'>";
        html += "<button type='button' onclick='stepRackTest(-5)' class='btn btn-secondary'>-5°</button>";
        html += "<button type='button' onclick='stepRackTest(-1)' class='btn btn-secondary'>-1°</button>";
        html += "<button type='button' onclick='stepRackTest(1)' class='btn btn-secondary'>+1°</button>";
        html += "<button type='button' onclick='stepRackTest(5)' class='btn btn-secondary'>+5°</button>";
        html += "<button type='button' onclick='useCurrentAsRack(\"engaged\")' class='btn btn-secondary' style='background:#065f46;color:#fff;'>Usar Atual como Encosto</button>";
        html += "<button type='button' onclick='useCurrentAsRack(\"released\")' class='btn btn-secondary' style='background:#92400e;color:#fff;'>Usar Atual como Livre</button>";
        html += "<button type='button' onclick='relaxRackServo()' class='btn btn-secondary' style='background:#475569;color:#fff;'>Relaxar Servo</button>";
        html += "</div></div></div>";

        // Card 4.2: Calibração do Tambor do Guincho (Sensor Hall DRV5032 - GPIO 20)
        html += "<div class='card' style='border:1px solid #0284c7;'>";
        html += "<div class='card-title'><span><i class='fa-solid fa-compass-drafting'></i> Calibração do Tambor do Guincho (Sensor Hall DRV5032 - GPIO 20)</span></div>";
        html += "<div style='display:flex;flex-wrap:wrap;gap:16px;align-items:flex-end;'>";
        html += "<div><label style='display:block;font-size:0.8rem;color:var(--muted);margin-bottom:4px;'>Diâmetro do Tambor (mm):</label>";
        html += "<input type='number' id='cfg_drum_diam' step='0.5' min='5' max='500' value='" + String(_config.getAnchorDrumDiamMm(), 1) + "' style='width:130px;margin:0;'></div>";
        html += "<div><label style='display:block;font-size:0.8rem;color:var(--muted);margin-bottom:4px;'>Pulsos por Volta (Ímanes):</label>";
        html += "<input type='number' id='cfg_pulses_per_rev' min='1' max='24' value='" + String(_config.getAnchorPulsesPerRev()) + "' style='width:110px;margin:0;'></div>";
        html += "<button type='button' onclick='saveAnchorConfig()' class='btn btn-primary'><i class='fa-solid fa-floppy-disk'></i> Guardar Calibração do Tambor</button>";
        html += "</div></div>";

        // Card 5: Ações de Gravação & Gestão
        html += "<div class='card' style='border-color:var(--primary);'>";
        html += "<div class='card-title'><span><i class='fa-solid fa-floppy-disk'></i> Gravar e Sincronizar</span></div>";
        html += "<div id='statusMsg' style='display:none;padding:12px;border-radius:8px;margin-bottom:14px;font-size:0.9rem;'></div>";
        html += "<div class='btn-group'>";
        html += "<button type='submit' class='btn btn-primary' style='padding:12px 24px;font-size:1rem;'><i class='fa-solid fa-floppy-disk'></i> Guardar no Ficheiro (/config.json + NVS)</button>";
        html += "<button type='button' onclick='reloadConfig()' class='btn btn-secondary'><i class='fa-solid fa-rotate'></i> Recarregar do Ficheiro</button>";
        if (hasSd) {
            html += "<a href='/sd' class='btn btn-secondary'><i class='fa-solid fa-folder-open'></i> Explorar Ficheiros MicroSD</a>";
        }
        html += "</div>";
        html += "</div>";
        html += "</form>";

        // Script interativo
        html += "<script>";
        html += "let slideTimer=null,rackTimer=null;";
        html += "function onBrightSlide(v){";
        html += "  document.getElementById('brightDisplayVal').innerText=v+'%';";
        html += "  document.getElementById('brightValBadge').innerText=v+'%';";
        html += "  document.getElementById('summaryBright').innerText=v+'%';";
        html += "  if(slideTimer) clearTimeout(slideTimer);";
        html += "  slideTimer=setTimeout(()=>{";
        html += "    fetch('/api/display/brightness?val='+v,{method:'POST'}).catch(()=>{});";
        html += "  },60);";
        html += "}";
        html += "function onBrightChange(v){";
        html += "  fetch('/api/display/brightness?val='+v,{method:'POST'}).catch(()=>{});";
        html += "}";
        html += "function setPreset(v){";
        html += "  document.getElementById('cfg_bright').value=v;";
        html += "  onBrightSlide(v);";
        html += "  onBrightChange(v);";
        html += "}";
        html += "function updateIdPreview(id){";
        html += "  const num=parseInt(id)||1;";
        html += "  const tag='ROVER-'+(num<10?'0':'')+num;";
        html += "  const el=document.getElementById('roverTag');if(el)el.innerText=tag;";
        html += "}";
        html += "function togglePass(){";
        html += "  const p=document.getElementById('cfg_pass');const eye=document.getElementById('passEye');const isPass=(p.type==='password');p.type=isPass?'text':'password';if(eye)eye.className=isPass?'fa-solid fa-eye-slash':'fa-solid fa-eye';";
        html += "}";
        html += "function showStatus(msg,isErr=false){";
        html += "  const d=document.getElementById('statusMsg');";
        html += "  d.style.display='block';";
        html += "  d.style.background=isErr?'#450a0a':'#064e3b';";
        html += "  d.style.color=isErr?'#fecaca':'#a7f3d0';";
        html += "  d.style.border='1px solid '+(isErr?'#ef4444':'#10b981');";
        html += "  d.innerHTML=(isErr?'<i class=\"fa-solid fa-circle-xmark\"></i> ':'<i class=\"fa-solid fa-circle-check\"></i> ')+msg;";
        html += "  setTimeout(()=>{d.style.display='none';},6000);";
        html += "}";
        html += "function onRackTestSlide(v){const el=document.getElementById('rackLiveAngleVal');if(el)el.innerText=v+'°';if(rackTimer)clearTimeout(rackTimer);rackTimer=setTimeout(()=>{fetch('/api/control/rack/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({test_angle:parseInt(v)})});},40);}";
        html += "function stepRackTest(d){const s=document.getElementById('rackTestSlider');if(!s)return;let v=Math.max(0,Math.min(180,(parseInt(s.value)||0)+d));s.value=v;onRackTestSlide(v);}";
        html += "function testRackPoint(which){const id=which==='engaged'?'cfg_rack_engaged':'cfg_rack_released';const v=parseInt(document.getElementById(id).value)||0;const s=document.getElementById('rackTestSlider');if(s)s.value=v;onRackTestSlide(v);}";
        html += "function useCurrentAsRack(which){const s=document.getElementById('rackTestSlider');if(!s)return;const id=which==='engaged'?'cfg_rack_engaged':'cfg_rack_released';document.getElementById(id).value=s.value;saveRackConfig();}";
        html += "function relaxRackServo(){fetch('/api/control/action',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action:13})}).then(r=>r.json()).then(res=>showStatus(res.msg||'Servo relaxado!'));}";
        html += "function saveRackConfig(){const eng=parseInt(document.getElementById('cfg_rack_engaged').value)||0;const rel=parseInt(document.getElementById('cfg_rack_released').value)||90;const inv=document.getElementById('cfg_rack_inverted').checked;";
        html += "fetch('/api/control/rack/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({angle_engaged:eng,angle_released:rel,inverted:inv})})";
        html += ".then(r=>r.json()).then(res=>{showStatus(res.msg||'Calibração da cremalheira guardada!');const b=document.getElementById('rackDirBadge');if(b){b.innerText=inv?'DIREÇÃO: INVERTIDA':'DIREÇÃO: NORMAL';b.style.background=inv?'#065f46':'#1e293b';b.style.color=inv?'#6ee7b7':'var(--muted)';}});}";
        html += "function saveAnchorConfig(){const d=parseFloat(document.getElementById('cfg_drum_diam').value)||30.0;const p=parseInt(document.getElementById('cfg_pulses_per_rev').value)||1;";
        html += "fetch('/api/control/anchor/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({diam_mm:d,pulses_per_rev:p})})";
        html += ".then(r=>r.json()).then(res=>showStatus(res.msg||'Calibração do tambor guardada!'));}";
        html += "function saveAllConfig(e){";
        html += "  e.preventDefault();";
        html += "  const id=parseInt(document.getElementById('cfg_id').value);";
        html += "  const ssid=document.getElementById('cfg_ssid').value;";
        html += "  const pass=document.getElementById('cfg_pass').value;";
        html += "  const bright=parseInt(document.getElementById('cfg_bright').value);";
        html += "  const rEng=parseInt(document.getElementById('cfg_rack_engaged')?.value??0);";
        html += "  const rRel=parseInt(document.getElementById('cfg_rack_released')?.value??90);";
        html += "  const rInv=document.getElementById('cfg_rack_inverted')?.checked??true;";
        html += "  const dDiam=parseFloat(document.getElementById('cfg_drum_diam')?.value??30.0);";
        html += "  const pRev=parseInt(document.getElementById('cfg_pulses_per_rev')?.value??1);";
        html += "  fetch('/api/rover/config',{";
        html += "    method:'POST',";
        html += "    headers:{'Content-Type':'application/json'},";
        html += "    body:JSON.stringify({rover_id:id,ap_ssid:ssid,ap_password:pass,screen_brightness:bright,rack_angle_engaged:rEng,rack_angle_released:rRel,rack_inverted:rInv,anchor_drum_diam_mm:dDiam,anchor_pulses_per_rev:pRev})";
        html += "  }).then(r=>r.json()).then(res=>{";
        html += "    if(res.success){";
        html += "      showStatus(res.msg||'Configurações guardadas com sucesso no ficheiro /config.json!');";
        html += "    }else{";
        html += "      showStatus(res.error||'Erro ao guardar configurações.',true);";
        html += "    }";
        html += "  }).catch(err=>showStatus('Erro de comunicação: '+err,true));";
        html += "}";
        html += "function reloadConfig(){";
        html += "  if(confirm('Recarregar as configurações gravadas no ficheiro /config.json?')){";
        html += "    fetch('/api/rover/config/reload',{method:'POST'}).then(r=>r.json()).then(res=>{";
        html += "      alert(res.msg);location.reload();";
        html += "    }).catch(err=>alert('Erro ao recarregar: '+err));";
        html += "  }";
        html += "}";
        html += "</script>";

        html += getHtmlFooter();
        _server.send(200, "text/html", html);
    }

    // -------------------------------------------------------------
    // PÁGINA 2: TELEMETRIA GPS
    // -------------------------------------------------------------
    void handleGpsPage() {
        String html = getHtmlHeader("Telemetria GPS", "gps");

        html += "<style>";
        html += ".rate-btn { flex: 1; min-width: 60px; padding: 10px 6px; border-radius: 8px; border: 1px solid var(--card-border); background: rgba(15,23,42,0.6); color: var(--muted); cursor: pointer; font-weight: 600; transition: all 0.2s ease; display: flex; flex-direction: column; align-items: center; gap: 4px; }";
        html += ".rate-btn.active { background: rgba(6,182,212,0.15); border-color: var(--primary); color: var(--text); box-shadow: 0 0 12px rgba(6,182,212,0.25); }";
        html += ".rate-btn:hover:not(.active) { border-color: var(--muted); color: var(--text); }";
        html += ".rate-btn .sub { font-size: 0.72rem; font-weight: normal; color: var(--muted); }";
        html += ".rate-btn.active .sub { color: var(--primary); }";
        html += "</style>";

        html += "<div class='card'>";
        html += "<div class='card-title'><span><i class='fa-solid fa-location-dot'></i> Posicionamento e Navegação Quectel LC29H</span></div>";

        html += "<div class='grid-4'>";
        html += "<div class='stat-box'><div class='stat-label'>Estado Satélites</div><div class='stat-value' id='gpsFix' style='color:" + String(_gps.hasFix() ? "var(--green)" : "var(--red)") + "'>";
        html += _gps.hasFix() ? "FIX (" + String(_gps.getSatellites()) + " sats)" : "SEM FIX";
        html += "</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Velocidade</div><div class='stat-value' id='gpsSpeed' style='color:var(--primary);'>" + String(_gps.getSpeedKnots(), 1) + " kn</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Rumo Proa</div><div class='stat-value' id='gpsHeading'>" + String(_gps.getHeading(), 0) + "°</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Modo Motor</div><div class='stat-value' id='motorMode' style='color:var(--yellow);'>" + String(get_motor_status_str(_motorStatus)) + "</div></div>";
        html += "</div>";

        html += "<div class='grid-2' style='margin-top:16px;'>";
        html += "<div class='stat-box'><div class='stat-label'>Latitude (WGS84)</div><div class='stat-value' id='gpsLat' style='font-size:1.6rem;'>" + String(_gps.getLatitude(), 6) + "</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Longitude (WGS84)</div><div class='stat-value' id='gpsLng' style='font-size:1.6rem;'>" + String(_gps.getLongitude(), 6) + "</div></div>";
        html += "</div>";

        // Botões de Ação de Mapa
        html += "<div style='margin-top:20px;' class='btn-group'>";
        html += "<a id='osmLink' href='https://www.openstreetmap.org/?mlat=" + String(_gps.getLatitude(), 6) + "&mlon=" + String(_gps.getLongitude(), 6) + "#map=18/" + String(_gps.getLatitude(), 6) + "/" + String(_gps.getLongitude(), 6) + "' target='_blank' class='btn btn-primary'><i class='fa-solid fa-map'></i> Ver no OpenStreetMap</a>";
        html += "<a id='gmapsLink' href='https://maps.google.com/?q=" + String(_gps.getLatitude(), 6) + "," + String(_gps.getLongitude(), 6) + "' target='_blank' class='btn btn-secondary'><i class='fa-solid fa-location-dot'></i> Google Maps</a>";
        html += "</div></div>";

        // Painel de Configuração Quectel LC29H
        html += "<div class='card' style='margin-top:20px;'>";
        html += "<div class='card-title' style='display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:10px;'>";
        html += "  <span><i class='fa-solid fa-satellite-dish'></i> Configuração Quectel LC29H (Dual-Band L1/L5)</span>";
        html += "  <span id='gpsSaveBadge' class='badge' style='display:none;background:rgba(16,185,129,0.2);color:var(--green);border:1px solid var(--green);'><i class='fa-solid fa-check'></i> Aplicado</span>";
        html += "</div>";

        html += "<div style='margin:12px 0;background:rgba(0,0,0,0.25);padding:14px;border-radius:10px;border:1px solid var(--card-border);'>";
        html += "  <div style='display:flex;justify-content:space-between;align-items:center;margin-bottom:10px;flex-wrap:wrap;gap:6px;'>";
        html += "    <span class='stat-label' style='font-weight:600;color:var(--text);font-size:0.9rem;'><i class='fa-solid fa-gauge-high'></i> Taxa de Atualização de Navegação:</span>";
        html += "    <span id='rateLabel' style='font-size:0.8rem;color:var(--primary);font-weight:600;'>1 Hz &bull; 1000 ms</span>";
        html += "  </div>";
        html += "  <div style='display:grid;grid-template-columns:repeat(4,1fr);gap:8px;margin-bottom:12px;'>";
        html += "    <button type='button' class='rate-btn active' id='btnRate1' onclick='selectRate(1)'><span style='font-size:1.1rem;font-weight:bold;'>1 Hz</span><span class='sub'>1000 ms</span></button>";
        html += "    <button type='button' class='rate-btn' id='btnRate2' onclick='selectRate(2)'><span style='font-size:1.1rem;font-weight:bold;'>2 Hz</span><span class='sub'>500 ms</span></button>";
        html += "    <button type='button' class='rate-btn' id='btnRate5' onclick='selectRate(5)'><span style='font-size:1.1rem;font-weight:bold;'>5 Hz</span><span class='sub'>200 ms &bull; Rec.</span></button>";
        html += "    <button type='button' class='rate-btn' id='btnRate10' onclick='selectRate(10)'><span style='font-size:1.1rem;font-weight:bold;'>10 Hz</span><span class='sub'>100 ms &bull; Max</span></button>";
        html += "  </div>";
        html += "  <div style='display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:10px;padding-top:10px;border-top:1px solid rgba(255,255,255,0.06);'>";
        html += "    <label style='display:flex;align-items:center;gap:8px;font-size:0.85rem;color:var(--text);cursor:pointer;'><input type='checkbox' id='chkSaveFlash' checked style='accent-color:var(--primary);width:16px;height:16px;'><span>Gravar na Flash NVRAM ($PAIR513)</span></label>";
        html += "    <button type='button' class='btn btn-primary' onclick='applyGpsRate()'><i class='fa-solid fa-bolt'></i> Aplicar ao GPS</button>";
        html += "  </div>";
        html += "</div>";

        html += "<div style='margin-top:12px;background:rgba(0,0,0,0.2);padding:12px;border-radius:8px;border:1px solid var(--card-border);'>";
        html += "  <div style='font-size:0.85rem;color:var(--muted);margin-bottom:8px;font-weight:600;'><i class='fa-solid fa-terminal'></i> Envio Direto de Comandos ($PAIR / NMEA):</div>";
        html += "  <div style='display:flex;gap:8px;flex-wrap:wrap;'>";
        html += "    <input type='text' id='gpsCustomCmd' placeholder='Ex: PAIR004 ou PAIR067 ou PAIR051' style='flex:1;min-width:180px;background:rgba(15,23,42,0.8);border:1px solid var(--card-border);border-radius:6px;padding:8px 12px;color:var(--text);font-family:monospace;font-size:0.85rem;'>";
        html += "    <button type='button' class='btn btn-secondary' onclick='sendCustomGpsCmd()'><i class='fa-solid fa-paper-plane'></i> Enviar</button>";
        html += "  </div>";
        html += "  <div id='cmdFeedback' style='font-size:0.78rem;color:var(--muted);margin-top:6px;font-family:monospace;'></div>";
        html += "</div>";
        html += "</div>";

        html += "<script>";
        html += "let selectedRate=1,rateInitialized=false;";
        html += "function selectRate(hz){";
        html += "  selectedRate=hz;";
        html += "  [1,2,5,10].forEach(h=>{";
        html += "    const el=document.getElementById('btnRate'+h);";
        html += "    if(el)el.classList.toggle('active',h===hz);";
        html += "  });";
        html += "  const lbl=document.getElementById('rateLabel');";
        html += "  if(lbl){";
        html += "    const desc=hz===1?'1000 ms • Económico':(hz===2?'500 ms • Moderado':(hz===5?'200 ms • Recomendado':'100 ms • Alta Taxa'));";
        html += "    lbl.innerText=hz+' Hz • '+desc;";
        html += "  }";
        html += "}";
        html += "function applyGpsRate(){";
        html += "  const saveFlash=document.getElementById('chkSaveFlash').checked;";
        html += "  const badge=document.getElementById('gpsSaveBadge');";
        html += "  fetch('/api/gps/rate',{";
        html += "    method:'POST',";
        html += "    headers:{'Content-Type':'application/json'},";
        html += "    body:JSON.stringify({rate_hz:selectedRate,save_flash:saveFlash})";
        html += "  }).then(r=>r.json()).then(res=>{";
        html += "    if(badge){";
        html += "      badge.style.display='inline-flex';";
        html += "      badge.innerHTML='<i class=\"fa-solid fa-check\"></i> '+(res.msg||(selectedRate+' Hz Ativo'));";
        html += "      setTimeout(()=>{badge.style.display='none';},4000);";
        html += "    }";
        html += "  }).catch(e=>{alert('Erro ao comunicar com o Rover: '+e);});";
        html += "}";
        html += "function sendCustomGpsCmd(){";
        html += "  const input=document.getElementById('gpsCustomCmd');";
        html += "  const fb=document.getElementById('cmdFeedback');";
        html += "  const val=input.value.trim();";
        html += "  if(!val)return;";
        html += "  fetch('/api/gps/command',{";
        html += "    method:'POST',";
        html += "    headers:{'Content-Type':'application/json'},";
        html += "    body:JSON.stringify({cmd:val})";
        html += "  }).then(r=>r.json()).then(res=>{";
        html += "    if(fb)fb.innerText=res.msg||'Instrução enviada!';";
        html += "    input.value='';";
        html += "    setTimeout(()=>{if(fb)fb.innerText='';},5000);";
        html += "  }).catch(e=>{if(fb)fb.innerText='Falha: '+e;});";
        html += "}";
        html += "setInterval(()=>{fetch('/api/status').then(r=>r.json()).then(data=>{";
        html += "const g=data.gps;";
        html += "if(!g)return;";
        html += "document.getElementById('gpsFix').innerText=g.fix?('FIX ('+g.satellites+' sats)'):'SEM FIX';";
        html += "document.getElementById('gpsFix').style.color=g.fix?'var(--green)':'var(--red)';";
        html += "document.getElementById('gpsSpeed').innerText=g.speed_kn.toFixed(1)+' kn';";
        html += "document.getElementById('gpsHeading').innerText=g.heading.toFixed(0)+'°';";
        html += "document.getElementById('motorMode').innerText=data.actuators.motor_status_str;";
        html += "document.getElementById('gpsLat').innerText=g.lat.toFixed(6);";
        html += "document.getElementById('gpsLng').innerText=g.lng.toFixed(6);";
        html += "document.getElementById('osmLink').href='https://www.openstreetmap.org/?mlat='+g.lat+'&mlon='+g.lng+'#map=18/'+g.lat+'/'+g.lng;";
        html += "document.getElementById('gmapsLink').href='https://maps.google.com/?q='+g.lat+','+g.lng;";
        html += "if(g.rate_hz&&!rateInitialized){selectRate(g.rate_hz);rateInitialized=true;}";
        html += "});},1000);";
        html += "</script>";

        html += getHtmlFooter();
        _server.send(200, "text/html", html);
    }

    // -------------------------------------------------------------
    // PÁGINA 3: MONITORAMENTO DA BATERIA LIPO 4S
    // -------------------------------------------------------------
    void handleBatteryPage() {
        String html = getHtmlHeader("Monitor da Bateria LiPo", "battery");

        _battery.update();
        uint8_t pct = _battery.getPercentage();
        float vTot = _battery.getVoltageMv() / 1000.0f;
        float vCell = _battery.getCellAverageV();
        float currentA = _battery.getCurrentAmps();
        float consumedMah = _battery.getConsumedMah();
        uint8_t cells = _battery.getCellCount();
        String barColor = pct < 25 ? "var(--red)" : (pct < 50 ? "var(--yellow)" : "var(--green)");

        html += "<style>";
        html += ".batt-type-btn { flex: 1; min-width: 90px; padding: 10px 14px; border-radius: 8px; border: 1px solid var(--card-border); background: rgba(15,23,42,0.6); color: var(--muted); cursor: pointer; font-weight: 600; transition: all 0.2s ease; display: flex; flex-direction: column; align-items: center; gap: 4px; }";
        html += ".batt-type-btn.active { background: rgba(6,182,212,0.15); border-color: var(--primary); color: var(--text); box-shadow: 0 0 12px rgba(6,182,212,0.25); }";
        html += ".batt-type-btn:hover:not(.active) { border-color: var(--muted); color: var(--text); }";
        html += ".batt-type-btn .sub { font-size: 0.72rem; font-weight: normal; color: var(--muted); }";
        html += ".batt-type-btn.active .sub { color: var(--primary); }";
        html += "</style>";

        html += "<div class='card'>";
        html += "<div class='card-title' style='display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:10px;'>";
        html += "  <span><i class='fa-solid fa-battery-full'></i> Monitorização APM Power Module (28V / 90A)</span>";
        html += "  <span id='saveStatusBadge' class='badge' style='display:none;background:rgba(16,185,129,0.2);color:var(--green);border:1px solid var(--green);'><i class='fa-solid fa-check'></i> Gravado</span>";
        html += "</div>";

        // Selector 3S / 4S / 6S
        html += "<div style='margin:12px 0 20px 0;background:rgba(0,0,0,0.25);padding:14px;border-radius:10px;border:1px solid var(--card-border);'>";
        html += "  <div style='display:flex;justify-content:space-between;align-items:center;margin-bottom:10px;flex-wrap:wrap;gap:6px;'>";
        html += "    <span class='stat-label' style='font-weight:600;color:var(--text);font-size:0.9rem;'><i class='fa-solid fa-sliders'></i> Configuração da Bateria LiPo:</span>";
        html += "    <span id='battSpecs' style='font-size:0.8rem;color:var(--primary);font-weight:600;'></span>";
        html += "  </div>";
        html += "  <div style='display:grid;grid-template-columns:repeat(3,1fr);gap:10px;'>";
        html += "    <button type='button' class='batt-type-btn" + String(cells == 3 ? " active" : "") + "' id='btn3S' onclick='selectBattery(3)'><span style='font-size:1.1rem;display:flex;align-items:center;gap:6px;'><i class='fa-solid fa-battery-half'></i> 3S LiPo</span><span class='sub'>11.1V Nom &bull; 12.6V Max</span></button>";
        html += "    <button type='button' class='batt-type-btn" + String(cells == 4 ? " active" : "") + "' id='btn4S' onclick='selectBattery(4)'><span style='font-size:1.1rem;display:flex;align-items:center;gap:6px;'><i class='fa-solid fa-battery-three-quarters'></i> 4S LiPo</span><span class='sub'>14.8V Nom &bull; 16.8V Max</span></button>";
        html += "    <button type='button' class='batt-type-btn" + String(cells == 6 ? " active" : "") + "' id='btn6S' onclick='selectBattery(6)'><span style='font-size:1.1rem;display:flex;align-items:center;gap:6px;'><i class='fa-solid fa-battery-full'></i> 6S LiPo</span><span class='sub'>22.2V Nom &bull; 25.2V Max</span></button>";
        html += "  </div>";
        html += "</div>";

        // Gauge Grande
        html += "<div style='text-align:center;padding:16px 0;'>";
        html += "<div style='font-size:3.5rem;font-weight:800;color:" + barColor + ";' id='batPct'>" + String(pct) + "%</div>";
        html += "<div class='progress-bg' style='max-width:400px;margin:0 auto;'><div id='batBar' class='progress-bar' style='width:" + String(pct) + "%;background:" + barColor + ";'></div></div>";
        html += "</div>";

        html += "<div class='grid-4'>";
        html += "<div class='stat-box'><div class='stat-label'>Tensão Total</div><div class='stat-value' id='vTotVal' style='color:var(--text);'>" + String(vTot, 2) + " V</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Média p/ Célula</div><div class='stat-value' id='vCellVal' style='color:var(--primary);'>" + String(vCell, 2) + " V/cel</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Corrente Instantânea</div><div class='stat-value' id='currVal' style='color:var(--text);'>" + String(currentA, 2) + " A</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Consumo Acumulado</div><div class='stat-value' id='consVal' style='color:var(--yellow);'>" + String(consumedMah, 0) + " mAh</div></div>";
        html += "</div>";

        // Tabela representativa dinâmica
        html += "<div style='margin-top:20px;'>";
        html += "<div class='stat-label' style='margin-bottom:8px;'>Distribuição Estimada de Tensão (<span id='cellsCountLabel'>" + String(cells) + "S</span> LiPo):</div>";
        html += "<div id='cellsGrid' style='display:grid;grid-template-columns:repeat(auto-fit,minmax(110px,1fr));gap:10px;'></div>";
        html += "</div></div>";

        html += "<script>";
        html += "let currentCells = " + String(cells) + "; let lastKnownAvg = " + String(vCell, 2) + ";";
        html += "function updateBatteryUI(cells) {";
        html += "  currentCells = cells;";
        html += "  [3,4,6].forEach(c => { const btn = document.getElementById('btn' + c + 'S'); if(btn){ if(c===cells) btn.classList.add('active'); else btn.classList.remove('active'); } });";
        html += "  const sp = document.getElementById('battSpecs');";
        html += "  if(sp){ if(cells===3) sp.innerText='3S LiPo • 9.9V min • 12.6V max'; else if(cells===4) sp.innerText='4S LiPo • 13.2V min • 16.8V max'; else if(cells===6) sp.innerText='6S LiPo • 19.8V min • 25.2V max'; }";
        html += "  const lbl = document.getElementById('cellsCountLabel'); if(lbl) lbl.innerText = cells + 'S';";
        html += "  renderCellsGrid(cells, lastKnownAvg);";
        html += "}";
        html += "function renderCellsGrid(cells, avgV) {";
        html += "  const grid = document.getElementById('cellsGrid'); if(!grid) return; let h = '';";
        html += "  for(let i=1; i<=cells; i++){";
        html += "    h += '<div class=\"stat-box\" style=\"text-align:center;padding:10px 8px;\"><div class=\"stat-label\" style=\"font-size:0.75rem;\"><i class=\"fa-solid fa-bolt\"></i> Célula ' + i + '</div><div class=\"stat-value cell-v\" style=\"font-size:1.15rem;color:var(--primary);margin-top:4px;\">' + (avgV>0 ? avgV.toFixed(2) + ' V' : '-- V') + '</div></div>';";
        html += "  }";
        html += "  grid.innerHTML = h;";
        html += "}";
        html += "function selectBattery(cells) {";
        html += "  updateBatteryUI(cells);";
        html += "  fetch('/api/battery/config', { method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({cells:cells}) })";
        html += "    .then(r=>r.json()).then(res=>{";
        html += "      const badge = document.getElementById('saveStatusBadge');";
        html += "      if(badge){ badge.style.display='inline-flex'; setTimeout(()=>{ badge.style.display='none'; }, 2500); }";
        html += "    }).catch(e=>console.error(e));";
        html += "}";
        html += "updateBatteryUI(currentCells);";
        html += "setInterval(()=>{fetch('/api/status').then(r=>r.json()).then(data=>{";
        html += "  const b=data.battery; if(!b) return;";
        html += "  const p=b.pct;const v=b.voltage;const vc=b.cell_avg;";
        html += "  lastKnownAvg = vc;";
        html += "  if(b.cells && b.cells !== currentCells) updateBatteryUI(b.cells);";
        html += "  const col=p<25?'var(--red)':(p<50?'var(--yellow)':'var(--green)');";
        html += "  document.getElementById('batPct').innerText=p+'%';document.getElementById('batPct').style.color=col;";
        html += "  document.getElementById('batBar').style.width=p+'%';document.getElementById('batBar').style.background=col;";
        html += "  document.getElementById('vTotVal').innerText=v.toFixed(2)+' V';";
        html += "  document.getElementById('vCellVal').innerText=vc.toFixed(2)+' V/cel';";
        html += "  document.getElementById('currVal').innerText=b.current_a.toFixed(2)+' A';";
        html += "  document.getElementById('consVal').innerText=b.consumed_mah.toFixed(0)+' mAh';";
        html += "  document.querySelectorAll('.cell-v').forEach(el=>el.innerText=vc.toFixed(2)+' V');";
        html += "});},1000);";
        html += "</script>";

        html += getHtmlFooter();
        _server.send(200, "text/html", html);
    }

    // -------------------------------------------------------------
    // PÁGINA 4: COMANDAR A BOIA (SERVOS, MOTOR ESC, LEME, GUINCHO)
    // -------------------------------------------------------------
    void handleControlPage() {
        String html = getHtmlHeader("Comandar Boia", "control");

        // Status atual dos atuadores
        html += "<div class='card'>";
        html += "<div class='card-title'><span><i class='fa-solid fa-gamepad'></i> Painel de Controlo Direto da Boia</span></div>";
        html += "<div class='grid-4' style='margin-bottom:16px;'>";
        html += "<div class='stat-box'><div class='stat-label'>Modo Motor</div><div class='stat-value' id='curMotor' style='color:var(--yellow);font-size:1.1rem;'>" + String(get_motor_status_str(_motorStatus)) + "</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Estado Âncora</div><div class='stat-value' id='curAnchor' style='color:var(--primary);font-size:1.1rem;'>" + String(get_anchor_status_str(_actuators.getAnchorStatus())) + "</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Profundidade</div><div class='stat-value' id='curDepth'>" + String(_actuators.getAnchorDepthCm() / 100.0f, 1) + " m</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Cremalheira</div><div class='stat-value' id='curRack' style='color:var(--green);font-size:1.1rem;'>" + String(_actuators.isRackReleased() ? "LIVRE" : "TRAVADA") + "</div></div>";
        html += "</div></div>";

        // Card: Propulsão & Leme
        html += "<div class='card'>";
        html += "<div class='card-title'><i class='fa-solid fa-gauge-high'></i> Motor Principal (ESC) & Leme de Direção</div>";
        html += "<div class='grid-2'>";
        
        // Motor ESC
        html += "<div>";
        html += "<div class='stat-label'>Aceleração ESC: <span id='thrVal' style='font-size:1.1rem;color:var(--primary);'>" + String(_actuators.getThrottle()) + "%</span></div>";
        html += "<input type='range' id='throttleSlider' min='-100' max='100' value='" + String(_actuators.getThrottle()) + "' class='slider' oninput='updateSliders()' onchange='sendActuators()'>";
        html += "<div class='btn-group' style='margin-top:12px;'>";
        html += "<button onclick='setThrottle(0)' class='btn btn-secondary'>0% (Stop)</button>";
        html += "<button onclick='setThrottle(25)' class='btn btn-secondary'>+25%</button>";
        html += "<button onclick='setThrottle(50)' class='btn btn-secondary'>+50%</button>";
        html += "<button onclick='setThrottle(100)' class='btn btn-primary'>+100%</button>";
        html += "<button onclick='setThrottle(-50)' class='btn btn-secondary'>-50% (Ré)</button>";
        html += "</div></div>";

        // Leme
        html += "<div>";
        html += "<div class='stat-label'>Ângulo do Leme: <span id='rudVal' style='font-size:1.1rem;color:var(--primary);'>" + String(_actuators.getRudder()) + "%</span></div>";
        html += "<input type='range' id='rudderSlider' min='-100' max='100' value='" + String(_actuators.getRudder()) + "' class='slider' oninput='updateSliders()' onchange='sendActuators()'>";
        html += "<div class='btn-group' style='margin-top:12px;'>";
        html += "<button onclick='setRudder(-75)' class='btn btn-secondary'><i class='fa-solid fa-arrow-left'></i> Bombordo (-75%)</button>";
        html += "<button onclick='setRudder(0)' class='btn btn-secondary'><i class='fa-solid fa-compass'></i> Centro (0%)</button>";
        html += "<button onclick='setRudder(75)' class='btn btn-secondary'>Estibordo (+75%) <i class='fa-solid fa-arrow-right'></i></button>";
        html += "</div></div>";

        html += "</div></div>";

        // Card: Guincho de Âncora & Cremalheira (Modo ON / OFF e Direto)
        html += "<div class='card'>";
        html += "<div class='card-title'><i class='fa-solid fa-anchor'></i> Guincho da Âncora (GPIO 23) & Travão da Cremalheira (GPIO 21)</div>";
        html += "<div class='btn-group'>";
        html += "<button onclick='sendAction(4)' class='btn btn-primary'><i class='fa-solid fa-play'></i> LIGAR Puxar (1800 µs)</button>";
        html += "<button onclick='sendAction(14)' class='btn btn-secondary'><i class='fa-solid fa-rotate-left'></i> LIGAR Descer (1200 µs)</button>";
        html += "<button onclick='cutWinchSignal()' class='btn btn-danger' style='background:#dc2626;font-weight:700;'><i class='fa-solid fa-power-off'></i> CORTAR SINAL (OFF / 0V)</button>";
        html += "<button onclick='sendAction(16)' class='btn btn-primary' style='background:#059669;color:#fff;'><i class='fa-solid fa-lock'></i> Travar Cremalheira (Encosto)</button>";
        html += "<button onclick='sendAction(17)' class='btn btn-secondary' style='background:#d97706;color:#fff;'><i class='fa-solid fa-lock-open'></i> Libertar Cremalheira (Livre)</button>";
        html += "<button onclick='sendAction(6)' class='btn btn-secondary'><i class='fa-solid fa-rotate'></i> Alternar Travão</button>";
        html += "<button onclick='sendAction(13)' class='btn btn-secondary' style='background:#475569;color:#fff;'><i class='fa-solid fa-feather'></i> Relaxar Servo</button>";
        html += "</div></div>";

        // Card: Contador de Cabo da Âncora (Sensor Hall DRV5032)
        html += "<div class='card' style='border:1px solid #0284c7;'>";
        html += "<div class='card-title' style='display:flex;justify-content:space-between;align-items:center;'>";
        html += "<span><i class='fa-solid fa-compass-drafting'></i> Contador de Cabo da Âncora (Sensor Hall DRV5032 - GPIO 20)</span>";
        html += "<span id='anchorLimitBadge' class='badge' style='background:#1e293b;border:1px solid #334155;color:var(--muted);font-size:0.85rem;'>Fim de Curso: LIVRE</span>";
        html += "</div>";
        html += "<div class='grid-4' style='margin-bottom:14px;'>";
        html += "<div class='stat-box'><div class='stat-label'>Cabo Desenrolado</div><div class='stat-value' id='hallDepthVal' style='color:var(--primary);font-size:1.4rem;font-weight:700;'>" + String(_actuators.getAnchorDepthCm() / 100.0f, 2) + " m</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Voltas do Tambor</div><div class='stat-value' id='hallRevsVal' style='color:#38bdf8;font-size:1.4rem;font-weight:700;'>" + String(_actuators.getAnchorRevolutions(), 2) + "</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Total de Pulsos</div><div class='stat-value' id='hallPulsesVal' style='font-size:1.4rem;font-weight:700;'>" + String(_actuators.getAnchorHallPulses()) + "</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Perímetro / Volta</div><div class='stat-value' id='hallCircVal' style='color:var(--yellow);font-size:1.2rem;font-weight:600;'>" + String((3.14159f * _config.getAnchorDrumDiamMm() / 10.0f), 1) + " cm</div></div>";
        html += "</div>";
        html += "<div class='btn-group'>";
        html += "<button type='button' onclick='zeroAnchorCounter()' class='btn btn-secondary' style='background:#0284c7;color:#fff;'><i class='fa-solid fa-rotate-left'></i> Zerar Contador (0.0 m)</button>";
        html += "</div></div>";

        // Card: Modos Operacionais & Emergência
        html += "<div class='card'>";
        html += "<div class='card-title'><i class='fa-solid fa-sliders'></i> Modos de Operação & Paragem Imediata</div>";
        html += "<div class='btn-group'>";
        html += "<button onclick='sendAction(1)' class='btn' style='background:#0284c7;color:#fff;'><i class='fa-solid fa-location-dot'></i> Hold Station (GPS)</button>";
        html += "<button onclick='sendAction(7)' class='btn' style='background:#7c3aed;color:#fff;'><i class='fa-solid fa-house'></i> Return to Launch (RTL)</button>";
        html += "<button onclick='sendAction(10)' class='btn btn-secondary'><i class='fa-solid fa-gamepad'></i> Modo Manual</button>";
        html += "<button onclick='sendAction(8)' class='btn' style='background:var(--yellow);color:#0f172a;'><i class='fa-solid fa-bell'></i> Alarme Sonoro / Strobe</button>";
        html += "<button onclick='sendEmergencyStop()' class='btn btn-danger' style='font-size:1rem;padding:12px 24px;'><i class='fa-solid fa-hand'></i> PARAGEM DE EMERGÊNCIA</button>";
        html += "</div></div>";

        html += "<script>";
        html += "function showFeedback(msg){";
        html += "  let el=document.getElementById('cmdFeedback');";
        html += "  if(!el){el=document.createElement('div');el.id='cmdFeedback';";
        html += "    el.style.position='fixed';el.style.bottom='24px';el.style.right='24px';";
        html += "    el.style.background='#0891b2';el.style.color='#fff';el.style.padding='10px 20px';";
        html += "    el.style.borderRadius='8px';el.style.boxShadow='0 4px 14px rgba(0,0,0,0.6)';";
        html += "    el.style.fontSize='0.95rem';el.style.fontWeight='600';el.style.zIndex='9999';";
        html += "    el.style.transition='opacity 0.3s';document.body.appendChild(el);";
        html += "  }";
        html += "  el.innerText=msg;el.style.opacity='1';el.style.display='block';";
        html += "  if(window._fbTimer)clearTimeout(window._fbTimer);";
        html += "  window._fbTimer=setTimeout(()=>{el.style.opacity='0';setTimeout(()=>el.style.display='none',300);},2200);";
        html += "}";
        html += "function updateSliders(){document.getElementById('thrVal').innerText=document.getElementById('throttleSlider').value+'%';document.getElementById('rudVal').innerText=document.getElementById('rudderSlider').value+'%';}";
        html += "function setThrottle(v){document.getElementById('throttleSlider').value=v;updateSliders();sendActuators();}";
        html += "function setRudder(v){document.getElementById('rudderSlider').value=v;updateSliders();sendActuators();}";
        html += "function sendActuators(){const t=parseInt(document.getElementById('throttleSlider').value);const r=parseInt(document.getElementById('rudderSlider').value);";
        html += "fetch('/api/control/actuator',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({throttle:t,rudder:r})});}";
        html += "function sendAction(a){fetch('/api/control/action',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action:a})})";
        html += ".then(r=>r.json()).then(res=>{if(res&&res.msg)showFeedback(res.msg);});}";
        html += "function zeroAnchorCounter(){sendAction(15);}";
        html += "function sendEmergencyStop(){setThrottle(0);setRudder(0);sendAction(2);}";
        html += "function cutWinchSignal(){sendAction(5);fetch('/api/control/winch',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({pulseUs:0})})";
        html += ".then(r=>r.json()).then(res=>{showFeedback('Sinal PWM cortado (0V / OFF)');});}";
        html += "setInterval(()=>{fetch('/api/status').then(r=>r.json()).then(data=>{const a=data.actuators;if(!a)return;";
        html += "document.getElementById('curMotor').innerText=a.motor_status_str||'';";
        html += "document.getElementById('curAnchor').innerText=a.anchor_status_str||'';";
        html += "document.getElementById('curDepth').innerText=(a.anchor_depth_m||0).toFixed(2)+' m';";
        html += "document.getElementById('curRack').innerText=a.rack_released?'LIVRE':'TRAVADA';";
        html += "const hd=document.getElementById('hallDepthVal');if(hd&&a.anchor_depth_m!==undefined){hd.innerText=a.anchor_depth_m.toFixed(2)+' m ('+(a.anchor_depth_cm||0)+' cm)';}";
        html += "const hr=document.getElementById('hallRevsVal');if(hr&&a.anchor_revs!==undefined){hr.innerText=a.anchor_revs.toFixed(2);}";
        html += "const hp=document.getElementById('hallPulsesVal');if(hp&&a.anchor_pulses!==undefined){hp.innerText=a.anchor_pulses;}";
        html += "const hc=document.getElementById('hallCircVal');if(hc&&a.anchor_drum_diam_mm!==undefined){hc.innerText=(Math.PI*a.anchor_drum_diam_mm/10.0).toFixed(1)+' cm';}";
        html += "const lb=document.getElementById('anchorLimitBadge');if(lb){if(a.anchor_limit){lb.innerText='Fim de Curso: NO TOPO (Acionado)';lb.style.background='#059669';lb.style.color='#fff';}else{lb.innerText='Fim de Curso: LIVRE';lb.style.background='#1e293b';lb.style.color='var(--muted)';}}";
        html += "if(document.getElementById('rudVal')&&a.rudder!==undefined){document.getElementById('rudVal').innerText=a.rudder+'%';if(document.getElementById('rudderSlider'))document.getElementById('rudderSlider').value=a.rudder;}";
        html += "if(document.getElementById('thrVal')&&a.throttle!==undefined){document.getElementById('thrVal').innerText=a.throttle+'%';if(document.getElementById('throttleSlider'))document.getElementById('throttleSlider').value=a.throttle;}";
        html += "});},300);";
        html += "</script>";

        html += getHtmlFooter();
        _server.send(200, "text/html", html);
    }

    // -------------------------------------------------------------
    // PÁGINA 5: EXPLORADOR DO CARTÃO MICROSD TF
    // -------------------------------------------------------------
    void handleSdPage() {
        String html = getHtmlHeader("Explorador do Cartão MicroSD", "sd");

        bool sdOk = _sd.isReady();
        uint64_t totalMb = sdOk ? (_sd.getTotalBytes() / (1024ULL * 1024ULL)) : 0;
        uint64_t usedMb = sdOk ? (_sd.getUsedBytes() / (1024ULL * 1024ULL)) : 0;
        uint64_t freeMb = sdOk ? (_sd.getFreeBytes() / (1024ULL * 1024ULL)) : 0;
        uint8_t usedPct = (totalMb > 0) ? (uint8_t)((usedMb * 100ULL) / totalMb) : 0;

        // Estilos específicos do Explorador de Ficheiros
        html += "<style>";
        html += ".fe-window{background:var(--card);border:1px solid var(--card-border);border-radius:14px;overflow:hidden;box-shadow:0 14px 30px rgba(0,0,0,0.4);margin-bottom:20px;position:relative;}";
        html += ".fe-toolbar{background:linear-gradient(180deg,#121d38,#0e172e);border-bottom:1px solid var(--card-border);padding:10px 14px;display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:10px;}";
        html += ".fe-nav-group{display:flex;align-items:center;gap:6px;flex-wrap:wrap;}";
        html += ".fe-btn-icon{background:rgba(255,255,255,0.05);border:1px solid var(--card-border);color:var(--text);padding:7px 11px;border-radius:8px;font-size:0.85rem;cursor:pointer;transition:all 0.2s;display:inline-flex;align-items:center;justify-content:center;gap:6px;}";
        html += ".fe-btn-icon:hover{background:var(--primary);color:#0f172a;border-color:var(--primary);}";
        html += ".fe-breadcrumbs{display:flex;align-items:center;gap:4px;background:rgba(0,0,0,0.35);border:1px solid var(--card-border);padding:4px 8px;border-radius:8px;font-size:0.825rem;overflow-x:auto;max-width:360px;}";
        html += ".fe-crumb{color:var(--muted);cursor:pointer;padding:3px 6px;border-radius:6px;transition:0.15s;display:inline-flex;align-items:center;gap:4px;white-space:nowrap;font-weight:500;}";
        html += ".fe-crumb:hover{color:var(--text);background:rgba(255,255,255,0.08);}";
        html += ".fe-crumb.active{color:var(--primary);font-weight:700;}";
        html += ".fe-crumb-sep{color:#475569;font-size:0.7rem;}";
        html += ".fe-search-wrap{position:relative;display:flex;align-items:center;min-width:160px;flex:1;max-width:240px;}";
        html += ".fe-search-wrap i{position:absolute;left:10px;color:var(--muted);font-size:0.8rem;pointer-events:none;}";
        html += ".fe-search-wrap input{width:100%;margin:0;padding:6px 10px 6px 30px;font-size:0.825rem;background:#090e1a;border:1px solid var(--card-border);border-radius:8px;color:#fff;}";
        html += ".fe-search-wrap input:focus{border-color:var(--primary);outline:none;}";
        html += ".fe-view-toggle{display:flex;background:rgba(0,0,0,0.35);border:1px solid var(--card-border);border-radius:8px;padding:2px;}";
        html += ".fe-view-btn{background:transparent;border:none;color:var(--muted);padding:5px 10px;border-radius:6px;cursor:pointer;font-size:0.85rem;transition:0.2s;}";
        html += ".fe-view-btn.active{background:var(--primary);color:#0f172a;font-weight:700;}";
        html += ".fe-body{position:relative;min-height:300px;padding:16px;}";
        html += ".fe-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(130px,1fr));gap:12px;}";
        html += ".fe-card{background:rgba(255,255,255,0.03);border:1px solid rgba(255,255,255,0.06);border-radius:12px;padding:14px 10px;text-align:center;cursor:pointer;transition:all 0.2s;position:relative;display:flex;flex-direction:column;align-items:center;}";
        html += ".fe-card:hover{background:rgba(6,182,212,0.08);border-color:rgba(6,182,212,0.4);transform:translateY(-2px);box-shadow:0 8px 20px rgba(0,0,0,0.35);}";
        html += ".fe-card-icon{font-size:2.5rem;margin-bottom:8px;transition:0.2s;display:flex;align-items:center;justify-content:center;height:48px;}";
        html += ".fe-card:hover .fe-card-icon{transform:scale(1.1);}";
        html += ".fe-card-name{font-size:0.8rem;font-weight:600;color:var(--text);width:100%;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;}";
        html += ".fe-card-meta{font-size:0.7rem;color:var(--muted);margin-top:4px;}";
        html += ".fe-card-actions{display:flex;gap:4px;margin-top:10px;opacity:0.3;transition:0.2s;}";
        html += ".fe-card:hover .fe-card-actions{opacity:1;}";
        html += ".fe-card-btn{background:rgba(0,0,0,0.6);border:1px solid var(--card-border);color:var(--text);width:26px;height:26px;border-radius:6px;display:inline-flex;align-items:center;justify-content:center;font-size:0.75rem;cursor:pointer;text-decoration:none;transition:0.15s;}";
        html += ".fe-card-btn:hover{background:var(--primary);color:#0f172a;border-color:var(--primary);}";
        html += ".fe-card-btn.del:hover{background:var(--red);color:#fff;border-color:var(--red);}";
        html += ".fe-table{width:100%;border-collapse:collapse;}";
        html += ".fe-table th{background:rgba(0,0,0,0.25);padding:10px 14px;color:var(--muted);font-size:0.75rem;text-transform:uppercase;font-weight:600;text-align:left;border-bottom:1px solid var(--card-border);}";
        html += ".fe-table td{padding:10px 14px;border-bottom:1px solid rgba(255,255,255,0.05);font-size:0.85rem;}";
        html += ".fe-table tr{cursor:pointer;transition:background 0.15s;}";
        html += ".fe-table tr:hover{background:rgba(6,182,212,0.06);}";
        html += ".fe-statusbar{background:rgba(0,0,0,0.35);border-top:1px solid var(--card-border);padding:8px 16px;display:flex;align-items:center;justify-content:space-between;font-size:0.75rem;color:var(--muted);flex-wrap:wrap;gap:6px;}";
        html += ".fe-drop-overlay{position:absolute;top:0;left:0;right:0;bottom:0;background:rgba(11,19,41,0.92);backdrop-filter:blur(4px);border:2px dashed var(--primary);border-radius:14px;display:none;align-items:center;justify-content:center;flex-direction:column;gap:12px;z-index:50;pointer-events:none;}";
        html += ".fe-drop-overlay.active{display:flex;}";
        html += ".fe-empty{text-align:center;padding:50px 20px;color:var(--muted);}";
        html += ".fe-empty i{font-size:3.5rem;margin-bottom:12px;opacity:0.35;color:var(--muted);}";
        html += "</style>";

        // Card 1: Armazenamento MicroSD Onboard
        html += "<div class='card'>";
        html += "<div class='card-title'><span><i class='fa-solid fa-sd-card'></i> Estado do Cartão MicroSD Onboard</span>";
        html += "<span class='badge " + String(sdOk ? "badge-sta" : "badge-ap") + "'>" + String(sdOk ? "MONTADO (OK)" : "NÃO DETETADO") + "</span></div>";

        if (sdOk) {
            html += "<div style='display:flex;justify-content:space-between;align-items:baseline;flex-wrap:wrap;gap:8px;font-size:0.875rem;margin-bottom:8px;'>";
            html += "<span style='color:var(--muted);'>Espaço Utilizado: <strong id='statUsedMb' style='color:var(--text);font-size:0.95rem;'>" + String((uint32_t)usedMb) + " MB</strong> de <span id='statTotalMb'>" + String((uint32_t)_sd.getCardSizeMB()) + " MB</span> (<span id='statUsedPct'>" + String(usedPct) + "%</span>)</span>";
            html += "<span style='color:var(--muted);'>Livre: <strong id='statFreeMb' style='color:var(--green);font-size:0.95rem;'>" + String((uint32_t)freeMb) + " MB</strong></span>";
            html += "</div>";
            html += "<div class='progress-bg' style='height:10px;margin-top:0;'><div id='statProgressBar' class='progress-bar' style='width:" + String(usedPct) + "%;background:var(--primary);'></div></div>";
        } else {
            html += "<p style='color:var(--red);font-size:0.9rem;'>O cartão MicroSD TF não está montado ou não foi detetado no slot. Insira um cartão formatado em FAT32.</p>";
        }
        html += "</div>";

        // Card 2: Explorador de Ficheiros (Finder / Explorer Style)
        html += "<div class='fe-window' id='explorerWindow'>";
        html += "<div class='fe-drop-overlay' id='feDropOverlay'><i class='fa-solid fa-cloud-arrow-up' style='font-size:3rem;color:var(--primary);'></i><span style='font-weight:600;font-size:1.1rem;color:#fff;'>Largar ficheiros para enviar para esta pasta</span></div>";

        // Toolbar
        html += "<div class='fe-toolbar'>";
        html += "<div class='fe-nav-group'>";
        html += "<button onclick='goUpDir()' class='fe-btn-icon' title='Subir um nível'><i class='fa-solid fa-arrow-up'></i></button>";
        html += "<button onclick='loadFileList()' class='fe-btn-icon' title='Atualizar'><i class='fa-solid fa-rotate'></i></button>";
        html += "<div class='fe-breadcrumbs' id='feBreadcrumbs'><span class='fe-crumb active'><i class='fa-solid fa-house'></i> Raiz</span></div>";
        html += "</div>";

        html += "<div class='fe-search-wrap'>";
        html += "<i class='fa-solid fa-magnifying-glass'></i>";
        html += "<input type='text' id='feSearch' placeholder='Filtrar ficheiros...' oninput='onSearchInput(this.value)'>";
        html += "</div>";

        html += "<div class='fe-nav-group'>";
        html += "<div class='fe-view-toggle'>";
        html += "<button id='viewGridBtn' onclick='setViewMode(\"grid\")' class='fe-view-btn active' title='Vista em Grelha'><i class='fa-solid fa-table-cells'></i></button>";
        html += "<button id='viewListBtn' onclick='setViewMode(\"list\")' class='fe-view-btn' title='Vista em Lista'><i class='fa-solid fa-list'></i></button>";
        html += "</div>";
        html += "<button onclick='createSdDir()' class='btn btn-secondary' style='padding:6px 12px;font-size:0.8rem;'><i class='fa-solid fa-folder-plus'></i> Nova Pasta</button>";
        html += "<button onclick='toggleUploadDrawer()' class='btn btn-primary' style='padding:6px 12px;font-size:0.8rem;'><i class='fa-solid fa-cloud-arrow-up'></i> Carregar</button>";
        html += "<button id='btnDeleteDir' onclick='deleteCurrentDir()' class='btn btn-danger' style='padding:6px 12px;font-size:0.8rem;display:none;' title='Eliminar esta pasta'><i class='fa-solid fa-trash-can'></i></button>";
        html += "</div>";
        html += "</div>";

        // Viewport Body
        html += "<div class='fe-body' id='feBody'>";
        html += "<div style='text-align:center;padding:40px;color:var(--muted);'><i class='fa-solid fa-rotate' style='animation:spin 1s linear infinite;'></i> A carregar explorador...</div>";
        html += "</div>";

        // Status Bar
        html += "<div class='fe-statusbar'>";
        html += "<div id='feStatusCount'>0 itens</div>";
        html += "<div id='feStatusStorage'>SD Card FAT32</div>";
        html += "</div>";
        html += "</div>";

        // Card 3: Gaveta de Upload com Drag & Drop (Expansível / Rápida)
        html += "<div class='card' id='uploadDrawer' style='display:none;'>";
        html += "<div class='card-title'><span><i class='fa-solid fa-cloud-arrow-up'></i> Carregar Ficheiros para o Cartão SD</span>";
        html += "<div class='btn-group'><span style='font-size:0.8rem;color:var(--muted);'>Destino: <strong id='uploadDestLabel' style='color:var(--primary);'>/</strong></span>";
        html += "<button onclick='toggleUploadDrawer()' class='btn btn-secondary' style='padding:3px 8px;font-size:0.75rem;'><i class='fa-solid fa-xmark'></i></button></div></div>";
        html += "<div id='sdDropzone' style='border:2px dashed #334155;border-radius:12px;padding:26px 16px;text-align:center;cursor:pointer;background:rgba(15,23,42,0.4);transition:all 0.2s;'>";
        html += "<div style='font-size:2.8rem;margin-bottom:6px;color:var(--primary);'><i class='fa-solid fa-cloud-arrow-up'></i></div>";
        html += "<div style='font-weight:600;font-size:1rem;color:var(--text);margin-bottom:4px;'>Arraste e solte ficheiros aqui</div>";
        html += "<div style='font-size:0.825rem;color:var(--muted);margin-bottom:12px;'>ou clique para escolher do computador (suporta múltiplos ficheiros)</div>";
        html += "<button type='button' class='btn btn-secondary' style='pointer-events:none;font-size:0.8rem;padding:6px 14px;'><i class='fa-solid fa-magnifying-glass'></i> Escolher Ficheiros</button>";
        html += "<input type='file' id='sdFileInput' multiple style='display:none;' onchange='handleSdFileSelect(this.files)'>";
        html += "</div>";
        html += "<div id='uploadProgressContainer' style='display:none;margin-top:14px;background:rgba(0,0,0,0.25);border:1px solid #1e293b;border-radius:8px;padding:12px 14px;'>";
        html += "<div style='display:flex;justify-content:space-between;align-items:center;margin-bottom:6px;'>";
        html += "<span id='uploadProgressStatus' style='font-size:0.85rem;font-weight:600;color:var(--primary);'>A preparar envio...</span>";
        html += "<span id='uploadProgressPercent' style='font-size:0.8rem;color:var(--muted);'>0%</span>";
        html += "</div>";
        html += "<div class='progress-bg' style='height:8px;margin-top:0;'><div id='uploadProgressBar' class='progress-bar' style='width:0%;background:var(--primary);'></div></div>";
        html += "<div id='uploadResultMsg' style='margin-top:8px;font-size:0.85rem;'></div>";
        html += "</div>";
        html += "</div>";

        // Card 4: Visualizador / Editor de Texto Em Linha
        html += "<div class='card' id='editorCard' style='display:none;'>";
        html += "<div class='card-title'><span id='editorTitle'><i class='fa-solid fa-file-pen'></i> Editor de Ficheiro</span>";
        html += "<div class='btn-group'>";
        html += "<button onclick='copyEditorContent()' class='btn btn-secondary' style='padding:6px 12px;' title='Copiar'><i class='fa-solid fa-copy'></i> Copiar</button>";
        html += "<button onclick='saveEditorFile()' class='btn btn-primary' style='padding:6px 12px;'><i class='fa-solid fa-floppy-disk'></i> Guardar</button>";
        html += "<button onclick='closeEditor()' class='btn btn-secondary' style='padding:6px 12px;'><i class='fa-solid fa-xmark'></i> Fechar</button>";
        html += "</div></div>";
        html += "<input type='hidden' id='editorFilePath'>";
        html += "<textarea id='editorText' rows='14' style='font-family:monospace;background:#030712;color:#67e8f9;border:1px solid #1e293b;border-radius:8px;line-height:1.45;'></textarea>";
        html += "</div>";

        // Script Moderno do Explorador de Ficheiros
        html += "<script>";
        html += "let currentDir = '/';";
        html += "let viewMode = localStorage.getItem('sd_view_mode') || 'grid';";
        html += "let rawFiles = [];";
        html += "let searchFilter = '';";
        html += "function formatBytes(b){if(b<1024)return b+' B';if(b<1048576)return(b/1024).toFixed(1)+' KB';return(b/1048576).toFixed(1)+' MB';}";
        html += "function getFileInfo(name, isDir){";
        html += "if(isDir) return {icon:'fa-solid fa-folder',color:'#f59e0b',type:'Pasta'};";
        html += "const ext = name.split('.').pop().toLowerCase();";
        html += "if(['woff2','woff','otf','ttf'].includes(ext)) return {icon:'fa-solid fa-font',color:'#f43f5e',type:'Fonte Web ('+ext.toUpperCase()+')'};";
        html += "if(['json','js','css','html','xml','cpp','h'].includes(ext)) return {icon:'fa-solid fa-file-code',color:'#06b6d4',type:'Código / Config ('+ext.toUpperCase()+')'};";
        html += "if(['csv','log','txt'].includes(ext)) return {icon:'fa-solid fa-file-lines',color:'#10b981',type:'Registo / Texto ('+ext.toUpperCase()+')'};";
        html += "if(['png','jpg','jpeg','svg','ico','bmp','gif'].includes(ext)) return {icon:'fa-solid fa-file-image',color:'#a855f7',type:'Imagem ('+ext.toUpperCase()+')'};";
        html += "if(['bin','hex','dat','zip','tar','gz'].includes(ext)) return {icon:'fa-solid fa-file-zipper',color:'#f97316',type:'Binário ('+ext.toUpperCase()+')'};";
        html += "return {icon:'fa-solid fa-file',color:'#94a3b8',type:'Ficheiro ('+ext.toUpperCase()+')'};";
        html += "}";
        html += "function renderBreadcrumbs(){";
        html += "const bc = document.getElementById('feBreadcrumbs');";
        html += "if(!bc) return;";
        html += "let h = '<span class=\"fe-crumb '+(currentDir==='/'?'active':'')+'\" onclick=\"changeDir(\\'/\\')\"><i class=\"fa-solid fa-house\"></i> Raiz</span>';";
        html += "if(currentDir !== '/'){";
        html += "const parts = currentDir.split('/').filter(p=>p.length>0);";
        html += "let accum = '';";
        html += "parts.forEach((p, idx)=>{";
        html += "accum += '/' + p;";
        html += "const isLast = (idx === parts.length - 1);";
        html += "h += '<span class=\"fe-crumb-sep\"><i class=\"fa-solid fa-arrow-right\" style=\"font-size:0.6rem;\"></i></span>';";
        html += "h += '<span class=\"fe-crumb '+(isLast?'active':'')+'\" onclick=\"changeDir(\\''+accum+'\\')\"><i class=\"fa-solid fa-folder\"></i> '+p+'</span>';";
        html += "});";
        html += "}";
        html += "bc.innerHTML = h;";
        html += "}";
        html += "function renderExplorer(){";
        html += "const body = document.getElementById('feBody');";
        html += "if(!body) return;";
        html += "renderBreadcrumbs();";
        html += "const udl = document.getElementById('uploadDestLabel'); if(udl) udl.innerText = currentDir;";
        html += "const btnDel = document.getElementById('btnDeleteDir'); if(btnDel) btnDel.style.display = (currentDir !== '/') ? 'inline-flex' : 'none';";
        html += "const filtered = rawFiles.filter(f=>{";
        html += "if(!searchFilter) return true;";
        html += "return f.name.toLowerCase().includes(searchFilter.toLowerCase());";
        html += "});";
        html += "const dirCount = filtered.filter(f=>f.is_dir).length;";
        html += "const fileCount = filtered.length - dirCount;";
        html += "const sc = document.getElementById('feStatusCount');";
        html += "if(sc) sc.innerText = filtered.length + ' itens (' + dirCount + ' pastas, ' + fileCount + ' ficheiros)';";
        html += "if(filtered.length === 0){";
        html += "if(searchFilter){";
        html += "body.innerHTML = '<div class=\"fe-empty\"><i class=\"fa-solid fa-filter\"></i><div style=\"font-weight:600;font-size:1rem;\">Nenhum resultado encontrado</div><div style=\"font-size:0.8rem;margin-top:4px;\">Não há ficheiros a corresponder a \"'+searchFilter+'\".</div></div>';";
        html += "} else {";
        html += "body.innerHTML = '<div class=\"fe-empty\"><i class=\"fa-solid fa-folder-open\"></i><div style=\"font-weight:600;font-size:1rem;\">Esta pasta está vazia</div><div style=\"font-size:0.8rem;margin-top:6px;\"><button onclick=\"toggleUploadDrawer()\" class=\"btn btn-primary\" style=\"padding:6px 12px;font-size:0.8rem;\"><i class=\"fa-solid fa-cloud-arrow-up\"></i> Carregar Ficheiros</button></div></div>';";
        html += "}";
        html += "return;";
        html += "}";
        html += "if(viewMode === 'grid'){";
        html += "let h = '<div class=\"fe-grid\">';";
        html += "filtered.forEach(f=>{";
        html += "const fullPath = f.path || (currentDir==='/' ? '/'+f.name : currentDir+'/'+f.name);";
        html += "const isDir = f.is_dir === true;";
        html += "const info = getFileInfo(f.name, isDir);";
        html += "h += '<div class=\"fe-card\" title=\"'+f.name+'\" onclick=\"onItemClick(event,\\''+fullPath+'\\','+isDir+')\">';";
        html += "h += '<div class=\"fe-card-icon\" style=\"color:'+info.color+';\"><i class=\"'+info.icon+'\"></i></div>';";
        html += "h += '<div class=\"fe-card-name\">'+f.name+'</div>';";
        html += "h += '<div class=\"fe-card-meta\">'+(isDir ? 'Pasta' : formatBytes(f.size))+'</div>';";
        html += "h += '<div class=\"fe-card-actions\" onclick=\"event.stopPropagation()\">';";
        html += "if(isDir){";
        html += "h += '<button onclick=\"changeDir(\\''+fullPath+'\\')\" class=\"fe-card-btn\" title=\"Abrir\"><i class=\"fa-solid fa-folder-open\"></i></button>';";
        html += "h += '<button onclick=\"deleteSdDir(\\''+fullPath+'\\')\" class=\"fe-card-btn del\" title=\"Eliminar\"><i class=\"fa-solid fa-trash-can\"></i></button>';";
        html += "} else {";
        html += "h += '<a href=\"/api/sd/download?file='+encodeURIComponent(fullPath)+'\" class=\"fe-card-btn\" title=\"Descarregar\"><i class=\"fa-solid fa-download\"></i></a>';";
        html += "h += '<button onclick=\"viewFile(\\''+fullPath+'\\')\" class=\"fe-card-btn\" title=\"Ver / Editar\"><i class=\"fa-solid fa-file-pen\"></i></button>';";
        html += "h += '<button onclick=\"deleteSdFile(\\''+fullPath+'\\')\" class=\"fe-card-btn del\" title=\"Eliminar\"><i class=\"fa-solid fa-trash-can\"></i></button>';";
        html += "}";
        html += "h += '</div></div>';";
        html += "});";
        html += "h += '</div>';";
        html += "body.innerHTML = h;";
        html += "} else {";
        html += "let h = '<table class=\"fe-table\"><thead><tr><th>Nome</th><th>Tipo</th><th>Tamanho</th><th style=\"text-align:right;\">Ações</th></tr></thead><tbody>';";
        html += "filtered.forEach(f=>{";
        html += "const fullPath = f.path || (currentDir==='/' ? '/'+f.name : currentDir+'/'+f.name);";
        html += "const isDir = f.is_dir === true;";
        html += "const info = getFileInfo(f.name, isDir);";
        html += "h += '<tr onclick=\"onItemClick(event,\\''+fullPath+'\\','+isDir+')\">';";
        html += "h += '<td><i class=\"'+info.icon+'\" style=\"color:'+info.color+';margin-right:8px;font-size:1.1rem;\"></i> <strong>'+f.name+'</strong></td>';";
        html += "h += '<td style=\"color:var(--muted);\">'+info.type+'</td>';";
        html += "h += '<td>'+(isDir ? '-' : formatBytes(f.size))+'</td>';";
        html += "h += '<td style=\"text-align:right;\" onclick=\"event.stopPropagation()\"><div class=\"btn-group\" style=\"justify-content:flex-end;\">';";
        html += "if(isDir){";
        html += "h += '<button onclick=\"changeDir(\\''+fullPath+'\\')\" class=\"btn btn-secondary\" style=\"padding:4px 8px;font-size:0.75rem;\"><i class=\"fa-solid fa-folder-open\"></i> Abrir</button>';";
        html += "h += '<button onclick=\"deleteSdDir(\\''+fullPath+'\\')\" class=\"btn btn-danger\" style=\"padding:4px 8px;font-size:0.75rem;\"><i class=\"fa-solid fa-trash-can\"></i></button>';";
        html += "} else {";
        html += "h += '<a href=\"/api/sd/download?file='+encodeURIComponent(fullPath)+'\" class=\"btn btn-primary\" style=\"padding:4px 8px;font-size:0.75rem;\" title=\"Descarregar\"><i class=\"fa-solid fa-download\"></i></a>';";
        html += "h += '<button onclick=\"viewFile(\\''+fullPath+'\\')\" class=\"btn btn-secondary\" style=\"padding:4px 8px;font-size:0.75rem;\" title=\"Editar\"><i class=\"fa-solid fa-file-pen\"></i></button>';";
        html += "h += '<button onclick=\"deleteSdFile(\\''+fullPath+'\\')\" class=\"btn btn-danger\" style=\"padding:4px 8px;font-size:0.75rem;\" title=\"Eliminar\"><i class=\"fa-solid fa-trash-can\"></i></button>';";
        html += "}";
        html += "h += '</div></td></tr>';";
        html += "});";
        html += "h += '</tbody></table>';";
        html += "body.innerHTML = h;";
        html += "}";
        html += "}";
        html += "function onItemClick(e, path, isDir){";
        html += "if(isDir){ changeDir(path); }";
        html += "else {";
        html += "const ext = path.split('.').pop().toLowerCase();";
        html += "if(['json','txt','log','csv','css','js','html','xml','cpp','h'].includes(ext)){ viewFile(path); }";
        html += "else { window.location.href = '/api/sd/download?file=' + encodeURIComponent(path); }";
        html += "}";
        html += "}";
        html += "function setViewMode(mode){";
        html += "viewMode = mode;";
        html += "localStorage.setItem('sd_view_mode', mode);";
        html += "const gb = document.getElementById('viewGridBtn');";
        html += "const lb = document.getElementById('viewListBtn');";
        html += "if(gb) gb.className = 'fe-view-btn ' + (mode==='grid'?'active':'');";
        html += "if(lb) lb.className = 'fe-view-btn ' + (mode==='list'?'active':'');";
        html += "renderExplorer();";
        html += "}";
        html += "function onSearchInput(val){ searchFilter = val.trim(); renderExplorer(); }";
        html += "function loadFileList(dir){";
        html += "if(dir !== undefined) currentDir = dir;";
        html += "renderBreadcrumbs();";
        html += "const body = document.getElementById('feBody');";
        html += "if(body) body.innerHTML = '<div style=\"text-align:center;padding:40px;color:var(--muted);\"><i class=\"fa-solid fa-rotate\" style=\"animation:spin 1s linear infinite;\"></i> A carregar pasta '+currentDir+'...</div>';";
        html += "fetch('/api/sd/list?dir=' + encodeURIComponent(currentDir)).then(r=>r.json()).then(data=>{";
        html += "if(!data.ready){";
        html += "if(body) body.innerHTML = '<div class=\"fe-empty\"><i class=\"fa-solid fa-triangle-exclamation\" style=\"color:var(--red);\"></i><div style=\"color:var(--red);font-weight:600;\">Cartão SD não está pronto ou não foi detetado.</div></div>';";
        html += "return;";
        html += "}";
        html += "if(data.total_mb !== undefined && document.getElementById('statTotalMb')) document.getElementById('statTotalMb').innerText = data.total_mb + ' MB';";
        html += "if(data.used_mb !== undefined && document.getElementById('statUsedMb')) document.getElementById('statUsedMb').innerText = data.used_mb + ' MB';";
        html += "if(data.free_mb !== undefined && document.getElementById('statFreeMb')) document.getElementById('statFreeMb').innerText = data.free_mb + ' MB';";
        html += "if(data.total_mb > 0 && data.used_mb !== undefined){";
        html += "const p = Math.min(100, Math.round((data.used_mb / data.total_mb) * 100));";
        html += "if(document.getElementById('statUsedPct')) document.getElementById('statUsedPct').innerText = p + '%';";
        html += "if(document.getElementById('statProgressBar')) document.getElementById('statProgressBar').style.width = p + '%';";
        html += "}";
        html += "const ss = document.getElementById('feStatusStorage');";
        html += "if(ss && data.free_mb !== undefined) ss.innerText = data.free_mb + ' MB livres de ' + data.total_mb + ' MB';";
        html += "rawFiles = data.files || [];";
        html += "rawFiles.sort((a,b)=>{ if(a.is_dir !== b.is_dir) return a.is_dir ? -1 : 1; return a.name.localeCompare(b.name); });";
        html += "renderExplorer();";
        html += "}).catch(e=>{";
        html += "if(body) body.innerHTML = '<div class=\"fe-empty\"><i class=\"fa-solid fa-triangle-exclamation\" style=\"color:var(--red);\"></i><div style=\"color:var(--red);\">Erro ao carregar ficheiros: '+e+'</div></div>';";
        html += "});";
        html += "}";
        html += "function changeDir(d){ currentDir = d; searchFilter = ''; const si = document.getElementById('feSearch'); if(si) si.value = ''; loadFileList(); }";
        html += "function goUpDir(){";
        html += "if(currentDir === '/') return;";
        html += "const parts = currentDir.split('/').filter(p=>p.length>0);";
        html += "parts.pop();";
        html += "const parent = parts.length === 0 ? '/' : '/' + parts.join('/');";
        html += "changeDir(parent);";
        html += "}";
        html += "function toggleUploadDrawer(){";
        html += "const d = document.getElementById('uploadDrawer');";
        html += "if(d){ d.style.display = (d.style.display==='none'||!d.style.display) ? 'block' : 'none'; if(d.style.display==='block') d.scrollIntoView({behavior:'smooth'}); }";
        html += "}";
        html += "function viewFile(path){";
        html += "fetch('/api/sd/read?file=' + encodeURIComponent(path)).then(r=>{ if(!r.ok) throw new Error('Falha ao ler'); return r.text(); }).then(txt=>{";
        html += "document.getElementById('editorFilePath').value = path;";
        html += "document.getElementById('editorTitle').innerHTML = '<i class=\"fa-solid fa-file-pen\"></i> ' + path;";
        html += "document.getElementById('editorText').value = txt;";
        html += "document.getElementById('editorCard').style.display = 'block';";
        html += "document.getElementById('editorCard').scrollIntoView({behavior:'smooth'});";
        html += "}).catch(e=>alert('Erro ao abrir ficheiro: ' + e));";
        html += "}";
        html += "function copyEditorContent(){";
        html += "const t = document.getElementById('editorText');";
        html += "if(t){ navigator.clipboard.writeText(t.value).then(()=>alert('Conteúdo copiado para a área de transferência!')); }";
        html += "}";
        html += "function saveEditorFile(){";
        html += "const p = document.getElementById('editorFilePath').value;";
        html += "const c = document.getElementById('editorText').value;";
        html += "fetch('/api/sd/save', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({file:p, content:c})})";
        html += ".then(r=>r.json()).then(res=>{ alert(res.msg); loadFileList(); });";
        html += "}";
        html += "function closeEditor(){ document.getElementById('editorCard').style.display = 'none'; }";
        html += "function deleteSdFile(path){";
        html += "if(confirm('Eliminar o ficheiro \"' + path + '\" do cartão SD?')){";
        html += "fetch('/api/sd/delete', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({file:path, path:path})})";
        html += ".then(r=>r.json()).then(res=>{ alert(res.msg); loadFileList(); });";
        html += "}}";
        html += "function deleteSdDir(path){";
        html += "if(confirm('ATENÇÃO: Deseja realmente eliminar o diretório \"' + path + '\" e todo o seu conteúdo do cartão SD?')){";
        html += "fetch('/api/sd/delete_dir', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({path:path, dir:path})})";
        html += ".then(r=>r.json()).then(res=>{";
        html += "alert(res.msg);";
        html += "if(res.success){ if(currentDir === path || currentDir.startsWith(path + '/')){ goUpDir(); } else { loadFileList(); } }";
        html += "}).catch(e=>alert('Erro ao eliminar diretório: ' + e));";
        html += "}}";
        html += "function deleteCurrentDir(){ if(!currentDir || currentDir === '/'){ alert('Não é permitido apagar o diretório raiz.'); return; } deleteSdDir(currentDir); }";
        html += "function createSdDir(){";
        html += "const name = prompt('Nome da nova pasta a criar em ' + currentDir + ':');";
        html += "if(!name || !name.trim()) return;";
        html += "const cleanName = name.trim().replace(/[^a-zA-Z0-9_\\-\\.]/g, '_');";
        html += "const newPath = currentDir === '/' ? ('/' + cleanName) : (currentDir + '/' + cleanName);";
        html += "fetch('/api/sd/mkdir', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({path:newPath})})";
        html += ".then(r=>r.json()).then(res=>{ alert(res.msg); loadFileList(); }).catch(e=>alert('Erro ao criar pasta: ' + e));";
        html += "}";
        html += "let uploadQueue = []; let isUploading = false; let uploadSuccessCount = 0; let uploadTotalCount = 0;";
        html += "function handleSdFileSelect(files){";
        html += "if(!files || files.length === 0) return;";
        html += "for(let i=0; i<files.length; i++){ uploadQueue.push(files[i]); }";
        html += "uploadTotalCount = uploadQueue.length; uploadSuccessCount = 0;";
        html += "const d = document.getElementById('uploadDrawer'); if(d) d.style.display = 'block';";
        html += "if(!isUploading){ processUploadQueue(); }";
        html += "}";
        html += "function processUploadQueue(){";
        html += "const prog = document.getElementById('uploadProgressContainer');";
        html += "const statusEl = document.getElementById('uploadProgressStatus');";
        html += "const barEl = document.getElementById('uploadProgressBar');";
        html += "const pctEl = document.getElementById('uploadProgressPercent');";
        html += "const resEl = document.getElementById('uploadResultMsg');";
        html += "if(uploadQueue.length === 0){";
        html += "isUploading = false;";
        html += "if(barEl) barEl.style.width = '100%';";
        html += "if(pctEl) pctEl.innerText = '100%';";
        html += "if(statusEl) statusEl.innerText = 'Envio concluído!';";
        html += "if(resEl) resEl.innerHTML = '<span style=\"color:var(--green);font-weight:600;\"><i class=\"fa-solid fa-circle-check\"></i> ' + uploadSuccessCount + ' de ' + uploadTotalCount + ' ficheiro(s) enviado(s) com sucesso para ' + currentDir + '!</span>';";
        html += "const fi = document.getElementById('sdFileInput'); if(fi) fi.value = '';";
        html += "loadFileList(currentDir);";
        html += "return;";
        html += "}";
        html += "isUploading = true;";
        html += "const file = uploadQueue.shift();";
        html += "const currentIdx = uploadTotalCount - uploadQueue.length;";
        html += "if(prog) prog.style.display = 'block';";
        html += "if(resEl) resEl.innerHTML = '';";
        html += "if(statusEl) statusEl.innerText = 'A enviar (' + currentIdx + '/' + uploadTotalCount + '): ' + file.name + ' (' + formatBytes(file.size) + ')...';";
        html += "if(barEl) barEl.style.width = '0%';";
        html += "if(pctEl) pctEl.innerText = '0%';";
        html += "const fd = new FormData();";
        html += "fd.append('upload', file, file.name);";
        html += "const xhr = new XMLHttpRequest();";
        html += "xhr.open('POST', '/api/sd/upload?ajax=1&dir=' + encodeURIComponent(currentDir), true);";
        html += "xhr.upload.onprogress = function(e){";
        html += "if(e.lengthComputable){";
        html += "const p = Math.round((e.loaded / e.total) * 100);";
        html += "if(barEl) barEl.style.width = p + '%';";
        html += "if(pctEl) pctEl.innerText = p + '% (' + formatBytes(e.loaded) + ' / ' + formatBytes(e.total) + ')';";
        html += "}};";
        html += "xhr.onload = function(){";
        html += "if(xhr.status >= 200 && xhr.status < 400){ uploadSuccessCount++; }";
        html += "else { if(resEl) resEl.innerHTML += '<div style=\"color:var(--red);\"><i class=\"fa-solid fa-circle-xmark\"></i> Falha ao enviar ' + file.name + ' (código ' + xhr.status + ')</div>'; }";
        html += "processUploadQueue();";
        html += "};";
        html += "xhr.onerror = function(){";
        html += "if(resEl) resEl.innerHTML += '<div style=\"color:var(--red);\"><i class=\"fa-solid fa-circle-xmark\"></i> Erro de rede ao enviar ' + file.name + '</div>';";
        html += "processUploadQueue();";
        html += "};";
        html += "xhr.send(fd);";
        html += "}";
        html += "const dz = document.getElementById('sdDropzone');";
        html += "if(dz){";
        html += "dz.onclick = function(){ const fi = document.getElementById('sdFileInput'); if(fi) fi.click(); };";
        html += "['dragenter','dragover'].forEach(evt=>{ dz.addEventListener(evt, e=>{ e.preventDefault(); e.stopPropagation(); dz.style.borderColor='var(--primary)'; dz.style.background='rgba(6,182,212,0.12)'; }, false); });";
        html += "['dragleave','drop'].forEach(evt=>{ dz.addEventListener(evt, e=>{ e.preventDefault(); e.stopPropagation(); dz.style.borderColor='#334155'; dz.style.background='rgba(15,23,42,0.4)'; }, false); });";
        html += "dz.addEventListener('drop', e=>{ const dt = e.dataTransfer; if(dt && dt.files && dt.files.length > 0){ handleSdFileSelect(dt.files); } }, false);";
        html += "}";
        html += "const win = document.getElementById('explorerWindow');";
        html += "const overlay = document.getElementById('feDropOverlay');";
        html += "if(win && overlay){";
        html += "let dragCounter = 0;";
        html += "win.addEventListener('dragenter', e=>{ e.preventDefault(); dragCounter++; overlay.className = 'fe-drop-overlay active'; }, false);";
        html += "win.addEventListener('dragover', e=>{ e.preventDefault(); }, false);";
        html += "win.addEventListener('dragleave', e=>{ e.preventDefault(); dragCounter--; if(dragCounter <= 0){ overlay.className = 'fe-drop-overlay'; dragCounter = 0; } }, false);";
        html += "win.addEventListener('drop', e=>{ e.preventDefault(); dragCounter = 0; overlay.className = 'fe-drop-overlay'; const dt = e.dataTransfer; if(dt && dt.files && dt.files.length > 0){ handleSdFileSelect(dt.files); } }, false);";
        html += "}";
        html += "window.addEventListener('dragover', e=>{ e.preventDefault(); }, false);";
        html += "window.addEventListener('drop', e=>{ e.preventDefault(); }, false);";
        html += "setViewMode(viewMode);";
        html += "loadFileList();";
        html += "</script>";

        html += getHtmlFooter();
        _server.send(200, "text/html", html);
    }

    // -------------------------------------------------------------
    // REST APIS
    // -------------------------------------------------------------
    void handleApiStatus() {
        _battery.update();
        JsonDocument doc;

        doc["rover_id"] = _roverId;

        JsonObject g = doc["gps"].to<JsonObject>();
        g["lat"] = _gps.getLatitude();
        g["lng"] = _gps.getLongitude();
        g["speed_kn"] = _gps.getSpeedKnots();
        g["heading"] = _gps.getHeading();
        g["satellites"] = _gps.getSatellites();
        g["fix"] = _gps.hasFix();
        g["chars_processed"] = _gps.getCharsProcessed();
        g["hw_locked"] = _gps.isHardwareLocked();
        g["rx_pin"] = _gps.getRxPin();
        g["tx_pin"] = _gps.getTxPin();
        g["baud"] = _gps.getBaud();
        g["bytes"] = _gps.getTotalBytes();
        g["raw"] = _gps.getRawSentence();
        g["rate_hz"] = _gps.getRateHz();

        JsonObject b = doc["battery"].to<JsonObject>();
        b["pct"] = _battery.getPercentage();
        b["voltage"] = _battery.getVoltageMv() / 1000.0f;
        b["cell_avg"] = _battery.getCellAverageV();
        b["cells"] = _battery.getCellCount();
        b["capacity_mah"] = _battery.getCapacityMah();
        b["remaining_mah"] = _battery.getRemainingMah();
        b["autonomy_hrs"] = _battery.getEstimatedHoursRemaining();
        b["current_a"] = _battery.getCurrentAmps();
        b["consumed_mah"] = _battery.getConsumedMah();

        JsonObject a = doc["actuators"].to<JsonObject>();
        a["throttle"] = _actuators.getThrottle();
        a["rudder"] = _actuators.getRudder();
        a["motor_status"] = _motorStatus;
        a["motor_status_str"] = get_motor_status_str(_motorStatus);
        a["anchor_status"] = _actuators.getAnchorStatus();
        a["anchor_status_str"] = get_anchor_status_str(_actuators.getAnchorStatus());
        a["anchor_depth_m"] = _actuators.getAnchorDepthCm() / 100.0f;
        a["anchor_depth_cm"] = _actuators.getAnchorDepthCm();
        a["anchor_pulses"] = _actuators.getAnchorHallPulses();
        a["anchor_revs"] = _actuators.getAnchorRevolutions();
        a["anchor_drum_diam_mm"] = _config.getAnchorDrumDiamMm();
        a["anchor_pulses_per_rev"] = _config.getAnchorPulsesPerRev();
        a["anchor_limit"] = _actuators.isAnchorAtTopLimit();
        a["rack_released"] = _actuators.isRackReleased();
        a["rack_angle"] = _actuators.getCurrentRackAngle();
        a["rack_angle_engaged"] = _config.getRackAngleEngaged();
        a["rack_angle_released"] = _config.getRackAngleReleased();
        a["rack_inverted"] = _config.isRackInverted();
        a["rack_pulse_us"] = _actuators.getCurrentRackPulseUs();
        a["alarm_active"] = _actuators.isAlarmActive();
        a["winch_pulse_us"] = _actuators.getWinchPulseUs();
        a["winch_neutral_us"] = _actuators.getWinchNeutral();

        JsonObject w = doc["wifi"].to<JsonObject>();
        w["is_ap"] = _wifi.isAPMode();
        w["ssid"] = _wifi.getSSID();
        w["ip"] = _wifi.getIPAddress();
        w["rssi"] = _wifi.getRSSI();

        JsonObject sd = doc["sd"].to<JsonObject>();
        sd["ready"] = _sd.isReady();
        sd["card_type"] = _sd.getCardTypeStr();

        JsonObject lObj = doc["lora"].to<JsonObject>();
        lObj["hw_ready"] = _loraDiag ? _loraDiag->hwReady : true;
        lObj["role"] = "rover";
        lObj["freq_mhz"] = LORA_FREQUENCY;
        lObj["bw_khz"] = LORA_BANDWIDTH;
        lObj["sf"] = LORA_SPREAD_FACTOR;
        lObj["cr"] = "4/5";
        lObj["power_dbm"] = LORA_OUTPUT_POWER;
        lObj["tx_telemetry"] = _loraDiag ? _loraDiag->txTelems : 0;
        lObj["rx_polls"] = _loraDiag ? _loraDiag->rxPolls : 0;
        lObj["last_comm_seconds_ago"] = _loraDiag ? _loraDiag->lastRxSecAgo : 999999;
        lObj["has_contact"] = _loraDiag ? _loraDiag->hasContact : false;
        lObj["rssi"] = _loraDiag ? _loraDiag->rssi : 0.0f;
        lObj["snr"] = _loraDiag ? _loraDiag->snr : 0.0f;

        JsonObject v = doc["version"].to<JsonObject>();
        v["lafvin"] = ROVER_LAFVIN_VERSION;
        v["c6"] = _c6Version;
        v["bundle"] = ROVER_FIRMWARE_VERSION_STR;
        v["build_date"] = __DATE__;

        String res;
        serializeJson(doc, res);
        _server.send(200, "application/json", res);
    }

    void handleApiGpsRate() {
        uint8_t rateHz = 1;
        bool saveFlash = true;

        if (_server.hasArg("plain")) {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, _server.arg("plain"));
            if (!err) {
                rateHz = doc["rate_hz"] | doc["hz"] | 1;
                saveFlash = doc["save_flash"] | true;
            }
        } else if (_server.hasArg("rate_hz") || _server.hasArg("hz")) {
            rateHz = _server.hasArg("rate_hz") ? _server.arg("rate_hz").toInt() : _server.arg("hz").toInt();
            if (_server.hasArg("save_flash")) {
                saveFlash = (_server.arg("save_flash") == "1" || _server.arg("save_flash") == "true");
            }
        }

        if (rateHz != 1 && rateHz != 2 && rateHz != 5 && rateHz != 10) {
            _server.send(400, "application/json", "{\"error\":\"Taxa inválida. Use 1, 2, 5 ou 10 Hz\"}");
            return;
        }

        bool ok = _gps.setUpdateRate(rateHz, saveFlash);
        if (_display) {
            _display->bootLogf(UI_CYAN, 50, "[GPS] Taxa: %d Hz %s", rateHz, saveFlash ? "(gravado)" : "");
        }

        JsonDocument res;
        res["success"] = ok;
        res["rate_hz"] = rateHz;
        res["interval_ms"] = 1000 / rateHz;
        res["saved_flash"] = saveFlash;
        res["msg"] = String("Taxa do GPS Quectel LC29H atualizada para ") + rateHz + " Hz" + (saveFlash ? " e gravada na Flash!" : "!");

        String out;
        serializeJson(res, out);
        _server.send(200, "application/json", out);
    }

    void handleApiGpsCommand() {
        String cmd = "";
        if (_server.hasArg("plain")) {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, _server.arg("plain"));
            if (!err) {
                cmd = doc["cmd"].as<String>();
            }
        } else if (_server.hasArg("cmd")) {
            cmd = _server.arg("cmd");
        }

        if (cmd.length() == 0) {
            _server.send(400, "application/json", "{\"error\":\"Comando vazio\"}");
            return;
        }

        bool ok = _gps.sendNmeaCommand(cmd.c_str());
        JsonDocument res;
        res["success"] = ok;
        res["cmd"] = cmd;
        res["msg"] = ok ? "Instrução enviada ao Quectel LC29H com sucesso!" : "Erro ao enviar instrução.";

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
            item["auth"] = WiFi.encryptionType(i);
        }

        String res;
        serializeJson(doc, res);
        _server.send(200, "application/json", res);
    }

    void handleApiRoverConfigGet() {
        JsonDocument doc;
        doc["rover_id"] = _config.getRoverId();
        doc["ap_ssid"] = _config.getApSsid();
        doc["ap_password"] = _config.getApPassword();
        doc["screen_brightness"] = _config.getScreenBrightness();
        doc["battery_cells"] = _config.getBatteryCells();
        doc["battery_capacity_mah"] = _config.getBatteryCapacityMah();
        doc["rack_angle_engaged"] = _config.getRackAngleEngaged();
        doc["rack_angle_released"] = _config.getRackAngleReleased();
        doc["rack_inverted"] = _config.isRackInverted();
        doc["anchor_drum_diam_mm"] = _config.getAnchorDrumDiamMm();
        doc["anchor_pulses_per_rev"] = _config.getAnchorPulsesPerRev();
        doc["sd_ready"] = _sd.isReady();
        doc["has_config_file"] = (_sd.isReady() && _sd.fileExists("/config.json"));
        doc["version_lafvin"] = ROVER_LAFVIN_VERSION;
        doc["version_c6"] = _c6Version;
        doc["version_bundle"] = ROVER_FIRMWARE_VERSION_STR;
        doc["build_date"] = __DATE__;
        String out;
        serializeJson(doc, out);
        _server.send(200, "application/json", out);
    }

    void handleApiRoverConfigSave() {
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
        uint8_t id = doc["rover_id"] | _config.getRoverId();
        String ssid = doc["ap_ssid"] | _config.getApSsid();
        String pass = doc["ap_password"] | _config.getApPassword();
        uint8_t bright = doc["screen_brightness"] | doc["brightness"] | _config.getScreenBrightness();
        if (bright > 100) bright = 100;
        uint8_t cells = doc["battery_cells"] | doc["cells"] | _config.getBatteryCells();
        if (cells != 3 && cells != 4 && cells != 6) cells = _config.getBatteryCells();
        uint16_t cap = doc["battery_capacity_mah"] | doc["capacity_mah"] | doc["capacity"] | _config.getBatteryCapacityMah();

        _config.updateConfig(id, ssid, pass, bright, cells, cap);
        if (doc["rack_angle_engaged"].is<int>() || doc["rack_angle_released"].is<int>() || doc["rack_inverted"].is<bool>()) {
            uint8_t rEng = doc["rack_angle_engaged"] | _config.getRackAngleEngaged();
            uint8_t rRel = doc["rack_angle_released"] | _config.getRackAngleReleased();
            bool rInv = doc["rack_inverted"].is<bool>() ? doc["rack_inverted"].as<bool>() : _config.isRackInverted();
            if (rEng > 180) rEng = 180;
            if (rRel > 180) rRel = 180;
            _config.setRackConfig(rEng, rRel, rInv);
            _actuators.setRackConfig(rEng, rRel, rInv, true);
            if (_commandQueueCb) {
                char rCmd[48];
                snprintf(rCmd, sizeof(rCmd), "CMD:RACK_CFG:%u:%u:%d", rEng, rRel, rInv ? 1 : 0);
                _commandQueueCb(rCmd);
            }
        }
        if (doc["anchor_drum_diam_mm"].is<float>() || doc["anchor_drum_diam_mm"].is<int>() || doc["anchor_pulses_per_rev"].is<int>()) {
            float diam = doc["anchor_drum_diam_mm"] | _config.getAnchorDrumDiamMm();
            uint8_t pulses = doc["anchor_pulses_per_rev"] | _config.getAnchorPulsesPerRev();
            _config.setAnchorCalibration(diam, pulses);
            _actuators.setAnchorCalibration(diam, pulses);
            if (_commandQueueCb) {
                char aCmd[32];
                snprintf(aCmd, sizeof(aCmd), "CMD:ANC_CFG:%.1f:%d", diam, pulses);
                _commandQueueCb(aCmd);
            }
        }
        _roverId = _config.getRoverId();
        _battery.setCellCount(cells);
        if (cap >= 500 && cap <= 50000) {
            _battery.setCapacityMah(cap);
        }

        if (_commandQueueCb) {
            String cmd = "CMD:BATT:" + String(cells);
            _commandQueueCb(cmd.c_str());
        }

        if (_display) {
            _display->setBrightness(bright);
        }

        JsonDocument res;
        res["success"] = true;
        res["msg"] = "Configuração do Rover guardada na NVS e no Cartão SD (/config.json)!";
        res["rover_id"] = _roverId;
        res["ap_ssid"] = _config.getApSsid();
        res["screen_brightness"] = _config.getScreenBrightness();
        res["battery_cells"] = _config.getBatteryCells();
        res["battery_capacity_mah"] = _config.getBatteryCapacityMah();

        String out;
        serializeJson(res, out);
        _server.send(200, "application/json", out);
    }

    void handleApiBatteryConfigGet() {
        JsonDocument doc;
        uint8_t c = _battery.getCellCount();
        doc["cells"] = c;
        doc["capacity_mah"] = _battery.getCapacityMah();
        doc["remaining_mah"] = _battery.getRemainingMah();
        doc["autonomy_hrs"] = _battery.getEstimatedHoursRemaining();
        doc["voltage_min"] = (c * 3300) / 1000.0f;
        doc["voltage_max"] = (c * 4200) / 1000.0f;
        doc["voltage_nom"] = (c * 3700) / 1000.0f;
        String res;
        serializeJson(doc, res);
        _server.send(200, "application/json", res);
    }

    void handleApiBatteryConfigSet() {
        uint8_t cells = 0;
        uint16_t cap = 0;
        bool reset = false;

        if (_server.hasArg("cells")) cells = _server.arg("cells").toInt();
        if (_server.hasArg("capacity")) cap = _server.arg("capacity").toInt();
        if (_server.hasArg("capacity_mah")) cap = _server.arg("capacity_mah").toInt();
        if (_server.hasArg("reset")) reset = (_server.arg("reset") == "1" || _server.arg("reset") == "true");

        if (_server.hasArg("plain")) {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, _server.arg("plain"));
            if (!err) {
                if (doc["cells"].is<int>()) cells = doc["cells"].as<uint8_t>();
                if (doc["capacity_mah"].is<int>()) cap = doc["capacity_mah"].as<uint16_t>();
                else if (doc["capacity"].is<int>()) cap = doc["capacity"].as<uint16_t>();
                if (doc["reset"].is<bool>()) reset = doc["reset"].as<bool>();
            }
        }

        bool changed = false;
        if (cells == 3 || cells == 4 || cells == 6) {
            _battery.setCellCount(cells);
            _config.setBatteryCells(cells);
            changed = true;
            if (_commandQueueCb) {
                String cmd = "CMD:BATT:" + String(cells);
                _commandQueueCb(cmd.c_str());
            }
        }

        if (cap >= 500 && cap <= 50000) {
            _battery.setCapacityMah(cap);
            _config.setBatteryCapacityMah(cap);
            changed = true;
        }

        if (reset) {
            _battery.resetConsumed();
        }

        if (changed) {
            _config.saveConfig();
        }

        JsonDocument res;
        res["success"] = true;
        res["cells"] = _battery.getCellCount();
        res["capacity_mah"] = _battery.getCapacityMah();
        res["remaining_mah"] = _battery.getRemainingMah();
        res["autonomy_hrs"] = _battery.getEstimatedHoursRemaining();
        res["msg"] = "Configuração da Bateria atualizada com sucesso!";
        String out;
        serializeJson(res, out);
        _server.send(200, "application/json", out);
    }

    void handleApiBatteryReset() {
        _battery.resetConsumed();
        JsonDocument res;
        res["success"] = true;
        res["msg"] = "Contador de consumo acumulado reposto a zero!";
        res["remaining_mah"] = _battery.getRemainingMah();
        res["capacity_mah"] = _battery.getCapacityMah();
        String out;
        serializeJson(res, out);
        _server.send(200, "application/json", out);
    }

    void handleApiRoverConfigReload() {
        _config.loadConfig();
        if (_display) {
            _display->setBrightness(_config.getScreenBrightness());
        }
        _roverId = _config.getRoverId();
        _battery.setCellCount(_config.getBatteryCells());
        _battery.setCapacityMah(_config.getBatteryCapacityMah());
        _actuators.setRackConfig(_config.getRackAngleEngaged(), _config.getRackAngleReleased(), _config.isRackInverted(), true);
        if (_commandQueueCb) {
            String cmd = "CMD:BATT:" + String(_config.getBatteryCells());
            _commandQueueCb(cmd.c_str());
            char rCmd[48];
            snprintf(rCmd, sizeof(rCmd), "CMD:RACK_CFG:%u:%u:%d",
                     _config.getRackAngleEngaged(), _config.getRackAngleReleased(), _config.isRackInverted() ? 1 : 0);
            _commandQueueCb(rCmd);
        }

        JsonDocument res;
        res["success"] = true;
        res["msg"] = "Configurações recarregadas com sucesso do ficheiro /config.json (ou NVS)!";
        res["rover_id"] = _roverId;
        res["ap_ssid"] = _config.getApSsid();
        res["screen_brightness"] = _config.getScreenBrightness();
        res["battery_cells"] = _config.getBatteryCells();

        String out;
        serializeJson(res, out);
        _server.send(200, "application/json", out);
    }

    void handleApiDisplayBrightness() {
        uint8_t val = _config.getScreenBrightness();
        if (_server.hasArg("val")) {
            val = _server.arg("val").toInt();
        } else if (_server.hasArg("plain")) {
            JsonDocument doc;
            deserializeJson(doc, _server.arg("plain"));
            val = doc["screen_brightness"] | doc["brightness"] | val;
        }
        if (val > 100) val = 100;

        if (_display) {
            _display->setBrightness(val);
        }
        _config.setScreenBrightness(val);
        _config.saveConfig();

        JsonDocument res;
        res["success"] = true;
        res["screen_brightness"] = val;
        res["msg"] = "Brilho do ecrã ajustado para " + String(val) + "%";
        String out;
        serializeJson(res, out);
        _server.send(200, "application/json", out);
    }

    void handleApiWifiSave() {
        if (!_server.hasArg("plain")) {
            _server.send(400, "application/json", "{\"error\":\"Missing body\"}");
            return;
        }

        JsonDocument doc;
        deserializeJson(doc, _server.arg("plain"));

        String ssid = doc["ssid"] | "";
        String pass = doc["password"] | "";
        bool isDefault = doc["is_default"] | false;

        if (ssid.length() == 0) {
            _server.send(400, "application/json", "{\"error\":\"SSID vazio\"}");
            return;
        }

        _wifi.addOrUpdateNetwork(ssid, pass, isDefault);

        JsonDocument resDoc;
        resDoc["success"] = true;
        resDoc["msg"] = "Rede guardada na NVS e no Cartão SD (/wifi_networks.json)!";
        String res;
        serializeJson(resDoc, res);
        _server.send(200, "application/json", res);
    }

    void handleApiWifiSetDefault() {
        if (!_server.hasArg("plain")) {
            _server.send(400, "application/json", "{\"error\":\"Missing body\"}");
            return;
        }

        JsonDocument doc;
        deserializeJson(doc, _server.arg("plain"));
        String ssid = doc["ssid"] | "";

        bool ok = _wifi.setDefault(ssid);
        JsonDocument resDoc;
        resDoc["success"] = ok;
        resDoc["msg"] = ok ? "Rede padrão definida e sincronizada com o SD!" : "Rede não encontrada";
        String res;
        serializeJson(resDoc, res);
        _server.send(200, "application/json", res);
    }

    void handleApiWifiDelete() {
        if (!_server.hasArg("plain")) {
            _server.send(400, "application/json", "{\"error\":\"Missing body\"}");
            return;
        }

        JsonDocument doc;
        deserializeJson(doc, _server.arg("plain"));
        String ssid = doc["ssid"] | "";

        bool ok = _wifi.removeNetwork(ssid);
        JsonDocument resDoc;
        resDoc["success"] = ok;
        resDoc["msg"] = ok ? "Rede eliminada da NVS e do Cartão SD!" : "Rede não encontrada";
        String res;
        serializeJson(resDoc, res);
        _server.send(200, "application/json", res);
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

        JsonDocument resDoc;
        resDoc["success"] = true;
        resDoc["msg"] = "A tentar ligar à rede '" + ssid + "'... Verifique o visor LCD do Rover para o novo IP.";
        String res;
        serializeJson(resDoc, res);
        _server.send(200, "application/json", res);

        delay(400);
        bool ok = _wifi.connectTo(ssid, 10000);
        if (!ok) {
            Serial.println("[ROVER] Ligacao manual falhou. A restaurar Ponto de Acesso...");
            _wifi.startAccessPoint(_config.getApSsid().c_str(), _config.getApPassword().c_str());
        }
    }

    void handleApiWifiReconnect() {
        JsonDocument resDoc;
        resDoc["success"] = true;
        resDoc["msg"] = "A tentar reconectar às redes guardadas...";
        String res;
        serializeJson(resDoc, res);
        _server.send(200, "application/json", res);

        delay(500);
        bool connected = _wifi.autoConnect(10000);
        if (!connected) {
            _wifi.startAccessPoint(_config.getApSsid().c_str(), _config.getApPassword().c_str());
        }
    }

    void handleApiActuator() {
        if (!_server.hasArg("plain")) {
            _server.send(400, "application/json", "{\"error\":\"Missing body\"}");
            return;
        }

        JsonDocument doc;
        deserializeJson(doc, _server.arg("plain"));

        if (doc["throttle"].is<int>()) {
            int8_t thr = constrain(doc["throttle"].as<int>(), -100, 100);
            _actuators.setThrottle(thr);
            if (_commandQueueCb) {
                char buf[32];
                snprintf(buf, sizeof(buf), "CMD:MOTOR:%d", thr);
                _commandQueueCb(buf);
            }
        }

        if (doc["rudder"].is<int>()) {
            int8_t rud = constrain(doc["rudder"].as<int>(), -100, 100);
            _actuators.setRudder(rud);
            if (_commandQueueCb) {
                char buf[32];
                snprintf(buf, sizeof(buf), "CMD:LEME:%d", rud);
                _commandQueueCb(buf);
            }
        }

        if (doc["winchUs"].is<int>()) {
            int wUs = doc["winchUs"].as<int>();
            if (wUs <= 0) {
                _actuators.stopWinch();
                if (_commandQueueCb) _commandQueueCb("CMD:WINCH:0");
            } else if (doc["set_neutral"].as<bool>() || doc["save_neutral"].as<bool>()) {
                _actuators.setWinchNeutral(wUs);
            } else {
                _actuators.setWinchMicroseconds(wUs);
                if (_commandQueueCb) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "CMD:WINCH:%d", wUs);
                    _commandQueueCb(buf);
                }
            }
        }

        _motorStatus = MOTOR_MANUAL;
        _lastControlTime = millis();

        JsonDocument resDoc;
        resDoc["success"] = true;
        String res;
        serializeJson(resDoc, res);
        _server.send(200, "application/json", res);
    }

    void handleApiControlWinch() {
        int pulse = -1;
        bool setNeutral = false;

        if (_server.hasArg("pulse")) {
            pulse = _server.arg("pulse").toInt();
        }
        if (_server.hasArg("pulseUs")) {
            pulse = _server.arg("pulseUs").toInt();
        }
        if (_server.hasArg("set_neutral") || _server.hasArg("save_neutral")) {
            setNeutral = true;
        }

        if (_server.hasArg("plain")) {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, _server.arg("plain"));
            if (!err) {
                if (doc["pulse"].is<int>()) {
                    pulse = doc["pulse"].as<int>();
                } else if (doc["pulseUs"].is<int>()) {
                    pulse = doc["pulseUs"].as<int>();
                }
                if (doc["set_neutral"].is<bool>()) {
                    setNeutral = doc["set_neutral"].as<bool>();
                } else if (doc["save_neutral"].is<bool>()) {
                    setNeutral = doc["save_neutral"].as<bool>();
                }
            }
        }

        if (pulse == 0) {
            _actuators.stopWinch();
            if (_commandQueueCb) _commandQueueCb("CMD:WINCH:0");
        } else if (pulse >= 800 && pulse <= 2200) {
            if (setNeutral) {
                _actuators.setWinchNeutral(pulse);
            } else {
                _actuators.setWinchMicroseconds(pulse);
                if (_commandQueueCb) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "CMD:WINCH:%d", pulse);
                    _commandQueueCb(buf);
                }
            }
        }

        JsonDocument resDoc;
        resDoc["success"] = true;
        resDoc["pulseUs"] = _actuators.getWinchPulseUs();
        resDoc["neutralUs"] = _actuators.getWinchNeutral();
        String res;
        serializeJson(resDoc, res);
        _server.send(200, "application/json", res);
    }

    void handleApiAction() {
        if (!_server.hasArg("plain")) {
            _server.send(400, "application/json", "{\"error\":\"Missing body\"}");
            return;
        }

        JsonDocument doc;
        deserializeJson(doc, _server.arg("plain"));

        uint8_t action = doc["action"] | 0;
        String msg = "Ação executada";

        _lastControlTime = millis();

        switch (action) {
            case 1: // Hold Station
                _motorStatus = MOTOR_HOLDING_STATION;
                msg = "Modo Hold Station ativado!";
                break;
            case 2: // Emergency Stop
                _actuators.emergencyStop();
                _motorStatus = MOTOR_IDLE;
                if (_commandQueueCb) _commandQueueCb("CMD:EMERGENCY_STOP");
                msg = "PARAGEM DE EMERGÊNCIA ACIONADA!";
                break;
            case 3: // Drop Anchor
                _actuators.commandDropAnchor(200); // 2 metros
                if (_commandQueueCb) _commandQueueCb("CMD:DROP_ANCHOR");
                msg = "Comando Lançar Âncora enviado (2.0m)";
                break;
            case 4: // Raise / Winch Hoist
                _actuators.jogWinch(1);
                if (_commandQueueCb) _commandQueueCb("CMD:WINCH:1800");
                msg = "Guincho a ENROLAR (1800µs contínuo até Parar)";
                break;
            case 5: // Stop Winch
                _actuators.stopWinch();
                if (_commandQueueCb) _commandQueueCb("CMD:WINCH:1500");
                msg = "Guincho travado no Ponto Morto (1500µs Hold)";
                break;
            case 14: // Winch Lower / Reverse
                _actuators.jogWinch(-1);
                if (_commandQueueCb) _commandQueueCb("CMD:WINCH:1200");
                msg = "Guincho a DESENROLAR (1200µs contínuo até Parar)";
                break;
            case 6: // Toggle Rack
                if (_actuators.isRackReleased()) {
                    _actuators.engageRack();
                    if (_commandQueueCb) _commandQueueCb("CMD:RACK:ENGAGE");
                    msg = String("Travão da cremalheira TRAVADO (encosto: ") + _config.getRackAngleEngaged() + "°)";
                } else {
                    _actuators.releaseRack();
                    if (_commandQueueCb) _commandQueueCb("CMD:RACK:RELEASE");
                    msg = String("Travão da cremalheira LIVRE (solto: ") + _config.getRackAngleReleased() + "°)";
                }
                break;
            case 16: // Explicitly Engage Rack (Encosto / Travar)
                _actuators.engageRack();
                if (_commandQueueCb) _commandQueueCb("CMD:RACK:ENGAGE");
                msg = String("Travão da cremalheira TRAVADO (encosto: ") + _config.getRackAngleEngaged() + "°)";
                break;
            case 17: // Explicitly Release Rack (Livre / Soltar)
                _actuators.releaseRack();
                if (_commandQueueCb) _commandQueueCb("CMD:RACK:RELEASE");
                msg = String("Travão da cremalheira LIVRE (solto: ") + _config.getRackAngleReleased() + "°)";
                break;
            case 7: // RTL
                _motorStatus = MOTOR_RTL;
                msg = "Modo RTL (Return to Launch) ativado!";
                break;
            case 8: // Toggle Alarm
                if (_actuators.isAlarmActive()) {
                    _actuators.stopAlarm();
                    if (_commandQueueCb) _commandQueueCb("CMD:ALARM:0");
                    msg = "Alarme sonoro/strobe DESLIGADO";
                } else {
                    _actuators.triggerAlarm(10);
                    if (_commandQueueCb) _commandQueueCb("CMD:ALARM:10");
                    msg = "Alarme sonoro/strobe LIGADO (10s)";
                }
                break;
            case 10: // Manual mode
                _motorStatus = MOTOR_MANUAL;
                if (_commandQueueCb) _commandQueueCb("CMD:MODE:MANUAL");
                msg = "Modo Manual ativado";
                break;
            case 13: // Relax rack servo (stop heating)
                _actuators.relaxRack();
                if (_commandQueueCb) _commandQueueCb("CMD:RACK:RELAX");
                msg = "Servo do trinco relaxado (sem forçar)!";
                break;
            case 15: // Zero Anchor Counter
                _actuators.zeroAnchorCounter();
                if (_commandQueueCb) _commandQueueCb("CMD:ZERO_ANCHOR");
                msg = "Contador de cabo da âncora ZERADO (0.0 m)!";
                break;
            default:
                msg = "Comando desconhecido";
                break;
        }

        Serial.printf("[WEB-SERVER] Acao Recebida ID=%u -> %s\n", action, msg.c_str());

        JsonDocument resDoc;
        resDoc["success"] = true;
        resDoc["msg"] = msg;
        String res;
        serializeJson(resDoc, res);
        _server.send(200, "application/json", res);
    }

    void handleApiAnchorConfig() {
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
        float diam = doc["diam_mm"] | _config.getAnchorDrumDiamMm();
        uint8_t pulses = doc["pulses_per_rev"] | _config.getAnchorPulsesPerRev();
        _config.setAnchorCalibration(diam, pulses);
        _actuators.setAnchorCalibration(diam, pulses);
        if (_commandQueueCb) {
            char cmd[32];
            snprintf(cmd, sizeof(cmd), "CMD:ANC_CFG:%.1f:%d", diam, pulses);
            _commandQueueCb(cmd);
        }
        _server.send(200, "application/json", "{\"success\":true,\"msg\":\"Calibração do tambor guardada com sucesso!\"}");
    }

    void handleApiRackConfig() {
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

        // Modo de teste ao vivo de um ângulo específico (sem necessariamente gravar em flash)
        if (doc["test_angle"].is<int>() && !doc["angle_engaged"].is<int>() && !doc["angle_released"].is<int>() && !doc["inverted"].is<bool>()) {
            uint8_t testAng = (uint8_t)constrain(doc["test_angle"].as<int>(), 0, 180);
            _actuators.setRackAngle(testAng);
            if (_commandQueueCb) {
                char cmd[32];
                snprintf(cmd, sizeof(cmd), "CMD:RACK:ANGLE:%u", testAng);
                _commandQueueCb(cmd);
            }
            JsonDocument res;
            res["success"] = true;
            res["angle"] = testAng;
            res["pulse_us"] = _actuators.getCurrentRackPulseUs();
            res["msg"] = String("Servo da cremalheira movido para ") + testAng + "° (" + _actuators.getCurrentRackPulseUs() + " µs)";
            String out;
            serializeJson(res, out);
            _server.send(200, "application/json", out);
            return;
        }

        uint8_t eng = doc["angle_engaged"].is<int>() ? (uint8_t)constrain(doc["angle_engaged"].as<int>(), 0, 180) : _config.getRackAngleEngaged();
        uint8_t rel = doc["angle_released"].is<int>() ? (uint8_t)constrain(doc["angle_released"].as<int>(), 0, 180) : _config.getRackAngleReleased();
        bool inv = doc["inverted"].is<bool>() ? doc["inverted"].as<bool>() : _config.isRackInverted();

        _config.setRackConfig(eng, rel, inv);
        _actuators.setRackConfig(eng, rel, inv, true);
        if (_commandQueueCb) {
            char cmd[48];
            snprintf(cmd, sizeof(cmd), "CMD:RACK_CFG:%u:%u:%d", eng, rel, inv ? 1 : 0);
            _commandQueueCb(cmd);
        }

        JsonDocument res;
        res["success"] = true;
        res["angle_engaged"] = eng;
        res["angle_released"] = rel;
        res["inverted"] = inv;
        res["msg"] = String("Calibração da cremalheira guardada (Encosto: ") + eng + "°, Livre: " + rel + "°, Direção: " + (inv ? "Invertida" : "Normal") + ")!";
        String out;
        serializeJson(res, out);
        _server.send(200, "application/json", out);
    }

    // -------------------------------------------------------------
    // APIs DO CARTÃO MICROSD
    // -------------------------------------------------------------
    void handleApiSdList() {
        String dir = "/";
        if (_server.hasArg("dir")) {
            dir = _server.arg("dir");
            if (!dir.startsWith("/")) dir = "/" + dir;
        }

        JsonDocument doc;
        doc["ready"] = _sd.isReady();
        doc["current_dir"] = dir;
        doc["card_type"] = _sd.getCardTypeStr();
        doc["total_mb"] = (uint32_t)(_sd.getTotalBytes() / (1024ULL * 1024ULL));
        doc["used_mb"] = (uint32_t)(_sd.getUsedBytes() / (1024ULL * 1024ULL));
        doc["free_mb"] = (uint32_t)(_sd.getFreeBytes() / (1024ULL * 1024ULL));

        JsonArray arr = doc["files"].to<JsonArray>();
        if (_sd.isReady()) {
            auto entries = _sd.listEntries(dir.c_str());
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

        if (!_sd.isReady() || !_sd.fileExists(path.c_str())) {
            _server.send(404, "text/plain", "Ficheiro não encontrado");
            return;
        }

        String content = _sd.readFile(path.c_str(), 16384);
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

        if (!_sd.isReady() || !_sd.fileExists(path.c_str())) {
            _server.send(404, "text/plain", "Ficheiro não encontrado no cartão SD");
            return;
        }

        File f = _sd.openFile(path.c_str(), FILE_READ);
        if (!f || f.isDirectory()) {
            if (f) f.close();
            _server.send(400, "text/plain", "Ficheiro inválido ou é uma diretoria");
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

        bool ok = _sd.writeFile(path.c_str(), content.c_str());

        // Se o ficheiro editado for o de redes wifi, sincronizar imediatamente na memória
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

        bool ok = false;
        if (isDir) {
            ok = _sd.deleteDir(path.c_str(), true);
        } else {
            ok = _sd.deleteFile(path.c_str());
            if (!ok) {
                ok = _sd.deleteDir(path.c_str(), true);
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

        bool ok = _sd.deleteDir(path.c_str(), true);

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

        bool ok = _sd.ensureDir(path.c_str());

        JsonDocument res;
        res["success"] = ok;
        res["msg"] = ok ? "Diretório criado com sucesso no cartão SD!" : "Erro ao criar diretório no cartão SD";
        String out;
        serializeJson(res, out);
        _server.send(200, "application/json", out);
    }

    void handleApiSdUploadData() {
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
            if (dir != "/" && dir.length() > 0) {
                _sd.ensureDir(dir.c_str());
            }

            String filename = upload.filename;
            if (filename.startsWith("/")) filename = filename.substring(1);
            String fullPath = (dir == "/") ? ("/" + filename) : (dir + "/" + filename);
            _uploadCurrentPath = fullPath;
            Serial.printf("[SD UPLOAD] A iniciar envio de: %s\n", fullPath.c_str());

            if (_sd.fileExists(fullPath.c_str())) {
                _sd.deleteFile(fullPath.c_str());
            }

            _uploadFile = _sd.openFile(fullPath.c_str(), FILE_WRITE);
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            yield();
            if (_uploadFile) {
                _sd.prepareBus();
                _uploadFile.write(upload.buf, upload.currentSize);
            }
        } else if (upload.status == UPLOAD_FILE_END) {
            if (_uploadFile) {
                _uploadFile.flush();
                _uploadFile.close();
                _uploadSuccess = true;
                Serial.printf("[SD UPLOAD] Concluído com sucesso: %s (%u bytes)\n", upload.filename.c_str(), upload.totalSize);
            }
            _isBusy = false;
        } else if (upload.status == UPLOAD_FILE_ABORTED) {
            _uploadSuccess = false;
            if (_uploadFile) {
                _uploadFile.close();
                if (_sd.fileExists(_uploadCurrentPath.c_str())) {
                    _sd.deleteFile(_uploadCurrentPath.c_str());
                }
                Serial.printf("[SD UPLOAD] Envio cancelado/abortado: %s\n", upload.filename.c_str());
            }
            _isBusy = false;
        }
    }

    void handleApiSdUploadFinish() {
        _server.sendHeader("Connection", "close");
        if (_server.hasArg("ajax")) {
            if (_uploadSuccess) {
                _server.send(200, "application/json", "{\"success\":true,\"msg\":\"Upload concluído com sucesso\"}");
            } else {
                _server.send(500, "application/json", "{\"success\":false,\"msg\":\"Falha ou aborto durante o upload\"}");
            }
            return;
        }
        _server.sendHeader("Location", "/sd");
        _server.send(303, "text/plain", _uploadSuccess ? "Upload concluído com sucesso" : "Erro no upload");
    }

    void handleOtaC6UploadData() {
        HTTPUpload& upload = _server.upload();
        if (upload.status == UPLOAD_FILE_START) {
            _server.client().setTimeout(40000);
            _uploadSuccess = false;
            _isBusy = true;
            _c6OtaProgress = 5;
            _c6OtaState = "UPLOADING";
            _c6OtaDetail = "A receber binario c6.bin no cartao SD...";
            Serial.printf("[OTA C6] Recebendo firmware: %s\n", upload.filename.c_str());

            if (_sd.isReady()) {
                _sd.ensureDir("/firmware");
                _uploadFile = _sd.openFile("/firmware/c6.bin", FILE_WRITE);
            }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            yield();
            if (_uploadFile) {
                _sd.prepareBus();
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
                Serial.printf("[OTA C6] Firmware gravado no SD: %u bytes\n", upload.totalSize);
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
        _server.sendHeader("Connection", "close");
        if (!_uploadSuccess) {
            _server.send(500, "application/json", "{\"success\":false,\"error\":\"Falha ao receber c6.bin no cartao SD\"}");
            return;
        }

        // Obter credenciais de rede
        String ssid = _wifi.getSSID();
        String pass = _wifi.getConnectedPassword();
        String ip   = _wifi.getIPAddress();

        // Se estiver em modo AP, IP for 0.0.0.0 ou SSID desconectado, garantir credenciais do AP
        if (_wifi.isAPMode() || ip == "0.0.0.0" || ssid == "Desconectado" || ssid.isEmpty()) {
            char apName[32];
            snprintf(apName, sizeof(apName), "WINDDRAGONS-ROVER-%02d", _roverId);
            ssid = (_wifi.getSSID() != "Desconectado" && !_wifi.getSSID().isEmpty()) ? _wifi.getSSID() : String(apName);
            pass = _wifi.getAPPassword();
            if (pass.isEmpty()) pass = "winddragon";
            ip = "192.168.4.1";
        }

        String otaUrl = "http://" + ip + "/firmware/c6.bin";
        String cmd = "CMD:OTA:" + ssid + "|" + pass + "|" + otaUrl;

        _c6OtaProgress = 20;
        _c6OtaState = "STARTING";
        _c6OtaDetail = "A aguardar ligacao do Coprocessador C6...";

        // Enviar resposta HTTP 200 PRIMEIRO para garantir entrega ao browser antes de ligar WiFi no C6 / desenhar LCD
        _server.send(200, "application/json", "{\"success\":true,\"state\":\"STARTING\",\"url\":\"" + otaUrl + "\"}");
        _server.client().flush();
        delay(50);

        Serial.printf("[OTA C6] Disparando OTA para C6: %s\n", cmd.c_str());
        if (_commandQueueCb) {
            _commandQueueCb(cmd.c_str());
        }
        if (_display) _display->showOtaScreen("COPROCESSADOR C6", "PASSO 1/2: DOWNLOAD C6", 20, _c6OtaDetail.c_str(), UI_YELLOW);
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

    void handleApiOtaLafvinData() {
        HTTPUpload& upload = _server.upload();
        if (upload.status == UPLOAD_FILE_START) {
            _server.client().setTimeout(40000);
            _isBusy = true;
            _lafvinOtaBytes = 0;
            Serial.printf("[OTA LAFVIN] A iniciar gravacao: %s\n", upload.filename.c_str());
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
                Serial.printf("[OTA LAFVIN] Gravacao concluida: %u bytes\n", upload.totalSize);
                if (_display) _display->showOtaScreen("MASTER LAFVIN", "CONCLUIDO COM SUCESSO", 100, "100% OK! A reiniciar Rover...", UI_GREEN);
            } else {
                Update.printError(Serial);
                if (_display) _display->showOtaScreen("MASTER LAFVIN", "FALHA NA VERIFICACAO", 0, "Erro de integridade na Flash", UI_RED);
            }
            _isBusy = false;
        } else if (upload.status == UPLOAD_FILE_ABORTED) {
            Update.end();
            _isBusy = false;
            Serial.println("[OTA LAFVIN] Gravacao abortada");
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
