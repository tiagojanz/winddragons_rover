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
.fa-flag-checkered:before { content: '\\f11e'; }
.fa-flag:before { content: '\\f024'; }
.fa-sailboat:before { content: '\\f445'; }
.fa-rocket:before { content: '\\f135'; }
.fa-box-archive:before { content: '\\f187'; }
.fa-hourglass-half:before { content: '\\f252'; }
.fa-satellite-dish:before { content: '\\f7c0'; }
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
      const stationSpan = document.getElementById('navStationId');
      if (stationSpan && (data.station_id || data.mac_address)) {
        stationSpan.innerText = data.station_id || data.mac_address;
      }
      const baseMacEl = document.getElementById('baseMacVal');
      if (baseMacEl && (data.station_id || data.mac_address)) {
        baseMacEl.innerText = data.station_id || data.mac_address;
      }
      const baseNameEl = document.getElementById('baseStationName');
      if (baseNameEl && data.station_name) {
        baseNameEl.innerText = data.station_name;
      }
      const roverSpan = document.getElementById('navRoverId');
      if (roverSpan && data.rover_id) {
        roverSpan.innerText = 'Rover-' + data.rover_id;
      }
      const opBadge = document.getElementById('navOpStateBadge');
      const opText = document.getElementById('navOpStateText');
      if (opText && data.operation_state) {
        opText.innerText = data.operation_state;
        const colors = {
          'STORAGE': '#64748b',
          'WAITING_ROVERS': '#f59e0b',
          'WAITING ROVERS': '#f59e0b',
          'IDLE': '#06b6d4',
          'GO': '#10b981'
        };
        if (opBadge) {
          opBadge.style.background = colors[data.operation_state] || '#64748b';
        }
      if (data.version) {
        const laf = data.version.lafvin || 'v2.1.0';
        const c6 = data.version.c6 || '--';
        const bun = data.version.bundle || 'v2.1.0';

        const navLaf = document.getElementById('navLafvinVer');
        if (navLaf) navLaf.innerText = 'Lafvin ' + laf;
        const navC6 = document.getElementById('navC6Ver');
        if (navC6) {
          navC6.innerText = 'C6 ' + c6;
          navC6.style.color = (c6 !== '--' && !c6.includes('detetar')) ? 'var(--yellow)' : 'var(--muted)';
        }

        const footLaf = document.getElementById('footerLafvinVer');
        if (footLaf) footLaf.innerText = laf;
        const footC6 = document.getElementById('footerC6Ver');
        if (footC6) footC6.innerText = c6;
        const footBun = document.getElementById('footerBundleVer');
        if (footBun) footBun.innerText = bun;

        const mainLaf = document.getElementById('mainLafvinVer');
        if (mainLaf) mainLaf.innerText = laf;
        const mainC6 = document.getElementById('mainC6Ver');
        if (mainC6) {
          mainC6.innerText = c6;
          mainC6.style.color = (c6 !== '--' && !c6.includes('detetar')) ? 'var(--yellow)' : 'var(--muted)';
        }

        const cfgLaf = document.getElementById('fwLafvinVer');
        if (cfgLaf) cfgLaf.innerText = laf;
        const cfgC6 = document.getElementById('fwC6Ver');
        if (cfgC6) {
          cfgC6.innerText = c6;
          cfgC6.style.color = (c6 !== '--' && !c6.includes('detetar')) ? 'var(--yellow)' : 'var(--muted)';
        }
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
    <div style="display:flex;align-items:center;gap:8px;flex-wrap:wrap;">
      <a href="/config" id="navVersionBadge" class="badge" style="background:rgba(6,182,212,0.12);border:1px solid rgba(6,182,212,0.3);color:var(--primary);text-decoration:none;font-size:0.75rem;display:inline-flex;align-items:center;gap:6px;" title="Ver detalhes de firmware nas Configurações">
        <i class="fa-solid fa-code-branch"></i> <span id="navLafvinVer">Lafvin: v2.1.0</span> &bull; <span id="navC6Ver">C6: ...</span>
      </a>
      <span id="navWifiBadge" class="badge badge-sta">
        <i class="fa-solid fa-wifi"></i> A carregar...
      </span>
    </div>
  </header>

  <main class="container">
    {body_content}
  </main>

  <footer style="text-align:center;color:var(--muted);font-size:0.75rem;margin-top:24px;display:flex;justify-content:center;align-items:center;gap:12px;flex-wrap:wrap;padding:0 16px;">
    <span>WindDragons Autonomous Surface Rover / Buoy &copy; 2026</span>
    <span>&bull;</span>
    <span>Master Lafvin: <b style="color:var(--green);" id="footerLafvinVer">v2.1.0</b></span>
    <span>&bull;</span>
    <span>Coprocessador C6: <b style="color:var(--yellow);" id="footerC6Ver">--</b></span>
    <span>&bull;</span>
    <span>Pacote Bundle: <b style="color:var(--primary);" id="footerBundleVer">v2.1.0</b></span>
  </footer>
</body>
</html>
"""

def generate_base_html_page(title, active_tab, body_content):
    tabs = [
        ("mission", "/mission", "fa-flag-checkered", "Missão"),
        ("portal", "/portal", "fa-cloud", "Portal Cloud"),
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
      <span id="navOpStateBadge" class="badge" style="background:#64748b;color:#fff;font-weight:700;">
        <i class="fa-solid fa-flag"></i> <span id="navOpStateText">STORAGE</span>
      </span>
      <span id="navStationBadge" class="badge" style="background:rgba(2,132,199,0.2);color:#38bdf8;border:1px solid rgba(56,189,248,0.3);font-family:monospace;">
        <i class="fa-solid fa-fingerprint"></i> <span id="navStationId">ID Base</span>
      </span>
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
<style>
.rate-btn { flex: 1; min-width: 60px; padding: 10px 6px; border-radius: 8px; border: 1px solid var(--card-border); background: rgba(15,23,42,0.6); color: var(--muted); cursor: pointer; font-weight: 600; transition: all 0.2s ease; display: flex; flex-direction: column; align-items: center; gap: 4px; }
.rate-btn.active { background: rgba(6,182,212,0.15); border-color: var(--primary); color: var(--text); box-shadow: 0 0 12px rgba(6,182,212,0.25); }
.rate-btn:hover:not(.active) { border-color: var(--muted); color: var(--text); }
.rate-btn .sub { font-size: 0.72rem; font-weight: normal; color: var(--muted); }
.rate-btn.active .sub { color: var(--primary); }
</style>

<div style="display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:10px;background:rgba(15,23,42,0.65);padding:10px 14px;border-radius:10px;border:1px solid var(--card-border);margin-bottom:16px;font-size:0.83rem;">
  <div style="display:flex;align-items:center;gap:8px;">
    <i class="fa-solid fa-microchip" style="color:var(--primary);"></i>
    <span style="color:var(--muted);">Master Lafvin:</span>
    <strong style="color:var(--green);" id="mainLafvinVer">v2.1.0</strong>
  </div>
  <div style="display:flex;align-items:center;gap:8px;">
    <i class="fa-solid fa-satellite-dish" style="color:var(--yellow);"></i>
    <span style="color:var(--muted);">Coprocessador C6:</span>
    <strong style="color:var(--yellow);" id="mainC6Ver">A detetar...</strong>
  </div>
  <div style="display:flex;align-items:center;gap:8px;">
    <i class="fa-solid fa-box-archive" style="color:var(--primary);"></i>
    <span style="color:var(--muted);">Pacote OTA:</span>
    <strong style="color:var(--primary);" id="mainBundleVer">v2.1.0</strong>
  </div>
  <div>
    <a href="/config" class="badge" style="background:rgba(6,182,212,0.15);color:var(--primary);border:1px solid rgba(6,182,212,0.3);text-decoration:none;font-size:0.75rem;padding:4px 10px;">
      <i class="fa-solid fa-cloud-arrow-up"></i> Atualizar Firmware
    </a>
  </div>
</div>

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

<!-- Painel de Configuração Quectel LC29H -->
<div class="card" style="margin-top:20px;">
  <div class="card-title" style="display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:10px;">
    <span><i class="fa-solid fa-satellite-dish"></i> Configuração Quectel LC29H (Dual-Band L1/L5)</span>
    <span id="gpsSaveBadge" class="badge" style="display:none;background:rgba(16,185,129,0.2);color:var(--green);border:1px solid var(--green);">
      <i class="fa-solid fa-check"></i> Aplicado
    </span>
  </div>

  <div style="margin:12px 0;background:rgba(0,0,0,0.25);padding:14px;border-radius:10px;border:1px solid var(--card-border);">
    <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:10px;flex-wrap:wrap;gap:6px;">
      <span class="stat-label" style="font-weight:600;color:var(--text);font-size:0.9rem;">
        <i class="fa-solid fa-gauge-high"></i> Taxa de Atualização de Navegação:
      </span>
      <span id="rateLabel" style="font-size:0.8rem;color:var(--primary);font-weight:600;">1 Hz &bull; 1000 ms</span>
    </div>
    <div style="display:grid;grid-template-columns:repeat(4,1fr);gap:8px;margin-bottom:12px;">
      <button type="button" class="rate-btn active" id="btnRate1" onclick="selectRate(1)"><span style="font-size:1.1rem;font-weight:bold;">1 Hz</span><span class="sub">1000 ms</span></button>
      <button type="button" class="rate-btn" id="btnRate2" onclick="selectRate(2)"><span style="font-size:1.1rem;font-weight:bold;">2 Hz</span><span class="sub">500 ms</span></button>
      <button type="button" class="rate-btn" id="btnRate5" onclick="selectRate(5)"><span style="font-size:1.1rem;font-weight:bold;">5 Hz</span><span class="sub">200 ms &bull; Rec.</span></button>
      <button type="button" class="rate-btn" id="btnRate10" onclick="selectRate(10)"><span style="font-size:1.1rem;font-weight:bold;">10 Hz</span><span class="sub">100 ms &bull; Max</span></button>
    </div>
    <div style="display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:10px;padding-top:10px;border-top:1px solid rgba(255,255,255,0.06);">
      <label style="display:flex;align-items:center;gap:8px;font-size:0.85rem;color:var(--text);cursor:pointer;">
        <input type="checkbox" id="chkSaveFlash" checked style="accent-color:var(--primary);width:16px;height:16px;">
        <span>Gravar na Flash NVRAM ($PAIR513)</span>
      </label>
      <button type="button" class="btn btn-primary" onclick="applyGpsRate()"><i class="fa-solid fa-bolt"></i> Aplicar ao GPS</button>
    </div>
  </div>

  <div style="margin-top:12px;background:rgba(0,0,0,0.2);padding:12px;border-radius:8px;border:1px solid var(--card-border);">
    <div style="font-size:0.85rem;color:var(--muted);margin-bottom:8px;font-weight:600;"><i class="fa-solid fa-terminal"></i> Envio Direto de Comandos ($PAIR / NMEA):</div>
    <div style="display:flex;gap:8px;flex-wrap:wrap;">
      <input type="text" id="gpsCustomCmd" placeholder="Ex: PAIR004 ou PAIR067 ou PAIR051" style="flex:1;min-width:180px;background:rgba(15,23,42,0.8);border:1px solid var(--card-border);border-radius:6px;padding:8px 12px;color:var(--text);font-family:monospace;font-size:0.85rem;">
      <button type="button" class="btn btn-secondary" onclick="sendCustomGpsCmd()"><i class="fa-solid fa-paper-plane"></i> Enviar</button>
    </div>
    <div id="cmdFeedback" style="font-size:0.78rem;color:var(--muted);margin-top:6px;font-family:monospace;"></div>
  </div>
</div>

<script>
let selectedRate = 1, rateInitialized = false;
function selectRate(hz) {
  selectedRate = hz;
  [1, 2, 5, 10].forEach(h => {
    const el = document.getElementById('btnRate' + h);
    if (el) el.classList.toggle('active', h === hz);
  });
  const lbl = document.getElementById('rateLabel');
  if (lbl) {
    const desc = hz === 1 ? '1000 ms • Económico' : (hz === 2 ? '500 ms • Moderado' : (hz === 5 ? '200 ms • Recomendado' : '100 ms • Alta Taxa'));
    lbl.innerText = hz + ' Hz • ' + desc;
  }
}
function applyGpsRate() {
  const saveFlash = document.getElementById('chkSaveFlash').checked;
  const badge = document.getElementById('gpsSaveBadge');
  fetch('/api/gps/rate', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({rate_hz: selectedRate, save_flash: saveFlash})
  }).then(r => r.json()).then(res => {
    if (badge) {
      badge.style.display = 'inline-flex';
      badge.innerHTML = '<i class="fa-solid fa-check"></i> ' + (res.msg || (selectedRate + ' Hz Ativo'));
      setTimeout(() => { badge.style.display = 'none'; }, 4000);
    }
  }).catch(e => { alert('Erro ao comunicar com o Rover: ' + e); });
}
function sendCustomGpsCmd() {
  const input = document.getElementById('gpsCustomCmd');
  const fb = document.getElementById('cmdFeedback');
  const val = input.value.trim();
  if (!val) return;
  fetch('/api/gps/command', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({cmd: val})
  }).then(r => r.json()).then(res => {
    if (fb) fb.innerText = res.msg || 'Instrução enviada!';
    input.value = '';
    setTimeout(() => { if (fb) fb.innerText = ''; }, 5000);
  }).catch(e => { if (fb) fb.innerText = 'Falha: ' + e; });
}
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
      if (g.rate_hz && !rateInitialized) {
        selectRate(g.rate_hz);
        rateInitialized = true;
      }
    }).catch(() => {});
}, 1000);
</script>
"""

# Battery Page
BATTERY_CONTENT = """
<div class="card">
  <div class="card-title" style="display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:10px;">
    <span><i class="fa-solid fa-battery-full"></i> Monitorização APM Power Module (28V / 90A)</span>
    <span id="saveStatusBadge" class="badge" style="display:none;background:rgba(16,185,129,0.2);color:var(--green);border:1px solid var(--green);">
      <i class="fa-solid fa-check"></i> Gravado
    </span>
  </div>

  <!-- Selector de Tipo de Bateria (3S / 4S / 6S) e Capacidade -->
  <div style="margin: 12px 0 20px 0; background: rgba(0,0,0,0.25); padding: 14px; border-radius: 10px; border: 1px solid var(--card-border);">
    <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:10px; flex-wrap:wrap; gap:6px;">
      <span class="stat-label" style="font-weight:600;color:var(--text);font-size:0.9rem;">
        <i class="fa-solid fa-sliders"></i> Configuração da Bateria LiPo:
      </span>
      <span id="battSpecs" style="font-size:0.8rem;color:var(--primary);font-weight:600;">4S LiPo &bull; 13.2V min &bull; 16.8V max</span>
    </div>
    <div style="display:grid; grid-template-columns: repeat(3, 1fr); gap: 10px; margin-bottom: 12px;">
      <button type="button" class="batt-type-btn" id="btn3S" onclick="selectBattery(3)">
        <span style="font-size:1.1rem;display:flex;align-items:center;gap:6px;"><i class="fa-solid fa-battery-half"></i> 3S LiPo</span>
        <span class="sub">11.1V Nom &bull; 12.6V Max</span>
      </button>
      <button type="button" class="batt-type-btn active" id="btn4S" onclick="selectBattery(4)">
        <span style="font-size:1.1rem;display:flex;align-items:center;gap:6px;"><i class="fa-solid fa-battery-three-quarters"></i> 4S LiPo</span>
        <span class="sub">14.8V Nom &bull; 16.8V Max</span>
      </button>
      <button type="button" class="batt-type-btn" id="btn6S" onclick="selectBattery(6)">
        <span style="font-size:1.1rem;display:flex;align-items:center;gap:6px;"><i class="fa-solid fa-battery-full"></i> 6S LiPo</span>
        <span class="sub">22.2V Nom &bull; 25.2V Max</span>
      </button>
    </div>

    <!-- Capacidade Nominal e Ações de Bateria -->
    <div style="display:flex; align-items:center; justify-content:space-between; flex-wrap:wrap; gap:12px; padding-top:12px; border-top:1px solid rgba(255,255,255,0.08);">
      <div style="display:flex; align-items:center; gap:8px; flex-wrap:wrap;">
        <label for="batCapacityInput" style="font-size:0.85rem;color:var(--muted);font-weight:600;">Capacidade Nominal:</label>
        <div style="display:flex; align-items:center; gap:6px;">
          <input type="number" id="batCapacityInput" min="500" max="50000" step="50" value="2650"
                 style="width:110px; padding:6px 10px; background:rgba(15,23,42,0.8); border:1px solid var(--card-border); border-radius:6px; color:var(--text); font-weight:600; text-align:right;">
          <span style="font-size:0.85rem;color:var(--muted);">mAh</span>
          <button type="button" class="btn btn-secondary" onclick="saveCapacity()" style="padding:6px 12px; font-size:0.8rem;">
            <i class="fa-solid fa-floppy-disk"></i> Guardar
          </button>
        </div>
      </div>
      <div>
        <button type="button" onclick="resetConsumedCounter()" title="Repor contador de mAh quando carregar/trocar a bateria"
                style="padding:6px 12px; font-size:0.8rem; border:1px solid var(--primary); color:var(--primary); background:transparent; border-radius:6px; cursor:pointer;">
          <i class="fa-solid fa-rotate-left"></i> Bateria Nova (Zerar mAh)
        </button>
      </div>
    </div>
  </div>

  <div style="text-align:center;padding:12px 0;">
    <div style="font-size:3.5rem;font-weight:800;color:var(--green);" id="batPct">--%</div>
    <div class="progress-bg" style="max-width:400px;margin:0 auto;">
      <div id="batBar" class="progress-bar" style="width:0%;background:var(--green);"></div>
    </div>
  </div>

  <div class="grid-4" style="grid-template-columns: repeat(auto-fit, minmax(140px, 1fr));">
    <div class="stat-box">
      <div class="stat-label">Tensão Total</div>
      <div class="stat-value" id="vTotVal" style="color:var(--text);">-- V</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Média p/ Célula</div>
      <div class="stat-value" id="vCellVal" style="color:var(--primary);">-- V/cel</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Corrente Atual</div>
      <div class="stat-value" id="currVal" style="color:var(--text);">-- A</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Consumo Total</div>
      <div class="stat-value" id="consVal" style="color:var(--yellow);">-- mAh</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Carga Restante</div>
      <div class="stat-value" id="remVal" style="color:var(--green);">-- mAh</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Autonomia Estimada</div>
      <div class="stat-value" id="autVal" style="color:var(--primary);">--</div>
    </div>
  </div>

  <!-- Distribuição Dinâmica de Células (3, 4 ou 6) -->
  <div style="margin-top:20px;">
    <div class="stat-label" id="cellsTitle" style="margin-bottom:8px;">Distribuição Estimada de Tensão (<span id="cellsCountLabel">4S</span> LiPo):</div>
    <div id="cellsGrid" style="display:grid; grid-template-columns: repeat(auto-fit, minmax(110px, 1fr)); gap:10px;">
    </div>
  </div>
</div>

<script>
let currentCells = 4;
let lastKnownAvg = 0;

function updateBatteryUI(cells) {
  currentCells = cells;
  [3, 4, 6].forEach(c => {
    const btn = document.getElementById('btn' + c + 'S');
    if (btn) {
      if (c === cells) btn.classList.add('active');
      else btn.classList.remove('active');
    }
  });

  const specsEl = document.getElementById('battSpecs');
  if (specsEl) {
    if (cells === 3) specsEl.innerText = '3S LiPo • 9.9V min • 12.6V max';
    else if (cells === 4) specsEl.innerText = '4S LiPo • 13.2V min • 16.8V max';
    else if (cells === 6) specsEl.innerText = '6S LiPo • 19.8V min • 25.2V max';
  }

  const lbl = document.getElementById('cellsCountLabel');
  if (lbl) lbl.innerText = cells + 'S';

  renderCellsGrid(cells, lastKnownAvg);
}

function renderCellsGrid(cells, avgV) {
  const grid = document.getElementById('cellsGrid');
  if (!grid) return;
  let html = '';
  for (let i = 1; i <= cells; i++) {
    html += `
      <div class="stat-box" style="text-align:center;padding:10px 8px;">
        <div class="stat-label" style="font-size:0.75rem;"><i class="fa-solid fa-bolt"></i> Célula ${i}</div>
        <div class="stat-value cell-v" style="font-size:1.15rem;color:var(--primary);margin-top:4px;">${avgV > 0 ? avgV.toFixed(2) + ' V' : '-- V'}</div>
      </div>
    `;
  }
  grid.innerHTML = html;
}

function showSaveBadge() {
  const badge = document.getElementById('saveStatusBadge');
  if (badge) {
    badge.style.display = 'inline-flex';
    setTimeout(() => { badge.style.display = 'none'; }, 2500);
  }
}

function selectBattery(cells) {
  updateBatteryUI(cells);
  fetch('/api/battery/config', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ cells: cells })
  })
  .then(r => r.json())
  .then(res => showSaveBadge())
  .catch(err => console.error('Erro ao guardar config de bateria:', err));
}

function saveCapacity() {
  const cap = parseInt(document.getElementById('batCapacityInput').value, 10);
  if (isNaN(cap) || cap < 500 || cap > 50000) {
    alert('Introduza um valor válido de capacidade entre 500 e 50000 mAh.');
    return;
  }
  fetch('/api/battery/config', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ capacity_mah: cap })
  })
  .then(r => r.json())
  .then(res => showSaveBadge())
  .catch(err => console.error('Erro ao guardar capacidade:', err));
}

function resetConsumedCounter() {
  if (!confirm('Deseja repor a zero o contador de mAh consumidos para a nova bateria?')) return;
  fetch('/api/battery/reset', { method: 'POST' })
    .then(r => r.json())
    .then(res => showSaveBadge())
    .catch(err => console.error(err));
}

// Obter configuração inicial de células e capacidade
fetch('/api/battery/config')
  .then(r => r.json())
  .then(data => {
    if (data.cells) updateBatteryUI(data.cells);
    else updateBatteryUI(4);
    if (data.capacity_mah) {
      const inp = document.getElementById('batCapacityInput');
      if (inp) inp.value = data.capacity_mah;
    }
  })
  .catch(() => updateBatteryUI(4));

setInterval(() => {
  fetch('/api/status')
    .then(r => r.json())
    .then(data => {
      const b = data.battery;
      if (!b) return;
      const p = b.pct || 0;
      const v = b.voltage || 0;
      const vc = b.cell_avg || 0;
      const cap = b.capacity_mah || 2650;
      const rem = (b.remaining_mah !== undefined) ? b.remaining_mah : Math.max(0, cap - (b.consumed_mah || 0));
      const hrs = b.autonomy_hrs;
      lastKnownAvg = vc;
      
      if (b.cells && b.cells !== currentCells) {
        updateBatteryUI(b.cells);
      }

      const col = p < 25 ? 'var(--red)' : (p < 50 ? 'var(--yellow)' : 'var(--green)');
      document.getElementById('batPct').innerText = p + '%';
      document.getElementById('batPct').style.color = col;
      document.getElementById('batBar').style.width = p + '%';
      document.getElementById('batBar').style.background = col;
      document.getElementById('vTotVal').innerText = v.toFixed(2) + ' V';
      document.getElementById('vCellVal').innerText = vc.toFixed(2) + ' V/cel';
      document.getElementById('currVal').innerText = (b.current_a || 0).toFixed(2) + ' A';
      document.getElementById('consVal').innerText = (b.consumed_mah || 0).toFixed(0) + ' mAh';
      document.getElementById('remVal').innerText = rem.toFixed(0) + ' / ' + cap + ' mAh';

      const autEl = document.getElementById('autVal');
      if (autEl) {
        if (hrs !== undefined && hrs > 0) {
          const h = Math.floor(hrs);
          const m = Math.round((hrs - h) * 60);
          autEl.innerText = '~' + h + 'h ' + (m < 10 ? '0' : '') + m + 'm';
          autEl.style.color = (hrs < 0.5) ? 'var(--red)' : ((hrs < 1.0) ? 'var(--yellow)' : 'var(--green)');
        } else {
          autEl.innerText = ((b.current_a || 0) > 0.05) ? '< 1 min' : 'Standby / Parado';
          autEl.style.color = 'var(--muted)';
        }
      }

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
  <div class="card-title" style="display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:8px;">
    <span><i class="fa-solid fa-gamepad"></i> Motor Principal (ESC), Leme & Joystick 2D</span>
    <span id="joyLiveBadge" class="badge" style="background:#1e293b;border:1px solid #334155;color:var(--muted);font-size:0.85rem;">Joystick: CENTRADO (0%)</span>
  </div>
  <div style="display:grid;grid-template-columns:1fr auto;gap:24px;align-items:center;">
    <div>
      <div style="margin-bottom:16px;">
        <div class="stat-label">Aceleração ESC (Motor): <span id="thrVal" style="font-size:1.1rem;font-weight:700;color:var(--primary);">0%</span></div>
        <input type="range" id="throttleSlider" min="-100" max="100" value="0" class="slider" oninput="updateSliders()" onchange="isUserDraggingThr=false;sendActuators()" onmousedown="isUserDraggingThr=true" ontouchstart="isUserDraggingThr=true" onmouseup="isUserDraggingThr=false" ontouchend="isUserDraggingThr=false">
        <div class="btn-group" style="margin-top:10px;">
          <button type="button" onclick="setThrottle(0)" class="btn btn-secondary">0% (Stop)</button>
          <button type="button" onclick="setThrottle(25)" class="btn btn-secondary">+25%</button>
          <button type="button" onclick="setThrottle(50)" class="btn btn-secondary">+50%</button>
          <button type="button" onclick="setThrottle(100)" class="btn btn-primary">+100%</button>
          <button type="button" onclick="setThrottle(-50)" class="btn btn-secondary">-50% (Ré)</button>
        </div>
      </div>

      <div>
        <div class="stat-label">Ângulo do Leme (Direção): <span id="rudVal" style="font-size:1.1rem;font-weight:700;color:var(--primary);">0%</span></div>
        <input type="range" id="rudderSlider" min="-100" max="100" value="0" class="slider" oninput="updateSliders()" onchange="isUserDraggingRud=false;sendActuators()" onmousedown="isUserDraggingRud=true" ontouchstart="isUserDraggingRud=true" onmouseup="isUserDraggingRud=false" ontouchend="isUserDraggingRud=false">
        <div class="btn-group" style="margin-top:10px;">
          <button type="button" onclick="setRudder(-75)" class="btn btn-secondary"><i class="fa-solid fa-arrow-left"></i> Bombordo (-75%)</button>
          <button type="button" onclick="setRudder(0)" class="btn btn-secondary"><i class="fa-solid fa-compass"></i> Centro (0%)</button>
          <button type="button" onclick="setRudder(75)" class="btn btn-secondary">Estibordo (+75%) <i class="fa-solid fa-arrow-right"></i></button>
        </div>
      </div>
    </div>

    <div style="display:flex;flex-direction:column;align-items:center;background:#0b1120;border:1px solid #1e293b;border-radius:12px;padding:16px;">
      <div style="font-size:0.8rem;font-weight:700;color:var(--muted);margin-bottom:8px;text-transform:uppercase;letter-spacing:0.5px;">Deflexão Joystick 2D</div>
      <canvas id="joyCanvas" width="180" height="180" style="background:#030712;border-radius:50%;border:2px solid #1e293b;cursor:crosshair;touch-action:none;"></canvas>
      <div style="font-size:0.75rem;color:var(--muted);margin-top:8px;">Arrasta ou move o stick físico</div>
    </div>
  </div>
</div>

<div class="card" style="border:1px solid #0284c7;">
  <div class="card-title" style="display:flex;justify-content:space-between;align-items:center;">
    <span><i class="fa-solid fa-compass-drafting"></i> Contador de Cabo da Âncora (Sensor Hall DRV5032 - GPIO 20)</span>
    <span id="anchorLimitBadge" class="badge" style="background:#1e293b;border:1px solid #334155;color:var(--muted);font-size:0.85rem;">Fim de Curso: LIVRE</span>
  </div>
  
  <div class="grid-4" style="margin-bottom:14px;">
    <div class="stat-box">
      <div class="stat-label">Cabo Desenrolado</div>
      <div class="stat-value" id="hallDepthVal" style="color:var(--primary);font-size:1.4rem;font-weight:700;">0.00 m</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Voltas do Tambor</div>
      <div class="stat-value" id="hallRevsVal" style="color:#38bdf8;font-size:1.4rem;font-weight:700;">0.00</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Total de Pulsos</div>
      <div class="stat-value" id="hallPulsesVal" style="font-size:1.4rem;font-weight:700;">0</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Perímetro / Volta</div>
      <div class="stat-value" id="hallCircVal" style="color:var(--yellow);font-size:1.2rem;font-weight:600;">9.4 cm</div>
    </div>
  </div>

  <div style="background:rgba(15,23,42,0.6);border:1px solid #334155;border-radius:8px;padding:14px;">
    <div style="font-weight:600;font-size:0.95rem;margin-bottom:10px;color:var(--text);"><i class="fa-solid fa-gear"></i> Calibração do Tambor do Guincho</div>
    <div style="display:flex;flex-wrap:wrap;gap:16px;align-items:flex-end;">
      <div>
        <label style="display:block;font-size:0.8rem;color:var(--muted);margin-bottom:4px;">Diâmetro do Tambor (mm):</label>
        <input type="number" id="cfgDrumDiam" step="0.5" min="5" max="500" value="30.0" style="background:#0f172a;border:1px solid #334155;color:#fff;padding:8px 12px;border-radius:6px;width:130px;font-size:1rem;">
      </div>
      <div>
        <label style="display:block;font-size:0.8rem;color:var(--muted);margin-bottom:4px;">Pulsos por Volta (Ímanes):</label>
        <input type="number" id="cfgPulsesPerRev" min="1" max="24" value="1" style="background:#0f172a;border:1px solid #334155;color:#fff;padding:8px 12px;border-radius:6px;width:110px;font-size:1rem;">
      </div>
      <button type="button" onclick="saveAnchorConfig()" class="btn btn-primary" style="padding:10px 18px;"><i class="fa-solid fa-floppy-disk"></i> Guardar Calibração</button>
      <button type="button" onclick="zeroAnchorCounter()" class="btn btn-secondary" style="background:#0284c7;color:#fff;padding:10px 18px;"><i class="fa-solid fa-rotate-left"></i> Zerar Contador (0.0 m)</button>
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
let isUserDraggingThr = false;
let isUserDraggingRud = false;
let isUserDraggingPad = false;
let curThr = 0;
let curRud = 0;

const joyCanvas = document.getElementById('joyCanvas');
const joyCtx = joyCanvas ? joyCanvas.getContext('2d') : null;

function drawJoystickPad(thr, rud) {
  if (!joyCtx || !joyCanvas) return;
  const w = joyCanvas.width;
  const h = joyCanvas.height;
  const cx = w / 2;
  const cy = h / 2;
  const r = (w / 2) - 12;

  joyCtx.clearRect(0, 0, w, h);

  // Outer circular area
  joyCtx.beginPath();
  joyCtx.arc(cx, cy, r, 0, Math.PI * 2);
  joyCtx.fillStyle = '#0f172a';
  joyCtx.fill();
  joyCtx.strokeStyle = '#334155';
  joyCtx.lineWidth = 2;
  joyCtx.stroke();

  // Deadband zone
  joyCtx.beginPath();
  joyCtx.arc(cx, cy, r * 0.15, 0, Math.PI * 2);
  joyCtx.strokeStyle = 'rgba(51,65,85,0.6)';
  joyCtx.lineWidth = 1;
  joyCtx.stroke();

  // Crosshairs
  joyCtx.strokeStyle = '#1e293b';
  joyCtx.beginPath();
  joyCtx.moveTo(cx, 12);
  joyCtx.lineTo(cx, h - 12);
  joyCtx.moveTo(12, cy);
  joyCtx.lineTo(w - 12, cy);
  joyCtx.stroke();

  // Labels
  joyCtx.fillStyle = '#64748b';
  joyCtx.font = '10px sans-serif';
  joyCtx.textAlign = 'center';
  joyCtx.fillText('FRENTE', cx, 20);
  joyCtx.fillText('RÉ', cx, h - 12);
  joyCtx.fillText('BOM', 22, cy + 4);
  joyCtx.fillText('EST', w - 22, cy + 4);

  // Puck Position: rud (-100..100) -> X, thr (-100..100) -> Y (inverted)
  const puckX = cx + (rud / 100.0) * (r - 16);
  const puckY = cy - (thr / 100.0) * (r - 16);

  // Line from center
  joyCtx.beginPath();
  joyCtx.moveTo(cx, cy);
  joyCtx.lineTo(puckX, puckY);
  joyCtx.strokeStyle = (thr !== 0 || rud !== 0) ? 'rgba(6,182,212,0.6)' : 'rgba(100,116,139,0.2)';
  joyCtx.lineWidth = 3;
  joyCtx.stroke();

  // Puck circle
  joyCtx.beginPath();
  joyCtx.arc(puckX, puckY, 13, 0, Math.PI * 2);
  joyCtx.fillStyle = (thr !== 0 || rud !== 0) ? '#06b6d4' : '#64748b';
  joyCtx.fill();
  joyCtx.strokeStyle = '#ffffff';
  joyCtx.lineWidth = 2;
  joyCtx.stroke();

  joyCtx.beginPath();
  joyCtx.arc(puckX, puckY, 3, 0, Math.PI * 2);
  joyCtx.fillStyle = '#ffffff';
  joyCtx.fill();
}

function updateSliders() {
  const t = parseInt(document.getElementById('throttleSlider').value) || 0;
  const r = parseInt(document.getElementById('rudderSlider').value) || 0;
  document.getElementById('thrVal').innerText = t + '%';
  document.getElementById('rudVal').innerText = r + '%';
  drawJoystickPad(t, r);
  updateJoyBadge(t, r);
}

function updateJoyBadge(t, r) {
  const badge = document.getElementById('joyLiveBadge');
  if (badge) {
    if (t !== 0 || r !== 0) {
      badge.innerText = `Joystick: ATIVO (T:${t>0?'+':''}${t}% | R:${r>0?'+':''}${r}%)`;
      badge.style.background = '#059669';
      badge.style.color = '#fff';
    } else {
      badge.innerText = 'Joystick: CENTRADO (0%)';
      badge.style.background = '#1e293b';
      badge.style.color = 'var(--muted)';
    }
  }
}

function setThrottle(v) {
  isUserDraggingThr = false;
  document.getElementById('throttleSlider').value = v;
  updateSliders();
  sendActuators();
}

function setRudder(v) {
  isUserDraggingRud = false;
  document.getElementById('rudderSlider').value = v;
  updateSliders();
  sendActuators();
}

function sendActuators() {
  const t = parseInt(document.getElementById('throttleSlider').value) || 0;
  const r = parseInt(document.getElementById('rudderSlider').value) || 0;
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

function zeroAnchorCounter() {
  sendAction(15);
}

function saveAnchorConfig() {
  const d = parseFloat(document.getElementById('cfgDrumDiam').value) || 30.0;
  const p = parseInt(document.getElementById('cfgPulsesPerRev').value) || 1;
  fetch('/api/control/anchor/config', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ diam_mm: d, pulses_per_rev: p })
  })
  .then(r => r.json())
  .then(res => showToast(res.msg || 'Calibração guardada com sucesso!'));
}

function sendEmergencyStop() {
  setThrottle(0);
  setRudder(0);
  sendAction(2);
}

// Interatividade no Canvas 2D
if (joyCanvas) {
  function handlePointer(e) {
    const rect = joyCanvas.getBoundingClientRect();
    const clientX = e.touches ? e.touches[0].clientX : e.clientX;
    const clientY = e.touches ? e.touches[0].clientY : e.clientY;
    const cx = rect.width / 2;
    const cy = rect.height / 2;
    const dx = clientX - rect.left - cx;
    const dy = clientY - rect.top - cy;
    const maxR = (rect.width / 2) - 12;

    let rud = Math.round(Math.max(-100, Math.min(100, (dx / maxR) * 100)));
    let thr = Math.round(Math.max(-100, Math.min(100, (-dy / maxR) * 100)));

    curRud = rud;
    curThr = thr;
    document.getElementById('throttleSlider').value = thr;
    document.getElementById('rudderSlider').value = rud;
    updateSliders();
    sendActuators();
  }

  joyCanvas.addEventListener('mousedown', (e) => {
    isUserDraggingPad = true;
    handlePointer(e);
  });
  window.addEventListener('mousemove', (e) => {
    if (isUserDraggingPad) handlePointer(e);
  });
  window.addEventListener('mouseup', () => {
    if (isUserDraggingPad) {
      isUserDraggingPad = false;
      curRud = 0;
      curThr = 0;
      document.getElementById('throttleSlider').value = 0;
      document.getElementById('rudderSlider').value = 0;
      updateSliders();
      sendActuators();
    }
  });

  joyCanvas.addEventListener('touchstart', (e) => {
    isUserDraggingPad = true;
    handlePointer(e);
    e.preventDefault();
  }, { passive: false });
  joyCanvas.addEventListener('touchmove', (e) => {
    if (isUserDraggingPad) {
      handlePointer(e);
      e.preventDefault();
    }
  }, { passive: false });
  joyCanvas.addEventListener('touchend', (e) => {
    if (isUserDraggingPad) {
      isUserDraggingPad = false;
      curRud = 0;
      curThr = 0;
      document.getElementById('throttleSlider').value = 0;
      document.getElementById('rudderSlider').value = 0;
      updateSliders();
      sendActuators();
      e.preventDefault();
    }
  }, { passive: false });
}

// Inicializar Canvas
drawJoystickPad(0, 0);

setInterval(() => {
  fetch('/api/status')
    .then(r => r.json())
    .then(data => {
      const a = data.actuators;
      if (!a) return;
      document.getElementById('curMotor').innerText = a.motor_status_str || 'idle';
      document.getElementById('curAnchor').innerText = a.anchor_status_str || 'recolhida';
      document.getElementById('curDepth').innerText = (a.anchor_depth_m || 0).toFixed(2) + ' m';
      document.getElementById('curRack').innerText = a.rack_released ? 'LIVRE' : 'TRAVADA';

      const hd = document.getElementById('hallDepthVal');
      if (hd && a.anchor_depth_m !== undefined) {
        hd.innerText = a.anchor_depth_m.toFixed(2) + ' m (' + (a.anchor_depth_cm || 0) + ' cm)';
      }
      const hr = document.getElementById('hallRevsVal');
      if (hr && a.anchor_revs !== undefined) {
        hr.innerText = a.anchor_revs.toFixed(2);
      }
      const hp = document.getElementById('hallPulsesVal');
      if (hp && a.anchor_pulses !== undefined) {
        hp.innerText = a.anchor_pulses;
      }
      const hc = document.getElementById('hallCircVal');
      if (hc && a.anchor_drum_diam_mm !== undefined) {
        const circ = (Math.PI * a.anchor_drum_diam_mm / 10.0);
        hc.innerText = circ.toFixed(1) + ' cm';
        const dInput = document.getElementById('cfgDrumDiam');
        if (dInput && !dInput.matches(':focus')) {
          dInput.value = a.anchor_drum_diam_mm;
        }
      }
      const pInput = document.getElementById('cfgPulsesPerRev');
      if (pInput && a.anchor_pulses_per_rev !== undefined && !pInput.matches(':focus')) {
        pInput.value = a.anchor_pulses_per_rev;
      }
      const lb = document.getElementById('anchorLimitBadge');
      if (lb) {
        if (a.anchor_limit) {
          lb.innerText = 'Fim de Curso: NO TOPO (Acionado)';
          lb.style.background = '#059669';
          lb.style.color = '#fff';
        } else {
          lb.innerText = 'Fim de Curso: LIVRE';
          lb.style.background = '#1e293b';
          lb.style.color = 'var(--muted)';
        }
      }

      if (!isUserDraggingThr && !isUserDraggingPad && a.throttle !== undefined) {
        document.getElementById('throttleSlider').value = a.throttle;
        document.getElementById('thrVal').innerText = a.throttle + '%';
      }

      if (!isUserDraggingRud && !isUserDraggingPad && a.rudder !== undefined) {
        document.getElementById('rudderSlider').value = a.rudder;
        document.getElementById('rudVal').innerText = a.rudder + '%';
      }

      if (!isUserDraggingPad && a.throttle !== undefined && a.rudder !== undefined) {
        drawJoystickPad(a.throttle, a.rudder);
        updateJoyBadge(a.throttle, a.rudder);
      }
    }).catch(() => {});
}, 250);
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

<!-- Secção OTA (Dual ESP32-C6) -->
<div class="card" style="border-color:var(--primary);margin-top:24px;">
  <div class="card-title">
    <span><i class="fa-solid fa-cloud-arrow-up"></i> Atualização de Firmware Over-The-Air (OTA)</span>
    <span class="badge" style="background:#1e293b;color:var(--primary);font-size:0.85rem;font-weight:700;">Dual ESP32-C6</span>
  </div>
  <p style="color:var(--muted);font-size:0.875rem;margin-bottom:16px;">
    Envie um único pacote <code>rover_firmware.zip</code> contendo os binários de ambos os processadores (ou um ficheiro <code>.bin</code> individual).
    A atualização é sequencial e à prova de falhas: o <strong>Coprocessador C6</strong> é atualizado e validado primeiro, seguido pelo <strong>Master Lafvin</strong>.
  </p>

  <!-- Informações de Versão Ativa dos Módulos -->
  <div style="display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-bottom:16px;">
    <div style="background:rgba(15,23,42,0.7);padding:14px;border-radius:8px;border:1px solid var(--card-border);">
      <div style="color:var(--muted);font-size:0.75rem;text-transform:uppercase;font-weight:600;margin-bottom:4px;">
        <i class="fa-solid fa-microchip" style="color:var(--primary);margin-right:6px;"></i>Master Lafvin (LCD+SD+WiFi)
      </div>
      <div id="fwLafvinVer" style="font-size:1.3rem;font-weight:700;color:var(--green);">v2.1.0</div>
      <div style="color:var(--muted);font-size:0.7rem;margin-top:2px;">ST7789 Display & Servidor Web</div>
    </div>
    <div style="background:rgba(15,23,42,0.7);padding:14px;border-radius:8px;border:1px solid var(--card-border);">
      <div style="color:var(--muted);font-size:0.75rem;text-transform:uppercase;font-weight:600;margin-bottom:4px;">
        <i class="fa-solid fa-satellite-dish" style="color:var(--yellow);margin-right:6px;"></i>Coprocessador C6 (LoRa+Nav)
      </div>
      <div id="fwC6Ver" style="font-size:1.3rem;font-weight:700;color:var(--yellow);">A detetar...</div>
      <div style="color:var(--muted);font-size:0.7rem;margin-top:2px;">LoRa SX1278, GPS, ADC & Atuadores</div>
    </div>
  </div>

  <div id="otaDropZone" style="background:rgba(0,0,0,0.25);padding:24px;border-radius:12px;border:2px dashed var(--card-border);text-align:center;transition:all 0.3s;"
       ondragover="event.preventDefault();this.style.borderColor='var(--primary)';"
       ondragleave="this.style.borderColor='var(--card-border)';"
       ondrop="event.preventDefault();this.style.borderColor='var(--card-border)';if(event.dataTransfer.files.length)onOtaFileSelected(event.dataTransfer.files[0]);">
    <i class="fa-solid fa-file-zipper" style="font-size:2.8rem;color:var(--primary);margin-bottom:12px;display:inline-block;"></i>
    <div style="font-size:1.05rem;font-weight:600;margin-bottom:6px;">Arraste o pacote <code>rover_firmware.zip</code> para aqui</div>
    <div style="font-size:0.85rem;color:var(--muted);margin-bottom:16px;">Ou clique abaixo para selecionar o ficheiro (.zip com c6.bin + lafvin.bin)</div>
    <input type="file" id="otaFileInput" accept=".zip,.bin" style="display:none;" onchange="if(this.files.length)onOtaFileSelected(this.files[0])">
    <button type="button" class="btn btn-secondary" onclick="document.getElementById('otaFileInput').click()" style="padding:10px 20px;">
      <i class="fa-solid fa-folder-open"></i> Escolher Ficheiro de Firmware
    </button>
  </div>

  <!-- Painel de Progresso do OTA -->
  <div id="otaProgressPanel" style="display:none;margin-top:20px;background:#0b1120;padding:18px;border-radius:10px;border:1px solid var(--card-border);">
    <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:8px;">
      <span id="otaStepTitle" style="font-weight:700;color:var(--primary);font-size:0.95rem;">
        <i class="fa-solid fa-spinner fa-spin"></i> A preparar atualização...
      </span>
      <span id="otaPercentText" style="font-weight:700;font-size:1rem;color:var(--text);">0%</span>
    </div>
    <div style="background:#1e293b;border-radius:6px;height:12px;overflow:hidden;margin-bottom:12px;">
      <div id="otaProgressBar" style="width:0%;height:100%;background:linear-gradient(90deg, #06b6d4, #10b981);transition:width 0.3s ease;"></div>
    </div>
    <div id="otaLogConsole" style="background:#030712;padding:12px;border-radius:6px;font-family:monospace;font-size:0.75rem;max-height:140px;overflow-y:auto;color:#94a3b8;line-height:1.5;">
      <div>[OTA] À espera de comando...</div>
    </div>
  </div>
</div>

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
      if (data.version_lafvin && document.getElementById('fwLafvinVer')) document.getElementById('fwLafvinVer').innerText = data.version_lafvin;
      if (data.version_c6 && document.getElementById('fwC6Ver')) document.getElementById('fwC6Ver').innerText = data.version_c6;
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

// ----------------- Funções OTA Dual ESP32-C6 -----------------
function otaLog(msg, color) {
  const c = document.getElementById('otaLogConsole');
  if (!c) return;
  const line = document.createElement('div');
  line.innerText = '[' + new Date().toLocaleTimeString() + '] ' + msg;
  if (color) line.style.color = color;
  c.appendChild(line);
  c.scrollTop = c.scrollHeight;
}

function setOtaProgress(pct, title, isSpin = true) {
  const pBar = document.getElementById('otaProgressBar');
  const pTxt = document.getElementById('otaPercentText');
  const tEl  = document.getElementById('otaStepTitle');
  if (pBar) pBar.style.width = Math.min(100, Math.max(0, pct)) + '%';
  if (pTxt) pTxt.innerText = Math.round(pct) + '%';
  if (tEl && title) {
    tEl.innerHTML = (isSpin ? '<i class="fa-solid fa-spinner fa-spin"></i> ' : '') + title;
  }
}

async function parseZipBuffer(arrayBuffer) {
  const view = new DataView(arrayBuffer);
  let offset = 0;
  const files = {};
  while (offset + 30 <= arrayBuffer.byteLength) {
    const sig = view.getUint32(offset, true);
    if (sig !== 0x04034b50) break;
    const method = view.getUint16(offset + 8, true);
    const compSize = view.getUint32(offset + 18, true);
    const uncompSize = view.getUint32(offset + 22, true);
    const nameLen = view.getUint16(offset + 26, true);
    const extraLen = view.getUint16(offset + 28, true);
    const name = new TextDecoder().decode(new Uint8Array(arrayBuffer, offset + 30, nameLen));
    const compData = new Uint8Array(arrayBuffer, offset + 30 + nameLen + extraLen, compSize);
    
    let data;
    if (method === 0) {
      data = compData;
    } else if (method === 8) {
      const ds = new DecompressionStream('deflate-raw');
      const decompressed = new Response(new Blob([compData]).stream().pipeThrough(ds));
      const resBuf = await decompressed.arrayBuffer();
      data = new Uint8Array(resBuf);
    }
    if (data) {
      files[name.toLowerCase()] = { name: name, data: data, size: data.byteLength };
    }
    offset += 30 + nameLen + extraLen + compSize;
  }
  return files;
}

async function onOtaFileSelected(file) {
  if (!file) return;
  const panel = document.getElementById('otaProgressPanel');
  panel.style.display = 'block';
  document.getElementById('otaLogConsole').innerHTML = '';
  setOtaProgress(5, 'A analisar pacote ' + file.name + '...');
  otaLog('Ficheiro selecionado: ' + file.name + ' (' + Math.round(file.size/1024) + ' KB)');

  let c6Blob = null;
  let lafvinBlob = null;
  const webFiles = [];

  try {
    if (file.name.toLowerCase().endsWith('.zip')) {
      otaLog('A descompactar arquivo ZIP no navegador...');
      const buf = await file.arrayBuffer();
      const files = await parseZipBuffer(buf);
      
      for (const k in files) {
        const item = files[k];
        if (k.endsWith('c6.bin') || (k.includes('c6') && k.endsWith('.bin'))) {
          c6Blob = new Blob([item.data], { type: 'application/octet-stream' });
          otaLog('  -> Firmware C6: ' + item.name + ' (' + Math.round(item.size/1024) + ' KB)');
        } else if (k.endsWith('lafvin.bin') || ((k.includes('lafvin') || k.includes('master')) && k.endsWith('.bin'))) {
          lafvinBlob = new Blob([item.data], { type: 'application/octet-stream' });
          otaLog('  -> Firmware Master Lafvin: ' + item.name + ' (' + Math.round(item.size/1024) + ' KB)');
        } else if (k.startsWith('www/') || k.startsWith('system/')) {
          webFiles.push({ path: item.name, data: item.data, size: item.size });
        }
      }
      if (webFiles.length > 0) {
        otaLog('  -> Ficheiros Web & Sistema detetados no ZIP: ' + webFiles.length + ' ficheiros', '#06b6d4');
      }
    } else if (file.name.toLowerCase().endsWith('.bin')) {
      if (file.name.toLowerCase().includes('c6')) {
        c6Blob = file;
      } else {
        lafvinBlob = file;
      }
    }
  } catch (err) {
    otaLog('ERRO ao descompactar ZIP: ' + err.message, '#ef4444');
    setOtaProgress(0, 'Falha ao processar ficheiro', false);
    return;
  }

  if (!c6Blob && !lafvinBlob && webFiles.length === 0) {
    otaLog('ERRO: Nenhum firmware ou ficheiro web compatível encontrado no pacote!', '#ef4444');
    setOtaProgress(0, 'Nenhum item compatível detetado', false);
    return;
  }

  // PASSO 1: Atualização dos ficheiros Web no Cartão MicroSD (se presentes)
  if (webFiles.length > 0) {
    setOtaProgress(5, 'A atualizar frontend no Cartão SD (0/' + webFiles.length + ')...');
    otaLog('★ [Passo 1] A atualizar ' + webFiles.length + ' ficheiros web no Cartão MicroSD...', '#06b6d4');
    let okCount = 0;
    for (let i = 0; i < webFiles.length; i++) {
      const wf = webFiles[i];
      const parts = wf.path.split('/');
      const fileName = parts.pop();
      const dirPath = '/' + parts.join('/');
      
      const pct = 5 + Math.round((i / webFiles.length) * 15); // 5% a 20%
      setOtaProgress(pct, 'A gravar /' + wf.path + ' (' + (i + 1) + '/' + webFiles.length + ')...');
      
      const blob = new Blob([wf.data], { type: 'application/octet-stream' });
      const formData = new FormData();
      formData.append('file', blob, fileName);
      try {
        const uploadUrl = '/api/sd/upload?dir=' + encodeURIComponent(dirPath) + '&ajax=1';
        const res = await fetch(uploadUrl, { method: 'POST', body: formData });
        const resJson = await res.json();
        if (resJson && resJson.success) {
          okCount++;
        } else {
          otaLog('  ⚠ Falha ao gravar /' + wf.path, '#f59e0b');
        }
      } catch (err) {
        otaLog('  ⚠ Erro de rede em /' + wf.path + ': ' + err.message, '#f59e0b');
      }
    }
    otaLog('✔ ' + okCount + '/' + webFiles.length + ' ficheiros web atualizados com sucesso no MicroSD!', '#10b981');
  }

  // PASSO 2: Atualização do Coprocessador C6
  if (c6Blob) {
    const ok = await performC6Ota(c6Blob);
    if (!ok) {
      otaLog('Processo interrompido devido a erro no C6.', '#ef4444');
      return;
    }
  }

  // PASSO 3: Atualização do Master Lafvin
  if (lafvinBlob) {
    await performLafvinOta(lafvinBlob);
  } else {
    setOtaProgress(100, 'Atualização concluída com sucesso!', false);
    otaLog('★ Todas as operações de atualização foram concluídas!', '#10b981');
  }
}

async function performC6Ota(blob) {
  setOtaProgress(15, '[Passo 1/2] A enviar c6.bin para o Master...');
  otaLog('A enviar binário C6 (' + Math.round(blob.size/1024) + ' KB) para o cartão SD...');

  const formData = new FormData();
  formData.append('file', blob, 'c6.bin');

  try {
    const res = await fetch('/api/ota/c6', { method: 'POST', body: formData });
    const json = await res.json();
    if (!json.success) {
      otaLog('ERRO ao enviar c6.bin: ' + (json.error || 'Erro desconhecido'), '#ef4444');
      setOtaProgress(15, 'Falha no envio do C6', false);
      return false;
    }
    otaLog('c6.bin gravado no SD! Comando de download transmitido ao Coprocessador C6.');
  } catch (e) {
    otaLog('Falha de rede ao enviar c6.bin: ' + e.message, '#ef4444');
    return false;
  }

  // Polling do estado do C6
  setOtaProgress(35, '[Passo 1/2] Coprocessador C6 a gravar Flash...');
  otaLog('A aguardar confirmação de gravação do Coprocessador C6...');

  for (let i = 0; i < 35; i++) {
    await new Promise(r => setTimeout(r, 1000));
    try {
      const sRes = await fetch('/api/ota/c6/status');
      const sJson = await sRes.json();
      otaLog('  -> C6 Status: ' + sJson.state + (sJson.detail ? ' (' + sJson.detail + ')' : ''));
      if (sJson.progress !== undefined && sJson.progress > 0) {
        const pct = 15 + Math.round(sJson.progress * 0.35); // 15% -> 50%
        setOtaProgress(pct, '[Passo 1/2] Coprocessador C6 a gravar Flash (' + sJson.progress + '%)...');
      }
      if (sJson.state === 'SUCCESS') {
        setOtaProgress(50, '[Passo 1/2] Coprocessador C6 atualizado com sucesso!', false);
        otaLog('✔ Coprocessador C6 gravado e reiniciado com sucesso!', '#10b981');
        await new Promise(r => setTimeout(r, 2000));
        return true;
      } else if (sJson.state && sJson.state.startsWith('FAIL')) {
        otaLog('Falha no Coprocessador C6: ' + sJson.state, '#ef4444');
        setOtaProgress(35, 'Falha na gravação do C6', false);
        return false;
      }
    } catch (e) {}
  }
  otaLog('Aviso: Timeout à espera de resposta do C6. A prosseguir com o Master...', '#f59e0b');
  return true;
}

function performLafvinOta(blob) {
  return new Promise((resolve) => {
    setOtaProgress(55, '[Passo 2/2] A gravar Master Lafvin...');
    otaLog('A enviar e gravar lafvin.bin na partição OTA do Master (' + Math.round(blob.size/1024) + ' KB)...');

    const xhr = new XMLHttpRequest();
    xhr.open('POST', '/api/ota/lafvin', true);

    xhr.upload.onprogress = function(e) {
      if (e.lengthComputable) {
        const pct = 55 + (e.loaded / e.total) * 40; // 55% a 95%
        setOtaProgress(pct, '[Passo 2/2] A gravar Master Lafvin (' + Math.round(e.loaded/1024) + '/' + Math.round(e.total/1024) + ' KB)...');
      }
    };

    xhr.onload = function() {
      if (xhr.status === 200) {
        setOtaProgress(100, '✔ Atualização concluída com sucesso! A reiniciar Rover...', false);
        otaLog('★ Firmware do Master gravado com 100% de sucesso!', '#10b981');
        otaLog('O Rover está a reiniciar. A recarregar página em 6 segundos...', '#06b6d4');
        let count = 6;
        const timer = setInterval(() => {
          count--;
          if (count > 0) {
            setOtaProgress(100, 'A reiniciar... (' + count + 's)', false);
          } else {
            clearInterval(timer);
            window.location.reload();
          }
        }, 1000);
        resolve(true);
      } else {
        otaLog('ERRO na gravação do Master Lafvin: ' + xhr.responseText, '#ef4444');
        setOtaProgress(55, 'Falha no Master Lafvin', false);
        resolve(false);
      }
    };

    xhr.onerror = function() {
      otaLog('Falha de rede ao gravar Master Lafvin.', '#ef4444');
      resolve(false);
    };

    const formData = new FormData();
    formData.append('file', blob, 'lafvin.bin');
    xhr.send(formData);
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
    <div id="binaryNotice" style="display:none;padding:32px 20px;text-align:center;background:rgba(255,255,255,0.03);border:1px solid var(--border);border-radius:8px;margin-bottom:12px;">
      <div id="binaryIcon" style="font-size:3rem;margin-bottom:12px;"><i class="fa-solid fa-file"></i></div>
      <h3 id="binaryName" style="margin-bottom:6px;font-size:1.1rem;color:var(--text);">Ficheiro Binário</h3>
      <p style="color:var(--muted);font-size:0.85rem;margin-bottom:18px;">Este ficheiro não é um ficheiro de texto e não pode ser editado diretamente no browser.</p>
      <button type="button" onclick="downloadCurrentFile()" class="btn btn-primary" style="display:inline-flex;align-items:center;gap:8px;"><i class="fa-solid fa-download"></i> Descarregar Ficheiro</button>
    </div>
    <textarea id="viewerContent" rows="18" style="font-family:monospace;font-size:0.85rem;line-height:1.4;white-space:pre;"></textarea>
    <div class="btn-group" style="justify-content:space-between;margin-top:14px;">
      <span id="viewerPath" style="color:var(--muted);font-size:0.8rem;align-self:center;"></span>
      <div class="btn-group">
        <button type="button" onclick="closeModal('viewerModal')" class="btn btn-secondary">Fechar</button>
        <button type="button" id="viewerDownloadBtn" onclick="downloadCurrentFile()" class="btn btn-secondary"><i class="fa-solid fa-download"></i> Descarregar</button>
        <button type="button" id="viewerSaveBtn" onclick="saveEditedFile()" class="btn btn-primary"><i class="fa-solid fa-floppy-disk"></i> Guardar Alterações</button>
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

let currentViewingPath = '';

const BINARY_EXTENSIONS = [
  '.woff2', '.woff', '.ttf', '.otf', '.eot',
  '.png', '.jpg', '.jpeg', '.gif', '.ico', '.webp', '.bmp',
  '.bin', '.zip', '.tar', '.gz', '.pdf', '.mp3', '.wav'
];

function isBinaryFile(p) {
  const lower = p.toLowerCase();
  return BINARY_EXTENSIONS.some(ext => lower.endsWith(ext));
}

function onItemClick(path, isDir) {
  if (isDir) {
    changeDir(path);
  } else {
    previewFile(path);
  }
}

function previewFile(path) {
  currentViewingPath = path;
  document.getElementById('viewerPath').innerText = path;
  const fileName = path.split('/').pop();
  document.getElementById('viewerTitle').innerHTML = getFileIcon(fileName, false) + ' ' + fileName;

  const binaryBox = document.getElementById('binaryNotice');
  const txtArea = document.getElementById('viewerContent');
  const saveBtn = document.getElementById('viewerSaveBtn');

  if (isBinaryFile(path)) {
    binaryBox.style.display = 'block';
    document.getElementById('binaryName').innerText = fileName;
    document.getElementById('binaryIcon').innerHTML = getFileIcon(fileName, false);
    txtArea.style.display = 'none';
    saveBtn.style.display = 'none';
    document.getElementById('viewerModal').style.display = 'flex';
  } else {
    binaryBox.style.display = 'none';
    txtArea.style.display = 'block';
    saveBtn.style.display = 'inline-flex';
    txtArea.value = 'A carregar...';
    document.getElementById('viewerModal').style.display = 'flex';

    fetch('/api/sd/read?file=' + encodeURIComponent(path) + '&path=' + encodeURIComponent(path))
      .then(r => {
        if (!r.ok) throw new Error('HTTP ' + r.status);
        return r.text();
      })
      .then(content => {
        txtArea.value = content;
      })
      .catch(err => {
        txtArea.value = 'Erro ao ler ficheiro (' + err.message + ')';
        showToast('Erro ao ler ficheiro', true);
      });
  }
}

function downloadCurrentFile() {
  if (currentViewingPath) {
    downloadFile(currentViewingPath);
  }
}

function downloadFile(path) {
  const url = '/api/sd/download?file=' + encodeURIComponent(path) + '&path=' + encodeURIComponent(path);
  const a = document.createElement('a');
  a.href = url;
  a.download = path.split('/').pop();
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
}

function saveEditedFile() {
  const path = currentViewingPath || document.getElementById('viewerPath').innerText;
  const content = document.getElementById('viewerContent').value;
  showToast('A gravar ficheiro...');

  fetch('/api/sd/save', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ file: path, path: path, content: content })
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
  })
  .catch(() => showToast('Erro ao comunicar com o servidor', true));
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
    body: JSON.stringify({ file: path, path: path, is_dir: isDir })
  })
  .then(r => r.json())
  .then(res => {
    if (res.success) {
      showToast('Eliminado com sucesso!');
      refreshDir();
    } else {
      showToast(res.msg || 'Erro ao eliminar', true);
    }
  })
  .catch(() => showToast('Erro ao comunicar com o servidor', true));
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
        if (sel.is_online) {
          st.innerText = 'ONLINE';
          st.style.color = 'var(--green)';
        } else if (data.lora_ready) {
          st.innerText = 'RÁDIO OK (ESCUTA)';
          st.style.color = 'var(--primary)';
        } else if (data.lora_enabled) {
          st.innerText = 'FALHA RÁDIO';
          st.style.color = 'var(--red)';
        } else {
          st.innerText = 'REPOUSO';
          st.style.color = 'var(--muted)';
        }

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

BASE_WIFI_CARD = """
<div class="card">
  <div class="card-title">
    <span><i class="fa-solid fa-fingerprint"></i> Identificação Única da Base (Portal Cloud)</span>
  </div>
  <p style="color:var(--muted);font-size:0.875rem;margin-bottom:16px;">
    Este endereço MAC de hardware é o identificador único e exclusivo desta Base Station utilizado nas comunicações com o portal WindDragons (<span style="color:var(--primary);">https://winddragons.app</span>).
  </p>
  <div class="grid-2">
    <div class="stat-box">
      <div class="stat-label">Endereço MAC de Hardware (Station ID)</div>
      <div class="stat-value" id="baseMacVal" style="color:var(--primary);font-family:monospace;font-size:1.3rem;">--:--:--:--:--:--</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Nome da Estação Base</div>
      <div class="stat-value" id="baseStationName" style="color:var(--yellow);font-size:1.3rem;">BASE-ALCOCHETE-01</div>
    </div>
  </div>
</div>
"""

BASE_LCD_CARD = """
<div class="card">
  <div class="card-title">
    <span><i class="fa-solid fa-sun"></i> Brilho do Ecrã (Display LCD ST7789)</span>
    <span id="brightValBadge" class="badge" style="background:#1e293b;color:var(--primary);font-size:0.9rem;font-weight:700;">85%</span>
  </div>
  <p style="color:var(--muted);font-size:0.875rem;margin-bottom:16px;">
    Ajuste o brilho da retroiluminação por PWM (LEDC GPIO 22). Ao mover o cursor, o ecrã atualiza em tempo real.
  </p>

  <div style="background:rgba(0,0,0,0.25);padding:18px;border-radius:12px;border:1px solid var(--card-border);">
    <div style="display:flex;align-items:center;justify-content:space-between;margin-bottom:10px;">
      <span style="font-size:0.85rem;color:var(--muted);"><i class="fa-solid fa-moon"></i> 5% Mínimo</span>
      <span id="brightDisplayVal" style="font-size:1.8rem;font-weight:700;color:var(--primary);">85%</span>
      <span style="font-size:0.85rem;color:var(--muted);"><i class="fa-solid fa-sun"></i> 100% Máximo</span>
    </div>
    <input type="range" id="cfg_bright" min="5" max="100" step="1" value="85" class="slider" oninput="onBrightSlide(this.value)" onchange="onBrightChange(this.value)">

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
    <span><i class="fa-solid fa-tv"></i> Configuração do Ecrã LCD (Base Station)</span>
    <span class="badge" style="background:#1e3a8a;color:#93c5fd;">ST7789 320x172</span>
  </div>
  <p style="color:var(--muted);font-size:0.875rem;margin-bottom:14px;">
    O ecrã da Estação Base possui 3 ecrãs principais essenciais: <strong>1. Estação (ID, MAC, Estado)</strong>, <strong>2. Conectividade WiFi</strong> e <strong>3. Rover Selecionado</strong>.
  </p>
  <div class="form-group" style="display:flex;align-items:center;gap:12px;padding:12px;background:rgba(255,255,255,0.03);border:1px solid var(--card-border);border-radius:8px;">
    <input type="checkbox" id="joyScreenCheck" onchange="toggleJoyScreen(this.checked)" style="width:20px;height:20px;cursor:pointer;">
    <label for="joyScreenCheck" style="cursor:pointer;font-size:0.95rem;font-weight:600;">
      Ativar 4º Ecrã: Teste do Joystick
      <div style="font-size:0.75rem;font-weight:normal;color:var(--muted);margin-top:2px;">
        Adiciona a mira analógica 2D e diagnóstico dos eixos ADC ao carrossel de navegação do ecrã físico.
      </div>
    </label>
  </div>
</div>
<script>
let slideTimer = null;
function onBrightSlide(v) {
  const elVal = document.getElementById('brightDisplayVal');
  if (elVal) elVal.innerText = v + '%';
  const elBadge = document.getElementById('brightValBadge');
  if (elBadge) elBadge.innerText = v + '%';
  if (slideTimer) clearTimeout(slideTimer);
  slideTimer = setTimeout(() => {
    fetch('/api/display/brightness?val=' + v, { method: 'POST' }).catch(() => {});
  }, 60);
}
function onBrightChange(v) {
  fetch('/api/display/brightness?val=' + v, { method: 'POST' }).catch(() => {});
}
function setPreset(v) {
  const el = document.getElementById('cfg_bright');
  if (el) el.value = v;
  onBrightSlide(v);
  onBrightChange(v);
}
fetch('/api/display').then(r=>r.json()).then(d=>{
  const c = document.getElementById('joyScreenCheck');
  if (c) c.checked = !!d.joy_screen;
  const b = d.screen_brightness || d.brightness || 85;
  const el = document.getElementById('cfg_bright');
  if (el) el.value = b;
  const elVal = document.getElementById('brightDisplayVal');
  if (elVal) elVal.innerText = b + '%';
  const elBadge = document.getElementById('brightValBadge');
  if (elBadge) elBadge.innerText = b + '%';
}).catch(()=>{});
function toggleJoyScreen(enabled){
  fetch('/api/display',{
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body:JSON.stringify({joy_screen:enabled})
  })
  .then(r=>r.json())
  .then(res=>{
    alert(res.joy_screen ? '4º Ecrã (Teste Joystick) ativado no LCD!' : '4º Ecrã desativado. LCD em modo simplificado de 3 ecrãs.');
  })
  .catch(e=>{
    alert('Erro ao guardar configuração do ecrã');
  });
}
</script>
"""

BASE_WIFI_CONTENT = BASE_WIFI_CARD + WIFI_CONTENT.replace(
    "Ligue o Rover ao router Wi-Fi do cais ou centro de comando para telemetria em tempo real e acesso local direto.",
    "Ligue a Base Station ao router Wi-Fi local para permitir controlo via browser e telemetria da frota em tempo real."
).replace("Redes Guardadas no Rover", "Redes Guardadas na Base Station") + BASE_LCD_CARD

BASE_SD_CONTENT = SD_CONTENT.replace("Explorador do Cartão MicroSD", "Explorador MicroSD - Base Station").replace("Cartão MicroSD - Rover", "Cartão MicroSD - Base Station").replace("let currentDir = '/';", "let currentDir = '/www/base';")

BASE_MISSION_CONTENT = """
<div class="card">
  <div class="card-title">
    <span><i class="fa-solid fa-flag-checkered"></i> Estado de Operação da Estação Base</span>
    <span id="badgeState" class="badge" style="background:#64748b;color:#fff;font-size:0.85rem;padding:6px 14px;">
      STORAGE - Not in use
    </span>
  </div>
  <p style="color:var(--muted);font-size:0.875rem;margin-bottom:14px;">
    Selecione o estado operacional da Estação Base para a missão atual:
  </p>

  <div style="display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:12px;">
    <button onclick="setState('STORAGE')" id="btnStateSTORAGE" class="btn" style="display:flex;flex-direction:column;align-items:flex-start;padding:14px;background:rgba(255,255,255,0.05);color:#fff;border:1px solid var(--card-border);">
      <span style="font-size:1.05rem;font-weight:700;"><i class="fa-solid fa-box-archive"></i> Storage</span>
      <span style="font-size:0.75rem;color:var(--muted);margin-top:4px;">Not in use (Standby)</span>
    </button>

    <button onclick="setState('WAITING_ROVERS')" id="btnStateWAITING_ROVERS" class="btn" style="display:flex;flex-direction:column;align-items:flex-start;padding:14px;background:rgba(255,255,255,0.05);color:#fff;border:1px solid var(--card-border);">
      <span style="font-size:1.05rem;font-weight:700;"><i class="fa-solid fa-hourglass-half"></i> Waiting Rover List</span>
      <span style="font-size:0.75rem;color:var(--muted);margin-top:4px;">Connected and waiting</span>
    </button>

    <button onclick="setState('IDLE')" id="btnStateIDLE" class="btn" style="display:flex;flex-direction:column;align-items:flex-start;padding:14px;background:rgba(255,255,255,0.05);color:#fff;border:1px solid var(--card-border);">
      <span style="font-size:1.05rem;font-weight:700;"><i class="fa-solid fa-anchor"></i> Idle</span>
      <span style="font-size:0.75rem;color:var(--muted);margin-top:4px;">Rover list ready</span>
    </button>

    <button onclick="setState('GO')" id="btnStateGO" class="btn" style="display:flex;flex-direction:column;align-items:flex-start;padding:14px;background:rgba(255,255,255,0.05);color:#fff;border:1px solid var(--card-border);">
      <span style="font-size:1.05rem;font-weight:700;"><i class="fa-solid fa-rocket"></i> Go</span>
      <span style="font-size:0.75rem;color:var(--muted);margin-top:4px;">In mission (Ativo)</span>
    </button>
  </div>
</div>

<div class="card">
  <div class="card-title">
    <span><i class="fa-solid fa-sailboat"></i> Dados do Percurso / Regata</span>
  </div>
  <div class="grid-2">
    <div class="form-group">
      <label class="stat-label">ID da Regata / Prova</label>
      <input type="text" id="raceIdInput" placeholder="ex: REGATA-TEJO-2026-R1">
    </div>
    <div class="form-group">
      <label class="stat-label">Nome da Missão / Prova</label>
      <input type="text" id="missionNameInput" placeholder="ex: Troféu de Outono - Prova 1">
    </div>
  </div>
</div>

<div class="card">
  <div class="card-title">
    <span><i class="fa-solid fa-satellite-dish"></i> Lista de Bóias da Missão (Persistente no MicroSD)</span>
    <span style="font-size:0.8rem;color:var(--muted);font-weight:normal;">/config/rovers.json</span>
  </div>

  <div style="overflow-x:auto;">
    <table id="roversTable">
      <thead>
        <tr>
          <th>ID</th>
          <th>Nome</th>
          <th>MAC Address</th>
          <th>Papel / Role</th>
          <th>Coord. Alvo (Lat, Lng)</th>
          <th>Status LoRa / Bat / Âncora</th>
          <th>Ações</th>
        </tr>
      </thead>
      <tbody id="roversTbody">
        <tr><td colspan="7" style="text-align:center;color:var(--muted);">A carregar lista de bóias...</td></tr>
      </tbody>
    </table>
  </div>

  <div style="display:flex;gap:10px;margin-top:16px;flex-wrap:wrap;">
    <button onclick="addRow()" class="btn btn-secondary"><i class="fa-solid fa-plus"></i> Adicionar Bóia</button>
    <button onclick="saveRovers()" class="btn btn-primary" style="padding:10px 24px;"><i class="fa-solid fa-floppy-disk"></i> Guardar Lista no MicroSD</button>
    <button onclick="clearRovers()" class="btn btn-danger" style="background:#78350f;"><i class="fa-solid fa-rotate-left"></i> Repor Pré-definições</button>
  </div>
</div>

<script>
let currentRoversData = [];
const rolesList = ['largada CR', 'largada', 'boia 1', 'boia 2', 'boia 3', 'boia 4', 'chegada CR', 'chegada'];

function updateStateButtons(st) {
  const badge = document.getElementById('badgeState');
  const btnIds = ['STORAGE', 'WAITING_ROVERS', 'IDLE', 'GO'];
  const colors = {
    'STORAGE': { bg: '#475569', border: '#94a3b8', desc: 'Not in use (Standby)' },
    'WAITING_ROVERS': { bg: '#b45309', border: '#f59e0b', desc: 'Connected and waiting' },
    'IDLE': { bg: '#0e7490', border: '#06b6d4', desc: 'Rover list ready' },
    'GO': { bg: '#047857', border: '#10b981', desc: 'In mission (Ativo)' }
  };
  
  const c = colors[st] || colors['STORAGE'];
  if (badge) {
    badge.style.background = c.border;
    badge.innerText = st + ' - ' + c.desc;
  }

  btnIds.forEach(id => {
    const btn = document.getElementById('btnState' + id);
    if (btn) {
      if (id === st) {
        btn.style.background = c.bg;
        btn.style.border = '2px solid ' + c.border;
      } else {
        btn.style.background = 'rgba(255,255,255,0.05)';
        btn.style.border = '1px solid var(--card-border)';
      }
    }
  });
}

function setState(st) {
  fetch('/api/mission/state', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ state: st })
  })
  .then(r => r.json())
  .then(res => {
    if (res.status === 'ok') {
      updateStateButtons(res.operation_state);
      loadMissionData();
    }
  });
}

