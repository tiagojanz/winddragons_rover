#!/usr/bin/env python3
"""
build_web_assets.py
Generates clean, modern, modular HTML, CSS, and JS files in data/www/
and synchronizes them to the ESP32-C6 MicroSD card via HTTP upload.
"""

import os
import sys
import json
import urllib.request
import urllib.parse
from pathlib import Path

BASE_DIR = Path(__file__).resolve().parent.parent
WWW_DIR = BASE_DIR / "data" / "www"
WWW_DIR.mkdir(parents=True, exist_ok=True)

# ----------------------------------------------------------------------
# 1. STYLE.CSS
# ----------------------------------------------------------------------
STYLE_CSS = """/* WindDragons Rover - Modern Marine Dark UI System */
:root {
  --bg: #0b1329;
  --card: #16203c;
  --card-border: #233258;
  --primary: #06b6d4;
  --primary-hover: #0891b2;
  --text: #f8fafc;
  --muted: #94a3b8;
  --green: #10b981;
  --yellow: #f59e0b;
  --red: #ef4444;
}

* {
  box-sizing: border-box;
  margin: 0;
  padding: 0;
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;
}

body {
  background: var(--bg);
  color: var(--text);
  padding-bottom: 40px;
  line-height: 1.5;
  min-height: 100vh;
}

/* Navbar */
.navbar {
  background: linear-gradient(135deg, #0e1a38, #162348);
  border-bottom: 1px solid var(--card-border);
  padding: 12px 20px;
  display: flex;
  align-items: center;
  justify-content: space-between;
  flex-wrap: wrap;
  gap: 12px;
  position: sticky;
  top: 0;
  z-index: 100;
  box-shadow: 0 4px 12px rgba(0,0,0,0.3);
}

.brand {
  font-size: 1.25rem;
  font-weight: 700;
  color: var(--text);
  display: flex;
  align-items: center;
  gap: 8px;
  text-decoration: none;
}

.brand span {
  color: var(--primary);
}

.badge {
  padding: 5px 12px;
  border-radius: 999px;
  font-size: 0.75rem;
  font-weight: 600;
  text-transform: uppercase;
  display: inline-flex;
  align-items: center;
  gap: 6px;
}

.badge-ap {
  background: #78350f;
  color: #fde68a;
  border: 1px solid #b45309;
}

.badge-sta {
  background: #064e3b;
  color: #a7f3d0;
  border: 1px solid #059669;
}

.nav-links {
  display: flex;
  gap: 6px;
  background: rgba(0, 0, 0, 0.25);
  padding: 4px;
  border-radius: 10px;
  border: 1px solid var(--card-border);
  overflow-x: auto;
}

.nav-item {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  padding: 8px 14px;
  border-radius: 8px;
  color: var(--muted);
  text-decoration: none;
  font-size: 0.875rem;
  font-weight: 600;
  white-space: nowrap;
  transition: all 0.2s ease;
}

.nav-item:hover {
  color: var(--text);
  background: rgba(255, 255, 255, 0.05);
}

.nav-item.active {
  background: var(--primary);
  color: #0f172a;
}

/* Layout */
.container {
  max-width: 960px;
  margin: 24px auto;
  padding: 0 16px;
}

.card {
  background: var(--card);
  border: 1px solid var(--card-border);
  border-radius: 14px;
  padding: 20px;
  margin-bottom: 20px;
  box-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.3);
}

.card-title {
  font-size: 1.1rem;
  font-weight: 700;
  margin-bottom: 14px;
  display: flex;
  align-items: center;
  justify-content: space-between;
  color: var(--text);
}

.grid-2 {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
  gap: 16px;
}

.grid-3 {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
  gap: 14px;
}

.grid-4 {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(140px, 1fr));
  gap: 12px;
}

.stat-box {
  background: rgba(0, 0, 0, 0.2);
  padding: 14px;
  border-radius: 10px;
  border: 1px solid rgba(255, 255, 255, 0.05);
}

.stat-label {
  font-size: 0.75rem;
  color: var(--muted);
  text-transform: uppercase;
  font-weight: 600;
}

.stat-value {
  font-size: 1.35rem;
  font-weight: 700;
  margin-top: 4px;
  color: var(--text);
}

/* Buttons */
.btn {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  padding: 10px 18px;
  border-radius: 8px;
  font-weight: 600;
  font-size: 0.875rem;
  cursor: pointer;
  border: none;
  transition: all 0.2s;
  text-decoration: none;
  gap: 6px;
}

.btn-primary {
  background: var(--primary);
  color: #0f172a;
}

.btn-primary:hover {
  background: var(--primary-hover);
}

.btn-danger {
  background: var(--red);
  color: #fff;
}

.btn-danger:hover {
  background: #dc2626;
}

.btn-secondary {
  background: #334155;
  color: #fff;
}

.btn-secondary:hover {
  background: #475569;
}

.btn-group {
  display: flex;
  gap: 8px;
  flex-wrap: wrap;
}

/* Tables */
table {
  width: 100%;
  border-collapse: collapse;
  margin-top: 10px;
}

th, td {
  padding: 10px 12px;
  text-align: left;
  border-bottom: 1px solid var(--card-border);
  font-size: 0.875rem;
}

th {
  color: var(--muted);
  font-weight: 600;
}

tr:hover td {
  background: rgba(255, 255, 255, 0.02);
}

/* Inputs & Form Controls */
input, select, textarea {
  width: 100%;
  padding: 10px 12px;
  background: #0f172a;
  border: 1px solid var(--card-border);
  color: #fff;
  border-radius: 8px;
  margin-top: 6px;
  font-size: 0.9rem;
  outline: none;
}

input:focus, select:focus, textarea:focus {
  border-color: var(--primary);
}

.form-group {
  margin-bottom: 14px;
}

.progress-bg {
  width: 100%;
  height: 16px;
  background: #0f172a;
  border-radius: 8px;
  overflow: hidden;
  margin-top: 8px;
}

.progress-bar {
  height: 100%;
  transition: width 0.3s ease;
}

.slider {
  width: 100%;
  -webkit-appearance: none;
  height: 10px;
  background: #0f172a;
  border-radius: 5px;
  outline: none;
}

.slider::-webkit-slider-thumb {
  -webkit-appearance: none;
  width: 24px;
  height: 24px;
  border-radius: 50%;
  background: var(--primary);
  cursor: pointer;
  box-shadow: 0 0 10px rgba(6, 182, 212, 0.5);
}

/* File Explorer Custom Styles */
.explorer-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(130px, 1fr));
  gap: 12px;
  margin-top: 16px;
}

.explorer-card {
  background: rgba(0, 0, 0, 0.2);
  border: 1px solid var(--card-border);
  border-radius: 10px;
  padding: 14px 10px;
  display: flex;
  flex-direction: column;
  align-items: center;
  text-align: center;
  cursor: pointer;
  transition: all 0.2s;
  position: relative;
}

.explorer-card:hover {
  background: rgba(6, 182, 212, 0.08);
  border-color: var(--primary);
  transform: translateY(-2px);
}

.explorer-card-icon {
  font-size: 2.2rem;
  margin-bottom: 8px;
}

.explorer-card-name {
  font-size: 0.85rem;
  font-weight: 600;
  word-break: break-all;
  max-width: 100%;
  display: -webkit-box;
  -webkit-line-clamp: 2;
  -webkit-box-orient: vertical;
  overflow: hidden;
}

.explorer-card-size {
  font-size: 0.72rem;
  color: var(--muted);
  margin-top: 4px;
}

.explorer-card-actions {
  display: flex;
  gap: 4px;
  margin-top: 8px;
}

.explorer-card-actions button {
  background: #334155;
  border: none;
  color: #fff;
  border-radius: 4px;
  padding: 4px 6px;
  font-size: 0.75rem;
  cursor: pointer;
}

.explorer-card-actions button:hover {
  background: var(--primary);
  color: #0f172a;
}

.breadcrumb-bar {
  display: flex;
  align-items: center;
  gap: 6px;
  background: rgba(0, 0, 0, 0.25);
  padding: 8px 12px;
  border-radius: 8px;
  border: 1px solid var(--card-border);
  overflow-x: auto;
  white-space: nowrap;
}

.breadcrumb-item {
  color: var(--muted);
  cursor: pointer;
  font-size: 0.85rem;
  display: inline-flex;
  align-items: center;
  gap: 4px;
}

.breadcrumb-item:hover {
  color: var(--primary);
}

.breadcrumb-sep {
  color: var(--card-border);
}

/* Modal */
.modal-overlay {
  display: none;
  position: fixed;
  inset: 0;
  background: rgba(0, 0, 0, 0.7);
  backdrop-filter: blur(4px);
  z-index: 200;
  align-items: center;
  justify-content: center;
  padding: 16px;
}

.modal-card {
  background: var(--card);
  border: 1px solid var(--card-border);
  border-radius: 14px;
  max-width: 600px;
  width: 100%;
  padding: 24px;
  box-shadow: 0 20px 25px -5px rgba(0,0,0,0.5);
}

/* Toast */
#toast {
  position: fixed;
  bottom: 24px;
  right: 24px;
  padding: 12px 20px;
  border-radius: 8px;
  font-weight: 600;
  font-size: 0.875rem;
  display: none;
  z-index: 300;
  box-shadow: 0 10px 15px -3px rgba(0,0,0,0.4);
}

/* Font Awesome 6 Pro Solid (carregada do Cartão SD) */
@font-face {
  font-family: 'Font Awesome 6 Pro';
  src: url('/system/fonts/Font%20Awesome%206%20Pro-Solid-900.woff2') format('woff2'),
       url('/system/fonts/Font%20Awesome%206%20Pro-Solid-900.otf') format('opentype');
  font-weight: 900;
  font-style: normal;
  font-display: block;
}

.fa, .fas, .fa-solid {
  font-family: 'Font Awesome 6 Pro' !important;
  font-weight: 900;
  font-style: normal;
  font-variant: normal;
  line-height: 1;
  text-rendering: auto;
  display: inline-block;
  vertical-align: -0.125em;
  -webkit-font-smoothing: antialiased;
  -moz-osx-font-smoothing: grayscale;
}

.btn i, .nav-item i, .brand i { pointer-events: none; }

.fa-robot:before { content: '\\f544'; }
.fa-wifi:before { content: '\\f1eb'; }
.fa-gear:before, .fa-cog:before { content: '\\f013'; }
.fa-folder:before { content: '\\f07b'; }
.fa-folder-open:before { content: '\\f07c'; }
.fa-folder-plus:before { content: '\\f65e'; }
.fa-location-dot:before, .fa-map-marker-alt:before { content: '\\f3c5'; }
.fa-battery-full:before { content: '\\f240'; }
.fa-battery-three-quarters:before { content: '\\f241'; }
.fa-battery-half:before { content: '\\f242'; }
.fa-battery-quarter:before { content: '\\f243'; }
.fa-battery-empty:before { content: '\\f244'; }
.fa-gamepad:before { content: '\\f11b'; }
.fa-floppy-disk:before, .fa-save:before { content: '\\f0c7'; }
.fa-sun:before { content: '\\f185'; }
.fa-moon:before { content: '\\f186'; }
.fa-cloud-sun:before { content: '\\f6c4'; }
.fa-bolt:before { content: '\\f0e7'; }
.fa-tower-broadcast:before, .fa-broadcast-tower:before { content: '\\f519'; }
.fa-eye:before { content: '\\f06e'; }
.fa-eye-slash:before { content: '\\f070'; }
.fa-rotate:before, .fa-sync:before { content: '\\f021'; }
.fa-arrow-right:before { content: '\\f061'; }
.fa-arrow-left:before { content: '\\f060'; }
.fa-arrow-up:before { content: '\\f062'; }
.fa-arrow-turn-up:before, .fa-level-up-alt:before { content: '\\f3bf'; }
.fa-circle-check:before { content: '\\f058'; }
.fa-circle-xmark:before { content: '\\f057'; }
.fa-circle:before { content: '\\f111'; }
.fa-star:before { content: '\\f005'; }
.fa-plug:before { content: '\\f1e6'; }
.fa-network-wired:before { content: '\\f6ff'; }
.fa-map:before { content: '\\f279'; }
.fa-compass:before { content: '\\f14e'; }
.fa-gauge-high:before, .fa-tachometer-alt:before { content: '\\f625'; }
.fa-anchor:before { content: '\\f13d'; }
.fa-stop:before { content: '\\f04d'; }
.fa-hand:before { content: '\\f256'; }
.fa-lock:before { content: '\\f023'; }
.fa-lock-open:before { content: '\\f3c1'; }
.fa-house:before, .fa-home:before { content: '\\f015'; }
.fa-bell:before { content: '\\f0f3'; }
.fa-triangle-exclamation:before { content: '\\f071'; }
.fa-trash-can:before, .fa-trash:before { content: '\\f2ed'; }
.fa-file:before { content: '\\f15b'; }
.fa-file-lines:before { content: '\\f15c'; }
.fa-file-pen:before { content: '\\f31c'; }
.fa-download:before { content: '\\f019'; }
.fa-upload:before { content: '\\f093'; }
.fa-cloud-arrow-up:before { content: '\\f0ee'; }
.fa-magnifying-glass:before, .fa-search:before { content: '\\f002'; }
.fa-microchip:before { content: '\\f2db'; }
.fa-sd-card:before { content: '\\f7c2'; }
.fa-signal:before { content: '\\f012'; }
.fa-satellite:before { content: '\\f7bf'; }
.fa-sliders:before { content: '\\f1de'; }
.fa-xmark:before { content: '\\f00d'; }
.fa-plus:before { content: '\\2b'; }
.fa-font:before { content: '\\f031'; }
.fa-file-code:before { content: '\\f1c9'; }
.fa-file-image:before { content: '\\f1c5'; }
.fa-file-zipper:before { content: '\\f1c6'; }
.fa-table-cells:before, .fa-th:before { content: '\\f00a'; }
.fa-list:before { content: '\\f03a'; }
.fa-copy:before { content: '\\f0c5'; }
.fa-filter:before { content: '\\f0b0'; }
"""

