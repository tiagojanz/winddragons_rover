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

class RoverWebServer {
public:
    RoverWebServer(WiFiConfigManager &wifiMgr, GPSTracker &gps, BatteryMonitor &battery,
                   ActuatorController &actuators, uint8_t &motorStatus, uint32_t &lastControlTime,
                   RoverConfigManager &configMgr, SDCardManager &sdCard)
        : _server(80), _wifi(wifiMgr), _gps(gps), _battery(battery),
          _actuators(actuators), _motorStatus(motorStatus), _lastControlTime(lastControlTime),
          _config(configMgr), _roverId(configMgr.getRoverId()), _sd(sdCard) {}

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
    File _uploadFile;

    void setupRoutes() {
        // Rotas principais
        _server.on("/", [this]() {
            if (_wifi.isAPMode()) {
                handleWifiPage();
            } else {
                handleGpsPage();
            }
        });

        _server.on("/wifi", [this]() { handleWifiPage(); });
        _server.on("/gps", [this]() { 
            if (_wifi.isAPMode()) { _server.sendHeader("Location", "/wifi"); _server.send(302, "text/plain", ""); return; }
            handleGpsPage(); 
        });
        _server.on("/battery", [this]() { 
            if (_wifi.isAPMode()) { _server.sendHeader("Location", "/wifi"); _server.send(302, "text/plain", ""); return; }
            handleBatteryPage(); 
        });
        _server.on("/control", [this]() { 
            if (_wifi.isAPMode()) { _server.sendHeader("Location", "/wifi"); _server.send(302, "text/plain", ""); return; }
            handleControlPage(); 
        });
        _server.on("/sd", [this]() { handleSdPage(); });

        // APIs JSON & Operações
        _server.on("/api/status", [this]() { handleApiStatus(); });
        _server.on("/api/scan", [this]() { handleApiScan(); });
        _server.on("/api/wifi/save", [this]() { handleApiWifiSave(); });
        _server.on("/api/wifi/setdefault", [this]() { handleApiWifiSetDefault(); });
        _server.on("/api/wifi/delete", [this]() { handleApiWifiDelete(); });
        _server.on("/api/wifi/connect", [this]() { handleApiWifiConnect(); });
        _server.on("/api/wifi/reconnect", [this]() { handleApiWifiReconnect(); });
        
        // Configuração geral do Rover (ID, SSID AP, Password AP)
        _server.on("/api/rover/config", HTTP_GET, [this]() { handleApiRoverConfigGet(); });
        _server.on("/api/rover/config", HTTP_POST, [this]() { handleApiRoverConfigSave(); });

        // Controle de atuadores
        _server.on("/api/control/actuator", [this]() { handleApiActuator(); });
        _server.on("/api/control/action", [this]() { handleApiAction(); });

        // Gestão do Cartão SD
        _server.on("/api/sd/list", [this]() { handleApiSdList(); });
        _server.on("/api/sd/read", [this]() { handleApiSdRead(); });
        _server.on("/api/sd/download", [this]() { handleApiSdDownload(); });
        _server.on("/api/sd/save", [this]() { handleApiSdSave(); });
        _server.on("/api/sd/delete", [this]() { handleApiSdDelete(); });

        // Upload de ficheiros para o SD
        _server.on("/api/sd/upload", HTTP_POST, 
            [this]() { handleApiSdUploadFinish(); },
            [this]() { handleApiSdUploadData(); }
        );

        _server.onNotFound([this]() {
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
        html += "input,select,textarea{width:100%;padding:10px 12px;background:#0f172a;border:1px solid var(--card-border);color:#fff;border-radius:8px;margin-top:6px;font-size:0.9rem;}";
        html += ".form-group{margin-bottom:14px;}";
        html += ".progress-bg{width:100%;height:16px;background:#0f172a;border-radius:8px;overflow:hidden;margin-top:8px;}";
        html += ".progress-bar{height:100%;transition:width 0.3s;}";
        html += ".slider{width:100%;-webkit-appearance:none;height:10px;background:#0f172a;border-radius:5px;outline:none;}";
        html += ".slider::-webkit-slider-thumb{-webkit-appearance:none;width:24px;height:24px;border-radius:50%;background:var(--primary);cursor:pointer;}";
        html += "</style></head><body>";

        // Navbar
        html += "<header class='navbar'>";
        html += "<div class='brand'>🤖 WindDragons <span>Rover-" + String(_roverId) + "</span></div>";
        html += "<div class='nav-links'>";
        if (_wifi.isAPMode()) {
            html += "<a href='/wifi' class='nav-item " + String(activeTab == "wifi" ? "active" : "") + "'>Configuração WiFi</a>";
            html += "<a href='/sd' class='nav-item " + String(activeTab == "sd" ? "active" : "") + "'>📁 Cartão SD</a>";
        } else {
            html += "<a href='/wifi' class='nav-item " + String(activeTab == "wifi" ? "active" : "") + "'>📡 WiFi</a>";
            html += "<a href='/gps' class='nav-item " + String(activeTab == "gps" ? "active" : "") + "'>📍 GPS</a>";
            html += "<a href='/battery' class='nav-item " + String(activeTab == "battery" ? "active" : "") + "'>🔋 Bateria</a>";
            html += "<a href='/control' class='nav-item " + String(activeTab == "control" ? "active" : "") + "'>🎮 Comandos</a>";
            html += "<a href='/sd' class='nav-item " + String(activeTab == "sd" ? "active" : "") + "'>📁 Cartão SD</a>";
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
        return "</main><footer style='text-align:center;color:var(--muted);font-size:0.75rem;margin-top:20px;'>WindDragons Onboard Rover Telemetry &copy; 2026</footer></body></html>";
    }

    // -------------------------------------------------------------
    // PÁGINA 1: CONFIGURAÇÃO DE WIFI
    // -------------------------------------------------------------
    void handleWifiPage() {
        String html = getHtmlHeader("Configuração WiFi", "wifi");

        // Card 1: Estado Atual
        html += "<div class='card'>";
        html += "<div class='card-title'>Estado da Ligação de Rede da Boia</div>";
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
        html += "<div class='card-title'><span>Redes WiFi Guardadas</span> <span style='font-size:0.8rem;color:var(--muted);'>NVS + Cartão SD (/wifi_networks.json)</span></div>";
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

        // Card 3: Adicionar Nova Ligação
        html += "<div class='card'>";
        html += "<div class='card-title'>Adicionar Nova Ligação WiFi</div>";
        html += "<form id='wifiForm' onsubmit='saveWifi(event)'>";
        html += "<div class='form-group'><label class='stat-label'>Nome da Rede (SSID):</label><input type='text' id='ssid' name='ssid' required placeholder='ex: WiFi_Porto'></div>";
        html += "<div class='form-group'><label class='stat-label'>Palavra-passe (Password):</label><input type='password' id='pass' name='pass' placeholder='Password da rede'></div>";
        html += "<div class='form-group' style='display:flex;align-items:center;gap:10px;margin-top:10px;'>";
        html += "<input type='checkbox' id='is_default' name='is_default' style='width:auto;margin:0;'>";
        html += "<label for='is_default' style='cursor:pointer;font-size:0.875rem;'>Marcar esta rede como <strong>Padrão (Default)</strong> no arranque</label>";
        html += "</div>";
        html += "<div class='btn-group' style='margin-top:14px;'>";
        html += "<button type='submit' class='btn btn-primary'>Guardar Ligação (NVS + SD)</button>";
        html += "<button type='button' onclick='scanNetworks()' class='btn btn-secondary'>Procurar Redes Próximas</button>";
        if (_wifi.isAPMode()) {
            html += "<button type='button' onclick='reconnectSTA()' class='btn' style='background:var(--green);color:#fff;'>Tentar Ligar e Sair do AP</button>";
        }
        html += "</div></form>";
        html += "<div id='scanResults' style='margin-top:16px;'></div>";
        html += "</div>";

        // Card 4: Configuração Geral do Rover e AP (/config.json)
        html += "<div class='card'>";
        html += "<div class='card-title'><span>Identificação do Rover & Ponto de Acesso (AP)</span> <span style='font-size:0.8rem;color:var(--muted);'>NVS + Cartão SD (/config.json)</span></div>";
        html += "<form id='cfgForm' onsubmit='saveRoverCfg(event)'>";
        html += "<div class='form-group'><label class='stat-label'>ID do Rover (1 - 254):</label><input type='number' id='cfg_id' min='1' max='254' required value='" + String(_config.getRoverId()) + "'></div>";
        html += "<div class='form-group'><label class='stat-label'>Nome da Rede AP (SSID):</label><input type='text' id='cfg_ssid' required value='" + _config.getApSsid() + "'></div>";
        html += "<div class='form-group'><label class='stat-label'>Palavra-passe do AP (mín. 8 caracteres):</label><input type='password' id='cfg_pass' minlength='8' required value='" + _config.getApPassword() + "'></div>";
        html += "<div class='btn-group' style='margin-top:14px;'>";
        html += "<button type='submit' class='btn btn-primary'>Guardar Identificação (NVS + SD)</button>";
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
    // PÁGINA 2: TELEMETRIA GPS
    // -------------------------------------------------------------
    void handleGpsPage() {
        String html = getHtmlHeader("Telemetria GPS", "gps");

        html += "<div class='card'>";
        html += "<div class='card-title'><span>Posicionamento e Navegação Quectel LC29H</span></div>";

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
        html += "<a id='osmLink' href='https://www.openstreetmap.org/?mlat=" + String(_gps.getLatitude(), 6) + "&mlon=" + String(_gps.getLongitude(), 6) + "#map=18/" + String(_gps.getLatitude(), 6) + "/" + String(_gps.getLongitude(), 6) + "' target='_blank' class='btn btn-primary'>🗺️ Ver no OpenStreetMap</a>";
        html += "<a id='gmapsLink' href='https://maps.google.com/?q=" + String(_gps.getLatitude(), 6) + "," + String(_gps.getLongitude(), 6) + "' target='_blank' class='btn btn-secondary'>📍 Google Maps</a>";
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
        html += "<div class='card-title'><span>Monitoramento APM Power Module (28V / 90A)</span></div>";

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
        html += "<div class='card-title'><span>🎮 Painel de Controlo Direto da Boia</span></div>";
        html += "<div class='grid-4' style='margin-bottom:16px;'>";
        html += "<div class='stat-box'><div class='stat-label'>Modo Motor</div><div class='stat-value' id='curMotor' style='color:var(--yellow);font-size:1.1rem;'>" + String(get_motor_status_str(_motorStatus)) + "</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Estado Âncora</div><div class='stat-value' id='curAnchor' style='color:var(--primary);font-size:1.1rem;'>" + String(get_anchor_status_str(_actuators.getAnchorStatus())) + "</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Profundidade</div><div class='stat-value' id='curDepth'>" + String(_actuators.getAnchorDepthCm() / 100.0f, 1) + " m</div></div>";
        html += "<div class='stat-box'><div class='stat-label'>Cremalheira</div><div class='stat-value' id='curRack' style='color:var(--green);font-size:1.1rem;'>" + String(_actuators.isRackReleased() ? "LIVRE" : "TRAVADA") + "</div></div>";
        html += "</div></div>";

        // Card: Propulsão & Leme
        html += "<div class='card'>";
        html += "<div class='card-title'>Motor Principal (ESC) & Leme de Direção</div>";
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
        html += "<button onclick='sendAction(6)' class='btn btn-secondary'>🔒 Alternar Travão Cremalheira</button>";
        html += "</div></div>";

        // Card: Modos Operacionais & Emergência
        html += "<div class='card'>";
        html += "<div class='card-title'>Modos de Operação & Paragem Imediata</div>";
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
        html += "function sendAction(a){fetch('/api/control/action',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action:a})})";
        html += ".then(r=>r.json()).then(res=>alert(res.msg));}";
        html += "function sendEmergencyStop(){setThrottle(0);setRudder(0);sendAction(2);}";
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

        // Card 1: Informações de Armazenamento
        html += "<div class='card'>";
        html += "<div class='card-title'><span>💾 Estado do Cartão MicroSD Onboard</span>";
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

        // Card 2: Lista de Ficheiros
        html += "<div class='card'>";
        html += "<div class='card-title'><span>📁 Ficheiros e Pastas</span>";
        html += "<button onclick='loadFileList()' class='btn btn-secondary' style='padding:4px 10px;font-size:0.8rem;'>🔄 Atualizar</button></div>";
        html += "<div style='font-size:0.85rem;color:var(--muted);margin-bottom:10px;'>Diretório atual: <strong id='currentPathLabel' style='color:var(--primary);'>/</strong></div>";
        html += "<div id='fileListContainer'><p style='color:var(--muted);font-size:0.875rem;'>A carregar ficheiros...</p></div>";
        html += "</div>";

        // Card 3: Visualizador / Editor de Texto Em Linha
        html += "<div class='card' id='editorCard' style='display:none;'>";
        html += "<div class='card-title'><span id='editorTitle'>Visualizador de Ficheiro</span>";
        html += "<div class='btn-group'>";
        html += "<button onclick='saveEditorFile()' class='btn btn-primary' style='padding:6px 12px;'>💾 Guardar</button>";
        html += "<button onclick='closeEditor()' class='btn btn-secondary' style='padding:6px 12px;'>Fechar</button>";
        html += "</div></div>";
        html += "<input type='hidden' id='editorFilePath'>";
        html += "<textarea id='editorText' rows='12' style='font-family:monospace;background:#060d1f;color:#67e8f9;border:1px solid #1e293b;'></textarea>";
        html += "</div>";

        // Card 4: Upload de Ficheiro para o SD
        html += "<div class='card'>";
        html += "<div class='card-title'>📤 Carregar Ficheiro para o Cartão SD</div>";
        html += "<form method='POST' action='/api/sd/upload' enctype='multipart/form-data'>";
        html += "<input type='hidden' id='uploadDir' name='dir' value='/'>";
        html += "<div class='form-group'>";
        html += "<label class='stat-label'>Escolha o ficheiro para enviar:</label>";
        html += "<input type='file' name='upload' required style='padding:8px;'>";
        html += "</div>";
        html += "<button type='submit' class='btn btn-primary'>Enviar Ficheiro</button>";
        html += "</form></div>";

        // Script de Gestão de Ficheiros
        html += "<script>";
        html += "let currentDir = '/';";
        html += "function formatBytes(b){if(b<1024)return b+' B';if(b<1048576)return(b/1024).toFixed(1)+' KB';return(b/1048576).toFixed(1)+' MB';}";
        html += "function loadFileList(dir){";
        html += "if(dir!==undefined) currentDir = dir;";
        html += "const c=document.getElementById('fileListContainer');";
        html += "const pl=document.getElementById('currentPathLabel');";
        html += "if(pl) pl.innerText = currentDir;";
        html += "const ud=document.getElementById('uploadDir');";
        html += "if(ud) ud.value = currentDir;";
        html += "fetch('/api/sd/list?dir='+encodeURIComponent(currentDir)).then(r=>r.json()).then(data=>{";
        html += "if(!data.ready){c.innerHTML='<p style=\"color:var(--red);\">Cartão SD não disponível.</p>';return;}";
        html += "if(data.card_type&&document.getElementById('statCardType'))document.getElementById('statCardType').innerText=data.card_type;";
        html += "if(data.total_mb!==undefined&&document.getElementById('statTotalMb'))document.getElementById('statTotalMb').innerText=data.total_mb+' MB';";
        html += "if(data.used_mb!==undefined&&document.getElementById('statUsedMb'))document.getElementById('statUsedMb').innerText=data.used_mb+' MB';";
        html += "if(data.free_mb!==undefined&&document.getElementById('statFreeMb'))document.getElementById('statFreeMb').innerText=data.free_mb+' MB';";
        html += "let h='<table><thead><tr><th>Nome</th><th>Tipo / Tamanho</th><th>Ações</th></tr></thead><tbody>';";
        html += "if(currentDir !== '/'){";
        html += "h+='<tr><td colspan=\"3\"><a href=\"javascript:void(0)\" onclick=\"goUpDir()\" style=\"color:var(--primary);text-decoration:none;font-weight:bold;\">📁 .. [Subir um nível]</a></td></tr>';";
        html += "}";
        html += "if(!data.files||data.files.length===0){";
        html += "h+='<tr><td colspan=\"3\" style=\"text-align:center;color:var(--muted);padding:14px;\">Esta pasta está vazia.</td></tr>';";
        html += "} else {";
        html += "data.files.forEach(f=>{";
        html += "const fullPath = f.path || (currentDir==='/' ? '/'+f.name : currentDir+'/'+f.name);";
        html += "const isDir = f.is_dir === true;";
        html += "if(isDir){";
        html += "h+='<tr><td><a href=\"javascript:void(0)\" onclick=\"changeDir(\\''+fullPath+'\\')\" style=\"color:#38bdf8;text-decoration:none;font-weight:bold;\">📁 '+f.name+'</a></td>';";
        html += "h+='<td style=\"color:var(--muted);\">Pasta</td>';";
        html += "h+='<td><button onclick=\"changeDir(\\''+fullPath+'\\')\" class=\"btn btn-secondary\" style=\"padding:4px 8px;font-size:0.75rem;\">📂 Abrir</button></td></tr>';";
        html += "} else {";
        html += "h+='<tr><td><strong>📄 '+f.name+'</strong></td>';";
        html += "h+='<td>'+formatBytes(f.size)+'</td>';";
        html += "h+='<td><div class=\"btn-group\">';";
        html += "h+='<a href=\"/api/sd/download?file='+encodeURIComponent(fullPath)+'\" class=\"btn btn-primary\" style=\"padding:4px 8px;font-size:0.75rem;\">📥 Descarregar</a>';";
        html += "h+='<button onclick=\"viewFile(\\''+fullPath+'\\')\" class=\"btn btn-secondary\" style=\"padding:4px 8px;font-size:0.75rem;\">👁️ Ver/Editar</button>';";
        html += "h+='<button onclick=\"deleteSdFile(\\''+fullPath+'\\')\" class=\"btn btn-danger\" style=\"padding:4px 8px;font-size:0.75rem;\">🗑️ Eliminar</button>';";
        html += "h+='</div></td></tr>';";
        html += "}";
        html += "});";
        html += "}";
        html += "h+='</tbody></table>';";
        html += "c.innerHTML=h;";
        html += "}).catch(e=>{document.getElementById('fileListContainer').innerHTML='<p style=\"color:var(--red);\">Erro ao carregar lista de ficheiros: '+e+'</p>';});}";
        html += "function changeDir(d){ loadFileList(d); }";
        html += "function goUpDir(){";
        html += "if(currentDir==='/') return;";
        html += "const parts=currentDir.split('/').filter(p=>p.length>0);";
        html += "parts.pop();";
        html += "const parent = parts.length===0 ? '/' : '/' + parts.join('/');";
        html += "loadFileList(parent);";
        html += "}";
        html += "function viewFile(path){";
        html += "fetch('/api/sd/read?file='+encodeURIComponent(path)).then(r=>{if(!r.ok)throw new Error('Falha ao ler');return r.text();}).then(txt=>{";
        html += "document.getElementById('editorFilePath').value=path;";
        html += "document.getElementById('editorTitle').innerText='Editar: '+path;";
        html += "document.getElementById('editorText').value=txt;";
        html += "document.getElementById('editorCard').style.display='block';";
        html += "document.getElementById('editorCard').scrollIntoView({behavior:'smooth'});";
        html += "}).catch(e=>alert('Erro ao abrir ficheiro: '+e));}";
        html += "function saveEditorFile(){";
        html += "const p=document.getElementById('editorFilePath').value;";
        html += "const c=document.getElementById('editorText').value;";
        html += "fetch('/api/sd/save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({file:p,content:c})})";
        html += ".then(r=>r.json()).then(res=>{alert(res.msg);loadFileList();});}";
        html += "function closeEditor(){document.getElementById('editorCard').style.display='none';}";
        html += "function deleteSdFile(path){";
        html += "if(confirm('Eliminar o ficheiro '+path+' do cartão SD?')){";
        html += "fetch('/api/sd/delete',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({file:path})})";
        html += ".then(r=>r.json()).then(res=>{alert(res.msg);loadFileList();});}}";
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

        _config.updateConfig(id, ssid, pass);
        _roverId = _config.getRoverId();

        JsonDocument res;
        res["success"] = true;
        res["msg"] = "Configuração do Rover guardada na NVS e no Cartão SD (/config.json)!";
        res["rover_id"] = _roverId;
        res["ap_ssid"] = _config.getApSsid();

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

        _motorStatus = MOTOR_MANUAL;
        _lastControlTime = millis();

        JsonDocument resDoc;
        resDoc["success"] = true;
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
            case 4: // Raise Anchor
                _actuators.commandRetractAnchor(); // Recolher
                msg = "Comando Recolher Âncora enviado";
                break;
            case 5: // Stop Winch
                _actuators.stopWinch();
                msg = "Guincho parado";
                break;
            case 6: // Toggle Rack
                if (_actuators.isRackReleased()) {
                    _actuators.engageRack();
                    msg = "Cremalheira travada";
                } else {
                    _actuators.releaseRack();
                    msg = "Cremalheira solta";
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
            default:
                msg = "Comando desconhecido";
                break;
        }

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

        String path = doc["file"] | "";
        if (!path.startsWith("/")) path = "/" + path;

        bool ok = _sd.deleteFile(path.c_str());

        JsonDocument res;
        res["success"] = ok;
        res["msg"] = ok ? "Ficheiro eliminado do cartão SD!" : "Erro ao eliminar ficheiro";
        String out;
        serializeJson(res, out);
        _server.send(200, "application/json", out);
    }

    void handleApiSdUploadData() {
        HTTPUpload& upload = _server.upload();
        if (upload.status == UPLOAD_FILE_START) {
            String filename = upload.filename;
            if (!filename.startsWith("/")) filename = "/" + filename;
            Serial.printf("[SD UPLOAD] A iniciar envio de: %s\n", filename.c_str());
            _uploadFile = _sd.openFile(filename.c_str(), FILE_WRITE);
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (_uploadFile) {
                _uploadFile.write(upload.buf, upload.currentSize);
            }
        } else if (upload.status == UPLOAD_FILE_END) {
            if (_uploadFile) {
                _uploadFile.close();
                Serial.printf("[SD UPLOAD] Concluído com sucesso: %s (%u bytes)\n", upload.filename.c_str(), upload.totalSize);
            }
        }
    }

    void handleApiSdUploadFinish() {
        _server.sendHeader("Location", "/sd");
        _server.send(303, "text/plain", "Upload concluído com sucesso");
    }
};