let currentSelectedRoverId = 0;

function loadMissionData() {
  fetch('/api/mission')
    .then(r => r.json())
    .then(data => {
      if (data.operation_state) {
        updateStateButtons(data.operation_state);
      }
      if (data.selected_rover_id !== undefined) {
        currentSelectedRoverId = data.selected_rover_id;
      }
      if (data.race_id && document.activeElement !== document.getElementById('raceIdInput')) {
        document.getElementById('raceIdInput').value = data.race_id;
      }
      if (data.mission_name && document.activeElement !== document.getElementById('missionNameInput')) {
        document.getElementById('missionNameInput').value = data.mission_name;
      }

      if (data.rovers && Array.isArray(data.rovers)) {
        renderRoversTable(data.rovers);
      }
    })
    .catch(() => {});
}

function renderRoversTable(rovers) {
  currentRoversData = rovers;
  const tbody = document.getElementById('roversTbody');
  if (!tbody) return;

  if (tbody.contains(document.activeElement)) return;

  let html = '';
  rovers.forEach((r, idx) => {
    const isOnline = r.is_online;
    const isSelected = (currentSelectedRoverId === r.id);
    const batCol = (r.battery_pct < 25) ? 'var(--red)' : ((r.battery_pct < 50) ? 'var(--yellow)' : 'var(--green)');
    
    let optHtml = '';
    rolesList.forEach(ro => {
      const sel = (r.role === ro) ? 'selected' : '';
      optHtml += `<option value="${ro}" ${sel}>${ro}</option>`;
    });

    html += `<tr id="rRow_${r.id}">
      <td><strong>${r.id}</strong></td>
      <td><input type="text" class="r-name" value="${r.name || ('Rover ' + r.id)}" style="width:120px;padding:6px;"></td>
      <td><input type="text" class="r-mac" value="${r.mac || r.mac_address || ''}" style="width:140px;font-family:monospace;padding:6px;"></td>
      <td><select class="r-role" style="width:120px;padding:6px;">${optHtml}</select></td>
      <td>
        <div style="display:flex;gap:4px;">
          <input type="number" step="0.000001" class="r-lat" value="${(r.target_lat || 0).toFixed(6)}" placeholder="Lat" style="width:90px;padding:6px;">
          <input type="number" step="0.000001" class="r-lng" value="${(r.target_lng || 0).toFixed(6)}" placeholder="Lng" style="width:90px;padding:6px;">
        </div>
      </td>
      <td>
        <span class="badge" style="background:${isOnline ? '#065f46' : '#7f1d1d'};color:${isOnline ? '#6ee7b7' : '#fca5a5'};">
          ${isOnline ? 'ONLINE' : 'OFFLINE'}
        </span>
        ${isOnline ? `<strong style="color:${batCol};margin-left:4px;">${r.battery_pct}%</strong>` : ''}
        ${isOnline && r.anchor_depth_m > 0.05 ? `<div style="font-size:0.75rem;color:var(--yellow);margin-top:2px;"><i class="fa-solid fa-anchor"></i> Âncora: ${r.anchor_depth_m.toFixed(2)}m</div>` : ''}
      </td>
      <td>
        <div style="display:flex;gap:4px;">
          ${isSelected 
            ? `<button onclick="selectRover(0)" class="btn" style="background:#059669;color:#fff;padding:4px 8px;font-size:0.75rem;" title="Clique para desmarcar"><i class="fa-solid fa-check"></i> Ativo no LCD</button>` 
            : `<button onclick="selectRover(${r.id})" class="btn btn-primary" style="padding:4px 8px;font-size:0.75rem;">Selecionar</button>`}
          <button onclick="deleteRow(${r.id})" class="btn btn-danger" style="padding:4px 8px;font-size:0.75rem;" title="Remover Bóia">
            <i class="fa-solid fa-trash"></i>
          </button>
        </div>
      </td>
    </tr>`;
  });

  tbody.innerHTML = html;
}