# ----------------------------------------------------------------------
# 2. APP.JS
# ----------------------------------------------------------------------
APP_JS = """/* WindDragons Rover - Shared Client Logic */
function showToast(msg, isError = false) {
  let t = document.getElementById('toast');
  if (!t) {
    t = document.createElement('div');
    t.id = 'toast';
    document.body.appendChild(t);
  }
  t.innerText = msg;
  t.style.background = isError ? '#ef4444' : '#10b981';
  t.style.color = '#fff';
  t.style.display = 'block';
  setTimeout(() => { t.style.display = 'none'; }, 3500);
}

function formatBytes(bytes) {
  if (bytes === 0) return '0 B';
  const k = 1024;
  const sizes = ['B', 'KB', 'MB', 'GB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
}

// Background status poller for updating navbar badges
function pollGlobalStatus() {
  fetch('/api/status')
    .then(r => r.json())
    .then(data => {
      const badge = document.getElementById('navWifiBadge');
      if (badge && data.wifi) {
        if (data.wifi.is_ap) {
          badge.className = 'badge badge-ap';
          badge.innerHTML = '<i class="fa-solid fa-tower-broadcast"></i> AP: ' + data.wifi.ssid;
        } else {
          badge.className = 'badge badge-sta';
          badge.innerHTML = '<i class="fa-solid fa-wifi"></i> ' + data.wifi.ip + ' (' + data.wifi.rssi + ' dBm)';
        }
      }
      const roverSpan = document.getElementById('navRoverId');
      if (roverSpan && data.rover_id) {
        roverSpan.innerText = 'Rover-' + data.rover_id;
      }
    })
    .catch(() => {});
}

document.addEventListener('DOMContentLoaded', () => {
  pollGlobalStatus();
  setInterval(pollGlobalStatus, 3000);
});
"""

# Helper to generate HTML with common header & navbar
def generate_html_page(title, active_tab, body_content):
    tabs = [
        ("wifi", "/wifi", "fa-wifi", "WiFi"),
        ("gps", "/gps", "fa-location-dot", "GPS"),
        ("battery", "/battery", "fa-battery-three-quarters", "Bateria"),
        ("control", "/control", "fa-gamepad", "Comandos"),
        ("config", "/config", "fa-gear", "Configurações"),
        ("sd", "/sd", "fa-folder", "Cartão SD")
    ]
    nav_links = ""
    for tid, url, icon, label in tabs:
        act = "active" if active_tab == tid else ""
        nav_links += f'<a href="{url}" class="nav-item {act}"><i class="fa-solid {icon}"></i> {label}</a>\n'

    return f"""<!DOCTYPE html>
<html lang="pt">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>{title} - WindDragons Rover</title>
  <link rel="stylesheet" href="/style.css">
  <script src="/app.js"></script>
</head>
<body>
  <header class="navbar">
    <a href="/" class="brand">
      <i class="fa-solid fa-robot"></i> WindDragons <span id="navRoverId">Rover</span>
    </a>
    <nav class="nav-links">
      {nav_links}
    </nav>
    <div>
      <span id="navWifiBadge" class="badge badge-sta">
        <i class="fa-solid fa-wifi"></i> A carregar...
      </span>
    </div>
  </header>

  <main class="container">
    {body_content}
  </main>

  <footer style="text-align:center;color:var(--muted);font-size:0.75rem;margin-top:24px;">
    WindDragons Autonomous Surface Rover / Buoy &copy; 2026 &bull; MicroSD /www/rover/ Frontend
  </footer>
</body>
</html>
"""

def generate_base_html_page(title, active_tab, body_content):
    tabs = [
        ("wifi", "/wifi", "fa-wifi", "WiFi Base"),
        ("gps", "/gps", "fa-location-dot", "GPS Frota"),
        ("battery", "/battery", "fa-battery-three-quarters", "Bateria Frota"),
        ("control", "/control", "fa-gamepad", "Comandos"),
        ("sd", "/sd", "fa-folder", "Cartão SD")
    ]
    nav_links = ""
    for tid, url, icon, label in tabs:
        act = "active" if active_tab == tid else ""
        nav_links += f'<a href="{url}" class="nav-item {act}"><i class="fa-solid {icon}"></i> {label}</a>\n'

    return f"""<!DOCTYPE html>
<html lang="pt">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>{title} - WindDragons Base Station</title>
  <link rel="stylesheet" href="/style.css">
  <script src="/app.js"></script>
</head>
<body>
  <header class="navbar">
    <a href="/" class="brand">
      <i class="fa-solid fa-tower-broadcast"></i> WindDragons <span>Base Station</span>
    </a>
    <nav class="nav-links">
      {nav_links}
    </nav>
    <div style="display:flex;align-items:center;gap:10px;">
      <span id="navWifiBadge" class="badge badge-sta">
        <i class="fa-solid fa-wifi"></i> A carregar...
      </span>
    </div>
  </header>

  <main class="container">
    {body_content}
  </main>

  <footer style="text-align:center;color:var(--muted);font-size:0.75rem;margin-top:24px;">
    WindDragons Telemetry Base Station &copy; 2026 &bull; MicroSD /www/base/ Frontend
  </footer>
</body>
</html>
"""


# ----------------------------------------------------------------------
# 3. PAGES
# ----------------------------------------------------------------------

# GPS Page (index.html & gps.html)
GPS_CONTENT = """
<div class="card">
  <div class="card-title">
    <span><i class="fa-solid fa-location-dot"></i> Posicionamento e Navegação Quectel LC29H</span>
  </div>

  <div class="grid-4">
    <div class="stat-box">
      <div class="stat-label">Estado Satélites</div>
      <div class="stat-value" id="gpsFix" style="color:var(--red);">A ler...</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Velocidade</div>
      <div class="stat-value" id="gpsSpeed" style="color:var(--primary);">0.0 kn</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Rumo Proa</div>
      <div class="stat-value" id="gpsHeading">0°</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Modo Motor</div>
      <div class="stat-value" id="motorMode" style="color:var(--yellow);">idle</div>
    </div>
  </div>

  <div class="grid-2" style="margin-top:16px;">
    <div class="stat-box">
      <div class="stat-label">Latitude (WGS84)</div>
      <div class="stat-value" id="gpsLat" style="font-size:1.6rem;">0.000000</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Longitude (WGS84)</div>
      <div class="stat-value" id="gpsLng" style="font-size:1.6rem;">0.000000</div>
    </div>
  </div>

  <div style="margin-top:20px;" class="btn-group">
    <a id="osmLink" href="https://www.openstreetmap.org" target="_blank" class="btn btn-primary">
      <i class="fa-solid fa-map"></i> Ver no OpenStreetMap
    </a>
    <a id="gmapsLink" href="https://maps.google.com" target="_blank" class="btn btn-secondary">
      <i class="fa-solid fa-location-dot"></i> Google Maps
    </a>
  </div>
</div>

<script>
setInterval(() => {
  fetch('/api/status')
    .then(r => r.json())
    .then(data => {
      const g = data.gps;
      if (!g) return;
      document.getElementById('gpsFix').innerText = g.fix ? ('FIX (' + g.satellites + ' sats)') : 'SEM FIX';
      document.getElementById('gpsFix').style.color = g.fix ? 'var(--green)' : 'var(--red)';
      document.getElementById('gpsSpeed').innerText = (g.speed_kn || 0).toFixed(1) + ' kn';
      document.getElementById('gpsHeading').innerText = (g.heading || 0).toFixed(0) + '°';
      document.getElementById('motorMode').innerText = (data.actuators && data.actuators.motor_status_str) ? data.actuators.motor_status_str : 'idle';
      document.getElementById('gpsLat').innerText = (g.lat || 0).toFixed(6);
      document.getElementById('gpsLng').innerText = (g.lng || 0).toFixed(6);
      document.getElementById('osmLink').href = 'https://www.openstreetmap.org/?mlat=' + g.lat + '&mlon=' + g.lng + '#map=18/' + g.lat + '/' + g.lng;
      document.getElementById('gmapsLink').href = 'https://maps.google.com/?q=' + g.lat + ',' + g.lng;
    }).catch(() => {});
}, 1000);
</script>
"""

