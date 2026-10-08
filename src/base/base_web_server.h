#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "wifi_config_manager.h"
#include "fleet_manager.h"
#include "sd_card_manager.h"
#include "lora_protocol.h"
#include "network_config.h"
#include "base_network.h"
#include <esp_mac.h>

class BaseWebServer {
public:
    BaseWebServer(WiFiConfigManager &wifiMgr, FleetManager &fleet, SDCardManager *sdCard = nullptr, BaseNetwork *network = nullptr)
        : _server(80), _wifi(wifiMgr), _fleet(fleet), _sd(sdCard), _network(network),
          _webControlActive(false), _webThrottle(0), _webRudder(0), 
          _webAnchorJog(0), _lastWebControlTime(0), _nextCmdId(2000),
          _isBusy(false), _uploadSuccess(false), _uploadCurrentPath("") {}

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
    }

    void setSD(SDCardManager *sd) { _sd = sd; }
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
                if (tryServeSdFile("/www/index.html") || tryServeSdFile("/www/gps.html")) return;
                handleGpsPage(); // Na rede WiFi, a pagina principal e o GPS/Telemetria da Frota
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
        
        // Controle de atuadores e comandos
        _server.on("/api/control/actuator", [this]() { handleApiActuator(); });
        _server.on("/api/control/action", [this]() { handleApiAction(); });
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
            html += "<a href='/wifi' class='nav-item active'>Configuração WiFi (Modo AP)</a>";
        } else {
            html += "<a href='/portal' class='nav-item " + String(activeTab == "portal" ? "active" : "") + "'>☁️ Portal</a>";
            html += "<a href='/wifi' class='nav-item " + String(activeTab == "wifi" ? "active" : "") + "'>📡 WiFi</a>";
            html += "<a href='/gps' class='nav-item " + String(activeTab == "gps" ? "active" : "") + "'>📍 GPS Frota</a>";
            html += "<a href='/battery' class='nav-item " + String(activeTab == "battery" ? "active" : "") + "'>🔋 Bateria</a>";
            html += "<a href='/control' class='nav-item " + String(activeTab == "control" ? "active" : "") + "'>🎮 Comandos</a>";
            html += "<a href='/sd' class='nav-item " + String(activeTab == "sd" ? "active" : "") + "'>💾 Cartão SD</a>";
        }
        html += "</div>";
        html += "<div style='display:flex;align-items:center;gap:8px;'>";
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

        // Card 1: Estado Atual da Conexão
        html += "<div class='card'>";
        html += "<div class='card-title'>Estado da Ligação de Rede & Identificação</div>";
        html += "<div class='grid-2'>";
        html += "<div class='stat-box'><div class='stat-label'>ID Único / MAC Hardware (Portal)</div><div class='stat-value' style='color:var(--primary);font-family:monospace;'>" + mac + "</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Nome da Estação Base</div><div class='stat-value' style='color:var(--yellow);'>" + String(BASE_STATION_ID) + "</div></div>";
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

        // Script interativo
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

        html += "<div style='margin-top:16px;' class='grid-2'>";
        html += "<div class='stat-box'><div class='stat-label'>Endereço MAC de Hardware (Station ID)</div><div class='stat-value' style='color:var(--primary);font-family:monospace;'>" + mac + "</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Nome da Estação Base</div><div class='stat-value' style='color:var(--yellow);'>" + String(BASE_STATION_ID) + "</div></div>";
        html += "</div>";

        html += "<div style='margin-top:16px;' class='stat-box'><div class='stat-label'>Endpoint Cloud API</div><div class='stat-value' style='font-size:0.95rem;font-family:monospace;color:var(--text);word-break:break-all;'>" + String(WINDDRAGONS_API_URL) + "</div></div>";
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
        html += "const st=document.getElementById('fixStatus');st.innerText=sel.is_online?'ONLINE':'OFFLINE';st.style.color=sel.is_online?'var(--green)':'var(--red)';";
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
    // PÁGINA 4: COMANDAR A BOIA (SERVOS, MOTOR, ÂNCORA, ETC.)
    // -------------------------------------------------------------
    void handleControlPage() {
        FleetRover *r = _fleet.getSelectedRover();
        String html = getHtmlHeader("Comandar Boia", "control");

        html += "<div class='card'>";
        html += "<div class='card-title'>";
        html += "<span>🎮 Centro de Comando Remoto</span>";
        html += "<select id='targetRoverSelect' onchange='changeRover(this.value)' style='width:auto;display:inline-block;padding:4px 10px;'>";
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

        doc["wifi"]["mac"] = mac;
        doc["wifi"]["ap"] = _wifi.isAPMode();
        doc["wifi"]["ssid"] = _wifi.getSSID();
        doc["wifi"]["ip"] = _wifi.getIPAddress();
        doc["wifi"]["rssi"] = _wifi.getRSSI();

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
        deserializeJson(doc, _server.arg("plain"));

        uint8_t action = doc["action"] | 0;
        uint8_t targetRid = doc["rover_id"] | _fleet.getSelectedRoverId();
        uint16_t depth = doc["depth_cm"] | 0;

        _fleet.queueCommand(targetRid, ++_nextCmdId, action, depth);

        String msg = "Comando '" + String(get_action_str(action)) + "' colocado na fila LoRa para ROVER-" + String(targetRid);
        _server.send(200, "application/json", "{\"status\":\"queued\",\"msg\":\"" + msg + "\"}");
    }

    void handleApiSelectRover() {
        if (!_server.hasArg("plain")) {
            _server.send(400, "application/json", "{\"error\":\"Missing body\"}");
            return;
        }
        JsonDocument doc;
        deserializeJson(doc, _server.arg("plain"));
        uint8_t rid = doc["rover_id"] | 1;

        // Procurar o indice
        const auto &rovers = _fleet.getRovers();
        for (size_t i = 0; i < rovers.size(); ++i) {
            if (rovers[i].id == rid) {
                // seleciona ciclando
                while (_fleet.getSelectedRoverId() != rid) {
                    _fleet.selectNextRover();
                }
                break;
            }
        }
        _server.send(200, "application/json", "{\"status\":\"ok\"}");
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

        // Estatísticas do Cartão
        html += "<div class='grid-4'>";
        html += "<div class='stat-box'><div class='stat-label'>Tipo de Cartão</div><div class='stat-value' style='color:var(--primary);'>" + String(_sd->getCardTypeStr()) + "</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Capacidade Total</div><div class='stat-value'>" + String((uint32_t)totalMb) + " MB</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Espaço Utilizado</div><div class='stat-value' style='color:var(--yellow);'>" + String((uint32_t)usedMb) + " MB (" + String(usedPct) + "%)</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Espaço Livre</div><div class='stat-value' style='color:var(--green);'>" + String((uint32_t)freeMb) + " MB</div></div>";
        html += "</div>";

        html += "<div class='progress-bg' style='margin-top:14px;'><div class='progress-bar' style='width:" + String(usedPct) + "%;background:var(--primary);'></div></div>";
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
        if (!_server.hasArg("file")) {
            _server.send(400, "text/plain", "Parâmetro 'file' ausente");
            return;
        }
        String path = _server.arg("file");
        if (!path.startsWith("/")) path = "/" + path;

        if (!_sd || !_sd->isReady() || !_sd->fileExists(path.c_str())) {
            _server.send(404, "text/plain", "Ficheiro não encontrado no SD");
            return;
        }

        String content = _sd->readFile(path.c_str(), 16384);
        _server.send(200, "text/plain; charset=utf-8", content);
    }

    void handleApiSdDownload() {
        if (!_server.hasArg("file")) {
            _server.send(400, "text/plain", "Parâmetro 'file' ausente");
            return;
        }
        String path = _server.arg("file");
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

        String path = doc["file"] | "";
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
};