function selectRover(id) {
  fetch('/api/select_rover', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ rover_id: parseInt(id) })
  })
  .then(r => r.json())
  .then(() => {
    loadMissionData();
  });
}

function deleteRow(id) {
  const row = document.getElementById('rRow_' + id);
  if (row) row.remove();
}

function addRow() {
  const tbody = document.getElementById('roversTbody');
  const id = tbody.children.length + 1;
  const tr = document.createElement('tr');
  tr.id = 'rRow_' + id;
  
  let optHtml = '';
  rolesList.forEach((ro, i) => {
    optHtml += `<option value="${ro}" ${i === 0 ? 'selected' : ''}>${ro}</option>`;
  });

  tr.innerHTML = `<td><strong>${id}</strong></td>
    <td><input type="text" class="r-name" value="WD-Boia-${id}" style="width:120px;padding:6px;"></td>
    <td><input type="text" class="r-mac" value="00:00:00:00:00:0${id}" style="width:140px;font-family:monospace;padding:6px;"></td>
    <td><select class="r-role" style="width:120px;padding:6px;">${optHtml}</select></td>
    <td>
      <div style="display:flex;gap:4px;">
        <input type="number" step="0.000001" class="r-lat" value="0.000000" style="width:90px;padding:6px;">
        <input type="number" step="0.000001" class="r-lng" value="0.000000" style="width:90px;padding:6px;">
      </div>
    </td>
    <td><span class="badge" style="background:#334155;color:#94a3b8;">NOVO</span></td>
    <td>
      <button onclick="deleteRow(${id})" class="btn btn-danger" style="padding:4px 8px;font-size:0.75rem;">
        <i class="fa-solid fa-trash"></i>
      </button>
    </td>`;
  tbody.appendChild(tr);
}