# Battery Page
BATTERY_CONTENT = """
<div class="card">
  <div class="card-title">
    <span><i class="fa-solid fa-battery-full"></i> Monitoramento APM Power Module (28V / 90A)</span>
  </div>

  <div style="text-align:center;padding:16px 0;">
    <div style="font-size:3.5rem;font-weight:800;color:var(--green);" id="batPct">--%</div>
    <div class="progress-bg" style="max-width:400px;margin:0 auto;">
      <div id="batBar" class="progress-bar" style="width:0%;background:var(--green);"></div>
    </div>
  </div>

  <div class="grid-4">
    <div class="stat-box">
      <div class="stat-label">Tensão Total</div>
      <div class="stat-value" id="vTotVal" style="color:var(--text);">-- V</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Média p/ Célula</div>
      <div class="stat-value" id="vCellVal" style="color:var(--primary);">-- V/cel</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Corrente Instantânea</div>
      <div class="stat-value" id="currVal" style="color:var(--text);">-- A</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Consumo Acumulado</div>
      <div class="stat-value" id="consVal" style="color:var(--yellow);">-- mAh</div>
    </div>
  </div>

  <div style="margin-top:20px;">
    <div class="stat-label" style="margin-bottom:8px;">Distribuição Estimada de Tensão (4S LiPo):</div>
    <div class="grid-4">
      <div class="stat-box" style="text-align:center;">
        <div class="stat-label">Célula 1</div>
        <div class="stat-value cell-v" style="font-size:1.2rem;color:var(--primary);">-- V</div>
      </div>
      <div class="stat-box" style="text-align:center;">
        <div class="stat-label">Célula 2</div>
        <div class="stat-value cell-v" style="font-size:1.2rem;color:var(--primary);">-- V</div>
      </div>
      <div class="stat-box" style="text-align:center;">
        <div class="stat-label">Célula 3</div>
        <div class="stat-value cell-v" style="font-size:1.2rem;color:var(--primary);">-- V</div>
      </div>
      <div class="stat-box" style="text-align:center;">
        <div class="stat-label">Célula 4</div>
        <div class="stat-value cell-v" style="font-size:1.2rem;color:var(--primary);">-- V</div>
      </div>
    </div>
  </div>
</div>

<script>
setInterval(() => {
  fetch('/api/status')
    .then(r => r.json())
    .then(data => {
      const b = data.battery;
      if (!b) return;
      const p = b.pct || 0;
      const v = b.voltage || 0;
      const vc = b.cell_avg || 0;
      const col = p < 25 ? 'var(--red)' : (p < 50 ? 'var(--yellow)' : 'var(--green)');
      document.getElementById('batPct').innerText = p + '%';
      document.getElementById('batPct').style.color = col;
      document.getElementById('batBar').style.width = p + '%';
      document.getElementById('batBar').style.background = col;
      document.getElementById('vTotVal').innerText = v.toFixed(2) + ' V';
      document.getElementById('vCellVal').innerText = vc.toFixed(2) + ' V/cel';
      document.getElementById('currVal').innerText = (b.current_a || 0).toFixed(2) + ' A';
      document.getElementById('consVal').innerText = (b.consumed_mah || 0).toFixed(0) + ' mAh';
      document.querySelectorAll('.cell-v').forEach(el => el.innerText = vc.toFixed(2) + ' V');
    }).catch(() => {});
}, 1000);
</script>
"""

# Control Page
CONTROL_CONTENT = """
<div class="card">
  <div class="card-title">
    <span><i class="fa-solid fa-gamepad"></i> Painel de Controlo Direto da Boia</span>
  </div>
  <div class="grid-4" style="margin-bottom:16px;">
    <div class="stat-box">
      <div class="stat-label">Modo Motor</div>
      <div class="stat-value" id="curMotor" style="color:var(--yellow);font-size:1.1rem;">idle</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Estado Âncora</div>
      <div class="stat-value" id="curAnchor" style="color:var(--primary);font-size:1.1rem;">recolhida</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Profundidade</div>
      <div class="stat-value" id="curDepth">0.0 m</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Cremalheira</div>
      <div class="stat-value" id="curRack" style="color:var(--green);font-size:1.1rem;">TRAVADA</div>
    </div>
  </div>
</div>

<div class="card">
  <div class="card-title">
    <i class="fa-solid fa-gauge-high"></i> Motor Principal (ESC) & Leme de Direção
  </div>
  <div class="grid-2">
    <div>
      <div class="stat-label">Aceleração ESC: <span id="thrVal" style="font-size:1.1rem;color:var(--primary);">0%</span></div>
      <input type="range" id="throttleSlider" min="-100" max="100" value="0" class="slider" oninput="updateSliders()" onchange="sendActuators()">
      <div class="btn-group" style="margin-top:12px;">
        <button type="button" onclick="setThrottle(0)" class="btn btn-secondary">0% (Stop)</button>
        <button type="button" onclick="setThrottle(25)" class="btn btn-secondary">+25%</button>
        <button type="button" onclick="setThrottle(50)" class="btn btn-secondary">+50%</button>
        <button type="button" onclick="setThrottle(100)" class="btn btn-primary">+100%</button>
        <button type="button" onclick="setThrottle(-50)" class="btn btn-secondary">-50% (Ré)</button>
      </div>
    </div>

    <div>
      <div class="stat-label">Ângulo do Leme: <span id="rudVal" style="font-size:1.1rem;color:var(--primary);">0%</span></div>
      <input type="range" id="rudderSlider" min="-100" max="100" value="0" class="slider" oninput="updateSliders()" onchange="sendActuators()">
      <div class="btn-group" style="margin-top:12px;">
        <button type="button" onclick="setRudder(-75)" class="btn btn-secondary"><i class="fa-solid fa-arrow-left"></i> Bombordo (-75%)</button>
        <button type="button" onclick="setRudder(0)" class="btn btn-secondary"><i class="fa-solid fa-compass"></i> Centro (0%)</button>
        <button type="button" onclick="setRudder(75)" class="btn btn-secondary">Estibordo (+75%) <i class="fa-solid fa-arrow-right"></i></button>
      </div>
    </div>
  </div>
</div>

<div class="card">
  <div class="card-title"><i class="fa-solid fa-anchor"></i> Guincho da Âncora & Cremalheira de Travão</div>
  <div class="btn-group">
    <button type="button" onclick="sendAction(3)" class="btn btn-primary"><i class="fa-solid fa-anchor"></i> Lançar Âncora (Soltar)</button>
    <button type="button" onclick="sendAction(4)" class="btn btn-primary"><i class="fa-solid fa-arrow-up"></i> Recolher Âncora (Guincho)</button>
    <button type="button" onclick="sendAction(5)" class="btn btn-secondary"><i class="fa-solid fa-stop"></i> Parar Guincho</button>
    <button type="button" onclick="sendAction(6)" class="btn btn-secondary"><i class="fa-solid fa-lock"></i> Alternar Travão Cremalheira</button>
  </div>
</div>

<div class="card">
  <div class="card-title"><i class="fa-solid fa-sliders"></i> Modos de Operação & Paragem Imediata</div>
  <div class="btn-group">
    <button type="button" onclick="sendAction(1)" class="btn" style="background:#0284c7;color:#fff;"><i class="fa-solid fa-location-dot"></i> Hold Station (GPS)</button>
    <button type="button" onclick="sendAction(7)" class="btn" style="background:#7c3aed;color:#fff;"><i class="fa-solid fa-house"></i> Return to Launch (RTL)</button>
    <button type="button" onclick="sendAction(10)" class="btn btn-secondary"><i class="fa-solid fa-gamepad"></i> Modo Manual</button>
    <button type="button" onclick="sendAction(8)" class="btn" style="background:var(--yellow);color:#0f172a;"><i class="fa-solid fa-bell"></i> Alarme Sonoro / Strobe</button>
    <button type="button" onclick="sendEmergencyStop()" class="btn btn-danger" style="font-size:1rem;padding:12px 24px;"><i class="fa-solid fa-hand"></i> PARAGEM DE EMERGÊNCIA</button>
  </div>
</div>

<script>
function updateSliders() {
  document.getElementById('thrVal').innerText = document.getElementById('throttleSlider').value + '%';
  document.getElementById('rudVal').innerText = document.getElementById('rudderSlider').value + '%';
}
function setThrottle(v) {
  document.getElementById('throttleSlider').value = v;
  updateSliders();
  sendActuators();
}
function setRudder(v) {
  document.getElementById('rudderSlider').value = v;
  updateSliders();
  sendActuators();
}
function sendActuators() {
  const t = parseInt(document.getElementById('throttleSlider').value);
  const r = parseInt(document.getElementById('rudderSlider').value);
  fetch('/api/control/actuator', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ throttle: t, rudder: r })
  });
}
function sendAction(a) {
  fetch('/api/control/action', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ action: a })
  })
  .then(r => r.json())
  .then(res => showToast(res.msg));
}
function sendEmergencyStop() {
  setThrottle(0);
  setRudder(0);
  sendAction(2);
}
setInterval(() => {
  fetch('/api/status')
    .then(r => r.json())
    .then(data => {
      const a = data.actuators;
      if (!a) return;
      document.getElementById('curMotor').innerText = a.motor_status_str || 'idle';
      document.getElementById('curAnchor').innerText = a.anchor_status_str || 'recolhida';
      document.getElementById('curDepth').innerText = (a.anchor_depth_m || 0).toFixed(1) + ' m';
      document.getElementById('curRack').innerText = a.rack_released ? 'LIVRE' : 'TRAVADA';
    }).catch(() => {});
}, 1000);
</script>
"""

