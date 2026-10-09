#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "wifi_config_manager.h"
#include "rover_config_manager.h"
#include "sd_card_manager.h"
#include "lora_protocol.h"
#include "gps_tracker.h"
#include "battery_monitor.h"
#include "actuators.h"
#include "rover_display.h"

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
    }

    void setRoverId(uint8_t id) {
        _roverId = id;
    }

    void setDisplay(RoverDisplay *display) {
        _display = display;
    }

    bool isBusy() const {
        return _isBusy;
    }

private:
    WebServer _server;
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

        _server.client().setTimeout(30000);
        _server.streamFile(f, contentType);
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
        // Rotas principais (Prioridade ao Cartão SD /www/rover/... com fallback para /www/... e Flash)
        _server.on("/", [this]() {
            if (_wifi.isAPMode()) {
                if (tryServeSdFile("/www/wifi.html")) return;
                handleWifiPage();
            } else {
                if (tryServeSdFile("/www/index.html") || tryServeSdFile("/www/gps.html")) return;
                handleGpsPage();
            }
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
            handleControlPage(); 
        });
        _server.on("/www/control.html", [this]() { handleControlPage(); });
        _server.on("/www/rover/control.html", [this]() { handleControlPage(); });
        _server.on("/sd", [this]() {
            if (tryServeSdFile("/www/sd.html")) return;
            handleSdPage();
        });

        // APIs JSON & Operações
        _server.on("/api/status", [this]() { handleApiStatus(); });
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

        // Controle de atuadores
        _server.on("/api/control/actuator", [this]() { handleApiActuator(); });
        _server.on("/api/control/action", [this]() { handleApiAction(); });
        _server.on("/api/control/winch", [this]() { handleApiControlWinch(); });

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
            if (handleStaticFile(_server.uri())) {
                return;
            }
            if (_server.uri().startsWith("/") && !_server.uri().startsWith("/www/")) {
                if (handleStaticFile("/www/rover" + _server.uri())) {
                    return;
                }
                if (handleStaticFile("/www" + _server.uri())) {
                    return;
                }
            }
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

        // Card 1: Estado Atual
        html += "<div class='card'>";
        html += "<div class='card-title'><span><i class='fa-solid fa-network-wired'></i> Estado da Ligação de Rede da Boia</span></div>";
        html += "<div class='grid-2'>";
        html += "<div class='stat-box'><div class='stat-label'>Modo de Operação</div><div class='stat-value' style='color:" + String(_wifi.isAPMode() ? "var(--yellow)" : "var(--green)") + "'>";
        html += _wifi.isAPMode() ? "Ponto de Acesso (AP)" : "Conectado à Rede (STA)";
        html += "</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Endereço IP</div><div class='stat-value' style='color:var(--primary);'>" + _wifi.getIPAddress() + "</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>SSID Ativo</div><div class='stat-value'>" + _wifi.getSSID() + "</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>" + String(_wifi.isAPMode() ? "Dispositivos Conectados" : "Sinal WiFi (RSSI)") + "</div>";
        html += "<div class='stat-value'>" + String(_wifi.isAPMode() ? String(_wifi.getAPStationCount()) : String(_wifi.getRSSI()) + " dBm") + "</div></div>";
        html += "</div>";

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
        html += "let slideTimer=null;";
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
        html += "function saveAllConfig(e){";
        html += "  e.preventDefault();";
        html += "  const id=parseInt(document.getElementById('cfg_id').value);";
        html += "  const ssid=document.getElementById('cfg_ssid').value;";
        html += "  const pass=document.getElementById('cfg_pass').value;";
        html += "  const bright=parseInt(document.getElementById('cfg_bright').value);";
        html += "  fetch('/api/rover/config',{";
        html += "    method:'POST',";
        html += "    headers:{'Content-Type':'application/json'},";
        html += "    body:JSON.stringify({rover_id:id,ap_ssid:ssid,ap_password:pass,screen_brightness:bright})";
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

        html += "<script>";
        html += "setInterval(()=>{fetch('/api/status').then(r=>r.json()).then(data=>{";
        html += "const g=data.gps;";
        html += "document.getElementById('gpsFix').innerText=g.fix?('FIX ('+g.satellites+' sats)'):'SEM FIX';";
        html += "document.getElementById('gpsFix').style.color=g.fix?'var(--green)':'var(--red)';";
        html += "document.getElementById('gpsSpeed').innerText=g.speed_kn.toFixed(1)+' kn';";
        html += "document.getElementById('gpsHeading').innerText=g.heading.toFixed(0)+'°';";
        html += "document.getElementById('motorMode').innerText=data.actuators.motor_status_str;";
        html += "document.getElementById('gpsLat').innerText=g.lat.toFixed(6);";
        html += "document.getElementById('gpsLng').innerText=g.lng.toFixed(6);";
        html += "document.getElementById('osmLink').href='https://www.openstreetmap.org/?mlat='+g.lat+'&mlon='+g.lng+'#map=18/'+g.lat+'/'+g.lng;";
        html += "document.getElementById('gmapsLink').href='https://maps.google.com/?q='+g.lat+','+g.lng;";
        html += "});},1000);";
        html += "</script>";

        html += getHtmlFooter();
        _server.send(200, "text/html", html);
    }

    // -------------------------------------------------------------
    // PÁGINA 3: MONITORAMENTO DA BATERIA LIPO 4S
    // -------------------------------------------------------------
    void handleBatteryPage() {
        String html = getHtmlHeader("Monitor da Bateria LiPo 4S", "battery");

        _battery.update();
        uint8_t pct = _battery.getPercentage();
        float vTot = _battery.getVoltageMv() / 1000.0f;
        float vCell = _battery.getCellAverageV();
        float currentA = _battery.getCurrentAmps();
        float consumedMah = _battery.getConsumedMah();
        String barColor = pct < 25 ? "var(--red)" : (pct < 50 ? "var(--yellow)" : "var(--green)");

        html += "<div class='card'>";
        html += "<div class='card-title'><span><i class='fa-solid fa-battery-full'></i> Monitoramento APM Power Module (28V / 90A)</span></div>";

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

        // Tabela representativa das 4 células
        html += "<div style='margin-top:20px;'>";
        html += "<div class='stat-label' style='margin-bottom:8px;'>Distribuição Estimada de Tensão (4S LiPo):</div>";
        html += "<div class='grid-4'>";
        for (int c = 1; c <= 4; ++c) {
            html += "<div class='stat-box' style='text-align:center;'><div class='stat-label'>Célula " + String(c) + "</div><div class='stat-value cell-v' style='font-size:1.2rem;color:var(--primary);'>" + String(vCell, 2) + " V</div></div>";
        }
        html += "</div></div></div>";

        html += "<script>";
        html += "setInterval(()=>{fetch('/api/status').then(r=>r.json()).then(data=>{";
        html += "const b=data.battery;";
        html += "const p=b.pct;const v=b.voltage;const vc=b.cell_avg;";
        html += "const col=p<25?'var(--red)':(p<50?'var(--yellow)':'var(--green)');";
        html += "document.getElementById('batPct').innerText=p+'%';document.getElementById('batPct').style.color=col;";
        html += "document.getElementById('batBar').style.width=p+'%';document.getElementById('batBar').style.background=col;";
        html += "document.getElementById('vTotVal').innerText=v.toFixed(2)+' V';";
        html += "document.getElementById('vCellVal').innerText=vc.toFixed(2)+' V/cel';";
        html += "document.getElementById('currVal').innerText=b.current_a.toFixed(2)+' A';";
        html += "document.getElementById('consVal').innerText=b.consumed_mah.toFixed(0)+' mAh';";
        html += "document.querySelectorAll('.cell-v').forEach(el=>el.innerText=vc.toFixed(2)+' V');";
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
        html += "<div class='card-title'><i class='fa-solid fa-anchor'></i> Guincho da Âncora (GPIO 19) & Trinco (GPIO 23)</div>";
        html += "<div class='btn-group'>";
        html += "<button onclick='sendAction(4)' class='btn btn-primary'><i class='fa-solid fa-play'></i> LIGAR Puxar (1800 µs)</button>";
        html += "<button onclick='sendAction(14)' class='btn btn-secondary'><i class='fa-solid fa-rotate-left'></i> LIGAR Descer (1200 µs)</button>";
        html += "<button onclick='cutWinchSignal()' class='btn btn-danger' style='background:#dc2626;font-weight:700;'><i class='fa-solid fa-power-off'></i> CORTAR SINAL (OFF / 0V)</button>";
        html += "<button onclick='sendAction(6)' class='btn btn-secondary'><i class='fa-solid fa-lock'></i> Alternar Trinco</button>";
        html += "<button onclick='sendAction(13)' class='btn btn-secondary' style='background:#475569;color:#fff;'><i class='fa-solid fa-feather'></i> Relaxar Trinco</button>";
        html += "</div></div>";

        // Card: Calibração de Frequência / Neutro do Guincho (GPIO 19)
        html += "<div class='card' style='border:1px solid #0284c7;'>";
        html += "<div class='card-title'><i class='fa-solid fa-sliders'></i> Calibração de Frequência / Controlo ON-OFF (GPIO 19)</div>";
        html += "<p style='color:var(--muted);font-size:0.85rem;margin-bottom:12px;'>Arraste o slider para testar pulsos PWM, ou clique no botão vermelho para <b>cortar por completo o sinal (0V)</b>.</p>";
        
        html += "<div style='display:flex;align-items:center;justify-content:space-between;margin-bottom:8px;'>";
        html += "<span class='stat-label'>Sinal PWM no GPIO 19:</span>";
        html += "<span id='winchVal' style='font-size:1.6rem;font-weight:700;color:var(--primary);'>" + String(_actuators.getWinchPulseUs() > 0 ? (String(_actuators.getWinchPulseUs()) + " µs (ON)") : "OFF (0V - Cortado)") + "</span>";
        html += "</div>";

        html += "<input type='range' id='winchSlider' min='1000' max='2000' step='1' value='" + String(_actuators.getWinchPulseUs() > 0 ? _actuators.getWinchPulseUs() : 1500) + "' class='slider' oninput='onWinchInput(this.value)' onchange='sendWinchPulse(this.value)'>";

        html += "<div class='btn-group' style='margin-top:12px;'>";
        html += "<button onclick='stepWinch(-50)' class='btn btn-secondary'>-50 µs</button>";
        html += "<button onclick='stepWinch(-5)' class='btn btn-secondary'>-5 µs</button>";
        html += "<button onclick='stepWinch(-1)' class='btn btn-secondary'>-1 µs</button>";
        html += "<button onclick='setWinchDirect(1500)' class='btn btn-secondary' style='background:#334155;color:#fff;'>1500 µs</button>";
        html += "<button onclick='stepWinch(1)' class='btn btn-secondary'>+1 µs</button>";
        html += "<button onclick='stepWinch(5)' class='btn btn-secondary'>+5 µs</button>";
        html += "<button onclick='stepWinch(50)' class='btn btn-secondary'>+50 µs</button>";
        html += "</div>";

        html += "<div class='btn-group' style='margin-top:14px;'>";
        html += "<button onclick='cutWinchSignal()' class='btn btn-danger' style='background:#ef4444;font-size:1rem;padding:12px 20px;'><i class='fa-solid fa-power-off'></i> CORTAR SINAL COMPLETO (OFF / 0V)</button>";
        html += "<button onclick='saveCurrentAsNeutral()' class='btn btn-primary' style='background:#059669;'><i class='fa-solid fa-check'></i> Guardar como Neutro Padrão</button>";
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
        html += "function sendEmergencyStop(){setThrottle(0);setRudder(0);sendAction(2);}";
        html += "let winchTimer=null;";
        html += "function onWinchInput(v){document.getElementById('winchVal').innerText=v+' µs (ON)';if(winchTimer)clearTimeout(winchTimer);winchTimer=setTimeout(()=>{sendWinchPulse(v);},40);}";
        html += "function sendWinchPulse(v,saveNeutral=false){fetch('/api/control/winch',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({pulseUs:parseInt(v),set_neutral:saveNeutral})})";
        html += ".then(r=>r.json()).then(res=>{if(saveNeutral){showFeedback('Neutro guardado: '+res.neutralUs+' µs');}else if(parseInt(v)===0){showFeedback('Sinal cortado (OFF)');}else{document.getElementById('winchVal').innerText=v+' µs (ON)';}});};";
        html += "function stepWinch(delta){const s=document.getElementById('winchSlider');let val=parseInt(s.value)+delta;if(val<1000)val=1000;if(val>2000)val=2000;s.value=val;document.getElementById('winchVal').innerText=val+' µs (ON)';sendWinchPulse(val);}";
        html += "function setWinchDirect(val){const s=document.getElementById('winchSlider');s.value=val;document.getElementById('winchVal').innerText=val+' µs (ON)';sendWinchPulse(val);}";
        html += "function cutWinchSignal(){sendAction(5);fetch('/api/control/winch',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({pulseUs:0})})";
        html += ".then(r=>r.json()).then(res=>{document.getElementById('winchVal').innerText='OFF (0V - Cortado)';showFeedback('Sinal PWM cortado (0V / OFF)');});}";
        html += "function stopWinchCalib(){cutWinchSignal();}";
        html += "function saveCurrentAsNeutral(){const val=parseInt(document.getElementById('winchSlider').value);sendWinchPulse(val,true);}";
        html += "setInterval(()=>{fetch('/api/status').then(r=>r.json()).then(data=>{";
        html += "document.getElementById('curMotor').innerText=data.actuators.motor_status_str;";
        html += "document.getElementById('curAnchor').innerText=data.actuators.anchor_status_str;";
        html += "document.getElementById('curDepth').innerText=data.actuators.anchor_depth_m.toFixed(1)+' m';";
        html += "document.getElementById('curRack').innerText=data.actuators.rack_released?'LIVRE':'TRAVADA';";
        html += "});},1000);";
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
            html += "<div class='grid-4'>";
            html += "<div class='stat-box'><div class='stat-label'>Tipo de Cartão</div><div class='stat-value' id='statCardType' style='color:var(--primary);'>" + String(_sd.getCardTypeStr()) + "</div></div>";
            html += "<div class='stat-box'><div class='stat-label'>Capacidade Total</div><div class='stat-value' id='statTotalMb'>" + String((uint32_t)_sd.getCardSizeMB()) + " MB</div></div>";
            html += "<div class='stat-box'><div class='stat-label'>Espaço Utilizado</div><div class='stat-value' id='statUsedMb' style='color:var(--yellow);'>" + String((uint32_t)usedMb) + " MB</div></div>";
            html += "<div class='stat-box'><div class='stat-label'>Espaço Livre</div><div class='stat-value' id='statFreeMb' style='color:var(--green);'>" + String((uint32_t)freeMb) + " MB</div></div>";
            html += "</div>";

            html += "<div style='margin-top:14px;'>";
            html += "<div class='stat-label'>Ocupação do Sistema de Ficheiros: " + String(usedPct) + "%</div>";
            html += "<div class='progress-bg'><div class='progress-bar' style='width:" + String(usedPct) + "%;background:var(--primary);'></div></div>";
            html += "</div>";
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
        html += "if(data.card_type && document.getElementById('statCardType')) document.getElementById('statCardType').innerText = data.card_type;";
        html += "if(data.total_mb !== undefined && document.getElementById('statTotalMb')) document.getElementById('statTotalMb').innerText = data.total_mb + ' MB';";
        html += "if(data.used_mb !== undefined && document.getElementById('statUsedMb')) document.getElementById('statUsedMb').innerText = data.used_mb + ' MB';";
        html += "if(data.free_mb !== undefined && document.getElementById('statFreeMb')) document.getElementById('statFreeMb').innerText = data.free_mb + ' MB';";
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

        JsonObject b = doc["battery"].to<JsonObject>();
        b["pct"] = _battery.getPercentage();
        b["voltage"] = _battery.getVoltageMv() / 1000.0f;
        b["cell_avg"] = _battery.getCellAverageV();
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
        a["rack_released"] = _actuators.isRackReleased();
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

        String res;
        serializeJson(doc, res);
        _server.send(200, "application/json", res);
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
        doc["sd_ready"] = _sd.isReady();
        doc["has_config_file"] = (_sd.isReady() && _sd.fileExists("/config.json"));
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

        _config.updateConfig(id, ssid, pass, bright);
        _roverId = _config.getRoverId();

        if (_display) {
            _display->setBrightness(bright);
        }

        JsonDocument res;
        res["success"] = true;
        res["msg"] = "Configuração do Rover guardada na NVS e no Cartão SD (/config.json)!";
        res["rover_id"] = _roverId;
        res["ap_ssid"] = _config.getApSsid();
        res["screen_brightness"] = _config.getScreenBrightness();

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

        JsonDocument res;
        res["success"] = true;
        res["msg"] = "Configurações recarregadas com sucesso do ficheiro /config.json (ou NVS)!";
        res["rover_id"] = _roverId;
        res["ap_ssid"] = _config.getApSsid();
        res["screen_brightness"] = _config.getScreenBrightness();

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
        }

        if (doc["rudder"].is<int>()) {
            int8_t rud = constrain(doc["rudder"].as<int>(), -100, 100);
            _actuators.setRudder(rud);
        }

        if (doc["winchUs"].is<int>()) {
            int wUs = doc["winchUs"].as<int>();
            if (wUs <= 0) {
                _actuators.stopWinch();
            } else if (doc["set_neutral"].as<bool>() || doc["save_neutral"].as<bool>()) {
                _actuators.setWinchNeutral(wUs);
            } else {
                _actuators.setWinchMicroseconds(wUs);
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
        } else if (pulse >= 800 && pulse <= 2200) {
            if (setNeutral) {
                _actuators.setWinchNeutral(pulse);
            } else {
                _actuators.setWinchMicroseconds(pulse);
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
                msg = "PARAGEM DE EMERGÊNCIA ACIONADA!";
                break;
            case 3: // Drop Anchor
                _actuators.commandDropAnchor(200); // 2 metros
                msg = "Comando Lançar Âncora enviado (2.0m)";
                break;
            case 4: // Raise / Winch Hoist
                _actuators.jogWinch(1);
                msg = "Guincho a ENROLAR (1800µs contínuo até Parar)";
                break;
            case 5: // Stop Winch
                _actuators.stopWinch();
                msg = "Guincho parado (1500µs)";
                break;
            case 14: // Winch Lower / Reverse
                _actuators.jogWinch(-1);
                msg = "Guincho a DESENROLAR (1200µs contínuo até Parar)";
                break;
            case 6: // Toggle Rack
                if (_actuators.isRackReleased()) {
                    _actuators.engageRack();
                    msg = "Trinco da âncora travado (0°)";
                } else {
                    _actuators.releaseRack();
                    msg = "Trinco da âncora solto (90°)";
                }
                break;
            case 7: // RTL
                _motorStatus = MOTOR_RTL;
                msg = "Modo RTL (Return to Launch) ativado!";
                break;
            case 8: // Toggle Alarm
                if (_actuators.isAlarmActive()) {
                    _actuators.stopAlarm();
                    msg = "Alarme sonoro/strobe DESLIGADO";
                } else {
                    _actuators.triggerAlarm(10);
                    msg = "Alarme sonoro/strobe LIGADO (10s)";
                }
                break;
            case 10: // Manual mode
                _motorStatus = MOTOR_MANUAL;
                msg = "Modo Manual ativado";
                break;
            case 13: // Relax rack servo (stop heating)
                _actuators.relaxRack();
                msg = "Servo do trinco relaxado (sem forçar)!";
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
        if (!_server.hasArg("file")) {
            _server.send(400, "text/plain", "Parâmetro 'file' ausente");
            return;
        }

        String path = _server.arg("file");
        if (!path.startsWith("/")) path = "/" + path;

        if (!_sd.isReady() || !_sd.fileExists(path.c_str())) {
            _server.send(404, "text/plain", "Ficheiro não encontrado");
            return;
        }

        String content = _sd.readFile(path.c_str(), 16384);
        _server.send(200, "text/plain; charset=utf-8", content);
    }

    void handleApiSdDownload() {
        if (!_server.hasArg("file")) {
            _server.send(400, "text/plain", "Parâmetro 'file' ausente");
            return;
        }

        String path = _server.arg("file");
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

        String path = doc["file"] | "";
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

        bool ok = _sd.createDir(path.c_str());

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
};