function saveRovers() {
  const raceId = document.getElementById('raceIdInput').value;
  const missionName = document.getElementById('missionNameInput').value;
  const rows = document.querySelectorAll('#roversTbody tr');
  const rovers = [];

  rows.forEach((r, idx) => {
    const nameInput = r.querySelector('.r-name');
    if (!nameInput) return;
    const name = nameInput.value;
    const mac = r.querySelector('.r-mac').value;
    const role = r.querySelector('.r-role').value;
    const lat = parseFloat(r.querySelector('.r-lat').value) || 0.0;
    const lng = parseFloat(r.querySelector('.r-lng').value) || 0.0;
    rovers.push({ id: idx + 1, name: name, mac_address: mac, role: role, lat: lat, lng: lng });
  });

  fetch('/api/mission/rovers', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ race_id: raceId, mission_name: missionName, rovers: rovers })
  })
  .then(r => r.json())
  .then(res => {
    alert('Lista de ' + res.rover_count + ' bóias guardada no MicroSD com sucesso!');
    loadMissionData();
  })
  .catch(err => alert('Erro ao gravar: ' + err));
}

function clearRovers() {
  if (confirm('Tem a certeza que deseja repor a lista de bóias para os valores padrão?')) {
    fetch('/api/mission/clear', { method: 'POST' })
      .then(r => r.json())
      .then(() => loadMissionData());
  }
}