# Config Page
CONFIG_CONTENT = """
<div class="card">
  <div class="card-title">
    <span><i class="fa-solid fa-gear"></i> Definições Gerais do Rover</span>
  </div>
  <p style="color:var(--muted);font-size:0.875rem;margin-bottom:14px;">
    As opções abaixo são gravadas sincronizadamente no ficheiro <code>/config.json</code> do Cartão MicroSD e na partição não-volátil NVS do ESP32.
  </p>

  <div class="grid-4">
    <div class="stat-box">
      <div class="stat-label">ID Ativo</div>
      <div class="stat-value" id="summaryId" style="color:var(--primary);">Rover-1</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Brilho do Ecrã</div>
      <div class="stat-value" id="summaryBright" style="color:var(--yellow);">80%</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">SSID AP Local</div>
      <div class="stat-value" id="summarySsid" style="font-size:0.95rem;word-break:break-all;">WindDragon-AP</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Estado do MicroSD</div>
      <div class="stat-value" id="summarySd" style="font-size:0.95rem;color:var(--green);">Cartão Pronto</div>
    </div>
  </div>
</div>

<form id="configForm" onsubmit="saveAllConfig(event)">
  <div class="card">
    <div class="card-title">
      <span><i class="fa-solid fa-sun"></i> Brilho do Ecrã (Display LCD ST7789)</span>
      <span id="brightValBadge" class="badge" style="background:#1e293b;color:var(--primary);font-size:0.9rem;font-weight:700;">80%</span>
    </div>
    <p style="color:var(--muted);font-size:0.875rem;margin-bottom:16px;">
      Ajuste o brilho da retroiluminação por PWM (LEDC GPIO 22). Ao mover o cursor, o ecrã atualiza em tempo real.
    </p>

    <div style="background:rgba(0,0,0,0.25);padding:18px;border-radius:12px;border:1px solid var(--card-border);">
      <div style="display:flex;align-items:center;justify-content:space-between;margin-bottom:10px;">
        <span style="font-size:0.85rem;color:var(--muted);"><i class="fa-solid fa-moon"></i> 5% Mínimo</span>
        <span id="brightDisplayVal" style="font-size:1.8rem;font-weight:700;color:var(--primary);">80%</span>
        <span style="font-size:0.85rem;color:var(--muted);"><i class="fa-solid fa-sun"></i> 100% Máximo</span>
      </div>
      <input type="range" id="cfg_bright" min="5" max="100" step="1" value="80" class="slider" oninput="onBrightSlide(this.value)" onchange="onBrightChange(this.value)">

      <div style="display:flex;gap:8px;margin-top:16px;flex-wrap:wrap;">
        <button type="button" onclick="setPreset(25)" class="btn btn-secondary" style="padding:6px 12px;font-size:0.8rem;"><i class="fa-solid fa-moon"></i> 25% (Poupança)</button>
        <button type="button" onclick="setPreset(50)" class="btn btn-secondary" style="padding:6px 12px;font-size:0.8rem;"><i class="fa-solid fa-cloud-sun"></i> 50% (Normal)</button>
        <button type="button" onclick="setPreset(75)" class="btn btn-secondary" style="padding:6px 12px;font-size:0.8rem;"><i class="fa-solid fa-sun"></i> 75% (Dia)</button>
        <button type="button" onclick="setPreset(100)" class="btn btn-secondary" style="padding:6px 12px;font-size:0.8rem;"><i class="fa-solid fa-bolt"></i> 100% (Sol Máximo)</button>
      </div>
    </div>
  </div>

  <div class="card">
    <div class="card-title">
      <span><i class="fa-solid fa-robot"></i> Identificação do Rover & Rádio LoRa</span>
    </div>
    <div class="grid-2">
      <div class="form-group">
        <label class="stat-label">ID do Rover (1 - 254):</label>
        <input type="number" id="cfg_id" min="1" max="254" required value="1" oninput="updateIdPreview(this.value)">
        <small style="color:var(--muted);font-size:0.75rem;margin-top:4px;display:block;">
          Identificador único: <strong id="roverTag" style="color:var(--primary);">ROVER-01</strong>
        </small>
      </div>
      <div class="form-group">
        <label class="stat-label">Tipo de Equipamento:</label>
        <input type="text" disabled value="WindDragons Autonomous Surface Rover / Buoy" style="background:#0b1120;color:var(--muted);cursor:not-allowed;">
        <small style="color:var(--muted);font-size:0.75rem;margin-top:4px;display:block;">Hardware: ESP32-C6 + ST7789 1.47" + LoRa SX1278 + MicroSD</small>
      </div>
    </div>
  </div>

  <div class="card">
    <div class="card-title">
      <span><i class="fa-solid fa-tower-broadcast"></i> Ponto de Acesso Local (WiFi AP)</span>
    </div>
    <div class="grid-2">
      <div class="form-group">
        <label class="stat-label">Nome da Rede AP (SSID):</label>
        <input type="text" id="cfg_ssid" required value="WindDragon-AP">
        <small style="color:var(--muted);font-size:0.75rem;margin-top:4px;display:block;">Nome do WiFi emitido quando em modo AP</small>
      </div>
      <div class="form-group">
        <label class="stat-label">Palavra-passe do AP (mín. 8 caracteres):</label>
        <div style="display:flex;gap:6px;">
          <input type="password" id="cfg_pass" minlength="8" required value="12345678" style="margin-top:0;">
          <button type="button" onclick="togglePass()" class="btn btn-secondary" style="padding:0 12px;font-size:0.85rem;"><i id="passEye" class="fa-solid fa-eye"></i></button>
        </div>
        <small style="color:var(--muted);font-size:0.75rem;margin-top:4px;display:block;">Segurança WPA2-PSK para acesso local</small>
      </div>
    </div>
  </div>

  <div class="card" style="border-color:var(--primary);">
    <div class="card-title">
      <span><i class="fa-solid fa-floppy-disk"></i> Gravar e Sincronizar</span>
    </div>
    <div id="statusMsg" style="display:none;padding:12px;border-radius:8px;margin-bottom:14px;font-size:0.9rem;"></div>
    <div class="btn-group">
      <button type="submit" class="btn btn-primary" style="padding:12px 24px;font-size:1rem;">
        <i class="fa-solid fa-floppy-disk"></i> Guardar no Ficheiro (/config.json + NVS)
      </button>
      <button type="button" onclick="loadConfig()" class="btn btn-secondary">
        <i class="fa-solid fa-rotate"></i> Recarregar do Ficheiro
      </button>
      <a href="/sd" class="btn btn-secondary">
        <i class="fa-solid fa-folder-open"></i> Explorar Ficheiros MicroSD
      </a>
    </div>
  </div>
</form>

<script>
let slideTimer = null;
function onBrightSlide(v) {
  document.getElementById('brightDisplayVal').innerText = v + '%';
  document.getElementById('brightValBadge').innerText = v + '%';
  document.getElementById('summaryBright').innerText = v + '%';
  if (slideTimer) clearTimeout(slideTimer);
  slideTimer = setTimeout(() => {
    fetch('/api/display/brightness?val=' + v, { method: 'POST' }).catch(() => {});
  }, 60);
}
function onBrightChange(v) {
  fetch('/api/display/brightness?val=' + v, { method: 'POST' }).catch(() => {});
}
function setPreset(v) {
  document.getElementById('cfg_bright').value = v;
  onBrightSlide(v);
  onBrightChange(v);
}
function updateIdPreview(id) {
  const num = parseInt(id) || 1;
  const tag = 'ROVER-' + (num < 10 ? '0' : '') + num;
  const el = document.getElementById('roverTag');
  if (el) el.innerText = tag;
}
function togglePass() {
  const p = document.getElementById('cfg_pass');
  const eye = document.getElementById('passEye');
  const isPass = (p.type === 'password');
  p.type = isPass ? 'text' : 'password';
  if (eye) eye.className = isPass ? 'fa-solid fa-eye-slash' : 'fa-solid fa-eye';
}
function showStatus(msg, isErr = false) {
  const d = document.getElementById('statusMsg');
  d.style.display = 'block';
  d.style.background = isErr ? '#450a0a' : '#064e3b';
  d.style.color = isErr ? '#fecaca' : '#a7f3d0';
  d.innerHTML = (isErr ? '<i class="fa-solid fa-triangle-exclamation"></i> ' : '<i class="fa-solid fa-circle-check"></i> ') + msg;
  setTimeout(() => { d.style.display = 'none'; }, 4000);
}

function loadConfig() {
  fetch('/api/rover/config')
    .then(r => r.json())
    .then(data => {
      document.getElementById('cfg_id').value = data.rover_id || 1;
      updateIdPreview(data.rover_id || 1);
      document.getElementById('cfg_bright').value = data.screen_brightness || 80;
      onBrightSlide(data.screen_brightness || 80);
      document.getElementById('cfg_ssid').value = data.ap_ssid || 'WindDragon-AP';
      document.getElementById('cfg_pass').value = data.ap_password || '12345678';
      document.getElementById('summaryId').innerText = 'Rover-' + (data.rover_id || 1);
      document.getElementById('summarySsid').innerText = data.ap_ssid || 'WindDragon-AP';
    })
    .catch(() => {});
}

function saveAllConfig(e) {
  e.preventDefault();
  const id = parseInt(document.getElementById('cfg_id').value);
  const bright = parseInt(document.getElementById('cfg_bright').value);
  const ssid = document.getElementById('cfg_ssid').value;
  const pass = document.getElementById('cfg_pass').value;

  fetch('/api/rover/config', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      rover_id: id,
      screen_brightness: bright,
      ap_ssid: ssid,
      ap_password: pass
    })
  })
  .then(r => r.json())
  .then(res => {
    if (res.success) {
      showStatus('Configurações guardadas com sucesso no /config.json e na NVS!');
      loadConfig();
    } else {
      showStatus('Erro ao guardar configurações: ' + (res.error || 'Erro desconhecido'), true);
    }
  })
  .catch(err => {
    showStatus('Falha de rede ao comunicar com o Rover', true);
  });
}

document.addEventListener('DOMContentLoaded', loadConfig);
</script>
"""

# WiFi Manager Page
WIFI_CONTENT = """
<div class="card">
  <div class="card-title">
    <span><i class="fa-solid fa-wifi"></i> Gestão de Redes Wi-Fi</span>
    <button onclick="scanNetworks()" id="scanBtn" class="btn btn-secondary" style="padding:6px 12px;font-size:0.8rem;">
      <i class="fa-solid fa-rotate"></i> Procurar Redes
    </button>
  </div>
  <p style="color:var(--muted);font-size:0.875rem;margin-bottom:16px;">
    Ligue o Rover ao router Wi-Fi do cais ou centro de comando para telemetria em tempo real e acesso local direto.
  </p>

  <div id="scanStatus" style="display:none;color:var(--primary);margin-bottom:12px;font-size:0.85rem;">
    <i class="fa-solid fa-rotate fa-spin"></i> A procurar redes Wi-Fi nas imediações...
  </div>

  <div id="networksList" style="overflow-x:auto;">
    <table>
      <thead>
        <tr>
          <th>Rede (SSID)</th>
          <th>Sinal</th>
          <th>Segurança</th>
          <th>Ação</th>
        </tr>
      </thead>
      <tbody id="scanTableBody">
        <tr><td colspan="4" style="text-align:center;color:var(--muted);">Clique em "Procurar Redes" para ver redes Wi-Fi disponíveis.</td></tr>
      </tbody>
    </table>
  </div>
</div>

<div class="card">
  <div class="card-title">
    <span><i class="fa-solid fa-floppy-disk"></i> Redes Guardadas no Rover</span>
  </div>
  <div id="savedNetworksList" style="overflow-x:auto;">
    <table>
      <thead>
        <tr>
          <th>SSID</th>
          <th>Estado / Predefinição</th>
          <th>Ações</th>
        </tr>
      </thead>
      <tbody id="savedTableBody">
        <tr><td colspan="3" style="text-align:center;color:var(--muted);">A carregar redes guardadas...</td></tr>
      </tbody>
    </table>
  </div>
</div>

<!-- Modal Conectar -->
<div id="connectModal" class="modal-overlay">
  <div class="modal-card">
    <div class="card-title">
      <span><i class="fa-solid fa-plug"></i> Conectar à Rede Wi-Fi</span>
      <button onclick="closeConnectModal()" class="btn btn-secondary" style="padding:4px 8px;"><i class="fa-solid fa-xmark"></i></button>
    </div>
    <form onsubmit="submitConnect(event)">
      <div class="form-group">
        <label class="stat-label">Nome da Rede (SSID):</label>
        <input type="text" id="modalSsid" readonly style="background:#0b1120;color:var(--primary);">
      </div>
      <div class="form-group">
        <label class="stat-label">Palavra-passe:</label>
        <input type="password" id="modalPass" placeholder="Introduza a palavra-passe">
      </div>
      <div class="btn-group" style="justify-content:flex-end;margin-top:16px;">
        <button type="button" onclick="closeConnectModal()" class="btn btn-secondary">Cancelar</button>
        <button type="submit" class="btn btn-primary"><i class="fa-solid fa-plug"></i> Guardar e Ligar</button>
      </div>
    </form>
  </div>
</div>

<script>
function scanNetworks() {
  const status = document.getElementById('scanStatus');
  const tbody = document.getElementById('scanTableBody');
  status.style.display = 'block';
  tbody.innerHTML = '<tr><td colspan="4" style="text-align:center;color:var(--muted);"><i class="fa-solid fa-rotate fa-spin"></i> A varrer frequências 2.4 GHz...</td></tr>';

  fetch('/api/scan')
    .then(r => r.json())
    .then(networks => {
      status.style.display = 'none';
      if (!networks || networks.length === 0) {
        tbody.innerHTML = '<tr><td colspan="4" style="text-align:center;color:var(--yellow);">Nenhuma rede encontrada. Tente novamente.</td></tr>';
        return;
      }
      let html = '';
      networks.forEach(n => {
        const lockIcon = n.secure ? '<i class="fa-solid fa-lock" style="color:var(--yellow);"></i> Protegida' : '<i class="fa-solid fa-lock-open" style="color:var(--muted);"></i> Aberta';
        html += '<tr>';
        html += '<td><strong>' + n.ssid + '</strong></td>';
        html += '<td>' + n.rssi + ' dBm</td>';
        html += '<td>' + lockIcon + '</td>';
        html += '<td><button onclick="openConnectModal(\\'' + n.ssid + '\\')" class="btn btn-primary" style="padding:4px 10px;font-size:0.75rem;"><i class="fa-solid fa-plug"></i> Conectar</button></td>';
        html += '</tr>';
      });
      tbody.innerHTML = html;
    })
    .catch(() => {
      status.style.display = 'none';
      tbody.innerHTML = '<tr><td colspan="4" style="text-align:center;color:var(--red);">Erro ao pesquisar redes.</td></tr>';
    });
}

function openConnectModal(ssid) {
  document.getElementById('modalSsid').value = ssid;
  document.getElementById('modalPass').value = '';
  document.getElementById('connectModal').style.display = 'flex';
}
function closeConnectModal() {
  document.getElementById('connectModal').style.display = 'none';
}

function submitConnect(e) {
  e.preventDefault();
  const ssid = document.getElementById('modalSsid').value;
  const pass = document.getElementById('modalPass').value;
  closeConnectModal();
  showToast('A enviar credenciais ao Rover...');

  fetch('/api/wifi/save', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ ssid: ssid, pass: pass })
  })
  .then(r => r.json())
  .then(res => {
    showToast(res.msg || 'Rede guardada!');
    loadSavedNetworks();
  });
}

function loadSavedNetworks() {
  fetch('/api/status')
    .then(r => r.json())
    .then(data => {
      const tbody = document.getElementById('savedTableBody');
      const curSsid = (data.wifi && !data.wifi.is_ap) ? data.wifi.ssid : '';
      if (!curSsid) {
        tbody.innerHTML = '<tr><td colspan="3" style="text-align:center;color:var(--muted);">Modo AP Ativo ou nenhuma rede conectada no momento.</td></tr>';
        return;
      }
      tbody.innerHTML = '<tr><td><strong>' + curSsid + '</strong></td><td><span class="badge badge-sta"><i class="fa-solid fa-circle-check"></i> Conectada</span></td><td><button onclick="disconnectWifi()" class="btn btn-secondary" style="padding:4px 8px;font-size:0.75rem;"><i class="fa-solid fa-rotate"></i> Reconectar</button></td></tr>';
    }).catch(() => {});
}

function disconnectWifi() {
  fetch('/api/wifi/reconnect', { method: 'POST' })
    .then(() => showToast('A reconectar...'));
}

document.addEventListener('DOMContentLoaded', () => {
  loadSavedNetworks();
  scanNetworks();
});
</script>
"""

