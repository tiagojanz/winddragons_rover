#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "wifi_config_manager.h"
#include "fleet_manager.h"
#include "lora_protocol.h"

class BaseWebServer {
public:
    BaseWebServer(WiFiConfigManager &wifiMgr, FleetManager &fleet)
        : _server(80), _wifi(wifiMgr), _fleet(fleet),
          _webControlActive(false), _webThrottle(0), _webRudder(0), 
          _webAnchorJog(0), _lastWebControlTime(0), _nextCmdId(2000) {}

    void begin() {
        setupRoutes();
        _server.begin();
        Serial.println("[HTTP] Servidor Web ativo na porta 80");
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

    bool isWebControlActive() const { return _webControlActive; }
    int8_t getWebThrottle() const { return _webThrottle; }
    int8_t getWebRudder() const { return _webRudder; }
    int8_t getWebAnchorJog() const { return _webAnchorJog; }

private:
    WebServer _server;
    WiFiConfigManager &_wifi;
    FleetManager &_fleet;

    bool _webControlActive;
    int8_t _webThrottle;
    int8_t _webRudder;
    int8_t _webAnchorJog;
    uint32_t _lastWebControlTime;
    uint16_t _nextCmdId;

    void setupRoutes() {
        // Rotas comuns
        _server.on("/", [this]() {
            if (_wifi.isAPMode()) {
                handleWifiPage();
            } else {
                handleGpsPage(); // Na rede WiFi, a pagina principal e o GPS/Telemetria
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

        // APIs JSON
        _server.on("/api/status", [this]() { handleApiStatus(); });
        _server.on("/api/scan", [this]() { handleApiScan(); });
        _server.on("/api/wifi/save", [this]() { handleApiWifiSave(); });
        _server.on("/api/wifi/setdefault", [this]() { handleApiWifiSetDefault(); });
        _server.on("/api/wifi/delete", [this]() { handleApiWifiDelete(); });
        _server.on("/api/wifi/reconnect", [this]() { handleApiWifiReconnect(); });
        
        // Controle de atuadores e comandos
        _server.on("/api/control/actuator", [this]() { handleApiActuator(); });
        _server.on("/api/control/action", [this]() { handleApiAction(); });
        _server.on("/api/select_rover", [this]() { handleApiSelectRover(); });

        _server.onNotFound([this]() {
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
            html += "<a href='/wifi' class='nav-item " + String(activeTab == "wifi" ? "active" : "") + "'>📡 WiFi</a>";
            html += "<a href='/gps' class='nav-item " + String(activeTab == "gps" ? "active" : "") + "'>📍 GPS</a>";
            html += "<a href='/battery' class='nav-item " + String(activeTab == "battery" ? "active" : "") + "'>🔋 Bateria</a>";
            html += "<a href='/control' class='nav-item " + String(activeTab == "control" ? "active" : "") + "'>🎮 Comandos</a>";
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

        // Card 1: Estado Atual da Conexão
        html += "<div class='card'>";
        html += "<div class='card-title'>Estado da Ligação de Rede</div>";
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
                html += "<tr><td><strong>" + n.ssid + "</strong></td>";
                html += "<td>";
                if (n.is_default) {
                    html += "<span class='badge' style='background:#065f46;color:#6ee7b7;'>★ PADRÃO (DEFAULT)</span>";
                } else {
                    html += "<span style='color:var(--muted);font-size:0.8rem;'>Secundária</span>";
                }
                html += "</td>";
                html += "<td><div class='btn-group'>";
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
        doc["wifi"]["ap"] = _wifi.isAPMode();
        doc["wifi"]["ssid"] = _wifi.getSSID();
        doc["wifi"]["ip"] = _wifi.getIPAddress();
        doc["wifi"]["rssi"] = _wifi.getRSSI();

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
};