document.addEventListener('DOMContentLoaded', loadMissionData);
setInterval(loadMissionData, 2000);
</script>
"""

BASE_PORTAL_CONTENT = """
<div class="card">
  <div class="card-title">
    <span><i class="fa-solid fa-cloud"></i> Estado da Ligação ao Portal Cloud</span>
    <span id="portalBadge" class="badge" style="background:#065f46;color:#6ee7b7;font-size:0.85rem;">
      <i class="fa-solid fa-circle-check"></i> A verificar...
    </span>
  </div>
  <p style="color:var(--muted);font-size:0.875rem;margin-bottom:16px;">
    A Base Station efetua polling e telemetria contínua por HTTPS POST com o portal WindDragons (<a href="https://winddragons.app" target="_blank" style="color:var(--primary);text-decoration:none;">https://winddragons.app</a>).
  </p>

  <div class="grid-4">
    <div class="stat-box">
      <div class="stat-label">Estado da Conexão</div>
      <div class="stat-value" id="cloudConnVal" style="color:var(--green);">ONLINE</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Último Código HTTP</div>
      <div class="stat-value" id="cloudHttpVal" style="color:var(--primary);">200 OK</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Sincronizações OK</div>
      <div class="stat-value" id="cloudSuccessVal" style="color:var(--green);">0</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Falhas de Envio</div>
      <div class="stat-value" id="cloudFailVal" style="color:var(--muted);">0</div>
    </div>
  </div>

  <div style="margin-top:16px;" class="grid-2">
    <div class="stat-box">
      <div class="stat-label">Identificador Único (Hardware MAC)</div>
      <div class="stat-value" id="portalMacVal" style="color:var(--primary);font-family:monospace;font-size:1.3rem;">--:--:--:--:--:--</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Nome da Estação Base</div>
      <div class="stat-value" id="portalStationName" style="color:var(--yellow);font-size:1.3rem;">BASE-ALCOCHETE-01</div>
    </div>
  </div>

  <div style="margin-top:16px;" class="stat-box">
    <div class="stat-label">Endpoint de Telemetria & Comandos</div>
    <div class="stat-value" style="font-size:0.95rem;font-family:monospace;color:var(--text);word-break:break-all;">
      https://winddragons.app/api/circuits/station/telemetry
    </div>
  </div>

  <div style="margin-top:16px;display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:8px;">
    <div style="color:var(--muted);font-size:0.8rem;">
      <i class="fa-solid fa-clock"></i> Ciclo de Envio: <strong>3.0 segundos</strong> &bull; <span id="lastSyncAgo">Último envio: a sincronizar...</span>
    </div>
    <button onclick="refreshData()" class="btn btn-secondary" style="padding:6px 12px;font-size:0.8rem;">
      <i class="fa-solid fa-rotate"></i> Atualizar Agora
    </button>
  </div>
</div>

<div class="card">
  <div class="card-title">
    <span><i class="fa-solid fa-ship"></i> Rovers da Missão (Enviados pelo Portal)</span>
    <span id="activeRoversCount" class="badge" style="background:rgba(255,255,255,0.06);color:var(--muted);">0 Rovers do Portal</span>
  </div>
  <p style="color:var(--muted);font-size:0.875rem;margin-bottom:12px;">
    Rovers e bóias atribuídos a esta Estação Base através da plataforma Portal Cloud:
  </p>
  <div style="overflow-x:auto;">
    <table>
      <thead>
        <tr>
          <th>Código</th>
          <th>Nome</th>
          <th>Função (Role)</th>
          <th>Endereço MAC</th>
          <th>Estado</th>
          <th>Coordenadas (GPS)</th>
          <th>Bateria</th>
          <th>Sinal LoRa</th>
        </tr>
      </thead>
      <tbody id="cloudFleetTable">
        <tr>
          <td colspan="8" style="text-align:center;color:var(--muted);padding:32px 16px;">
            <i class="fa-solid fa-inbox" style="font-size:2rem;display:block;margin-bottom:8px;opacity:0.4;"></i>
            <strong style="color:var(--text);font-size:0.95rem;">Nenhum rover enviado pelo Portal ainda.</strong><br>
            <span style="font-size:0.82rem;color:var(--muted);display:inline-block;margin-top:4px;">
              A lista será apresentada automaticamente assim que o portal responder com os rovers atribuídos à missão.
            </span>
          </td>
        </tr>
      </tbody>
    </table>
  </div>
</div>

<script>
let lastSyncTimeMs = 0;

function refreshData() {
  fetch('/api/status')
    .then(r => r.json())
    .then(data => {
      // 1. MAC & Nome
      if (data.station_id || data.mac_address) {
        document.getElementById('portalMacVal').innerText = data.mac_address || data.station_id;
      }
      if (data.station_name) {
        document.getElementById('portalStationName').innerText = data.station_name;
      }

      // 2. Cloud status
      const c = data.cloud;
      const b = document.getElementById('portalBadge');
      const cv = document.getElementById('cloudConnVal');
      const hv = document.getElementById('cloudHttpVal');
      const sv = document.getElementById('cloudSuccessVal');
      const fv = document.getElementById('cloudFailVal');

      if (c) {
        if (c.sync_success) {
          b.className = 'badge';
          b.style.background = '#065f46';
          b.style.color = '#6ee7b7';
          b.innerHTML = '<i class="fa-solid fa-circle-check"></i> SINCRONIZADO (ONLINE)';

          cv.innerText = 'ONLINE';
          cv.style.color = 'var(--green)';

          hv.innerText = (c.last_http_status || 200) + ' OK';
          hv.style.color = 'var(--green)';
        } else if (c.connected) {
          b.className = 'badge';
          b.style.background = '#78350f';
          b.style.color = '#fde68a';
          b.innerHTML = '<i class="fa-solid fa-triangle-exclamation"></i> A CONECTAR...';

          cv.innerText = 'A CONECTAR';
          cv.style.color = 'var(--yellow)';

          hv.innerText = c.last_http_status ? (c.last_http_status + ' ERRO') : 'A tentar';
          hv.style.color = 'var(--red)';
        } else {
          b.className = 'badge';
          b.style.background = '#7f1d1d';
          b.style.color = '#fca5a5';
          b.innerHTML = '<i class="fa-solid fa-circle-xmark"></i> DESCONECTADO';

          cv.innerText = 'OFFLINE';
          cv.style.color = 'var(--red)';

          hv.innerText = '--';
          hv.style.color = 'var(--muted)';
        }

        sv.innerText = c.success_count || 0;
        fv.innerText = c.fail_count || 0;
        fv.style.color = (c.fail_count > 0) ? 'var(--red)' : 'var(--muted)';
        
        lastSyncTimeMs = c.last_sync_ms || 0;
      }

      // 3. Tabela da Frota do Portal
      const portalRoversReceived = !!data.portal_rovers_received;
      const roversList = (portalRoversReceived && data.rovers) ? data.rovers : [];
      const tbl = document.getElementById('cloudFleetTable');
      const badge = document.getElementById('activeRoversCount');

      if (!portalRoversReceived || !roversList.length) {
        if (tbl) {
          tbl.innerHTML = `<tr>
            <td colspan="8" style="text-align:center;color:var(--muted);padding:32px 16px;">
              <i class="fa-solid fa-inbox" style="font-size:2rem;display:block;margin-bottom:8px;opacity:0.4;"></i>
              <strong style="color:var(--text);font-size:0.95rem;">Nenhum rover enviado pelo Portal ainda.</strong><br>
              <span style="font-size:0.82rem;color:var(--muted);display:inline-block;margin-top:4px;">
                A lista será apresentada automaticamente assim que o portal responder com os rovers atribuídos à missão.
              </span>
            </td>
          </tr>`;
        }
        if (badge) {
          badge.innerText = '0 Rovers do Portal';
          badge.style.background = 'rgba(255,255,255,0.06)';
          badge.style.color = 'var(--muted)';
        }
      } else {
        let activeCount = 0;
        let rows = '';
        roversList.forEach(rov => {
          if (rov.is_online) activeCount++;
          rows += `<tr>
            <td><strong>${rov.code || ('ROVER-' + (rov.id < 10 ? '0' : '') + rov.id)}</strong></td>
            <td>${rov.name || '--'}</td>
            <td><span class="badge" style="background:rgba(59,130,246,0.15);color:var(--primary);">${rov.role || '--'}</span></td>
            <td style="font-family:monospace;font-size:0.85rem;color:var(--muted);">${rov.mac || rov.mac_address || '--'}</td>
            <td><span class="badge" style="background:${rov.is_online ? '#065f46' : '#7f1d1d'};color:${rov.is_online ? '#6ee7b7' : '#fca5a5'};">${rov.is_online ? 'ONLINE' : 'OFFLINE'}</span></td>
            <td>${(rov.lat || 0).toFixed(4)}, ${(rov.lng || 0).toFixed(4)}</td>
            <td><strong style="color:${(rov.battery_pct || 0) < 25 ? 'var(--red)' : 'var(--green)'};">${rov.battery_pct || 0}%</strong> (${(rov.battery_voltage || 0).toFixed(1)}V)</td>
            <td>${rov.rssi || 0} dBm</td>
          </tr>`;
        });
        if (tbl) tbl.innerHTML = rows;
        if (badge) {
          badge.innerText = `${activeCount} / ${roversList.length} Rovers Ativos`;
          badge.style.background = 'rgba(16,185,129,0.15)';
          badge.style.color = 'var(--green)';
        }
      }
    })
    .catch(() => {});
}

function updateTimer() {
  if (lastSyncTimeMs > 0) {
    const el = document.getElementById('lastSyncAgo');
    if (el) el.innerText = 'Último sync: OK (em ciclo contínuo de 3s)';
  }
}

setInterval(refreshData, 1500);
setInterval(updateTimer, 1000);
document.addEventListener('DOMContentLoaded', refreshData);
</script>
"""


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
    mission_base = generate_base_html_page("Gestão de Missão & Regata", "mission", BASE_MISSION_CONTENT)
    portal_base = generate_base_html_page("Estado da Ligação ao Portal", "portal", BASE_PORTAL_CONTENT)
    if (BASE_DIR / "gps.html").exists():
        with open(BASE_DIR / "gps.html", "r", encoding="utf-8") as f:
            gps_base = f.read()
    else:
        gps_base = generate_base_html_page("Telemetria GPS da Frota", "gps", BASE_GPS_CONTENT)
    bat_base = generate_base_html_page("Monitor de Bateria da Frota", "battery", BASE_BATTERY_CONTENT)
    ctrl_base = generate_base_html_page("Centro de Comando da Frota", "control", BASE_CONTROL_CONTENT)
    wifi_base = generate_base_html_page("Configuração WiFi da Base", "wifi", BASE_WIFI_CONTENT)
    sd_base = generate_base_html_page("Explorador MicroSD da Base", "sd", BASE_SD_CONTENT)

    base_files = [
        ("index.html", mission_base),
        ("mission.html", mission_base),
        ("portal.html", portal_base),
        ("gps.html", gps_base),
        ("battery.html", bat_base),
        ("control.html", ctrl_base),
        ("wifi.html", wifi_base),
        ("config.html", wifi_base),
        ("sd.html", sd_base)
    ]

    for fname, content in base_files:
        with open(BASE_DIR / fname, "w", encoding="utf-8") as f:
            f.write(content.strip() + "\n")
        # Mantém cópia de compatibilidade em /www/ se for mission.html
        if fname == "mission.html":
            with open(WWW_DIR / fname, "w", encoding="utf-8") as f:
                f.write(content.strip() + "\n")
    print(f"  [+] 8 páginas especializadas geradas em {BASE_DIR}")

    print("\n[OK] Todas as páginas web do Rover e da Base Station foram geradas com sucesso!")

if __name__ == "__main__":
    main()