# SD Explorer Page (Modern Grid / List dual view with breadcrumbs)
SD_CONTENT = """
<div class="card" style="margin-bottom:16px;">
  <div class="card-title" style="margin-bottom:8px;">
    <span><i class="fa-solid fa-sd-card"></i> Cartão MicroSD TF (SPI Bus)</span>
    <span id="sdBadge" class="badge badge-sta"><i class="fa-solid fa-circle-check"></i> Montado</span>
  </div>
  <div class="grid-4">
    <div class="stat-box">
      <div class="stat-label">Tipo de Cartão</div>
      <div class="stat-value" id="cardType" style="color:var(--primary);font-size:1.1rem;">SDHC / SDXC</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Espaço Total</div>
      <div class="stat-value" id="totalMb">-- MB</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Espaço Usado</div>
      <div class="stat-value" id="usedMb" style="color:var(--yellow);">-- MB</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Espaço Livre</div>
      <div class="stat-value" id="freeMb" style="color:var(--green);">-- MB</div>
    </div>
  </div>
</div>

<div class="card">
  <!-- Toolbar -->
  <div style="display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:10px;margin-bottom:14px;">
    <div class="breadcrumb-bar" id="breadcrumbBar" style="flex:1;min-width:240px;">
      <span class="breadcrumb-item" onclick="changeDir('/')"><i class="fa-solid fa-house"></i> Raiz</span>
    </div>
    <div class="btn-group">
      <button onclick="toggleView('grid')" id="btnViewGrid" class="btn btn-secondary" style="padding:6px 12px;" title="Vista em Grelha">
        <i class="fa-solid fa-table-cells"></i>
      </button>
      <button onclick="toggleView('list')" id="btnViewList" class="btn btn-secondary" style="padding:6px 12px;" title="Vista em Lista">
        <i class="fa-solid fa-list"></i>
      </button>
      <button onclick="openMkdirModal()" class="btn btn-secondary" style="padding:6px 12px;">
        <i class="fa-solid fa-folder-plus"></i> Nova Pasta
      </button>
      <button onclick="openUploadModal()" class="btn btn-primary" style="padding:6px 12px;">
        <i class="fa-solid fa-cloud-arrow-up"></i> Carregar Ficheiro
      </button>
      <button onclick="refreshDir()" class="btn btn-secondary" style="padding:6px 12px;" title="Atualizar">
        <i class="fa-solid fa-rotate"></i>
      </button>
    </div>
  </div>

  <!-- Search Filter -->
  <div style="margin-bottom:14px;">
    <input type="text" id="filterInput" placeholder="Filtrar ficheiros e pastas por nome..." oninput="onFilterChange(this.value)" style="margin-top:0;">
  </div>

  <!-- Views -->
  <div id="viewGrid" class="explorer-grid"></div>
  <div id="viewList" style="display:none;overflow-x:auto;">
    <table>
      <thead>
        <tr>
          <th>Nome</th>
          <th>Tamanho</th>
          <th>Tipo</th>
          <th style="text-align:right;">Ações</th>
        </tr>
      </thead>
      <tbody id="listTbody"></tbody>
    </table>
  </div>
</div>

<!-- Modal Nova Pasta -->
<div id="mkdirModal" class="modal-overlay">
  <div class="modal-card">
    <div class="card-title">
      <span><i class="fa-solid fa-folder-plus"></i> Criar Nova Pasta</span>
      <button onclick="closeModal('mkdirModal')" class="btn btn-secondary" style="padding:4px 8px;"><i class="fa-solid fa-xmark"></i></button>
    </div>
    <form onsubmit="submitMkdir(event)">
      <div class="form-group">
        <label class="stat-label">Caminho Atual:</label>
        <input type="text" id="mkdirCurrentPath" readonly style="background:#0b1120;color:var(--muted);">
      </div>
      <div class="form-group">
        <label class="stat-label">Nome da Nova Pasta:</label>
        <input type="text" id="mkdirName" required placeholder="ex: logs">
      </div>
      <div class="btn-group" style="justify-content:flex-end;margin-top:16px;">
        <button type="button" onclick="closeModal('mkdirModal')" class="btn btn-secondary">Cancelar</button>
        <button type="submit" class="btn btn-primary"><i class="fa-solid fa-folder-plus"></i> Criar Pasta</button>
      </div>
    </form>
  </div>
</div>

<!-- Modal Upload de Ficheiro -->
<div id="uploadModal" class="modal-overlay">
  <div class="modal-card">
    <div class="card-title">
      <span><i class="fa-solid fa-cloud-arrow-up"></i> Carregar Ficheiro para o Cartão SD</span>
      <button onclick="closeModal('uploadModal')" class="btn btn-secondary" style="padding:4px 8px;"><i class="fa-solid fa-xmark"></i></button>
    </div>
    <form onsubmit="submitUpload(event)">
      <div class="form-group">
        <label class="stat-label">Diretório de Destino:</label>
        <input type="text" id="uploadDestDir" readonly style="background:#0b1120;color:var(--muted);">
      </div>
      <div class="form-group">
        <label class="stat-label">Selecionar Ficheiro do Computador / Telemóvel:</label>
        <input type="file" id="uploadFileInput" required style="padding:8px;background:#0f172a;">
      </div>
      <div id="uploadProgressContainer" style="display:none;margin-top:12px;">
        <div class="stat-label" id="uploadProgressText">A enviar...</div>
        <div class="progress-bg"><div id="uploadProgressBar" class="progress-bar" style="width:0%;background:var(--primary);"></div></div>
      </div>
      <div class="btn-group" style="justify-content:flex-end;margin-top:16px;">
        <button type="button" onclick="closeModal('uploadModal')" class="btn btn-secondary">Cancelar</button>
        <button type="submit" id="uploadSubmitBtn" class="btn btn-primary"><i class="fa-solid fa-cloud-arrow-up"></i> Iniciar Upload</button>
      </div>
    </form>
  </div>
</div>

<!-- Modal Visualizador / Editor -->
<div id="viewerModal" class="modal-overlay">
  <div class="modal-card" style="max-width:800px;">
    <div class="card-title">
      <span id="viewerTitle"><i class="fa-solid fa-file-lines"></i> Ficheiro</span>
      <button onclick="closeModal('viewerModal')" class="btn btn-secondary" style="padding:4px 8px;"><i class="fa-solid fa-xmark"></i></button>
    </div>
    <textarea id="viewerContent" rows="18" style="font-family:monospace;font-size:0.85rem;line-height:1.4;white-space:pre;"></textarea>
    <div class="btn-group" style="justify-content:space-between;margin-top:14px;">
      <span id="viewerPath" style="color:var(--muted);font-size:0.8rem;align-self:center;"></span>
      <div class="btn-group">
        <button type="button" onclick="closeModal('viewerModal')" class="btn btn-secondary">Fechar</button>
        <button type="button" onclick="saveEditedFile()" class="btn btn-primary"><i class="fa-solid fa-floppy-disk"></i> Guardar Alterações</button>
      </div>
    </div>
  </div>
</div>

<script>
let currentDir = '/';
let allEntries = [];
let currentFilter = '';
let currentView = 'grid'; // 'grid' | 'list'

function toggleView(v) {
  currentView = v;
  document.getElementById('viewGrid').style.display = v === 'grid' ? 'grid' : 'none';
  document.getElementById('viewList').style.display = v === 'list' ? 'block' : 'none';
  document.getElementById('btnViewGrid').className = 'btn ' + (v === 'grid' ? 'btn-primary' : 'btn-secondary');
  document.getElementById('btnViewList').className = 'btn ' + (v === 'list' ? 'btn-primary' : 'btn-secondary');
}

function getFileIcon(name, isDir) {
  if (isDir) return '<i class="fa-solid fa-folder" style="color:var(--yellow);"></i>';
  const lower = name.toLowerCase();
  if (lower.endsWith('.html') || lower.endsWith('.htm')) return '<i class="fa-solid fa-file-code" style="color:var(--primary);"></i>';
  if (lower.endsWith('.css')) return '<i class="fa-solid fa-file-code" style="color:#38bdf8;"></i>';
  if (lower.endsWith('.js')) return '<i class="fa-solid fa-file-code" style="color:#facc15;"></i>';
  if (lower.endsWith('.json')) return '<i class="fa-solid fa-file-lines" style="color:#4ade80;"></i>';
  if (lower.endsWith('.csv') || lower.endsWith('.txt') || lower.endsWith('.log')) return '<i class="fa-solid fa-file-lines" style="color:var(--muted);"></i>';
  if (lower.endsWith('.woff2') || lower.endsWith('.woff') || lower.endsWith('.otf') || lower.endsWith('.ttf')) return '<i class="fa-solid fa-font" style="color:#c084fc;"></i>';
  if (lower.endsWith('.png') || lower.endsWith('.jpg') || lower.endsWith('.jpeg') || lower.endsWith('.svg') || lower.endsWith('.ico')) return '<i class="fa-solid fa-file-image" style="color:#f472b6;"></i>';
  if (lower.endsWith('.zip') || lower.endsWith('.tar') || lower.endsWith('.gz')) return '<i class="fa-solid fa-file-zipper" style="color:#fb923c;"></i>';
  return '<i class="fa-solid fa-file" style="color:var(--muted);"></i>';
}

function updateBreadcrumbs(dir) {
  const bar = document.getElementById('breadcrumbBar');
  if (dir === '/' || dir === '') {
    bar.innerHTML = '<span class="breadcrumb-item" onclick="changeDir(\\'/\\')"><i class="fa-solid fa-house"></i> Raiz (/)</span>';
    return;
  }
  const parts = dir.split('/').filter(p => p.length > 0);
  let html = '<span class="breadcrumb-item" onclick="changeDir(\\'/\\')"><i class="fa-solid fa-house"></i></span>';
  let accum = '';
  for (let i = 0; i < parts.length; i++) {
    accum += '/' + parts[i];
    html += '<span class="breadcrumb-sep">/</span>';
    const isLast = (i === parts.length - 1);
    if (isLast) {
      html += '<strong style="color:var(--text);font-size:0.85rem;">' + parts[i] + '</strong>';
    } else {
      const p = accum;
      html += '<span class="breadcrumb-item" onclick="changeDir(\\'' + p + '\\')">' + parts[i] + '</span>';
    }
  }
  bar.innerHTML = html;
}

function refreshDir() {
  fetch('/api/sd/list?dir=' + encodeURIComponent(currentDir))
    .then(r => r.json())
    .then(data => {
      document.getElementById('cardType').innerText = data.card_type || 'SD';
      document.getElementById('totalMb').innerText = (data.total_mb || 0) + ' MB';
      document.getElementById('usedMb').innerText = (data.used_mb || 0) + ' MB';
      document.getElementById('freeMb').innerText = (data.free_mb || 0) + ' MB';
      currentDir = data.current_dir || currentDir;
      updateBreadcrumbs(currentDir);
      allEntries = data.files || [];
      renderExplorer();
    })
    .catch(() => {
      showToast('Erro ao listar diretório', true);
    });
}

function changeDir(dir) {
  currentDir = dir;
  refreshDir();
}

function onFilterChange(q) {
  currentFilter = q.trim().toLowerCase();
  renderExplorer();
}

function renderExplorer() {
  const filtered = allEntries.filter(e => {
    if (!currentFilter) return true;
    return e.name.toLowerCase().includes(currentFilter);
  });

  // Ordenar: pastas primeiro, depois ficheiros alfabeticamente
  filtered.sort((a, b) => {
    if (a.is_dir === b.is_dir) return a.name.localeCompare(b.name);
    return a.is_dir ? -1 : 1;
  });

  // Render Grelha
  const gridEl = document.getElementById('viewGrid');
  let gridHtml = '';

  // Se não estiver na raiz, adiciona item para subir
  if (currentDir !== '/' && !currentFilter) {
    const parentDir = currentDir.substring(0, currentDir.lastIndexOf('/')) || '/';
    gridHtml += '<div class="explorer-card" onclick="changeDir(\\'' + parentDir + '\\')">';
    gridHtml += '<div class="explorer-card-icon" style="color:var(--primary);"><i class="fa-solid fa-arrow-turn-up"></i></div>';
    gridHtml += '<div class="explorer-card-name">.. (Subir)</div>';
    gridHtml += '<div class="explorer-card-size">Diretório</div>';
    gridHtml += '</div>';
  }

  filtered.forEach(e => {
    gridHtml += '<div class="explorer-card" onclick="onItemClick(\\'' + e.path + '\\',' + e.is_dir + ')">';
    gridHtml += '<div class="explorer-card-icon">' + getFileIcon(e.name, e.is_dir) + '</div>';
    gridHtml += '<div class="explorer-card-name" title="' + e.name + '">' + e.name + '</div>';
    gridHtml += '<div class="explorer-card-size">' + (e.is_dir ? 'Pasta' : formatBytes(e.size)) + '</div>';
    gridHtml += '<div class="explorer-card-actions" onclick="event.stopPropagation()">';
    if (!e.is_dir) {
      gridHtml += '<button onclick="previewFile(\\'' + e.path + '\\')" title="Ver / Editar"><i class="fa-solid fa-file-pen"></i></button>';
      gridHtml += '<button onclick="downloadFile(\\'' + e.path + '\\')" title="Descarregar"><i class="fa-solid fa-download"></i></button>';
    }
    gridHtml += '<button onclick="deleteItem(\\'' + e.path + '\\',' + e.is_dir + ')" title="Eliminar" style="background:#450a0a;color:#fecaca;"><i class="fa-solid fa-trash-can"></i></button>';
    gridHtml += '</div>';
    gridHtml += '</div>';
  });

  if (filtered.length === 0) {
    gridHtml = '<div style="grid-column:1/-1;text-align:center;padding:30px;color:var(--muted);"><i class="fa-solid fa-folder-open" style="font-size:2rem;margin-bottom:8px;display:block;"></i>Esta pasta está vazia</div>';
  }
  gridEl.innerHTML = gridHtml;

  // Render Lista
  const listEl = document.getElementById('listTbody');
  let listHtml = '';
  filtered.forEach(e => {
    listHtml += '<tr style="cursor:pointer;" onclick="onItemClick(\\'' + e.path + '\\',' + e.is_dir + ')">';
    listHtml += '<td><span style="margin-right:8px;">' + getFileIcon(e.name, e.is_dir) + '</span><strong>' + e.name + '</strong></td>';
    listHtml += '<td>' + (e.is_dir ? '--' : formatBytes(e.size)) + '</td>';
    listHtml += '<td>' + (e.is_dir ? 'Diretório' : 'Ficheiro') + '</td>';
    listHtml += '<td style="text-align:right;" onclick="event.stopPropagation()">';
    if (!e.is_dir) {
      listHtml += '<button onclick="previewFile(\\'' + e.path + '\\')" class="btn btn-secondary" style="padding:4px 8px;font-size:0.75rem;margin-right:4px;"><i class="fa-solid fa-file-pen"></i></button>';
      listHtml += '<button onclick="downloadFile(\\'' + e.path + '\\')" class="btn btn-secondary" style="padding:4px 8px;font-size:0.75rem;margin-right:4px;"><i class="fa-solid fa-download"></i></button>';
    }
    listHtml += '<button onclick="deleteItem(\\'' + e.path + '\\',' + e.is_dir + ')" class="btn btn-danger" style="padding:4px 8px;font-size:0.75rem;"><i class="fa-solid fa-trash-can"></i></button>';
    listHtml += '</td></tr>';
  });
  if (filtered.length === 0) {
    listHtml = '<tr><td colspan="4" style="text-align:center;padding:20px;color:var(--muted);">Esta pasta está vazia</td></tr>';
  }
  listEl.innerHTML = listHtml;
}

function onItemClick(path, isDir) {
  if (isDir) {
    changeDir(path);
  } else {
    previewFile(path);
  }
}

function previewFile(path) {
  fetch('/api/sd/read?path=' + encodeURIComponent(path))
    .then(r => r.text())
    .then(content => {
      document.getElementById('viewerPath').innerText = path;
      document.getElementById('viewerTitle').innerHTML = '<i class="fa-solid fa-file-lines"></i> ' + path.split('/').pop();
      document.getElementById('viewerContent').value = content;
      document.getElementById('viewerModal').style.display = 'flex';
    })
    .catch(() => showToast('Erro ao ler ficheiro', true));
}

function saveEditedFile() {
  const path = document.getElementById('viewerPath').innerText;
  const content = document.getElementById('viewerContent').value;
  showToast('A gravar ficheiro...');

  fetch('/api/sd/save', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ path: path, content: content })
  })
  .then(r => r.json())
  .then(res => {
    if (res.success) {
      showToast('Ficheiro guardado com sucesso!');
      closeModal('viewerModal');
      refreshDir();
    } else {
      showToast(res.msg || 'Erro ao gravar', true);
    }
  });
}

function downloadFile(path) {
  window.open('/api/sd/download?path=' + encodeURIComponent(path), '_blank');
}

function deleteItem(path, isDir) {
  const typeStr = isDir ? 'diretório e todo o seu conteúdo' : 'ficheiro';
  if (!confirm('Tem a certeza que deseja eliminar o ' + typeStr + ':\\n' + path + ' ?')) {
    return;
  }
  showToast('A eliminar...');

  fetch('/api/sd/delete', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ path: path, is_dir: isDir })
  })
  .then(r => r.json())
  .then(res => {
    if (res.success) {
      showToast('Eliminado com sucesso!');
      refreshDir();
    } else {
      showToast(res.msg || 'Erro ao eliminar', true);
    }
  });
}

function openMkdirModal() {
  document.getElementById('mkdirCurrentPath').value = currentDir;
  document.getElementById('mkdirName').value = '';
  document.getElementById('mkdirModal').style.display = 'flex';
}

function submitMkdir(e) {
  e.preventDefault();
  const name = document.getElementById('mkdirName').value.trim();
  if (!name) return;
  const fullPath = (currentDir === '/' ? '' : currentDir) + '/' + name;

  fetch('/api/sd/mkdir', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ path: fullPath })
  })
  .then(r => r.json())
  .then(res => {
    if (res.success) {
      showToast('Pasta criada com sucesso!');
      closeModal('mkdirModal');
      refreshDir();
    } else {
      showToast(res.msg || 'Erro ao criar pasta', true);
    }
  });
}

function openUploadModal() {
  document.getElementById('uploadDestDir').value = currentDir;
  document.getElementById('uploadFileInput').value = '';
  document.getElementById('uploadProgressContainer').style.display = 'none';
  document.getElementById('uploadSubmitBtn').disabled = false;
  document.getElementById('uploadModal').style.display = 'flex';
}

function submitUpload(e) {
  e.preventDefault();
  const fileInput = document.getElementById('uploadFileInput');
  if (!fileInput.files || fileInput.files.length === 0) return;
  const file = fileInput.files[0];

  const pContainer = document.getElementById('uploadProgressContainer');
  const pBar = document.getElementById('uploadProgressBar');
  const pText = document.getElementById('uploadProgressText');
  const btn = document.getElementById('uploadSubmitBtn');

  pContainer.style.display = 'block';
  btn.disabled = true;

  const formData = new FormData();
  formData.append('file', file);

  const xhr = new XMLHttpRequest();
  const uploadUrl = '/api/sd/upload?dir=' + encodeURIComponent(currentDir);
  xhr.open('POST', uploadUrl, true);

  xhr.upload.onprogress = function(event) {
    if (event.lengthComputable) {
      const pct = Math.round((event.loaded / event.total) * 100);
      pBar.style.width = pct + '%';
      pText.innerText = 'A enviar: ' + pct + '% (' + formatBytes(event.loaded) + ' de ' + formatBytes(event.total) + ')';
    }
  };

  xhr.onload = function() {
    btn.disabled = false;
    if (xhr.status === 200) {
      showToast('Upload concluído com sucesso!');
      closeModal('uploadModal');
      refreshDir();
    } else {
      showToast('Erro no upload: ' + xhr.responseText, true);
    }
  };

  xhr.onerror = function() {
    btn.disabled = false;
    showToast('Falha na ligação durante o upload', true);
  };

  xhr.send(formData);
}

function closeModal(id) {
  document.getElementById(id).style.display = 'none';
}

document.addEventListener('DOMContentLoaded', () => {
  toggleView('grid');
  refreshDir();
});
</script>
"""

# ----------------------------------------------------------------------
# 4. PÁGINAS ESPECIALIZADAS DA BASE STATION (/www/base/)
# ----------------------------------------------------------------------

BASE_GPS_CONTENT = """
<div class="card">
  <div class="card-title">
    <span><i class="fa-solid fa-satellite-dish"></i> Telemetria LoRa & Navegação da Frota</span>
    <div style="display:flex;align-items:center;gap:8px;">
      <label class="stat-label" style="margin:0;">Rover:</label>
      <select id="roverSelect" onchange="changeRover(this.value)" style="width:auto;margin:0;padding:6px 12px;background:#090e1a;color:var(--primary);font-weight:700;border:1px solid var(--primary);border-radius:8px;">
        <option value="1">Rover-1</option>
      </select>
    </div>
  </div>

  <div class="grid-4">
    <div class="stat-box">
      <div class="stat-label">Estado LoRa</div>
      <div class="stat-value" id="fixStatus" style="color:var(--red);">A ler...</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Velocidade</div>
      <div class="stat-value" id="speedVal" style="color:var(--primary);">0.0 kn</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Rumo Proa</div>
      <div class="stat-value" id="headingVal">0°</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Sinal Rádio</div>
      <div class="stat-value" id="rssiVal" style="color:var(--green);">0 dBm</div>
    </div>
  </div>

  <div class="grid-2" style="margin-top:16px;">
    <div class="stat-box">
      <div class="stat-label">Latitude (WGS84)</div>
      <div class="stat-value" id="latVal" style="font-size:1.6rem;color:var(--primary);">0.000000</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Longitude (WGS84)</div>
      <div class="stat-value" id="lngVal" style="font-size:1.6rem;color:var(--primary);">0.000000</div>
    </div>
  </div>

  <div style="margin-top:20px;" class="btn-group">
    <a id="osmLink" href="https://www.openstreetmap.org" target="_blank" class="btn btn-primary">
      <i class="fa-solid fa-map"></i> Ver no OpenStreetMap
    </a>
    <a id="mapsLink" href="https://maps.google.com" target="_blank" class="btn btn-secondary">
      <i class="fa-solid fa-location-dot"></i> Google Maps
    </a>
  </div>
</div>

<div class="card">
  <div class="card-title">
    <span><i class="fa-solid fa-ship"></i> Visão Geral da Frota de Rovers</span>
  </div>
  <div style="overflow-x:auto;">
    <table>
      <thead>
        <tr>
          <th>ID / Código</th>
          <th>Estado</th>
          <th>Coordenadas</th>
          <th>Velocidade</th>
          <th>Bateria</th>
          <th>Sinal LoRa</th>
          <th>Ação</th>
        </tr>
      </thead>
      <tbody id="fleetTableBody">
        <tr><td colspan="7" style="text-align:center;color:var(--muted);">A carregar telemetria da frota...</td></tr>
      </tbody>
    </table>
  </div>
</div>

<script>
function changeRover(id) {
  fetch('/api/select_rover', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({rover_id: parseInt(id)})
  }).then(() => refreshData());
}

function refreshData() {
  fetch('/api/status')
    .then(r => r.json())
    .then(data => {
      const sel = data.selected;
      if (sel) {
        document.getElementById('latVal').innerText = (sel.lat || 0).toFixed(6);
        document.getElementById('lngVal').innerText = (sel.lng || 0).toFixed(6);
        document.getElementById('speedVal').innerText = (sel.speed_knots || 0).toFixed(1) + ' kn';
        document.getElementById('headingVal').innerText = (sel.heading_deg || 0).toFixed(0) + '°';
        document.getElementById('rssiVal').innerText = (sel.rssi || 0) + ' dBm';
        
        const st = document.getElementById('fixStatus');
        st.innerText = sel.is_online ? 'ONLINE' : 'OFFLINE';
        st.style.color = sel.is_online ? 'var(--green)' : 'var(--red)';

        document.getElementById('mapsLink').href = 'https://maps.google.com/?q=' + sel.lat + ',' + sel.lng;
        document.getElementById('osmLink').href = 'https://www.openstreetmap.org/?mlat=' + sel.lat + '&mlon=' + sel.lng + '#map=17/' + sel.lat + '/' + sel.lng;
      }

      if (data.rovers && data.rovers.length) {
        let optionsHtml = '';
        let rowsHtml = '';
        data.rovers.forEach(rov => {
          const isSelected = sel && (sel.id === rov.id);
          optionsHtml += `<option value="${rov.id}" ${isSelected ? 'selected' : ''}>${rov.code} ${rov.is_online ? '[ONLINE]' : '[OFFLINE]'}</option>`;
          
          rowsHtml += `<tr>
            <td><strong>${rov.code}</strong></td>
            <td><span class="badge" style="background:${rov.is_online ? '#065f46' : '#7f1d1d'};color:${rov.is_online ? '#6ee7b7' : '#fca5a5'};">${rov.is_online ? 'ONLINE' : 'OFFLINE'}</span></td>
            <td>${(rov.lat || 0).toFixed(4)}, ${(rov.lng || 0).toFixed(4)}</td>
            <td>${(rov.speed_knots || 0).toFixed(1)} kn</td>
            <td><strong style="color:${rov.battery_pct < 25 ? 'var(--red)' : 'var(--green)'};">${rov.battery_pct || 0}%</strong> (${(rov.battery_voltage || 0).toFixed(1)}V)</td>
            <td>${rov.rssi || 0} dBm</td>
            <td><button onclick="changeRover(${rov.id})" class="btn ${isSelected ? 'btn-secondary' : 'btn-primary'}" style="padding:4px 10px;font-size:0.75rem;">${isSelected ? 'Selecionado' : 'Selecionar'}</button></td>
          </tr>`;
        });
        document.getElementById('roverSelect').innerHTML = optionsHtml;
        document.getElementById('fleetTableBody').innerHTML = rowsHtml;
      }
    }).catch(() => {});
}

setInterval(refreshData, 1500);
document.addEventListener('DOMContentLoaded', refreshData);
</script>
"""

BASE_BATTERY_CONTENT = """
<div class="card">
  <div class="card-title">
    <span><i class="fa-solid fa-battery-full"></i> Telemetria de Bateria LiPo 4S da Frota</span>
    <div style="display:flex;align-items:center;gap:8px;">
      <label class="stat-label" style="margin:0;">Rover:</label>
      <select id="roverSelect" onchange="changeRover(this.value)" style="width:auto;margin:0;padding:6px 12px;background:#090e1a;color:var(--primary);font-weight:700;border:1px solid var(--primary);border-radius:8px;">
        <option value="1">Rover-1</option>
      </select>
    </div>
  </div>

  <div style="text-align:center;padding:20px 0;">
    <div id="batPct" style="font-size:3.5rem;font-weight:800;color:var(--green);">0%</div>
    <div class="progress-bg" style="max-width:400px;margin:0 auto;">
      <div id="batBar" class="progress-bar" style="width:0%;background:var(--green);"></div>
    </div>
  </div>

  <div class="grid-4">
    <div class="stat-box">
      <div class="stat-label">Tensão Total (Pack)</div>
      <div class="stat-value" id="vTotVal">0.00 V</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Média por Célula</div>
      <div class="stat-value" id="vCellVal" style="color:var(--primary);">0.00 V/cel</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Química do Pack</div>
      <div class="stat-value">LiPo 4S</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Estado Pack</div>
      <div class="stat-value" id="batHealth" style="color:var(--green);">NORMAL</div>
    </div>
  </div>

  <div style="margin-top:20px;">
    <div class="stat-label" style="margin-bottom:8px;">Distribuição Estimada de Tensão por Célula (LiPo 4S):</div>
    <div class="grid-4">
      <div class="stat-box" style="text-align:center;">
        <div class="stat-label">Célula 1</div>
        <div class="stat-value cell-v" style="font-size:1.2rem;color:var(--primary);">0.00 V</div>
      </div>
      <div class="stat-box" style="text-align:center;">
        <div class="stat-label">Célula 2</div>
        <div class="stat-value cell-v" style="font-size:1.2rem;color:var(--primary);">0.00 V</div>
      </div>
      <div class="stat-box" style="text-align:center;">
        <div class="stat-label">Célula 3</div>
        <div class="stat-value cell-v" style="font-size:1.2rem;color:var(--primary);">0.00 V</div>
      </div>
      <div class="stat-box" style="text-align:center;">
        <div class="stat-label">Célula 4</div>
        <div class="stat-value cell-v" style="font-size:1.2rem;color:var(--primary);">0.00 V</div>
      </div>
    </div>
  </div>
</div>

<script>
function changeRover(id) {
  fetch('/api/select_rover', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({rover_id: parseInt(id)})
  }).then(() => refreshData());
}

function refreshData() {
  fetch('/api/status')
    .then(r => r.json())
    .then(data => {
      const sel = data.selected;
      if (!sel) return;
      const p = sel.battery_pct || 0;
      const v = sel.battery_voltage || 0;
      const vc = (v / 4.0);
      const col = p < 25 ? 'var(--red)' : (p < 50 ? 'var(--yellow)' : 'var(--green)');

      document.getElementById('batPct').innerText = p + '%';
      document.getElementById('batPct').style.color = col;
      document.getElementById('batBar').style.width = p + '%';
      document.getElementById('batBar').style.background = col;
      document.getElementById('vTotVal').innerText = v.toFixed(2) + ' V';
      document.getElementById('vCellVal').innerText = vc.toFixed(2) + ' V/cel';
      document.querySelectorAll('.cell-v').forEach(el => el.innerText = vc.toFixed(2) + ' V');

      const h = document.getElementById('batHealth');
      h.innerText = p < 20 ? 'CRÍTICO' : (p < 40 ? 'BAIXA' : 'NORMAL');
      h.style.color = col;

      if (data.rovers && data.rovers.length) {
        let opt = '';
        data.rovers.forEach(rov => {
          opt += `<option value="${rov.id}" ${sel.id === rov.id ? 'selected' : ''}>${rov.code} (${rov.battery_pct}%)</option>`;
        });
        document.getElementById('roverSelect').innerHTML = opt;
      }
    }).catch(() => {});
}

setInterval(refreshData, 1500);
document.addEventListener('DOMContentLoaded', refreshData);
</script>
"""

BASE_CONTROL_CONTENT = """
<div class="card">
  <div class="card-title">
    <span><i class="fa-solid fa-gamepad"></i> Centro de Comando Remoto da Frota</span>
    <div style="display:flex;align-items:center;gap:8px;">
      <label class="stat-label" style="margin:0;">Rover Alvo:</label>
      <select id="targetRoverSelect" onchange="changeRover(this.value)" style="width:auto;margin:0;padding:6px 12px;background:#090e1a;color:var(--primary);font-weight:700;border:1px solid var(--primary);border-radius:8px;">
        <option value="1">Rover-1</option>
      </select>
    </div>
  </div>

  <div class="grid-4">
    <div class="stat-box">
      <div class="stat-label">Modo Motor</div>
      <div class="stat-value" id="curMotor" style="color:var(--yellow);font-size:1.1rem;">idle</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Estado Âncora</div>
      <div class="stat-value" id="curAnchor" style="color:var(--primary);font-size:1.1rem;">retracted</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Profundidade</div>
      <div class="stat-value" id="curDepth">0.0 m</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Sinal Rádio</div>
      <div class="stat-value" id="curRssi" style="color:var(--green);">0 dBm</div>
    </div>
  </div>
</div>

<div class="card">
  <div class="card-title">
    <span><i class="fa-solid fa-fan"></i> Propulsão ESC & Leme de Direção</span>
  </div>
  <div class="grid-2">
    <div>
      <div class="stat-label">Aceleração ESC: <span id="thrVal" style="font-size:1.1rem;color:var(--primary);">0%</span></div>
      <input type="range" id="throttleSlider" min="-100" max="100" value="0" class="slider" oninput="updateSliders()" onchange="sendActuators()">
      <div class="btn-group" style="margin-top:12px;">
        <button onclick="setThrottle(0)" class="btn btn-secondary">0% (Stop)</button>
        <button onclick="setThrottle(25)" class="btn btn-secondary">+25%</button>
        <button onclick="setThrottle(50)" class="btn btn-secondary">+50%</button>
        <button onclick="setThrottle(100)" class="btn btn-primary">+100%</button>
        <button onclick="setThrottle(-50)" class="btn btn-secondary">-50% (Ré)</button>
      </div>
    </div>

    <div>
      <div class="stat-label">Ângulo do Leme: <span id="rudVal" style="font-size:1.1rem;color:var(--primary);">0%</span></div>
      <input type="range" id="rudderSlider" min="-100" max="100" value="0" class="slider" oninput="updateSliders()" onchange="sendActuators()">
      <div class="btn-group" style="margin-top:12px;">
        <button onclick="setRudder(-75)" class="btn btn-secondary">⬅ Bombordo (-75%)</button>
        <button onclick="setRudder(0)" class="btn btn-secondary">Centro (0%)</button>
        <button onclick="setRudder(75)" class="btn btn-secondary">Estibordo (+75%) ➡</button>
      </div>
    </div>
  </div>
</div>

<div class="card">
  <div class="card-title">
    <span><i class="fa-solid fa-anchor"></i> Guincho da Âncora & Travão</span>
  </div>
  <div class="btn-group">
    <button onclick="sendAction(3)" class="btn btn-primary"><i class="fa-solid fa-arrow-down"></i> Lançar Âncora (Soltar)</button>
    <button onclick="sendAction(4)" class="btn btn-primary"><i class="fa-solid fa-arrow-up"></i> Recolher Âncora (Guincho)</button>
    <button onclick="sendAction(5)" class="btn btn-secondary"><i class="fa-solid fa-stop"></i> Parar Guincho</button>
  </div>
</div>

<div class="card">
  <div class="card-title">
    <span><i class="fa-solid fa-compass"></i> Modos de Navegação & Segurança</span>
  </div>
  <div class="btn-group">
    <button onclick="sendAction(1)" class="btn" style="background:#0284c7;color:#fff;"><i class="fa-solid fa-location-crosshairs"></i> Hold Station (GPS)</button>
    <button onclick="sendAction(7)" class="btn" style="background:#7c3aed;color:#fff;"><i class="fa-solid fa-house"></i> Return to Launch (RTL)</button>
    <button onclick="sendAction(10)" class="btn btn-secondary"><i class="fa-solid fa-gamepad"></i> Modo Manual</button>
    <button onclick="sendAction(8)" class="btn" style="background:var(--yellow);color:#0f172a;"><i class="fa-solid fa-bell"></i> Alarme Sonoro / Strobe</button>
    <button onclick="sendEmergencyStop()" class="btn btn-danger" style="font-size:1rem;padding:12px 24px;"><i class="fa-solid fa-ban"></i> PARAGEM DE EMERGÊNCIA</button>
  </div>
</div>

<script>
function updateSliders() {
  document.getElementById('thrVal').innerText = document.getElementById('throttleSlider').value + '%';
  document.getElementById('rudVal').innerText = document.getElementById('rudderSlider').value + '%';
}

function setThrottle(v) {
  document.getElementById('throttleSlider').value = v;
  updateSliders();
  sendActuators();
}

function setRudder(v) {
  document.getElementById('rudderSlider').value = v;
  updateSliders();
  sendActuators();
}

function sendActuators() {
  const t = parseInt(document.getElementById('throttleSlider').value);
  const r = parseInt(document.getElementById('rudderSlider').value);
  fetch('/api/control/actuator', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({throttle: t, rudder: r})
  });
}

function sendAction(a) {
  const rid = parseInt(document.getElementById('targetRoverSelect').value);
  fetch('/api/control/action', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({action: a, rover_id: rid})
  }).then(r => r.json()).then(res => {
    showToast(res.msg || 'Comando enviado');
  });
}

function sendEmergencyStop() {
  setThrottle(0);
  setRudder(0);
  sendAction(2);
}

function changeRover(id) {
  fetch('/api/select_rover', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({rover_id: parseInt(id)})
  }).then(() => refreshData());
}

function refreshData() {
  fetch('/api/status')
    .then(r => r.json())
    .then(data => {
      const sel = data.selected;
      if (sel) {
        document.getElementById('curMotor').innerText = sel.motor_status !== undefined ? sel.motor_status : 'idle';
        document.getElementById('curAnchor').innerText = sel.anchor_status !== undefined ? sel.anchor_status : 'retracted';
        document.getElementById('curDepth').innerText = (sel.anchor_depth_m || 0).toFixed(1) + ' m';
        document.getElementById('curRssi').innerText = (sel.rssi || 0) + ' dBm';
      }
      if (data.rovers && data.rovers.length) {
        let opt = '';
        data.rovers.forEach(rov => {
          opt += `<option value="${rov.id}" ${sel && sel.id === rov.id ? 'selected' : ''}>${rov.code}</option>`;
        });
        document.getElementById('targetRoverSelect').innerHTML = opt;
      }
    }).catch(() => {});
}

setInterval(refreshData, 1500);
document.addEventListener('DOMContentLoaded', refreshData);
</script>
"""

BASE_WIFI_CONTENT = WIFI_CONTENT.replace(
    "Ligue o Rover ao router Wi-Fi do cais ou centro de comando para telemetria em tempo real e acesso local direto.",
    "Ligue a Base Station ao router Wi-Fi local para permitir controlo via browser e telemetria da frota em tempo real."
).replace("Redes Guardadas no Rover", "Redes Guardadas na Base Station")

BASE_SD_CONTENT = SD_CONTENT.replace("Explorador do Cartão MicroSD", "Explorador MicroSD - Base Station").replace("Cartão MicroSD - Rover", "Cartão MicroSD - Base Station").replace("let currentDir = '/';", "let currentDir = '/www/base';")


def main():
    ROVER_DIR = WWW_DIR / "rover"
    BASE_DIR = WWW_DIR / "base"
    ROVER_DIR.mkdir(parents=True, exist_ok=True)
    BASE_DIR.mkdir(parents=True, exist_ok=True)

    print(f"[*] A gerar ficheiros web em:")
    print(f"    - Raiz partilhada: {WWW_DIR}")
    print(f"    - Subpasta Rover:  {ROVER_DIR}")
    print(f"    - Subpasta Base:   {BASE_DIR}")

    # 1. style.css & app.js na raiz
    with open(WWW_DIR / "style.css", "w", encoding="utf-8") as f:
        f.write(STYLE_CSS.strip() + "\n")
    print(f"  [+] {WWW_DIR / 'style.css'} ({len(STYLE_CSS)} bytes)")

    with open(WWW_DIR / "app.js", "w", encoding="utf-8") as f:
        f.write(APP_JS.strip() + "\n")
    print(f"  [+] {WWW_DIR / 'app.js'} ({len(APP_JS)} bytes)")

    # 2. Páginas do Rover
    gps_rover = generate_html_page("Telemetria GPS", "gps", GPS_CONTENT)
    bat_rover = generate_html_page("Monitor da Bateria LiPo 4S", "battery", BATTERY_CONTENT)
    ctrl_rover = generate_html_page("Comandar Boia", "control", CONTROL_CONTENT)
    cfg_rover = generate_html_page("Configuração do Rover", "config", CONFIG_CONTENT)
    wifi_rover = generate_html_page("Configuração WiFi", "wifi", WIFI_CONTENT)
    sd_rover = generate_html_page("Explorador do Cartão MicroSD", "sd", SD_CONTENT)

    rover_files = [
        ("index.html", gps_rover),
        ("gps.html", gps_rover),
        ("battery.html", bat_rover),
        ("control.html", ctrl_rover),
        ("config.html", cfg_rover),
        ("wifi.html", wifi_rover),
        ("sd.html", sd_rover)
    ]

    for fname, content in rover_files:
        # Salva em /www/rover/
        with open(ROVER_DIR / fname, "w", encoding="utf-8") as f:
            f.write(content.strip() + "\n")
        # Mantém cópia de compatibilidade em /www/
        with open(WWW_DIR / fname, "w", encoding="utf-8") as f:
            f.write(content.strip() + "\n")
    print(f"  [+] 7 páginas geradas em {ROVER_DIR} e espelhadas em {WWW_DIR}")

    # 3. Páginas especializadas da Base Station
    gps_base = generate_base_html_page("Telemetria GPS da Frota", "gps", BASE_GPS_CONTENT)
    bat_base = generate_base_html_page("Monitor de Bateria da Frota", "battery", BASE_BATTERY_CONTENT)
    ctrl_base = generate_base_html_page("Centro de Comando da Frota", "control", BASE_CONTROL_CONTENT)
    wifi_base = generate_base_html_page("Configuração WiFi da Base", "wifi", BASE_WIFI_CONTENT)
    sd_base = generate_base_html_page("Explorador MicroSD da Base", "sd", BASE_SD_CONTENT)

    base_files = [
        ("index.html", gps_base),
        ("gps.html", gps_base),
        ("battery.html", bat_base),
        ("control.html", ctrl_base),
        ("wifi.html", wifi_base),
        ("sd.html", sd_base)
    ]

    for fname, content in base_files:
        with open(BASE_DIR / fname, "w", encoding="utf-8") as f:
            f.write(content.strip() + "\n")
    print(f"  [+] 6 páginas especializadas geradas em {BASE_DIR}")

    print("\n[OK] Todas as páginas web do Rover e da Base Station foram geradas com sucesso!")

if __name__ == "__main__":
    main()

