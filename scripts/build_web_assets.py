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
STYLE_CSS = """/* WindDragons Rover - Modern Marine UI System (Dual Dark & Light Theme) */
:root {
  color-scheme: dark;
  --bg: #0b1329;
  --bg-subtle: #0f172a;
  --card: #16203c;
  --card-border: #233258;
  --card-hover: #1e2c52;
  --primary: #06b6d4;
  --primary-hover: #0891b2;
  --primary-rgb: 6, 182, 212;
  --text: #f8fafc;
  --text-muted: #94a3b8;
  --muted: #94a3b8;
  --green: #10b981;
  --yellow: #f59e0b;
  --red: #ef4444;

  --navbar-bg: linear-gradient(135deg, #0e1a38, #162348);
  --navbar-border: #233258;
  --navbar-shadow: 0 4px 14px rgba(0, 0, 0, 0.35);
  --nav-links-bg: rgba(0, 0, 0, 0.25);
  --nav-item-color: #94a3b8;
  --nav-item-hover-bg: rgba(255, 255, 255, 0.06);
  --nav-item-hover-color: #f8fafc;
  --nav-item-active-bg: var(--primary);
  --nav-item-active-color: #0f172a;

  --stat-box-bg: rgba(0, 0, 0, 0.2);
  --stat-box-border: rgba(255, 255, 255, 0.05);

  --input-bg: #0f172a;
  --input-border: #233258;
  --input-text: #ffffff;

  --btn-secondary-bg: #334155;
  --btn-secondary-hover: #475569;
  --btn-secondary-color: #ffffff;

  --table-border: #233258;
  --table-hover: rgba(255, 255, 255, 0.03);

  --badge-ap-bg: #78350f;
  --badge-ap-color: #fde68a;
  --badge-ap-border: #b45309;

  --badge-sta-bg: #064e3b;
  --badge-sta-color: #a7f3d0;
  --badge-sta-border: #059669;

  --badge-ver-bg: rgba(6, 182, 212, 0.12);
  --badge-ver-border: rgba(6, 182, 212, 0.3);
  --badge-ver-color: var(--primary);

  --badge-station-bg: rgba(2, 132, 199, 0.2);
  --badge-station-border: rgba(56, 189, 248, 0.3);
  --badge-station-color: #38bdf8;

  --card-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.3);
  --modal-overlay: rgba(0, 0, 0, 0.75);
}

[data-theme="light"] {
  color-scheme: light;
  --bg: #f8fafc;
  --bg-subtle: #f1f5f9;
  --card: #ffffff;
  --card-border: #cbd5e1;
  --card-hover: #f1f5f9;
  --primary: #0284c7;
  --primary-hover: #0369a1;
  --primary-rgb: 2, 132, 199;
  --text: #0f172a;
  --text-muted: #64748b;
  --muted: #64748b;
  --green: #059669;
  --yellow: #d97706;
  --red: #dc2626;

  --navbar-bg: linear-gradient(135deg, #ffffff, #f1f5f9);
  --navbar-border: #cbd5e1;
  --navbar-shadow: 0 2px 10px rgba(0, 0, 0, 0.07);
  --nav-links-bg: #e2e8f0;
  --nav-item-color: #475569;
  --nav-item-hover-bg: rgba(2, 132, 199, 0.1);
  --nav-item-hover-color: #0f172a;
  --nav-item-active-bg: var(--primary);
  --nav-item-active-color: #ffffff;

  --stat-box-bg: #f8fafc;
  --stat-box-border: #e2e8f0;

  --input-bg: #ffffff;
  --input-border: #cbd5e1;
  --input-text: #0f172a;

  --btn-secondary-bg: #e2e8f0;
  --btn-secondary-hover: #cbd5e1;
  --btn-secondary-color: #1e293b;

  --table-border: #e2e8f0;
  --table-hover: rgba(2, 132, 199, 0.05);

  --badge-ap-bg: #fef3c7;
  --badge-ap-color: #92400e;
  --badge-ap-border: #fcd34d;

  --badge-sta-bg: #d1fae5;
  --badge-sta-color: #065f46;
  --badge-sta-border: #a7f3d0;

  --badge-ver-bg: rgba(2, 132, 199, 0.1);
  --badge-ver-border: rgba(2, 132, 199, 0.25);
  --badge-ver-color: var(--primary);

  --badge-station-bg: rgba(2, 132, 199, 0.12);
  --badge-station-border: rgba(2, 132, 199, 0.3);
  --badge-station-color: #0284c7;

  --card-shadow: 0 4px 12px rgba(0, 0, 0, 0.06);
  --modal-overlay: rgba(15, 23, 42, 0.45);
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
  transition: background 0.25s ease, color 0.25s ease;
}

/* Navbar */
.navbar {
  background: var(--navbar-bg);
  border-bottom: 1px solid var(--navbar-border);
  padding: 10px 20px;
  display: flex;
  align-items: center;
  justify-content: space-between;
  flex-wrap: wrap;
  gap: 12px;
  position: sticky;
  top: 0;
  z-index: 100;
  box-shadow: var(--navbar-shadow);
  transition: background 0.25s ease, border-color 0.25s ease;
}

/* Brand & Logotipo Oficial */
.brand {
  font-size: 1.25rem;
  font-weight: 700;
  color: var(--text);
  display: flex;
  align-items: center;
  gap: 10px;
  text-decoration: none;
  white-space: nowrap;
  user-select: none;
}

.brand-logo {
  height: 36px;
  width: auto;
  max-width: 40px;
  object-fit: contain;
  vertical-align: middle;
  filter: drop-shadow(0 2px 4px rgba(0,0,0,0.25));
  transition: transform 0.25s ease;
}

.brand:hover .brand-logo {
  transform: scale(1.08) rotate(-2deg);
}

.brand span {
  color: var(--text);
}

.brand span.brand-sub, .brand #navRoverId {
  color: var(--primary);
  font-weight: 800;
}

/* Visibilidade do Logotipo Consoante o Tema */
:root:not([data-theme="light"]) .brand-logo.logo-light {
  display: none !important;
}
:root:not([data-theme="light"]) .brand-logo.logo-dark {
  display: inline-block !important;
}

[data-theme="light"] .brand-logo.logo-light {
  display: inline-block !important;
}
[data-theme="light"] .brand-logo.logo-dark {
  display: none !important;
}

/* Contentor de Ações e Badges da Navbar */
.nav-actions {
  display: flex;
  align-items: center;
  gap: 8px;
  flex-wrap: wrap;
}

/* Botão Alternador de Tema (Light / Dark) */
.theme-toggle {
  background: var(--stat-box-bg);
  border: 1px solid var(--card-border);
  color: var(--text);
  width: 36px;
  height: 36px;
  border-radius: 999px;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  cursor: pointer;
  transition: all 0.25s ease;
  position: relative;
  overflow: hidden;
  padding: 0;
  box-shadow: 0 2px 6px rgba(0,0,0,0.15);
  flex-shrink: 0;
}

.theme-toggle:hover {
  border-color: var(--primary);
  color: var(--primary);
  transform: scale(1.08);
}

.theme-toggle .icon-sun,
.theme-toggle .icon-moon {
  font-size: 1.05rem;
  transition: transform 0.3s cubic-bezier(0.4, 0, 0.2, 1), opacity 0.3s ease;
  position: absolute;
}

/* No tema escuro: mostra o sol âmbar brilhante (clique para alternar para tema claro) */
:root:not([data-theme="light"]) .theme-toggle .icon-sun {
  opacity: 1;
  transform: scale(1) rotate(0deg);
  color: #fbbf24;
}
:root:not([data-theme="light"]) .theme-toggle .icon-moon {
  opacity: 0;
  transform: scale(0.4) rotate(-90deg);
}

/* No tema claro: mostra a lua azul celeste (clique para alternar para tema escuro) */
[data-theme="light"] .theme-toggle .icon-sun {
  opacity: 0;
  transform: scale(0.4) rotate(90deg);
}
[data-theme="light"] .theme-toggle .icon-moon {
  opacity: 1;
  transform: scale(1) rotate(0deg);
  color: #0284c7;
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
  transition: background 0.25s ease, border-color 0.25s ease;
}

.badge-ap {
  background: var(--badge-ap-bg);
  color: var(--badge-ap-color);
  border: 1px solid var(--badge-ap-border);
}

.badge-sta {
  background: var(--badge-sta-bg);
  color: var(--badge-sta-color);
  border: 1px solid var(--badge-sta-border);
}

.badge-ver {
  background: var(--badge-ver-bg) !important;
  border: 1px solid var(--badge-ver-border) !important;
  color: var(--badge-ver-color) !important;
  text-decoration: none;
}

.badge-station {
  background: var(--badge-station-bg) !important;
  border: 1px solid var(--badge-station-border) !important;
  color: var(--badge-station-color) !important;
  font-family: monospace;
}

.nav-links {
  display: flex;
  gap: 6px;
  background: var(--nav-links-bg);
  padding: 4px;
  border-radius: 10px;
  border: 1px solid var(--card-border);
  overflow-x: auto;
  transition: background 0.25s ease, border-color 0.25s ease;
}

.nav-item {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  padding: 8px 14px;
  border-radius: 8px;
  color: var(--nav-item-color);
  text-decoration: none;
  font-size: 0.875rem;
  font-weight: 600;
  white-space: nowrap;
  transition: all 0.2s ease;
}

.nav-item:hover {
  color: var(--nav-item-hover-color);
  background: var(--nav-item-hover-bg);
}

.nav-item.active {
  background: var(--nav-item-active-bg);
  color: var(--nav-item-active-color);
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
  box-shadow: var(--card-shadow);
  transition: background 0.25s ease, border-color 0.25s ease, box-shadow 0.25s ease;
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
  background: var(--stat-box-bg);
  padding: 14px;
  border-radius: 10px;
  border: 1px solid var(--stat-box-border);
  transition: background 0.25s ease, border-color 0.25s ease;
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
  color: #ffffff;
}

:root:not([data-theme="light"]) .btn-primary {
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
  background: var(--btn-secondary-bg);
  color: var(--btn-secondary-color);
}

.btn-secondary:hover {
  background: var(--btn-secondary-hover);
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
  border-bottom: 1px solid var(--table-border);
  font-size: 0.875rem;
}

th {
  color: var(--muted);
  font-weight: 600;
}

tr:hover td {
  background: var(--table-hover);
}

/* Inputs & Form Controls */
input, select, textarea {
  width: 100%;
  padding: 10px 12px;
  background: var(--input-bg);
  border: 1px solid var(--input-border);
  color: var(--input-text);
  border-radius: 8px;
  margin-top: 6px;
  font-size: 0.9rem;
  outline: none;
  transition: background 0.25s ease, border-color 0.2s ease, color 0.25s ease;
input:focus, select:focus, textarea:focus {
  border-color: var(--primary);
}

.form-group {
  margin-bottom: 14px;
}

.progress-bg {
  width: 100%;
  height: 16px;
  background: var(--bg-subtle);
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
  background: var(--bg-subtle);
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
  background: var(--stat-box-bg);
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
  background: var(--btn-secondary-bg);
  border: none;
  color: var(--btn-secondary-color);
  border-radius: 4px;
  padding: 4px 6px;
  font-size: 0.75rem;
  cursor: pointer;
  transition: all 0.2s;
}

.explorer-card-actions button:hover {
  background: var(--primary);
  color: #fff;
}

.breadcrumb-bar {
  display: flex;
  align-items: center;
  gap: 6px;
  background: var(--nav-links-bg);
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
  background: var(--modal-overlay);
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

/* Compatibilidade do Tema Claro com blocos inline */
[data-theme="light"] div[style*="background:rgba(15,23,42"],
[data-theme="light"] div[style*="background: rgba(15, 23, 42"],
[data-theme="light"] div[style*="background:rgba(15, 23, 42"],
[data-theme="light"] div[style*="background: #090e1a"],
[data-theme="light"] div[style*="background:#090e1a"] {
  background: var(--stat-box-bg) !important;
  border-color: var(--card-border) !important;
}

[data-theme="light"] input[style*="background:rgba(15,23,42"],
[data-theme="light"] input[style*="background: #0f172a"],
[data-theme="light"] input[style*="background:#0f172a"],
[data-theme="light"] input[style*="background:#090e1a"],
[data-theme="light"] select[style*="background:#090e1a"],
[data-theme="light"] select[style*="background: #090e1a"] {
  background: var(--input-bg) !important;
  border-color: var(--input-border) !important;
  color: var(--input-text) !important;
}

[data-theme="light"] .rate-btn {
  background: #f1f5f9 !important;
  border-color: #cbd5e1 !important;
  color: #475569 !important;
}
[data-theme="light"] .rate-btn.active {
  background: rgba(2, 132, 199, 0.12) !important;
  border-color: var(--primary) !important;
  color: var(--primary) !important;
}
[data-theme="light"] .rate-btn .sub {
  color: #64748b !important;
}
[data-theme="light"] .rate-btn.active .sub {
  color: var(--primary) !important;
}

/* Font Awesome 6 Pro Solid (carregada do Cartão SD) */
@font-face {
  font-family: 'Font Awesome 6 Pro';
  src: url('/system/fonts/Font%20Awesome%206%20Pro-Solid-900.woff2') format('woff2'),
       url('/system/fonts/Font%20Awesome%206%20Pro-Solid-900.otf') format('opentype');
  font-weight: 900;
  font-style: normal;
  font-display: swap;
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
.fa-terminal:before { content: '\\f120'; }
.fa-arrows-down-to-line:before { content: '\\e4b8'; }

/* Toggle Switch Component */
.switch {
  position: relative;
  display: inline-block;
  width: 44px;
  height: 24px;
  vertical-align: middle;
}
.switch input {
  opacity: 0;
  width: 0;
  height: 0;
}
.slider-toggle {
  position: absolute;
  cursor: pointer;
  top: 0; left: 0; right: 0; bottom: 0;
  background-color: #334155;
  transition: .3s;
  border-radius: 24px;
}
.slider-toggle:before {
  position: absolute;
  content: "";
  height: 18px;
  width: 18px;
  left: 3px;
  bottom: 3px;
  background-color: white;
  transition: .3s;
  border-radius: 50%;
}
input:checked + .slider-toggle {
  background-color: var(--primary);
}
input:checked + .slider-toggle:before {
  transform: translateX(20px);
}

/* Mobile Responsiveness Helpers */
@media (max-width: 640px) {
  .badge-hide-mobile {
    display: none !important;
  }
  .navbar {
    padding: 8px 10px;
    gap: 6px;
  }
  .nav-links {
    max-width: 100%;
    overflow-x: auto;
    -webkit-overflow-scrolling: touch;
  }
}
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

// --- GESTÃO DE TEMA CLARO / ESCURO (LIGHT / DARK) ---
function initTheme() {
  const saved = localStorage.getItem('wd_theme');
  const prefersDark = window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches;
  const theme = saved ? saved : (prefersDark ? 'dark' : 'dark');
  setTheme(theme, false);
}

function setTheme(theme, save = true) {
  document.documentElement.setAttribute('data-theme', theme);
  if (save) {
    try { localStorage.setItem('wd_theme', theme); } catch(e) {}
  }
  const btn = document.getElementById('themeToggleBtn');
  if (btn) {
    btn.setAttribute('aria-pressed', theme === 'dark' ? 'true' : 'false');
    btn.title = theme === 'dark' ? 'Mudar para tema claro' : 'Mudar para tema escuro';
  }
}

function toggleTheme() {
  const current = document.documentElement.getAttribute('data-theme') || 'dark';
  const next = current === 'dark' ? 'light' : 'dark';
  setTheme(next, true);
  if (typeof showToast === 'function') {
    showToast(next === 'dark' ? 'Tema escuro ativado' : 'Tema claro ativado');
  }
}

if (typeof document !== 'undefined') {
  initTheme();
}

document.addEventListener('DOMContentLoaded', () => {
  initTheme();
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
  <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.5.1/css/all.min.css">
  <link rel="stylesheet" href="/style.css?v=2.1.3">
  <script>
    (function(){{
      var t = localStorage.getItem('wd_theme') || 'dark';
      document.documentElement.setAttribute('data-theme', t);
    }})();
  </script>
  <script src="/app.js?v=2.1.1"></script>
</head>
<body>
  <header class="navbar">
    <a href="/" class="brand">
      <img src="/assets/logo_black_800.png" alt="WindDragons Logo" class="brand-logo logo-light" style="height:36px;width:auto;max-width:40px;object-fit:contain;display:none;">
      <img src="/assets/logo_white_800.png" alt="WindDragons Logo" class="brand-logo logo-dark" style="height:36px;width:auto;max-width:40px;object-fit:contain;display:inline-block;">
      <span>WindDragons</span> <span id="navRoverId">Rover</span>
    </a>
    <nav class="nav-links">
      {nav_links}
    </nav>
    <div class="nav-actions">
      <a href="/config" id="navVersionBadge" class="badge" style="background:rgba(6,182,212,0.12);border:1px solid rgba(6,182,212,0.3);color:var(--primary);text-decoration:none;font-size:0.75rem;display:inline-flex;align-items:center;gap:6px;" title="Ver detalhes de firmware nas Configurações">
        <i class="fa-solid fa-code-branch"></i> <span id="navLafvinVer">Lafvin: v2.1.0</span> &bull; <span id="navC6Ver">C6: ...</span>
      </a>
      <span id="navWifiBadge" class="badge badge-sta">
        <i class="fa-solid fa-wifi"></i> A carregar...
      </span>
      <button type="button" class="theme-toggle" id="themeToggleBtn" onclick="toggleTheme()" title="Alternar tema claro/escuro" aria-label="Alternar tema claro/escuro">
        <i class="fa-solid fa-sun icon-sun"></i>
        <i class="fa-solid fa-moon icon-moon"></i>
      </button>
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

def generate_base_html_page(title, active_tab, body_content, extra_head=""):
    tabs = [
        ("mission", "/mission", "fa-flag-checkered", "Missão"),
        ("portal", "/portal", "fa-cloud", "Portal Cloud"),
        ("config", "/config", "fa-gear", "Configuração"),
        ("wifi", "/wifi", "fa-wifi", "WiFi Base"),
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
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no, viewport-fit=cover">
  <title>{title} - WindDragons Base Station</title>
  <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.5.1/css/all.min.css">
  <link rel="stylesheet" href="/style.css?v=2.1.3">
  {extra_head}
  <script>
    (function(){{
      var t = localStorage.getItem('wd_theme') || 'dark';
      document.documentElement.setAttribute('data-theme', t);
    }})();
  </script>
  <script src="/app.js?v=2.1.2"></script>
</head>
<body>
  <header class="navbar">
    <a href="/" class="brand">
      <img src="/assets/logo_black_800.png" alt="WindDragons Logo" class="brand-logo logo-light" style="height:32px;width:auto;max-width:36px;object-fit:contain;display:none;">
      <img src="/assets/logo_white_800.png" alt="WindDragons Logo" class="brand-logo logo-dark" style="height:32px;width:auto;max-width:36px;object-fit:contain;display:inline-block;">
      <span>WindDragons</span> <span class="brand-sub">Base Station</span>
    </a>
    <nav class="nav-links">
      {nav_links}
    </nav>
    <div class="nav-actions">
      <a href="/config" id="navVersionBadge" class="badge badge-hide-mobile" style="background:rgba(6,182,212,0.12);border:1px solid rgba(6,182,212,0.3);color:var(--primary);text-decoration:none;font-size:0.75rem;display:inline-flex;align-items:center;gap:6px;" title="Ver versões das placas e atualização OTA na página de Configuração">
        <i class="fa-solid fa-code-branch"></i> <span id="navLafvinVer">Lafvin: v2.1.0</span> &bull; <span id="navC6Ver">C6: ...</span>
      </a>
      <span id="navOpStateBadge" class="badge" style="background:#64748b;color:#fff;font-weight:700;">
        <i class="fa-solid fa-flag"></i> <span id="navOpStateText">STORAGE</span>
      </span>
      <span id="navStationBadge" class="badge badge-hide-mobile" style="background:rgba(2,132,199,0.2);color:#38bdf8;border:1px solid rgba(56,189,248,0.3);font-family:monospace;">
        <i class="fa-solid fa-fingerprint"></i> <span id="navStationId">ID Base</span>
      </span>
      <span id="navWifiBadge" class="badge badge-sta">
        <i class="fa-solid fa-wifi"></i> A carregar...
      </span>
      <button type="button" class="theme-toggle" id="themeToggleBtn" onclick="toggleTheme()" title="Alternar tema claro/escuro" aria-label="Alternar tema claro/escuro">
        <i class="fa-solid fa-sun icon-sun"></i>
        <i class="fa-solid fa-moon icon-moon"></i>
      </button>
    </div>
  </header>

  <main class="container mission-container">
    {body_content}
  </main>

  <footer style="text-align:center;color:var(--muted);font-size:0.75rem;margin-top:24px;display:flex;justify-content:center;align-items:center;gap:12px;flex-wrap:wrap;padding:0 16px;">
    <span>WindDragons Telemetry Base Station &copy; 2026</span>
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
    const createdDirs = new Set(['/', '/www']);
    for (let i = 0; i < webFiles.length; i++) {
      const wf = webFiles[i];
      const parts = wf.path.split('/');
      const fileName = parts.pop();
      const dirPath = '/' + parts.join('/');
      
      // Assegura recursivamente a existência da pasta de destino no SD
      if (dirPath && dirPath !== '/' && !createdDirs.has(dirPath)) {
        const segs = dirPath.split('/').filter(Boolean);
        let curr = '';
        for (const s of segs) {
          curr += '/' + s;
          if (!createdDirs.has(curr)) {
            try {
              await fetch('/api/sd/mkdir', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ dir: curr, path: curr })
              });
              createdDirs.add(curr);
            } catch(e) {}
          }
        }
      }
      
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
<style>
.modal-overlay { display: none; position: fixed; inset: 0; background: rgba(0,0,0,0.75); backdrop-filter: blur(4px); z-index: 200; align-items: center; justify-content: center; padding: 16px; }
.modal-card { background: var(--card, #0f172a); border: 1px solid var(--card-border, #1e293b); border-radius: 14px; max-width: 600px; width: 100%; padding: 24px; box-shadow: 0 20px 25px -5px rgba(0,0,0,0.5); }
.explorer-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(130px, 1fr)); gap: 12px; margin-top: 16px; }
.explorer-card { background: var(--stat-box-bg, rgba(15,23,42,0.6)); border: 1px solid var(--card-border, #1e293b); border-radius: 10px; padding: 14px 10px; display: flex; flex-direction: column; align-items: center; text-align: center; cursor: pointer; transition: all 0.2s; position: relative; }
.explorer-card:hover { background: rgba(6, 182, 212, 0.08); border-color: var(--primary, #06b6d4); transform: translateY(-2px); }
.explorer-card-icon { font-size: 2.2rem; margin-bottom: 8px; }
.explorer-card-name { font-size: 0.85rem; font-weight: 600; word-break: break-all; max-width: 100%; display: -webkit-box; -webkit-line-clamp: 2; -webkit-box-orient: vertical; overflow: hidden; }
.explorer-card-size { font-size: 0.72rem; color: var(--muted, #64748b); margin-top: 4px; }
.explorer-card-actions { display: flex; gap: 4px; margin-top: 8px; }
.explorer-card-actions button { background: var(--btn-secondary-bg, #1e293b); border: none; color: var(--btn-secondary-color, #cbd5e1); border-radius: 4px; padding: 4px 6px; font-size: 0.75rem; cursor: pointer; transition: all 0.2s; }
.explorer-card-actions button:hover { background: var(--primary, #06b6d4); color: #fff; }
.breadcrumb-bar { display: flex; align-items: center; gap: 6px; background: var(--nav-links-bg, #0f172a); padding: 8px 12px; border-radius: 8px; border: 1px solid var(--card-border, #1e293b); overflow-x: auto; white-space: nowrap; }
.breadcrumb-item { color: var(--muted, #64748b); cursor: pointer; font-size: 0.85rem; display: inline-flex; align-items: center; gap: 4px; }
.breadcrumb-item:hover { color: var(--primary, #06b6d4); }
.breadcrumb-sep { color: var(--card-border, #1e293b); }
</style>
<div class="card" style="margin-bottom:16px;">
  <div class="card-title" style="margin-bottom:12px;">
    <span><i class="fa-solid fa-sd-card"></i> Cartão MicroSD TF (SPI Bus)</span>
    <span id="sdBadge" class="badge badge-sta"><i class="fa-solid fa-circle-check"></i> Montado</span>
  </div>
  <div style="display:flex;justify-content:space-between;align-items:baseline;flex-wrap:wrap;gap:8px;font-size:0.875rem;margin-bottom:8px;">
    <span style="color:var(--muted);">
      Espaço Utilizado: <strong id="usedMb" style="color:var(--text);font-size:0.95rem;">-- MB</strong> de <span id="totalMb">-- MB</span> (<span id="usedPct">0%</span>)
    </span>
    <span style="color:var(--muted);">
      Livre: <strong id="freeMb" style="color:var(--green);font-size:0.95rem;">-- MB</strong>
    </span>
  </div>
  <div class="progress-bg" style="height:10px;margin-top:0;">
    <div id="sdProgressBar" class="progress-bar" style="width:0%;background:var(--primary);"></div>
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
if (typeof formatBytes === 'undefined') {
  window.formatBytes = function(bytes) {
    if (!bytes || bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
  };
}
if (typeof showToast === 'undefined') {
  window.showToast = function(msg, isError = false) {
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
  };
}

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
      const total = data.total_mb || 0;
      const used = data.used_mb || 0;
      const free = data.free_mb !== undefined ? data.free_mb : (total > used ? total - used : 0);
      const pct = total > 0 ? Math.min(100, Math.round((used / total) * 100)) : 0;
      if (document.getElementById('totalMb')) document.getElementById('totalMb').innerText = total + ' MB';
      if (document.getElementById('usedMb')) document.getElementById('usedMb').innerText = used + ' MB';
      if (document.getElementById('freeMb')) document.getElementById('freeMb').innerText = free + ' MB';
      if (document.getElementById('usedPct')) document.getElementById('usedPct').innerText = pct + '%';
      const bar = document.getElementById('sdProgressBar');
      if (bar) {
        bar.style.width = pct + '%';
        bar.style.background = pct > 90 ? 'var(--red)' : (pct > 75 ? 'var(--yellow)' : 'var(--primary)');
      }
      if (data.ready !== undefined) {
        const badge = document.getElementById('sdBadge');
        if (badge) {
          badge.className = 'badge ' + (data.ready ? 'badge-sta' : 'badge-ap');
          badge.innerHTML = data.ready ? '<i class="fa-solid fa-circle-check"></i> Montado' : '<i class="fa-solid fa-circle-exclamation"></i> Desconectado';
        }
      }
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

CONSOLE_CARD_TEMPLATE = """
<!-- Consola de Sistema & Monitor Série em Tempo Real -->
<div class="card" id="consoleCard" style="margin-top:24px;">
  <div class="card-title" style="display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:10px;">
    <span><i class="fa-solid fa-terminal" style="color:var(--primary);"></i> __CONSOLE_TITLE__</span>
    <div style="display:flex;align-items:center;gap:12px;">
      <span id="consoleStatusBadge" class="badge" style="background:#334155;color:#94a3b8;font-size:0.75rem;padding:4px 10px;">PAUSADO</span>
      <label class="switch" title="Ativar/Desativar streaming em tempo real">
        <input type="checkbox" id="consoleToggle" onchange="toggleConsoleStreaming(this.checked)">
        <span class="slider-toggle"></span>
      </label>
    </div>
  </div>

  <p style="color:var(--muted);font-size:0.875rem;margin-bottom:14px;">
    Monitorização em tempo real das mensagens de sistema __CONSOLE_DEVICE__, tráfego UART inter-ESP com o Coprocessador C6 e rádio LoRa.
    Ative o interruptor para ligar o streaming de logs.
  </p>

  <div style="display:flex;gap:8px;margin-bottom:10px;flex-wrap:wrap;align-items:center;">
    <div style="flex:1;min-width:200px;">
      <input type="text" id="consoleFilter" placeholder="Filtrar por texto (ex: C6, TELEM, LORA, HTTP)..." oninput="applyConsoleFilter()" style="width:100%;padding:6px 12px;font-size:0.82rem;background:#090d16;border:1px solid rgba(255,255,255,0.1);border-radius:6px;color:#f1f5f9;">
    </div>
    <button type="button" onclick="clearConsoleLog()" class="btn btn-secondary" style="padding:6px 12px;font-size:0.8rem;">
      <i class="fa-solid fa-trash-can"></i> Limpar
    </button>
    <button type="button" id="consoleScrollBtn" onclick="toggleConsoleScroll()" class="btn btn-secondary" style="padding:6px 12px;font-size:0.8rem;background:rgba(6,182,212,0.15);color:var(--primary);border-color:rgba(6,182,212,0.3);">
      <i class="fa-solid fa-arrows-down-to-line"></i> Auto-Scroll: ON
    </button>
    <button type="button" onclick="copyConsoleLog()" class="btn btn-secondary" style="padding:6px 12px;font-size:0.8rem;">
      <i class="fa-solid fa-copy"></i> Copiar
    </button>
  </div>

  <div id="consoleTerminalWindow" style="height:320px;background:#070a12;border:1px solid rgba(255,255,255,0.08);border-radius:8px;padding:12px;overflow-y:auto;font-family:ui-monospace,SFMono-Regular,Menlo,Monaco,Consolas,'Liberation Mono','Courier New',monospace;font-size:0.82rem;line-height:1.5;color:#e2e8f0;white-space:pre-wrap;word-break:break-all;">
    <div id="consolePlaceholder" style="color:#64748b;font-style:italic;">Consola inativa. Ligue o interruptor no canto superior direito para receber logs em direto...</div>
  </div>

  <div style="display:flex;gap:8px;margin-top:10px;">
    <input type="text" id="consoleCmdInput" placeholder="Comando UART para o C6 (ex: __CONSOLE_CMD__, CMD:GET_VER, CMD:STATUS, CLEAR)..." onkeydown="if(event.key==='Enter'){event.preventDefault();sendConsoleCmd();}" style="flex:1;padding:8px 12px;background:#090d16;border:1px solid rgba(255,255,255,0.12);border-radius:6px;color:#38bdf8;font-family:monospace;font-size:0.85rem;">
    <button type="button" onclick="sendConsoleCmd()" class="btn btn-primary" style="padding:8px 18px;font-size:0.85rem;">
      <i class="fa-solid fa-paper-plane"></i> Enviar
    </button>
  </div>
</div>

<script>
let consoleStreaming = false;
let consoleTimer = null;
let consoleLastId = 0;
let consoleLines = [];
let consoleAutoScroll = true;

function toggleConsoleStreaming(active) {
  consoleStreaming = active;
  const badge = document.getElementById('consoleStatusBadge');
  const placeholder = document.getElementById('consolePlaceholder');

  if (active) {
    if (badge) {
      badge.textContent = 'ONLINE (1s)';
      badge.style.background = '#065f46';
      badge.style.color = '#34d399';
    }
    if (placeholder) placeholder.style.display = 'none';
    fetchConsoleLogs();
    if (!consoleTimer) {
      consoleTimer = setInterval(fetchConsoleLogs, 1000);
    }
  } else {
    if (badge) {
      badge.textContent = 'PAUSADO';
      badge.style.background = '#334155';
      badge.style.color = '#94a3b8';
    }
    if (consoleTimer) {
      clearInterval(consoleTimer);
      consoleTimer = null;
    }
  }
}

function fetchConsoleLogs() {
  fetch('/api/console?since=' + consoleLastId)
    .then(r => r.json())
    .then(data => {
      if (!data) return;
      if (data.last_id !== undefined) {
        consoleLastId = data.last_id;
      }
      if (data.lines && data.lines.length > 0) {
        data.lines.forEach(l => {
          consoleLines.push(l);
        });
        if (consoleLines.length > 500) {
          consoleLines = consoleLines.slice(consoleLines.length - 500);
        }
        renderConsoleTerminal();
      }
    })
    .catch(err => {
      console.warn('Erro consola:', err);
    });
}

function renderConsoleTerminal() {
  const term = document.getElementById('consoleTerminalWindow');
  if (!term) return;
  const filter = (document.getElementById('consoleFilter')?.value || '').toLowerCase().trim();

  let filtered = consoleLines;
  if (filter) {
    filtered = consoleLines.filter(l => (l.msg || '').toLowerCase().includes(filter));
  }

  if (filtered.length === 0) {
    term.innerHTML = '<div style="color:#64748b;font-style:italic;">' + (filter ? 'Nenhum registo corresponde ao filtro.' : 'A aguardar mensagens...') + '</div>';
    return;
  }

  let html = '';
  filtered.forEach(l => {
    let msg = escapeConsoleHtml(l.msg || '');
    let color = '#e2e8f0';
    if (msg.includes('<<<') || msg.includes('TELEM:')) color = '#34d399';
    else if (msg.includes('>>>') || msg.includes('TX')) color = '#38bdf8';
    else if (msg.includes('ERRO') || msg.includes('ERROR') || msg.includes('FAIL')) color = '#f87171';
    else if (msg.includes('★') || msg.includes('PONG:VER')) color = '#fbbf24';
    else if (msg.includes('[C6]')) color = '#c084fc';
    else if (msg.includes('[HTTP]') || msg.includes('[WIFI]')) color = '#67e8f9';

    const sec = (l.ms / 1000).toFixed(2);
    html += '<div style="margin-bottom:2px;color:' + color + ';">' +
      '<span style="color:#64748b;user-select:none;margin-right:8px;">[' + sec + 's]</span>' +
      msg +
      '</div>';
  });

  term.innerHTML = html;
  if (consoleAutoScroll) {
    term.scrollTop = term.scrollHeight;
  }
}

function applyConsoleFilter() {
  renderConsoleTerminal();
}

function toggleConsoleScroll() {
  consoleAutoScroll = !consoleAutoScroll;
  const btn = document.getElementById('consoleScrollBtn');
  if (btn) {
    if (consoleAutoScroll) {
      btn.innerHTML = '<i class="fa-solid fa-arrows-down-to-line"></i> Auto-Scroll: ON';
      btn.style.background = 'rgba(6,182,212,0.15)';
      btn.style.color = 'var(--primary)';
      btn.style.borderColor = 'rgba(6,182,212,0.3)';
      const term = document.getElementById('consoleTerminalWindow');
      if (term) term.scrollTop = term.scrollHeight;
    } else {
      btn.innerHTML = '<i class="fa-solid fa-arrows-down-to-line"></i> Auto-Scroll: OFF';
      btn.style.background = 'var(--btn-secondary-bg)';
      btn.style.color = 'var(--muted)';
      btn.style.borderColor = 'transparent';
    }
  }
}

function clearConsoleLog() {
  consoleLines = [];
  fetch('/api/console/clear', { method: 'POST' }).catch(() => {});
  const term = document.getElementById('consoleTerminalWindow');
  if (term) {
    term.innerHTML = '<div style="color:#64748b;font-style:italic;">Consola limpa.</div>';
  }
}

function copyConsoleLog() {
  const text = consoleLines.map(l => '[' + (l.ms/1000).toFixed(2) + 's] ' + (l.msg || '')).join('\\n');
  if (navigator.clipboard && window.isSecureContext) {
    navigator.clipboard.writeText(text).then(() => showToast('Logs copiados para a área de transferência!'));
  } else {
    const ta = document.createElement('textarea');
    ta.value = text;
    document.body.appendChild(ta);
    ta.select();
    document.execCommand('copy');
    document.body.removeChild(ta);
    showToast('Logs copiados!');
  }
}

function sendConsoleCmd() {
  const input = document.getElementById('consoleCmdInput');
  if (!input) return;
  const cmd = input.value.trim();
  if (!cmd) return;

  input.value = '';
  consoleLines.push({ id: 0, ms: Date.now() % 1000000, msg: '>>> [WEB TX] ' + cmd });
  renderConsoleTerminal();

  fetch('/api/console/send', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: 'cmd=' + encodeURIComponent(cmd)
  })
  .then(r => r.json())
  .then(res => {
    if (res.status === 'ok') {
      showToast('Comando enviado: ' + cmd);
      if (!consoleStreaming) {
        setTimeout(fetchConsoleLogs, 300);
      }
    } else {
      showToast('Erro ao enviar: ' + (res.msg || 'Falha'), true);
    }
  })
  .catch(err => {
    showToast('Erro de rede ao enviar comando', true);
  });
}

function escapeConsoleHtml(s) {
  return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}
</script>
"""

BASE_CONSOLE_CARD = CONSOLE_CARD_TEMPLATE.replace(
    "__CONSOLE_TITLE__", "Consola de Sistema & Monitor Série (Estação Base)"
).replace(
    "__CONSOLE_DEVICE__", "da Estação Base"
).replace(
    "__CONSOLE_CMD__", "PING"
)

ROVER_CONSOLE_CARD = CONSOLE_CARD_TEMPLATE.replace(
    "__CONSOLE_TITLE__", "Consola de Sistema & Monitor Série (Rover)"
).replace(
    "__CONSOLE_DEVICE__", "do Rover"
).replace(
    "__CONSOLE_CMD__", "CMD:JOY:0:0"
)

BASE_LORA_CARD = """
<div class="card" id="baseLoraCard">
  <div class="card-title">
    <span><i class="fa-solid fa-tower-broadcast"></i> Diagnóstico e Comunicações LoRa (433 MHz)</span>
    <div style="display:flex;gap:8px;align-items:center;flex-wrap:wrap;">
      <span id="baseLoraHwBadge" class="badge" style="background:#064e3b;color:#a7f3d0;border:1px solid #059669;">
        <i class="fa-solid fa-circle-check"></i> SX1278 ONLINE
      </span>
      <span id="baseLoraPollBadge" class="badge" style="background:#1e3a8a;color:#93c5fd;border:1px solid #3b82f6;">
        <i class="fa-solid fa-arrows-rotate fa-spin"></i> A FAZER POLLING (1 Hz)
      </span>
    </div>
  </div>
  <p style="color:var(--muted);font-size:0.875rem;margin-bottom:16px;">
    Monitorização em tempo real do rádio SX1278 no Coprocessador C6 via barramento FSPI e ciclo de polling contínuo aos rovers no canal 433.0 MHz.
  </p>

  <div class="grid-4" style="margin-bottom:16px;">
    <div class="stat-box">
      <div class="stat-label">Polls Enviados (TX)</div>
      <div class="stat-value" id="baseLoraTxPolls" style="color:var(--primary);">0</div>
      <div style="font-size:0.72rem;color:var(--muted);margin-top:2px;">Transmissões de pedido</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Telemetrias Recebidas (RX)</div>
      <div class="stat-value" id="baseLoraRxTelems" style="color:var(--green);">0</div>
      <div style="font-size:0.72rem;color:var(--muted);margin-top:2px;">Respostas dos Rovers</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Última Resposta</div>
      <div class="stat-value" id="baseLoraLastRxAgo" style="color:var(--yellow);font-size:1.15rem;">Nunca</div>
      <div style="font-size:0.72rem;color:var(--muted);margin-top:2px;" id="baseLoraLastRxSub">A aguardar sinal do Rover...</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Sinal LoRa (RSSI / SNR)</div>
      <div class="stat-value" id="baseLoraSignal" style="font-size:1.15rem;">-- dBm / -- dB</div>
      <div style="font-size:0.72rem;color:var(--muted);margin-top:2px;">Último pacote recebido</div>
    </div>
  </div>

  <div style="background:rgba(0,0,0,0.2);padding:14px 18px;border-radius:10px;border:1px solid var(--card-border);display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:12px;">
    <div style="font-size:0.8rem;color:var(--muted);">
      <strong style="color:var(--text);"><i class="fa-solid fa-sliders"></i> Configuração RF:</strong> 
      433.0 MHz &bull; BW 250 kHz &bull; SF7 &bull; CR 4/5 &bull; Sync 0x12 &bull; Potência 17 dBm
    </div>
    <div style="display:flex;align-items:center;gap:10px;">
      <button type="button" onclick="triggerBaseLoraPoll()" class="btn btn-primary" id="btnForcePoll">
        <i class="fa-solid fa-paper-plane"></i> Forçar Poll / Ping Imediato
      </button>
      <span id="basePollFeedback" style="font-size:0.8rem;color:var(--green);"></span>
    </div>
  </div>
</div>

<script>
function refreshBaseLoraStatus() {
  fetch('/api/lora/status')
    .then(r => r.json())
    .then(d => {
      const elHw = document.getElementById('baseLoraHwBadge');
      if (elHw) {
        if (d.hw_ready) {
          elHw.innerHTML = '<i class=\"fa-solid fa-circle-check\"></i> SX1278 ONLINE';
          elHw.style.background = '#064e3b';
          elHw.style.color = '#a7f3d0';
          elHw.style.borderColor = '#059669';
        } else {
          elHw.innerHTML = '<i class=\"fa-solid fa-triangle-exclamation\"></i> ERRO RF / DESCONECTADO';
          elHw.style.background = '#7f1d1d';
          elHw.style.color = '#fecaca';
          elHw.style.borderColor = '#dc2626';
        }
      }

      const elPoll = document.getElementById('baseLoraPollBadge');
      if (elPoll) {
        if (d.is_polling) {
          elPoll.innerHTML = '<i class=\"fa-solid fa-arrows-rotate fa-spin\"></i> A FAZER POLLING (1 Hz)';
          elPoll.style.background = '#1e3a8a';
          elPoll.style.color = '#93c5fd';
          elPoll.style.borderColor = '#3b82f6';
        } else {
          elPoll.innerHTML = '<i class=\"fa-solid fa-pause\"></i> POLLING PARADO';
          elPoll.style.background = '#334155';
          elPoll.style.color = '#cbd5e1';
          elPoll.style.borderColor = '#475569';
        }
      }

      const elTx = document.getElementById('baseLoraTxPolls');
      if (elTx) elTx.innerText = d.tx_polls || 0;

      const elRx = document.getElementById('baseLoraRxTelems');
      if (elRx) elRx.innerText = d.rx_telemetry || 0;

      const elAgo = document.getElementById('baseLoraLastRxAgo');
      const elSub = document.getElementById('baseLoraLastRxSub');
      if (elAgo) {
        const ago = d.last_comm_seconds_ago;
        if (ago === undefined || ago === null || ago >= 900000 || ago === 999999) {
          elAgo.innerText = 'Nunca';
          elAgo.style.color = 'var(--yellow)';
          if (elSub) elSub.innerText = 'A aguardar resposta do Rover...';
        } else if (ago <= 3) {
          elAgo.innerText = ago + 's atrás';
          elAgo.style.color = 'var(--green)';
          if (elSub) elSub.innerText = 'Ligação LoRa bidirecional ativa';
        } else if (ago < 10) {
          elAgo.innerText = ago + 's atrás';
          elAgo.style.color = 'var(--yellow)';
          if (elSub) elSub.innerText = 'Sinal recente';
        } else {
          elAgo.innerText = ago + 's atrás';
          elAgo.style.color = 'var(--red)';
          if (elSub) elSub.innerText = 'Sem contacto (>10s)';
        }
      }

      const elSig = document.getElementById('baseLoraSignal');
      if (elSig) {
        if (d.rx_telemetry > 0 && d.rssi !== 0) {
          elSig.innerText = d.rssi.toFixed(1) + ' dBm / ' + (d.snr ? d.snr.toFixed(1) : '0.0') + ' dB';
          elSig.style.color = d.rssi > -95 ? 'var(--green)' : 'var(--yellow)';
        } else {
          elSig.innerText = '-- dBm / -- dB';
          elSig.style.color = 'var(--muted)';
        }
      }
    })
    .catch(() => {});
}

function triggerBaseLoraPoll() {
  const btn = document.getElementById('btnForcePoll');
  const fb = document.getElementById('basePollFeedback');
  if (btn) btn.disabled = true;
  if (fb) fb.innerText = 'A transmitir...';

  fetch('/api/lora/poll', { method: 'POST' })
    .then(r => r.json())
    .then(d => {
      if (fb) fb.innerText = 'Poll enviado!';
      setTimeout(() => {
        refreshBaseLoraStatus();
        if (fb) fb.innerText = '';
        if (btn) btn.disabled = false;
      }, 1000);
    })
    .catch(e => {
      if (fb) fb.innerText = 'Erro ao enviar';
      if (btn) btn.disabled = false;
    });
}

setInterval(refreshBaseLoraStatus, 1500);
setTimeout(refreshBaseLoraStatus, 300);
</script>
"""

ROVER_LORA_CARD = """
<div class="card" id="roverLoraCard">
  <div class="card-title">
    <span><i class="fa-solid fa-tower-broadcast"></i> Diagnóstico e Comunicações LoRa (433 MHz)</span>
    <div style="display:flex;gap:8px;align-items:center;flex-wrap:wrap;">
      <span id="roverLoraHwBadge" class="badge" style="background:#064e3b;color:#a7f3d0;border:1px solid #059669;">
        <i class="fa-solid fa-circle-check"></i> SX1278 ONLINE
      </span>
      <span id="roverLoraContactBadge" class="badge" style="background:#78350f;color:#fde68a;border:1px solid #b45309;">
        <i class="fa-solid fa-clock"></i> A AGUARDAR BASE
      </span>
    </div>
  </div>
  <p style="color:var(--muted);font-size:0.875rem;margin-bottom:16px;">
    Estado do rádio SX1278 no Coprocessador C6 via barramento FSPI e monitorização de pedidos/polls recebidos da Estação Base.
  </p>

  <div class="grid-4" style="margin-bottom:16px;">
    <div class="stat-box">
      <div class="stat-label">Pedidos da Base (RX)</div>
      <div class="stat-value" id="roverLoraRxPolls" style="color:var(--primary);">0</div>
      <div style="font-size:0.72rem;color:var(--muted);margin-top:2px;">Polls e comandos recebidos</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Telemetrias Enviadas (TX)</div>
      <div class="stat-value" id="roverLoraTxTelems" style="color:var(--green);">0</div>
      <div style="font-size:0.72rem;color:var(--muted);margin-top:2px;">Pacotes 433MHz transmitidos</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Último Pedido Recebido</div>
      <div class="stat-value" id="roverLoraLastRxAgo" style="color:var(--yellow);font-size:1.15rem;">Nunca</div>
      <div style="font-size:0.72rem;color:var(--muted);margin-top:2px;" id="roverLoraLastRxSub">Aguardando sinal da Base...</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Sinal da Base (RSSI / SNR)</div>
      <div class="stat-value" id="roverLoraSignal" style="font-size:1.15rem;">-- dBm / -- dB</div>
      <div style="font-size:0.72rem;color:var(--muted);margin-top:2px;">Última receção pelo ar</div>
    </div>
  </div>

  <div style="background:rgba(0,0,0,0.2);padding:14px 18px;border-radius:10px;border:1px solid var(--card-border);display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:12px;">
    <div style="font-size:0.8rem;color:var(--muted);">
      <strong style="color:var(--text);"><i class="fa-solid fa-sliders"></i> Configuração RF:</strong> 
      433.0 MHz &bull; BW 250 kHz &bull; SF7 &bull; CR 4/5 &bull; Sync 0x12 &bull; Potência 17 dBm
    </div>
    <div style="display:flex;align-items:center;gap:10px;">
      <button type="button" onclick="triggerRoverLoraTelem()" class="btn btn-primary" id="btnForceTelem">
        <i class="fa-solid fa-paper-plane"></i> Transmitir Telemetria Agora
      </button>
      <span id="roverTelemFeedback" style="font-size:0.8rem;color:var(--green);"></span>
    </div>
  </div>
</div>

<script>
function refreshRoverLoraStatus() {
  fetch('/api/lora/status')
    .then(r => r.json())
    .then(d => {
      const elHw = document.getElementById('roverLoraHwBadge');
      if (elHw) {
        if (d.hw_ready) {
          elHw.innerHTML = '<i class=\"fa-solid fa-circle-check\"></i> SX1278 ONLINE';
          elHw.style.background = '#064e3b';
          elHw.style.color = '#a7f3d0';
          elHw.style.borderColor = '#059669';
        } else {
          elHw.innerHTML = '<i class=\"fa-solid fa-triangle-exclamation\"></i> ERRO RF / DESCONECTADO';
          elHw.style.background = '#7f1d1d';
          elHw.style.color = '#fecaca';
          elHw.style.borderColor = '#dc2626';
        }
      }

      const elContact = document.getElementById('roverLoraContactBadge');
      if (elContact) {
        if (d.has_contact || (d.last_comm_seconds_ago !== undefined && d.last_comm_seconds_ago < 5)) {
          elContact.innerHTML = '<i class=\"fa-solid fa-link\"></i> COM PEDIDOS DA BASE';
          elContact.style.background = '#064e3b';
          elContact.style.color = '#a7f3d0';
          elContact.style.borderColor = '#059669';
        } else if (d.rx_polls > 0) {
          elContact.innerHTML = '<i class=\"fa-solid fa-clock-rotate-left\"></i> CONTACTO PERDIDO (>5s)';
          elContact.style.background = '#78350f';
          elContact.style.color = '#fde68a';
          elContact.style.borderColor = '#b45309';
        } else {
          elContact.innerHTML = '<i class=\"fa-solid fa-clock\"></i> A AGUARDAR BASE';
          elContact.style.background = '#334155';
          elContact.style.color = '#cbd5e1';
          elContact.style.borderColor = '#475569';
        }
      }

      const elRx = document.getElementById('roverLoraRxPolls');
      if (elRx) elRx.innerText = d.rx_polls || 0;

      const elTx = document.getElementById('roverLoraTxTelems');
      if (elTx) elTx.innerText = d.tx_telemetry || 0;

      const elAgo = document.getElementById('roverLoraLastRxAgo');
      const elSub = document.getElementById('roverLoraLastRxSub');
      if (elAgo) {
        const ago = d.last_comm_seconds_ago;
        if (ago === undefined || ago === null || ago >= 900000 || ago === 999999) {
          elAgo.innerText = 'Nunca';
          elAgo.style.color = 'var(--yellow)';
          if (elSub) elSub.innerText = 'Nenhum pedido recebido da Base ainda';
        } else if (ago <= 3) {
          elAgo.innerText = ago + 's atrás';
          elAgo.style.color = 'var(--green)';
          if (elSub) elSub.innerText = 'Recebendo pedidos em tempo real';
        } else if (ago < 10) {
          elAgo.innerText = ago + 's atrás';
          elAgo.style.color = 'var(--yellow)';
          if (elSub) elSub.innerText = 'Pedido recente';
        } else {
          elAgo.innerText = ago + 's atrás';
          elAgo.style.color = 'var(--red)';
          if (elSub) elSub.innerText = 'Sem pedidos da Base (>10s)';
        }
      }

      const elSig = document.getElementById('roverLoraSignal');
      if (elSig) {
        if (d.rx_polls > 0 && d.rssi !== 0) {
          elSig.innerText = d.rssi.toFixed(1) + ' dBm / ' + (d.snr ? d.snr.toFixed(1) : '0.0') + ' dB';
          elSig.style.color = d.rssi > -95 ? 'var(--green)' : 'var(--yellow)';
        } else {
          elSig.innerText = '-- dBm / -- dB';
          elSig.style.color = 'var(--muted)';
        }
      }
    })
    .catch(() => {});
}

function triggerRoverLoraTelem() {
  const btn = document.getElementById('btnForceTelem');
  const fb = document.getElementById('roverTelemFeedback');
  if (btn) btn.disabled = true;
  if (fb) fb.innerText = 'A transmitir...';

  fetch('/api/lora/send_telem', { method: 'POST' })
    .then(r => r.json())
    .then(d => {
      if (fb) fb.innerText = 'Telemetria enviada!';
      setTimeout(() => {
        refreshRoverLoraStatus();
        if (fb) fb.innerText = '';
        if (btn) btn.disabled = false;
      }, 1000);
    })
    .catch(e => {
      if (fb) fb.innerText = 'Erro ao enviar';
      if (btn) btn.disabled = false;
    });
}

setInterval(refreshRoverLoraStatus, 1500);
setTimeout(refreshRoverLoraStatus, 300);
</script>
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

BASE_OTA_CARD = """
<!-- Secção OTA (Dual ESP32-C6 Base Station) -->
<div class="card" style="border-color:var(--primary);margin-top:24px;">
  <div class="card-title">
    <span><i class="fa-solid fa-cloud-arrow-up"></i> Atualização de Firmware Over-The-Air (OTA)</span>
    <span class="badge" style="background:#1e293b;color:var(--primary);font-size:0.85rem;font-weight:700;">Dual ESP32-C6 Base</span>
  </div>
  <p style="color:var(--muted);font-size:0.875rem;margin-bottom:16px;">
    Envie um único pacote <code>base_firmware.zip</code> contendo os binários de ambos os processadores (ou um ficheiro <code>.bin</code> individual).
    A atualização é sequencial e à prova de falhas: o <strong>Coprocessador C6 (LoRa 433MHz)</strong> é atualizado e validado primeiro, seguido pelo <strong>Master Lafvin</strong>.
  </p>

  <!-- Informações de Versão Ativa dos Módulos -->
  <div style="display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-bottom:16px;">
    <div style="background:rgba(15,23,42,0.7);padding:14px;border-radius:8px;border:1px solid var(--card-border);">
      <div style="color:var(--muted);font-size:0.75rem;text-transform:uppercase;font-weight:600;margin-bottom:4px;">
        <i class="fa-solid fa-microchip" style="color:var(--primary);margin-right:6px;"></i>Master Lafvin (LCD+SD+WiFi)
      </div>
      <div id="fwLafvinVer" style="font-size:1.3rem;font-weight:700;color:var(--green);">v2.1.0</div>
      <div style="color:var(--muted);font-size:0.7rem;margin-top:2px;">ST7789 Display, MicroSD, Web & Gestão</div>
    </div>
    <div style="background:rgba(15,23,42,0.7);padding:14px;border-radius:8px;border:1px solid var(--card-border);">
      <div style="color:var(--muted);font-size:0.75rem;text-transform:uppercase;font-weight:600;margin-bottom:4px;">
        <i class="fa-solid fa-satellite-dish" style="color:var(--yellow);margin-right:6px;"></i>Coprocessador C6 (LoRa 433MHz)
      </div>
      <div id="fwC6Ver" style="font-size:1.3rem;font-weight:700;color:var(--yellow);">A detetar...</div>
      <div style="color:var(--muted);font-size:0.7rem;margin-top:2px;">LoRa SX1278 (Hardware FSPI dedicado)</div>
    </div>
  </div>

  <div id="otaDropZone" style="background:rgba(0,0,0,0.25);padding:24px;border-radius:12px;border:2px dashed var(--card-border);text-align:center;transition:all 0.3s;"
       ondragover="event.preventDefault();this.style.borderColor='var(--primary)';"
       ondragleave="this.style.borderColor='var(--card-border)';"
       ondrop="event.preventDefault();this.style.borderColor='var(--card-border)';if(event.dataTransfer.files.length)onOtaFileSelected(event.dataTransfer.files[0]);">
    <i class="fa-solid fa-file-zipper" style="font-size:2.8rem;color:var(--primary);margin-bottom:12px;display:inline-block;"></i>
    <div style="font-size:1.05rem;font-weight:600;margin-bottom:6px;">Arraste o pacote <code>base_firmware.zip</code> para aqui</div>
    <div style="font-size:0.85rem;color:var(--muted);margin-bottom:16px;">Ou clique abaixo para selecionar o ficheiro (.zip com base_c6.bin + base_lafvin.bin)</div>
    <input type="file" id="otaFileInput" accept=".zip,.bin" style="display:none;" onchange="if(this.files.length)onOtaFileSelected(this.files[0])">
    <button type="button" class="btn btn-secondary" onclick="document.getElementById('otaFileInput').click()" style="padding:10px 20px;">
      <i class="fa-solid fa-folder-open"></i> Escolher Ficheiro de Firmware
    </button>
    <div style="margin-top:14px;display:flex;align-items:center;gap:8px;justify-content:center;background:rgba(255,255,255,0.03);padding:10px;border-radius:8px;border:1px solid rgba(255,255,255,0.06);">
      <input type="checkbox" id="otaSkipC6" style="cursor:pointer;width:18px;height:18px;accent-color:var(--primary);">
      <label for="otaSkipC6" style="font-size:0.85rem;color:var(--text);cursor:pointer;user-select:none;">
        <strong>Ignorar Coprocessador C6</strong> (atualizar apenas Master Lafvin se C6 estiver desligado)
      </label>
    </div>
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
    <div style="display:flex;gap:10px;justify-content:space-between;align-items:center;margin-top:12px;flex-wrap:wrap;">
      <button type="button" id="btnCancelOta" onclick="cancelActiveOta()" class="btn btn-secondary" style="padding:6px 12px;font-size:0.8rem;background:#334155;border:1px solid #475569;">
        <i class="fa-solid fa-xmark"></i> Cancelar OTA e Restaurar Ecrã
      </button>
      <button type="button" id="btnSkipC6ToLafvin" onclick="skipC6AndFlashLafvin()" class="btn" style="display:none;padding:6px 14px;font-size:0.8rem;background:#d97706;color:#fff;font-weight:600;">
        <i class="fa-solid fa-forward-step"></i> Ignorar C6 e Gravar Master Lafvin
      </button>
    </div>
  </div>
</div>

<script>
let currentOtaLafvinBlob = null;
let otaInProgress = false;
let otaC6AbortRequested = false;

window.addEventListener('beforeunload', function(e) {
  if (otaInProgress) {
    e.preventDefault();
    e.returnValue = 'Uma atualização OTA está em curso. Se sair da página agora, a gravação será interrompida!';
    return e.returnValue;
  }
});

async function cancelActiveOta() {
  otaC6AbortRequested = true;
  otaLog('A solicitar cancelamento de OTA e restauro do ecrã LCD...', '#f59e0b');
  try {
    const res = await fetch('/api/ota/cancel', { method: 'POST' });
    const j = await res.json();
    otaLog('✔ ' + (j.msg || 'OTA cancelado com sucesso.'), '#10b981');
  } catch(e) {
    otaLog('Aviso ao contactar /api/ota/cancel: ' + e.message, '#f59e0b');
  }
  otaInProgress = false;
  setOtaProgress(0, 'OTA Cancelado. Operação normal restaurada.', false);
  const skipBtn = document.getElementById('btnSkipC6ToLafvin');
  if (skipBtn) skipBtn.style.display = 'none';
}

async function skipC6AndFlashLafvin() {
  otaC6AbortRequested = true;
  const skipBtn = document.getElementById('btnSkipC6ToLafvin');
  if (skipBtn) skipBtn.style.display = 'none';
  otaLog('A ignorar Coprocessador C6 e a avançar para gravação do Master Lafvin...', '#d97706');
  if (currentOtaLafvinBlob) {
    await performLafvinOta(currentOtaLafvinBlob);
  } else {
    otaLog('Nenhum binário lafvin.bin disponível para gravar.', '#ef4444');
    cancelActiveOta();
  }
}

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
  const skipBtn = document.getElementById('btnSkipC6ToLafvin');
  if (skipBtn) skipBtn.style.display = 'none';

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
        } else if (k.endsWith('lafvin.bin') || ((k.includes('lafvin') || k.includes('master') || k.includes('base')) && k.endsWith('.bin'))) {
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

  currentOtaLafvinBlob = lafvinBlob;
  otaInProgress = true;
  otaC6AbortRequested = false;

  // PASSO 1: Atualização dos ficheiros Web no Cartão MicroSD (se presentes)
  if (webFiles.length > 0) {
    setOtaProgress(5, 'A atualizar frontend no Cartão SD da Base (0/' + webFiles.length + ')...');
    otaLog('★ [Passo 1] A atualizar ' + webFiles.length + ' ficheiros web no Cartão MicroSD...', '#06b6d4');
    let okCount = 0;
    const createdDirs = new Set(['/', '/www']);
    for (let i = 0; i < webFiles.length; i++) {
      if (otaC6AbortRequested) return;
      const wf = webFiles[i];
      const parts = wf.path.split('/');
      const fileName = parts.pop();
      const dirPath = '/' + parts.join('/');
      
      // Assegura recursivamente a existência da pasta de destino no SD
      if (dirPath && dirPath !== '/' && !createdDirs.has(dirPath)) {
        const segs = dirPath.split('/').filter(Boolean);
        let curr = '';
        for (const s of segs) {
          curr += '/' + s;
          if (!createdDirs.has(curr)) {
            try {
              await fetch('/api/sd/mkdir', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ dir: curr, path: curr })
              });
              createdDirs.add(curr);
            } catch(e) {}
          }
        }
      }
      
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
  const skipC6 = document.getElementById('otaSkipC6')?.checked;
  if (c6Blob && !skipC6) {
    const ok = await performC6Ota(c6Blob);
    if (!ok) {
      if (otaC6AbortRequested) return;
      otaLog('Processo interrompido no C6. Se o C6 estiver desligado, pode ignorá-lo.', '#ef4444');
      if (lafvinBlob) {
        const skipBtn = document.getElementById('btnSkipC6ToLafvin');
        if (skipBtn) skipBtn.style.display = 'inline-block';
        otaLog('Clique no botão "Ignorar C6 e Gravar Master Lafvin" para prosseguir com o Master.', '#f59e0b');
      }
      return;
    }
  } else if (c6Blob && skipC6) {
    otaLog('★ Coprocessador C6 ignorado conforme opção selecionada.', '#d97706');
  }

  // PASSO 3: Atualização do Master Lafvin
  if (lafvinBlob) {
    await performLafvinOta(lafvinBlob);
  } else {
    otaInProgress = false;
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
    if (otaC6AbortRequested) return false;
    await new Promise(r => setTimeout(r, 1000));
    if (otaC6AbortRequested) return false;
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
      } else if (sJson.state && (sJson.state.startsWith('FAIL') || sJson.state === 'ERROR')) {
        otaLog('Falha no Coprocessador C6: ' + (sJson.detail || sJson.state), '#ef4444');
        setOtaProgress(35, 'Falha no C6: ' + sJson.state, false);
        return false;
      }
    } catch (e) {}

    // Se após 5 segundos ainda estiver a aguardar resposta do C6, disponibiliza a opção de pular direto para o Master Lafvin
    if (i >= 4 && currentOtaLafvinBlob) {
      const skipBtn = document.getElementById('btnSkipC6ToLafvin');
      if (skipBtn) skipBtn.style.display = 'inline-block';
    }
  }
  otaLog('Aviso: Timeout à espera de resposta do C6.', '#f59e0b');
  return false;
}

function performLafvinOta(blob) {
  return new Promise((resolve) => {
    setOtaProgress(55, '[Passo 2/2] A gravar Master Lafvin da Base...');
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
      otaInProgress = false;
      if (xhr.status === 200) {
        setOtaProgress(100, '✔ Atualização concluída com sucesso! A reiniciar Base...', false);
        otaLog('★ Firmware do Master gravado com 100% de sucesso!', '#10b981');
        otaLog('A Base Station está a reiniciar. A recarregar página em 6 segundos...', '#06b6d4');
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
      otaInProgress = false;
      otaLog('Falha de rede ao gravar Master Lafvin.', '#ef4444');
      resolve(false);
    };

    const formData = new FormData();
    formData.append('file', blob, 'lafvin.bin');
    xhr.send(formData);
  });
}
</script>
"""

BASE_CONFIG_CONTENT = BASE_LORA_CARD + BASE_LCD_CARD + BASE_CONSOLE_CARD + BASE_OTA_CARD

BASE_WIFI_CONTENT = WIFI_CONTENT.replace(
    "Ligue o Rover ao router Wi-Fi do cais ou centro de comando para telemetria em tempo real e acesso local direto.",
    "Ligue a Base Station ao router Wi-Fi local para permitir controlo via browser e telemetria da frota em tempo real."
).replace("Redes Guardadas no Rover", "Redes Guardadas na Base Station")

BASE_SD_CONTENT = SD_CONTENT.replace("Explorador do Cartão MicroSD", "Explorador MicroSD - Base Station").replace("Cartão MicroSD - Rover", "Cartão MicroSD - Base Station").replace("let currentDir = '/';", "let currentDir = '/www/base';")

BASE_MISSION_CONTENT = """
<style>
/* Reset & Container */
.mission-container {
  max-width: 1200px;
  margin: 4px auto 20px auto;
  padding: 0 6px;
}
@media (min-width: 768px) {
  .mission-container {
    padding: 0 16px;
    margin: 12px auto 30px auto;
  }
}

/* Top Bar: Fleet Quick Selector & Mission Status */
.mission-top-bar {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 8px;
  background: var(--card);
  border: 1px solid var(--card-border);
  padding: 8px 12px;
  border-radius: 12px;
  margin-bottom: 10px;
  box-shadow: var(--card-shadow);
  flex-wrap: wrap;
}
.fleet-scroll {
  display: flex;
  align-items: center;
  gap: 6px;
  overflow-x: auto;
  -webkit-overflow-scrolling: touch;
  padding: 2px 0;
  flex: 1;
  min-width: 0;
}
.fleet-scroll::-webkit-scrollbar { display: none; }
.fleet-chip {
  padding: 6px 12px;
  border-radius: 999px;
  background: rgba(255,255,255,0.06);
  border: 1px solid var(--card-border);
  color: var(--text);
  font-size: 0.78rem;
  font-weight: 700;
  cursor: pointer;
  display: inline-flex;
  align-items: center;
  gap: 6px;
  white-space: nowrap;
  transition: all 0.15s ease;
  min-height: 36px;
  user-select: none;
}
.fleet-chip:active { transform: scale(0.96); }
.fleet-chip.active {
  background: var(--primary);
  color: #0b1329;
  border-color: var(--primary);
  box-shadow: 0 0 10px rgba(6, 182, 212, 0.4);
}
.fleet-chip .dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  display: inline-block;
}
.fleet-chip .dot.online { background: var(--green); box-shadow: 0 0 6px var(--green); }
.fleet-chip .dot.offline { background: var(--red); }

.state-pill-btn {
  background: #475569;
  border: 1px solid #94a3b8;
  color: #fff;
  padding: 6px 12px;
  border-radius: 999px;
  font-size: 0.75rem;
  font-weight: 700;
  cursor: pointer;
  display: inline-flex;
  align-items: center;
  gap: 6px;
  white-space: nowrap;
  min-height: 36px;
  transition: all 0.15s ease;
}
.state-pill-btn:active { transform: scale(0.96); }

/* Segmented Mobile Tab Bar */
.mission-tabs {
  display: grid;
  grid-template-columns: repeat(4, 1fr);
  gap: 6px;
  background: rgba(14, 26, 56, 0.95);
  border: 1px solid var(--card-border);
  padding: 4px;
  border-radius: 12px;
  margin-bottom: 12px;
  position: sticky;
  top: 6px;
  z-index: 1050;
  backdrop-filter: blur(12px);
  -webkit-backdrop-filter: blur(12px);
}
.m-tab-btn {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 3px;
  padding: 8px 4px;
  border-radius: 8px;
  border: 1px solid transparent;
  background: transparent;
  color: var(--muted);
  font-size: 0.72rem;
  font-weight: 700;
  cursor: pointer;
  transition: all 0.18s ease;
  min-height: 48px;
  user-select: none;
}
.m-tab-btn i { font-size: 1.15rem; }
.m-tab-btn.active {
  background: rgba(6, 182, 212, 0.18);
  color: var(--primary);
  border-color: var(--primary);
  box-shadow: 0 0 12px rgba(6, 182, 212, 0.25);
}
[data-theme=\"light\"] .mission-tabs {
  background: rgba(255, 255, 255, 0.95);
  border-color: #cbd5e1;
}
[data-theme=\"light\"] .m-tab-btn.active {
  background: rgba(2, 132, 199, 0.15);
  color: #0284c7;
  border-color: #0284c7;
}

/* Tab Panes */
.tab-pane {
  display: none;
}
.tab-pane.active {
  display: block;
  animation: fadeInPane 0.2s ease;
}
@keyframes fadeInPane {
  from { opacity: 0; transform: translateY(4px); }
  to { opacity: 1; transform: translateY(0); }
}

/* MAP SECTION */
.map-hud-bar {
  background: var(--card);
  border: 1px solid var(--card-border);
  border-radius: 10px;
  padding: 6px 10px;
  margin-bottom: 8px;
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 6px;
  overflow-x: auto;
  white-space: nowrap;
}
.map-hud-bar::-webkit-scrollbar { display: none; }
.hud-select {
  background: #090e1a;
  color: var(--primary);
  font-weight: 700;
  border: 1px solid var(--primary);
  border-radius: 6px;
  padding: 4px 8px;
  font-size: 0.78rem;
  outline: none;
}
[data-theme=\"light\"] .hud-select {
  background: #f1f5f9;
}
.hud-pill {
  font-size: 0.72rem;
  padding: 3px 8px;
  border-radius: 999px;
  display: inline-flex;
  align-items: center;
  gap: 5px;
  font-weight: 600;
  background: rgba(255,255,255,0.06);
  border: 1px solid rgba(255,255,255,0.1);
  color: var(--text);
}
[data-theme=\"light\"] .hud-pill {
  background: #f1f5f9;
  border-color: #cbd5e1;
  color: #0f172a;
}
.hud-btn {
  background: rgba(6, 182, 212, 0.15);
  color: var(--primary);
  border: 1px solid var(--primary);
  border-radius: 6px;
  padding: 4px 10px;
  font-size: 0.75rem;
  font-weight: 600;
  cursor: pointer;
  display: inline-flex;
  align-items: center;
  gap: 4px;
  white-space: nowrap;
}
.hud-btn:active { background: var(--primary); color: #04101e; }

.map-wrapper {
  position: relative;
  width: 100%;
}
#fleetMap {
  width: 100%;
  height: calc(100dvh - 280px);
  min-height: 380px;
  max-height: 620px;
  border-radius: 12px;
  border: 1px solid var(--card-border);
  background: #091325;
  position: relative;
  z-index: 10;
}
.map-bottom-bar {
  background: var(--card);
  border: 1px solid var(--card-border);
  border-radius: 10px;
  padding: 8px 10px;
  margin-top: 8px;
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 8px;
  flex-wrap: wrap;
}
.coords-text {
  font-family: monospace;
  font-size: 0.75rem;
  line-height: 1.35;
}
.coords-text strong { color: var(--text); }
.map-actions {
  display: flex;
  gap: 6px;
  align-items: center;
}
.btn-action-touch {
  padding: 8px 14px;
  border-radius: 8px;
  font-size: 0.78rem;
  font-weight: 700;
  border: none;
  cursor: pointer;
  display: inline-flex;
  align-items: center;
  gap: 6px;
  white-space: nowrap;
  min-height: 42px;
  transition: transform 0.1s ease;
}
.btn-action-touch:active { transform: scale(0.96); }
.btn-goto { background: #0284c7; color: #fff; }
.btn-hold { background: #0f766e; color: #fff; }
.btn-stop { background: #7f1d1d; color: #fca5a5; }

/* Drag Hint & Toast */
.drag-hint-pill {
  position: absolute;
  top: 14px;
  left: 50%;
  transform: translateX(-50%);
  background: rgba(9, 14, 26, 0.95);
  border: 1px solid var(--primary);
  color: #fff;
  padding: 6px 16px;
  border-radius: 999px;
  font-size: 0.75rem;
  font-weight: 700;
  z-index: 1500;
  pointer-events: none;
  display: none;
  box-shadow: 0 4px 16px rgba(0,0,0,0.6);
  white-space: nowrap;
}
.toast-banner {
  position: fixed;
  top: 20px;
  left: 50%;
  transform: translateX(-50%);
  background: #064e3b;
  color: #a7f3d0;
  border: 1px solid #10b981;
  padding: 8px 18px;
  border-radius: 8px;
  font-size: 0.8rem;
  font-weight: 600;
  z-index: 2500;
  display: none;
  box-shadow: 0 6px 20px rgba(0,0,0,0.6);
  text-align: center;
  max-width: 90%;
  animation: fadeInToast 0.25s ease;
}
@keyframes fadeInToast {
  from { opacity: 0; transform: translate(-50%, -10px); }
  to { opacity: 1; transform: translate(-50%, 0); }
}

/* Nautical Markers on Leaflet */
.rover-icon-container {
  position: relative;
  width: 38px;
  height: 38px;
  cursor: grab;
  touch-action: none;
}
.rover-icon-container:active { cursor: grabbing; }
.rover-boat-pin {
  width: 32px;
  height: 32px;
  border-radius: 50%;
  background: #0f1e3d;
  border: 2px solid var(--primary);
  box-shadow: 0 0 10px rgba(6, 182, 212, 0.6);
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  position: relative;
  transition: transform 0.2s ease;
}
.rover-boat-pin.selected {
  border-color: #38bdf8;
  box-shadow: 0 0 14px #38bdf8, inset 0 0 6px #38bdf8;
  transform: scale(1.12);
}
.rover-boat-pin.offline {
  border-color: var(--red);
  box-shadow: 0 0 8px rgba(239, 68, 68, 0.4);
}
.rover-heading-arrow {
  position: absolute;
  top: -7px;
  left: 50%;
  transform-origin: bottom center;
  margin-left: -5px;
  width: 0;
  height: 0;
  border-left: 5px solid transparent;
  border-right: 5px solid transparent;
  border-bottom: 8px solid var(--primary);
  filter: drop-shadow(0 0 4px var(--primary));
  transition: transform 0.3s ease;
}
.rover-code-text {
  font-size: 0.65rem;
  font-weight: 800;
  color: #fff;
  line-height: 1;
}
.rover-status-dot {
  width: 6px;
  height: 6px;
  border-radius: 50%;
  background: var(--green);
  position: absolute;
  bottom: 2px;
  right: 2px;
}
.rover-status-dot.offline { background: var(--red); }
.target-pin {
  width: 28px;
  height: 28px;
  border-radius: 50%;
  background: rgba(245, 158, 11, 0.3);
  border: 2px dashed #f59e0b;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 0.85rem;
  box-shadow: 0 0 12px rgba(245, 158, 11, 0.7);
  cursor: grab;
  animation: pulseTarget 1.8s infinite;
}
@keyframes pulseTarget {
  0% { transform: scale(0.95); opacity: 0.85; }
  50% { transform: scale(1.15); opacity: 1; }
  100% { transform: scale(0.95); opacity: 0.85; }
}

/* COMMANDS STYLES */
.btn-emergency {
  width: 100%;
  background: linear-gradient(135deg, #b91c1c, #dc2626);
  color: #fff;
  font-size: 1.05rem;
  font-weight: 800;
  padding: 14px;
  border-radius: 10px;
  border: 2px solid #ef4444;
  box-shadow: 0 0 16px rgba(239, 68, 68, 0.4);
  cursor: pointer;
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 8px;
  margin-bottom: 14px;
  letter-spacing: 0.5px;
  min-height: 52px;
  transition: transform 0.15s ease, box-shadow 0.15s ease;
}
.btn-emergency:active { transform: scale(0.98); }
.cmd-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(140px, 1fr));
  gap: 8px;
  margin-top: 10px;
}
.touch-slider-group {
  background: rgba(0,0,0,0.15);
  border: 1px solid var(--card-border);
  padding: 14px;
  border-radius: 10px;
  margin-bottom: 12px;
}
.slider-touch {
  width: 100%;
  -webkit-appearance: none;
  height: 14px;
  background: #0f172a;
  border-radius: 8px;
  outline: none;
  margin: 12px 0;
}
.slider-touch::-webkit-slider-thumb {
  -webkit-appearance: none;
  width: 32px;
  height: 32px;
  border-radius: 50%;
  background: var(--primary);
  cursor: pointer;
  box-shadow: 0 0 10px rgba(6, 182, 212, 0.6);
}

/* BATTERY STYLES */
.bat-circle-container {
  text-align: center;
  padding: 16px 0;
}
.bat-gauge-num {
  font-size: 3.6rem;
  font-weight: 800;
  line-height: 1;
}

/* Modal Backdrop */
.modal-overlay {
  display: none;
  position: fixed;
  top: 0; left: 0; width: 100%; height: 100%;
  background: rgba(0,0,0,0.7);
  z-index: 3000;
  align-items: center;
  justify-content: center;
  padding: 16px;
  backdrop-filter: blur(4px);
}
.modal-content-card {
  background: var(--card);
  border: 1px solid var(--card-border);
  border-radius: 16px;
  padding: 20px;
  max-width: 480px;
  width: 100%;
  box-shadow: 0 10px 25px rgba(0,0,0,0.5);
  animation: modalScale 0.2s ease;
}
@keyframes modalScale {
  from { opacity: 0; transform: scale(0.95); }
  to { opacity: 1; transform: scale(1); }
}

/* Responsive tweaks for phones */
@media (max-width: 640px) {
  .mission-tabs { grid-template-columns: repeat(4, 1fr); gap: 4px; padding: 3px; }
  .m-tab-btn { font-size: 0.68rem; padding: 6px 2px; min-height: 44px; }
  .m-tab-btn i { font-size: 0.95rem; }
  .map-actions { width: 100%; justify-content: space-between; }
  .btn-action-touch { flex: 1; justify-content: center; }
  #fleetMap { height: calc(100dvh - 300px); min-height: 320px; }
  .badge-hide-mobile { display: none !important; }
}
</style>

<!-- Barra Superior: Seleção Rápida de Bóias & Estado da Base -->
<div class="mission-top-bar">
  <div class="fleet-scroll" id="fleetChipsBar">
    <span class="fleet-chip active" id="chip_1" onclick="changeRover(1)">
      <i class="fa-solid fa-sailboat"></i> R1 <span class="dot online"></span> <b>--%</b>
    </span>
  </div>
  <div>
    <button type="button" class="state-pill-btn" id="stateBadgePill" onclick="toggleStateModal()" title="Toque para mudar o Estado da Base">
      <i class="fa-solid fa-flag"></i> <span id="topStateText">STORAGE</span> <i class="fa-solid fa-chevron-down" style="font-size:0.7rem;margin-left:2px;"></i>
    </button>
  </div>
</div>

<!-- Barra de Abas Tácteis Segmentadas (Otimizada para Telemóvel / Tablet) -->
<div class="mission-tabs">
  <button type="button" class="m-tab-btn active" id="segTab_map" onclick="switchMissionTab('map')">
    <i class="fa-solid fa-location-dot"></i>
    <span>Mapa GPS</span>
  </button>
  <button type="button" class="m-tab-btn" id="segTab_cmd" onclick="switchMissionTab('cmd')">
    <i class="fa-solid fa-gamepad"></i>
    <span>Comandos</span>
  </button>
  <button type="button" class="m-tab-btn" id="segTab_bat" onclick="switchMissionTab('bat')">
    <i class="fa-solid fa-battery-three-quarters"></i>
    <span>Bateria</span>
  </button>
  <button type="button" class="m-tab-btn" id="segTab_setup" onclick="switchMissionTab('setup')">
    <i class="fa-solid fa-sailboat"></i>
    <span>Regata & Bóias</span>
  </button>
</div>

<!-- ===================================================================== -->
<!-- TAB 1: MAPA GPS INTERATIVO -->
<!-- ===================================================================== -->
<div class="tab-pane active" id="pane_map">
  <!-- HUD Superior do Mapa -->
  <div class="map-hud-bar">
    <div style="display:flex;align-items:center;gap:6px;">
      <select id="mapRoverSelect" onchange="changeRover(this.value)" class="hud-select">
        <option value="1">R1</option>
      </select>
      <span id="fixStatusBadge" class="hud-pill" style="color:var(--green);border-color:var(--green);">
        ● ONLINE
      </span>
    </div>

    <div style="display:flex;align-items:center;gap:4px;">
      <span class="hud-pill" title="Bateria">
        <i class="fa-solid fa-battery-half" style="color:var(--green);"></i>
        <span id="batVal">--%</span>
      </span>
      <span class="hud-pill" title="Velocidade">
        <i class="fa-solid fa-gauge-high"></i>
        <span id="speedVal">0.0 kn</span>
      </span>
      <span class="hud-pill" title="Rumo Proa">
        <i class="fa-solid fa-compass"></i>
        <span id="headingVal">0°</span>
      </span>
      <span class="hud-pill" title="Sinal LoRa">
        <i class="fa-solid fa-signal"></i>
        <span id="rssiVal">-- dBm</span>
      </span>
    </div>

    <div style="display:flex;align-items:center;gap:4px;">
      <button type="button" onclick="centerMapOnRovers()" class="hud-btn" title="Centrar Mapa">
        <i class="fa-solid fa-crosshairs"></i> Focar
      </button>
      <button type="button" onclick="toggleMapLayer()" class="hud-btn" id="layerBtn" title="Alternar Satélite / Mapa">
        <i class="fa-solid fa-layer-group"></i> Sat
      </button>
    </div>
  </div>

  <!-- Contentor do Mapa com Indicador de Arraste -->
  <div class="map-wrapper">
    <div id="dragHint" class="drag-hint-pill">
      <i class="fa-solid fa-arrows-up-down-left-right"></i> <span id="dragHintText">A arrastar alvo...</span>
    </div>
    <div id="fleetMap"></div>
  </div>

  <!-- Barra Inferior de Coordenadas e Ações Rápidas -->
  <div class="map-bottom-bar">
    <div class="coords-text">
      <div>GPS: <strong id="curCoordTxt">--.------, --.------</strong></div>
      <div style="color:var(--primary);">Alvo: <strong id="tgtCoordTxt">Nenhum alvo definido</strong></div>
    </div>
    <div class="map-actions">
      <button type="button" onclick="sendTargetCommand(6)" class="btn-action-touch btn-goto" id="btnGoTarget" title="Navegar para o alvo">
        <i class="fa-solid fa-paper-plane"></i> Navegar
      </button>
      <button type="button" onclick="sendHoldStation()" class="btn-action-touch btn-hold" title="Manter Posição (Hold Station)">
        <i class="fa-solid fa-anchor"></i> Manter
      </button>
      <button type="button" onclick="sendStopRover()" class="btn-action-touch btn-stop" title="Parar Motores">
        <i class="fa-solid fa-ban"></i> Parar
      </button>
    </div>
  </div>
</div>

<!-- ===================================================================== -->
<!-- TAB 2: COMANDOS & ATUADORES -->
<!-- ===================================================================== -->
<div class="tab-pane" id="pane_cmd">
  <!-- Paragem de Emergência Imediata -->
  <button type="button" onclick="sendEmergencyStop()" class="btn-emergency">
    <i class="fa-solid fa-ban"></i> PARAGEM DE EMERGÊNCIA
  </button>

  <div class="card">
    <div class="card-title">
      <span><i class="fa-solid fa-gamepad"></i> Centro de Comando da Frota</span>
      <div style="display:flex;align-items:center;gap:6px;">
        <label class="stat-label" style="margin:0;">Alvo:</label>
        <select id="targetRoverSelect" onchange="changeRover(this.value)" class="hud-select">
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
        <div class="stat-label">Sinal LoRa</div>
        <div class="stat-value" id="curRssi" style="color:var(--green);">0 dBm</div>
      </div>
    </div>
  </div>

  <!-- Modos de Navegação e Segurança -->
  <div class="card">
    <div class="card-title">
      <span><i class="fa-solid fa-compass"></i> Modos de Navegação & Segurança</span>
    </div>
    <div style="display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:8px;">
      <button type="button" onclick="sendAction(1)" class="btn" style="background:#0284c7;color:#fff;padding:12px 10px;">
        <i class="fa-solid fa-location-crosshairs"></i> Hold Station
      </button>
      <button type="button" onclick="sendAction(7)" class="btn" style="background:#7c3aed;color:#fff;padding:12px 10px;">
        <i class="fa-solid fa-house"></i> RTL (Regresso)
      </button>
      <button type="button" onclick="sendAction(10)" class="btn btn-secondary" style="padding:12px 10px;">
        <i class="fa-solid fa-gamepad"></i> Modo Manual
      </button>
      <button type="button" onclick="sendAction(8)" class="btn" style="background:var(--yellow);color:#0f172a;padding:12px 10px;">
        <i class="fa-solid fa-bell"></i> Alarme / Strobe
      </button>
    </div>
  </div>

  <!-- Guincho da Âncora -->
  <div class="card">
    <div class="card-title">
      <span><i class="fa-solid fa-anchor"></i> Guincho da Âncora & Travão</span>
    </div>
    <div style="display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:8px;">
      <button type="button" onclick="sendAction(3)" class="btn btn-primary" style="padding:12px 10px;">
        <i class="fa-solid fa-arrow-down"></i> Lançar Âncora
      </button>
      <button type="button" onclick="sendAction(4)" class="btn btn-primary" style="padding:12px 10px;">
        <i class="fa-solid fa-arrow-up"></i> Recolher Âncora
      </button>
      <button type="button" onclick="sendAction(5)" class="btn btn-secondary" style="padding:12px 10px;">
        <i class="fa-solid fa-stop"></i> Parar Guincho
      </button>
    </div>
  </div>

  <!-- Propulsão ESC & Leme -->
  <div class="card">
    <div class="card-title">
      <span><i class="fa-solid fa-fan"></i> Propulsão ESC & Leme de Direção</span>
    </div>
    
    <div class="touch-slider-group">
      <div style="display:flex;justify-content:space-between;align-items:center;">
        <span class="stat-label">Aceleração ESC:</span>
        <strong id="thrVal" style="font-size:1.15rem;color:var(--primary);">0%</strong>
      </div>
      <input type="range" id="throttleSlider" min="-100" max="100" value="0" class="slider-touch" oninput="updateSliders()" onchange="sendActuators()">
      <div style="display:flex;gap:6px;flex-wrap:wrap;margin-top:8px;">
        <button type="button" onclick="setThrottle(0)" class="btn btn-secondary" style="flex:1;min-height:38px;">Stop (0%)</button>
        <button type="button" onclick="setThrottle(25)" class="btn btn-secondary" style="flex:1;min-height:38px;">+25%</button>
        <button type="button" onclick="setThrottle(50)" class="btn btn-secondary" style="flex:1;min-height:38px;">+50%</button>
        <button type="button" onclick="setThrottle(100)" class="btn btn-primary" style="flex:1;min-height:38px;">+100%</button>
        <button type="button" onclick="setThrottle(-50)" class="btn btn-secondary" style="flex:1;min-height:38px;">-50% Ré</button>
      </div>
    </div>

    <div class="touch-slider-group">
      <div style="display:flex;justify-content:space-between;align-items:center;">
        <span class="stat-label">Ângulo do Leme:</span>
        <strong id="rudVal" style="font-size:1.15rem;color:var(--primary);">0%</strong>
      </div>
      <input type="range" id="rudderSlider" min="-100" max="100" value="0" class="slider-touch" oninput="updateSliders()" onchange="sendActuators()">
      <div style="display:flex;gap:6px;flex-wrap:wrap;margin-top:8px;">
        <button type="button" onclick="setRudder(-75)" class="btn btn-secondary" style="flex:1;min-height:38px;">⬅ Bombordo (-75%)</button>
        <button type="button" onclick="setRudder(0)" class="btn btn-secondary" style="flex:1;min-height:38px;">Centro (0%)</button>
        <button type="button" onclick="setRudder(75)" class="btn btn-secondary" style="flex:1;min-height:38px;">Estibordo (+75%) ➡</button>
      </div>
    </div>
  </div>
</div>

<!-- ===================================================================== -->
<!-- TAB 3: BATERIA FROTA -->
<!-- ===================================================================== -->
<div class="tab-pane" id="pane_bat">
  <div class="card">
    <div class="card-title">
      <span><i class="fa-solid fa-battery-full"></i> Telemetria de Bateria LiPo 4S da Frota</span>
      <div style="display:flex;align-items:center;gap:6px;">
        <label class="stat-label" style="margin:0;">Rover:</label>
        <select id="batRoverSelect" onchange="changeRover(this.value)" class="hud-select">
          <option value="1">Rover-1</option>
        </select>
      </div>
    </div>

    <div class="bat-circle-container">
      <div id="batPct" class="bat-gauge-num" style="color:var(--green);">0%</div>
      <div class="progress-bg" style="max-width:380px;margin:10px auto;height:18px;">
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
        <div class="stat-label">Química</div>
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
          <div class="stat-value cell-v" id="cell1Val" style="font-size:1.15rem;color:var(--primary);">0.00 V</div>
        </div>
        <div class="stat-box" style="text-align:center;">
          <div class="stat-label">Célula 2</div>
          <div class="stat-value cell-v" id="cell2Val" style="font-size:1.15rem;color:var(--primary);">0.00 V</div>
        </div>
        <div class="stat-box" style="text-align:center;">
          <div class="stat-label">Célula 3</div>
          <div class="stat-value cell-v" id="cell3Val" style="font-size:1.15rem;color:var(--primary);">0.00 V</div>
        </div>
        <div class="stat-box" style="text-align:center;">
          <div class="stat-label">Célula 4</div>
          <div class="stat-value cell-v" id="cell4Val" style="font-size:1.15rem;color:var(--primary);">0.00 V</div>
        </div>
      </div>
    </div>
  </div>

  <!-- Resumo da Frota Inteira -->
  <div class="card">
    <div class="card-title">
      <span><i class="fa-solid fa-list-check"></i> Resumo de Energia de Toda a Frota</span>
    </div>
    <div id="fleetBatteryList" style="display:flex;flex-direction:column;gap:8px;">
      <div style="color:var(--muted);text-align:center;padding:12px;">A carregar dados da frota...</div>
    </div>
  </div>
</div>

<!-- ===================================================================== -->
<!-- TAB 4: GESTÃO DA PROVA & BÓIAS -->
<!-- ===================================================================== -->
<div class="tab-pane" id="pane_setup">
  <!-- Estado Operacional da Base Station -->
  <div class="card">
    <div class="card-title">
      <span><i class="fa-solid fa-flag-checkered"></i> Estado de Operação da Estação Base</span>
      <span id="badgeState" class="badge" style="background:#64748b;color:#fff;font-size:0.85rem;padding:6px 14px;">
        STORAGE - Not in use
      </span>
    </div>
    <p style="color:var(--muted);font-size:0.875rem;margin-bottom:12px;">
      Selecione o estado operacional da Estação Base para a missão atual:
    </p>

    <div style="display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:10px;">
      <button type="button" onclick="setState('STORAGE')" id="btnStateSTORAGE" class="btn" style="display:flex;flex-direction:column;align-items:flex-start;padding:12px;background:rgba(255,255,255,0.05);color:#fff;border:1px solid var(--card-border);">
        <span style="font-size:1rem;font-weight:700;"><i class="fa-solid fa-box-archive"></i> Storage</span>
        <span style="font-size:0.75rem;color:var(--muted);margin-top:2px;">Not in use (Standby)</span>
      </button>

      <button type="button" onclick="setState('WAITING_ROVERS')" id="btnStateWAITING_ROVERS" class="btn" style="display:flex;flex-direction:column;align-items:flex-start;padding:12px;background:rgba(255,255,255,0.05);color:#fff;border:1px solid var(--card-border);">
        <span style="font-size:1rem;font-weight:700;"><i class="fa-solid fa-hourglass-half"></i> Waiting Rover List</span>
        <span style="font-size:0.75rem;color:var(--muted);margin-top:2px;">Connected and waiting</span>
      </button>

      <button type="button" onclick="setState('IDLE')" id="btnStateIDLE" class="btn" style="display:flex;flex-direction:column;align-items:flex-start;padding:12px;background:rgba(255,255,255,0.05);color:#fff;border:1px solid var(--card-border);">
        <span style="font-size:1rem;font-weight:700;"><i class="fa-solid fa-anchor"></i> Idle</span>
        <span style="font-size:0.75rem;color:var(--muted);margin-top:2px;">Rover list ready</span>
      </button>

      <button type="button" onclick="setState('GO')" id="btnStateGO" class="btn" style="display:flex;flex-direction:column;align-items:flex-start;padding:12px;background:rgba(255,255,255,0.05);color:#fff;border:1px solid var(--card-border);">
        <span style="font-size:1rem;font-weight:700;"><i class="fa-solid fa-rocket"></i> Go</span>
        <span style="font-size:0.75rem;color:var(--muted);margin-top:2px;">In mission (Ativo)</span>
      </button>
    </div>
  </div>

  <!-- Dados da Regata -->
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

  <!-- Lista de Bóias da Missão -->
  <div class="card">
    <div class="card-title">
      <span><i class="fa-solid fa-satellite-dish"></i> Lista de Bóias da Missão (MicroSD)</span>
      <span style="font-size:0.78rem;color:var(--muted);font-weight:normal;">/config/rovers.json</span>
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

    <div style="display:flex;gap:8px;margin-top:16px;flex-wrap:wrap;">
      <button type="button" onclick="addRow()" class="btn btn-secondary"><i class="fa-solid fa-plus"></i> Adicionar Bóia</button>
      <button type="button" onclick="saveRovers()" class="btn btn-primary" style="padding:10px 20px;"><i class="fa-solid fa-floppy-disk"></i> Guardar Lista no MicroSD</button>
      <button type="button" onclick="clearRovers()" class="btn btn-danger" style="background:#78350f;"><i class="fa-solid fa-rotate-left"></i> Repor Pré-definições</button>
    </div>
  </div>
</div>

<!-- ===================================================================== -->
<!-- MODAL DE SELEÇÃO RÁPIDA DE ESTADO -->
<!-- ===================================================================== -->
<div class="modal-overlay" id="stateModal" onclick="if(event.target===this)toggleStateModal()">
  <div class="modal-content-card">
    <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:14px;">
      <h3 style="margin:0;font-size:1.1rem;"><i class="fa-solid fa-flag"></i> Mudar Estado Operacional</h3>
      <button type="button" onclick="toggleStateModal()" class="btn btn-secondary" style="padding:4px 8px;font-size:0.8rem;">✕</button>
    </div>
    <div style="display:flex;flex-direction:column;gap:8px;">
      <button type="button" onclick="setState('STORAGE')" class="btn" style="background:#475569;color:#fff;justify-content:flex-start;padding:12px;">
        <i class="fa-solid fa-box-archive"></i> <strong>STORAGE</strong> &mdash; Not in use (Standby)
      </button>
      <button type="button" onclick="setState('WAITING_ROVERS')" class="btn" style="background:#b45309;color:#fff;justify-content:flex-start;padding:12px;">
        <i class="fa-solid fa-hourglass-half"></i> <strong>WAITING_ROVERS</strong> &mdash; Connected and waiting
      </button>
      <button type="button" onclick="setState('IDLE')" class="btn" style="background:#0e7490;color:#fff;justify-content:flex-start;padding:12px;">
        <i class="fa-solid fa-anchor"></i> <strong>IDLE</strong> &mdash; Rover list ready
      </button>
      <button type="button" onclick="setState('GO')" class="btn" style="background:#047857;color:#fff;justify-content:flex-start;padding:12px;">
        <i class="fa-solid fa-rocket"></i> <strong>GO</strong> &mdash; In mission (Ativo)
      </button>
    </div>
  </div>
</div>

<!-- Banner Toast de Notificação -->
<div id="toastBanner" class="toast-banner">
  <i class="fa-solid fa-satellite-dish"></i> <span id="toastMsg">Notificação</span>
</div>

<!-- ===================================================================== -->
<!-- SCRIPTS DA PÁGINA UNIFICADA DE MISSÃO -->
<!-- ===================================================================== -->
<script>
// --- ESTADO GLOBAL ---
let selectedRoverId = 1;
let currentRoversData = [];
let currentMissionData = null;
const rolesList = ['largada CR', 'largada', 'boia 1', 'boia 2', 'boia 3', 'boia 4', 'chegada CR', 'chegada'];

// Estado do Mapa Leaflet
let map = null;
let osmLayer = null;
let satLayer = null;
let isSat = false;
let isDragging = false;
let roverMarkers = {};
let targetMarkers = {};
let targetLines = {};
let hasCenteredInitial = false;

// --- GESTÃO DE ABAS / TABS ---
function switchMissionTab(tabId) {
  const tabs = ['map', 'cmd', 'bat', 'setup'];
  tabs.forEach(t => {
    const pane = document.getElementById('pane_' + t);
    const btn = document.getElementById('segTab_' + t);
    if (pane) pane.classList.toggle('active', t === tabId);
    if (btn) btn.classList.toggle('active', t === tabId);
  });

  // Atualiza hash na URL para permitir bookmark direto (#map, #cmd, #bat, #setup)
  try {
    history.replaceState(null, '', '#' + tabId);
  } catch(e) {}

  if (tabId === 'map') {
    if (!map) {
      initMap();
    } else {
      map.invalidateSize();
      setTimeout(() => { if (map) map.invalidateSize(); }, 150);
    }
  }
}

// --- MODAL DE ESTADO ---
function toggleStateModal() {
  const m = document.getElementById('stateModal');
  if (!m) return;
  m.style.display = (m.style.display === 'flex') ? 'none' : 'flex';
}

function updateStateButtons(st) {
  const badge = document.getElementById('badgeState');
  const topText = document.getElementById('topStateText');
  const topPill = document.getElementById('stateBadgePill');
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
  if (topText) topText.innerText = st;
  if (topPill) {
    topPill.style.background = c.bg;
    topPill.style.borderColor = c.border;
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
      const m = document.getElementById('stateModal');
      if (m) m.style.display = 'none';
      showToast('Estado da Base alterado para: ' + res.operation_state);
      loadAllData();
    }
  });
}

// --- SELEÇÃO DE ROVER ---
function changeRover(id) {
  selectedRoverId = parseInt(id);

  ['mapRoverSelect', 'targetRoverSelect', 'batRoverSelect'].forEach(selId => {
    const el = document.getElementById(selId);
    if (el) el.value = selectedRoverId;
  });

  document.querySelectorAll('.fleet-chip').forEach(el => {
    el.classList.toggle('active', el.id === 'chip_' + selectedRoverId);
  });

  fetch('/api/select_rover', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ rover_id: selectedRoverId })
  }).then(() => loadAllData(true));
}

// --- COMANDOS & ATUADORES ---
function updateSliders() {
  const thr = document.getElementById('throttleSlider');
  const rud = document.getElementById('rudderSlider');
  if (thr) document.getElementById('thrVal').innerText = thr.value + '%';
  if (rud) document.getElementById('rudVal').innerText = rud.value + '%';
}

function setThrottle(v) {
  const thr = document.getElementById('throttleSlider');
  if (thr) thr.value = v;
  updateSliders();
  sendActuators();
}

function setRudder(v) {
  const rud = document.getElementById('rudderSlider');
  if (rud) rud.value = v;
  updateSliders();
  sendActuators();
}

function sendActuators() {
  const t = parseInt(document.getElementById('throttleSlider')?.value || 0);
  const r = parseInt(document.getElementById('rudderSlider')?.value || 0);
  fetch('/api/control/actuator', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({throttle: t, rudder: r})
  });
}

function sendAction(a) {
  fetch('/api/control/action', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({action: a, rover_id: selectedRoverId})
  }).then(r => r.json()).then(res => {
    showToast(res.msg || 'Comando enviado ao Rover ' + selectedRoverId);
  });
}

function sendEmergencyStop() {
  setThrottle(0);
  setRudder(0);
  sendAction(2); // ACTION_STOP
  showToast('🛑 PARAGEM DE EMERGÊNCIA ENVIADA!', true);
}

// --- MAPA LEAFLET ---
function initMap() {
  if (map || typeof L === 'undefined') return;

  const defaultLat = 38.7071;
  const defaultLng = -9.1355;

  map = L.map('fleetMap', {
    center: [defaultLat, defaultLng],
    zoom: 16,
    zoomControl: false,
    attributionControl: false
  });

  L.control.zoom({ position: 'topright' }).addTo(map);

  osmLayer = L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', { maxZoom: 19 }).addTo(map);
  satLayer = L.tileLayer('https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}', { maxZoom: 19 });

  if (navigator.geolocation && !hasCenteredInitial) {
    navigator.geolocation.getCurrentPosition(pos => {
      if (!hasCenteredInitial && (!currentRoversData.length || (currentRoversData[0].lat === 0 && currentRoversData[0].lng === 0))) {
        map.setView([pos.coords.latitude, pos.coords.longitude], 16);
      }
    }, () => {}, { timeout: 3000, enableHighAccuracy: false });
  }

  loadAllData();
}

function toggleMapLayer() {
  if (!map) return;
  if (isSat) {
    map.removeLayer(satLayer);
    map.addLayer(osmLayer);
    document.getElementById('layerBtn').innerHTML = '<i class=\"fa-solid fa-layer-group\"></i> Sat';
    isSat = false;
  } else {
    map.removeLayer(osmLayer);
    map.addLayer(satLayer);
    document.getElementById('layerBtn').innerHTML = '<i class=\"fa-solid fa-map\"></i> Mapa';
    isSat = true;
  }
}

function centerMapOnRovers() {
  if (!map) return;
  const sel = currentRoversData.find(r => r.id === selectedRoverId);
  if (sel && (sel.lat !== 0 || sel.lng !== 0)) {
    map.setView([sel.lat, sel.lng], 17);
    return;
  }
  const valid = currentRoversData.filter(r => r.lat !== 0 || r.lng !== 0);
  if (valid.length > 0) {
    const group = valid.map(r => [r.lat, r.lng]);
    if (group.length === 1) map.setView(group[0], 17);
    else map.fitBounds(L.latLngBounds(group).pad(0.2));
  }
}

function createRoverIconHtml(rover, isSelected) {
  const isOnline = !!rover.is_online;
  const heading = rover.heading_deg || 0;
  const code = rover.code ? rover.code.replace('ROVER-', 'R') : ('R' + rover.id);

  return `
    <div class=\"rover-icon-container\">
      <div class=\"rover-boat-pin ${isOnline ? 'online' : 'offline'} ${isSelected ? 'selected' : ''}\">
        <div class=\"rover-heading-arrow\" style=\"transform: rotate(${heading}deg);\"></div>
        <div class=\"rover-code-text\">${code}</div>
        <div class=\"rover-status-dot ${isOnline ? '' : 'offline'}\"></div>
      </div>
    </div>
  `;
}

function createTargetIconHtml(rover) {
  const code = rover.code ? rover.code.replace('ROVER-', 'R') : ('R' + rover.id);
  return `<div class=\"target-pin\" title=\"Alvo ${code}\">🎯</div>`;
}

function sendNewCoordinates(roverId, lat, lng) {
  const body = {
    rover_id: roverId,
    action: 6, // ACTION_NAVIGATE
    lat: parseFloat(lat.toFixed(6)),
    lng: parseFloat(lng.toFixed(6))
  };

  fetch('/api/control/action', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body)
  })
  .then(r => r.json())
  .then(res => {
    showToast(`📡 R${roverId}: Alvo enviado (${lat.toFixed(5)}, ${lng.toFixed(5)})`);
    const rov = currentRoversData.find(r => r.id === roverId);
    if (rov) {
      rov.target_lat = lat;
      rov.target_lng = lng;
      updateRoverTargetDisplay(rov);
    }
  })
  .catch(() => showToast('⚠️ Erro ao enviar coordenadas', true));
}

function sendTargetCommand(actionCode) {
  const sel = currentRoversData.find(r => r.id === selectedRoverId);
  if (!sel || !sel.target_lat || !sel.target_lng) {
    showToast('⚠️ Arrasta o Rover ou Waypoint no mapa primeiro para definir o alvo!', true);
    return;
  }
  sendNewCoordinates(sel.id, sel.target_lat, sel.target_lng);
}

function sendHoldStation() {
  fetch('/api/control/action', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ rover_id: selectedRoverId, action: 1 })
  }).then(() => showToast('⚓ Rover em Manutenção de Posição (Hold)'));
}

function sendStopRover() {
  fetch('/api/control/action', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ rover_id: selectedRoverId, action: 2 })
  }).then(() => showToast('🛑 Comando de Paragem enviado'));
}

function calcDistanceMeters(lat1, lon1, lat2, lon2) {
  const R = 6371000;
  const dLat = (lat2 - lat1) * Math.PI / 180;
  const dLon = (lon2 - lon1) * Math.PI / 180;
  const a = Math.sin(dLat / 2) * Math.sin(dLat / 2) +
            Math.cos(lat1 * Math.PI / 180) * Math.cos(lat2 * Math.PI / 180) *
            Math.sin(dLon / 2) * Math.sin(dLon / 2);
  return Math.round(R * 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a)));
}

function updateRoverTargetDisplay(rov) {
  if (!map) return;
  const id = rov.id;
  const hasTarget = rov.target_lat && rov.target_lng && (rov.target_lat !== 0 || rov.target_lng !== 0);

  if (hasTarget) {
    const targetLatLng = [rov.target_lat, rov.target_lng];

    if (!targetMarkers[id]) {
      const targetIcon = L.divIcon({
        html: createTargetIconHtml(rov),
        className: 'custom-target-icon',
        iconSize: [28, 28],
        iconAnchor: [14, 14]
      });

      const tMarker = L.marker(targetLatLng, { icon: targetIcon, draggable: true, autoPan: true }).addTo(map);

      tMarker.on('dragstart', () => { isDragging = true; });
      tMarker.on('drag', (e) => {
        const p = e.target.getLatLng();
        document.getElementById('dragHintText').innerText = `Alvo R${id}: ${p.lat.toFixed(5)}, ${p.lng.toFixed(5)}`;
        document.getElementById('dragHint').style.display = 'block';
        if (targetLines[id] && roverMarkers[id]) {
          targetLines[id].setLatLngs([roverMarkers[id].getLatLng(), p]);
        }
      });
      tMarker.on('dragend', (e) => {
        isDragging = false;
        document.getElementById('dragHint').style.display = 'none';
        const p = e.target.getLatLng();
        sendNewCoordinates(id, p.lat, p.lng);
      });

      targetMarkers[id] = tMarker;
    } else {
      targetMarkers[id].setLatLng(targetLatLng);
    }

    if (roverMarkers[id]) {
      const currentLatLng = roverMarkers[id].getLatLng();
      if (!targetLines[id]) {
        targetLines[id] = L.polyline([currentLatLng, targetLatLng], {
          color: '#06b6d4', weight: 2, opacity: 0.8, dashArray: '6, 8'
        }).addTo(map);
      } else {
        targetLines[id].setLatLngs([currentLatLng, targetLatLng]);
      }
    }
  } else {
    if (targetMarkers[id]) { map.removeLayer(targetMarkers[id]); delete targetMarkers[id]; }
    if (targetLines[id]) { map.removeLayer(targetLines[id]); delete targetLines[id]; }
  }
}

// Toast Notifier
let toastTimeout = null;
function showToast(msg, isError = false) {
  const banner = document.getElementById('toastBanner');
  const text = document.getElementById('toastMsg');
  if (!banner || !text) return;

  banner.style.background = isError ? '#7f1d1d' : '#064e3b';
  banner.style.borderColor = isError ? '#ef4444' : '#10b981';
  banner.style.color = isError ? '#fca5a5' : '#a7f3d0';
  text.innerText = msg;
  banner.style.display = 'block';

  if (toastTimeout) clearTimeout(toastTimeout);
  toastTimeout = setTimeout(() => { banner.style.display = 'none'; }, 3500);
}

// --- GESTÃO DE BÓIAS (TAB REGATA) ---
function renderRoversTable(rovers) {
  const tbody = document.getElementById('roversTbody');
  if (!tbody || tbody.contains(document.activeElement)) return;

  let html = '';
  rovers.forEach(r => {
    const isOnline = r.is_online;
    const isSelected = (selectedRoverId === r.id);
    const batCol = (r.battery_pct < 25) ? 'var(--red)' : ((r.battery_pct < 50) ? 'var(--yellow)' : 'var(--green)');
    
    let optHtml = '';
    rolesList.forEach(ro => {
      optHtml += `<option value=\"${ro}\" ${(r.role === ro) ? 'selected' : ''}>${ro}</option>`;
    });

    html += `<tr id=\"rRow_${r.id}\">
      <td><strong>${r.id}</strong></td>
      <td><input type=\"text\" class=\"r-name\" value=\"${r.name || ('Rover ' + r.id)}\" style=\"width:110px;padding:6px;\"></td>
      <td><input type=\"text\" class=\"r-mac\" value=\"${r.mac || r.mac_address || ''}\" style=\"width:130px;font-family:monospace;padding:6px;\"></td>
      <td><select class=\"r-role\" style=\"width:110px;padding:6px;\">${optHtml}</select></td>
      <td>
        <div style=\"display:flex;gap:4px;\">
          <input type=\"number\" step=\"0.000001\" class=\"r-lat\" value=\"${(r.target_lat || 0).toFixed(6)}\" placeholder=\"Lat\" style=\"width:85px;padding:6px;\">
          <input type=\"number\" step=\"0.000001\" class=\"r-lng\" value=\"${(r.target_lng || 0).toFixed(6)}\" placeholder=\"Lng\" style=\"width:85px;padding:6px;\">
        </div>
      </td>
      <td>
        <span class=\"badge\" style=\"background:${isOnline ? '#065f46' : '#7f1d1d'};color:${isOnline ? '#6ee7b7' : '#fca5a5'};font-size:0.7rem;\">
          ${isOnline ? 'ONLINE' : 'OFFLINE'}
        </span>
        ${isOnline ? `<strong style=\"color:${batCol};margin-left:4px;\">${r.battery_pct}%</strong>` : ''}
        ${isOnline && r.anchor_depth_m > 0.05 ? `<div style=\"font-size:0.7rem;color:var(--yellow);margin-top:2px;\"><i class=\"fa-solid fa-anchor\"></i> ${r.anchor_depth_m.toFixed(2)}m</div>` : ''}
      </td>
      <td>
        <div style=\"display:flex;gap:4px;\">
          ${isSelected 
            ? `<button type=\"button\" class=\"btn\" style=\"background:#059669;color:#fff;padding:4px 8px;font-size:0.75rem;\" title=\"Ativo\"><i class=\"fa-solid fa-check\"></i> Ativo</button>` 
            : `<button type=\"button\" onclick=\"changeRover(${r.id})\" class=\"btn btn-primary\" style=\"padding:4px 8px;font-size:0.75rem;\">Selecionar</button>`}
          <button type=\"button\" onclick=\"deleteRow(${r.id})\" class=\"btn btn-danger\" style=\"padding:4px 8px;font-size:0.75rem;\">
            <i class=\"fa-solid fa-trash\"></i>
          </button>
        </div>
      </td>
    </tr>`;
  });

  tbody.innerHTML = html;
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
    optHtml += `<option value=\"${ro}\" ${i === 0 ? 'selected' : ''}>${ro}</option>`;
  });

  tr.innerHTML = `<td><strong>${id}</strong></td>
    <td><input type=\"text\" class=\"r-name\" value=\"WD-Boia-${id}\" style=\"width:110px;padding:6px;\"></td>
    <td><input type=\"text\" class=\"r-mac\" value=\"00:00:00:00:00:0${id}\" style=\"width:130px;font-family:monospace;padding:6px;\"></td>
    <td><select class=\"r-role\" style=\"width:110px;padding:6px;\">${optHtml}</select></td>
    <td>
      <div style=\"display:flex;gap:4px;\">
        <input type=\"number\" step=\"0.000001\" class=\"r-lat\" value=\"0.000000\" style=\"width:85px;padding:6px;\">
        <input type=\"number\" step=\"0.000001\" class=\"r-lng\" value=\"0.000000\" style=\"width:85px;padding:6px;\">
      </div>
    </td>
    <td><span class=\"badge\" style=\"background:#334155;color:#94a3b8;font-size:0.7rem;\">NOVO</span></td>
    <td>
      <button type=\"button\" onclick=\"deleteRow(${id})\" class=\"btn btn-danger\" style=\"padding:4px 8px;font-size:0.75rem;\">
        <i class=\"fa-solid fa-trash\"></i>
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
    showToast('Lista de ' + res.rover_count + ' bóias guardada no MicroSD!');
    loadAllData();
  })
  .catch(err => alert('Erro ao gravar: ' + err));
}

function clearRovers() {
  if (confirm('Tem a certeza que deseja repor a lista de bóias para os valores padrão?')) {
    fetch('/api/mission/clear', { method: 'POST' })
      .then(r => r.json())
      .then(() => loadAllData());
  }
}

// --- ATUALIZAÇÃO CONTÍNUA DE TELEMETRIA ---
function loadAllData(forceMapCenter = false) {
  // 1. Carrega dados de telemetria em tempo real
  fetch('/api/status')
    .then(r => r.json())
    .then(data => {
      currentRoversData = data.rovers || [];
      const sel = data.selected || (currentRoversData.length ? currentRoversData.find(r => r.id === selectedRoverId) || currentRoversData[0] : null);
      if (sel) selectedRoverId = sel.id;

      // Dropdowns e Chips
      let chipHtml = '';
      let optHtml = '';
      currentRoversData.forEach(r => {
        const isSel = (r.id === selectedRoverId);
        const code = r.code ? r.code.replace('ROVER-', 'R') : ('R' + r.id);
        const dotCls = r.is_online ? 'online' : 'offline';
        const p = r.battery_pct !== undefined ? r.battery_pct : 0;
        
        chipHtml += `<span class=\"fleet-chip ${isSel ? 'active' : ''}\" id=\"chip_${r.id}\" onclick=\"changeRover(${r.id})\">
          <i class=\"fa-solid fa-sailboat\"></i> ${code} <span class=\"dot ${dotCls}\"></span> <b>${p}%</b>
        </span>`;

        optHtml += `<option value=\"${r.id}\" ${isSel ? 'selected' : ''}>${code} (${p}%)</option>`;
      });

      if (chipHtml) document.getElementById('fleetChipsBar').innerHTML = chipHtml;
      ['mapRoverSelect', 'targetRoverSelect', 'batRoverSelect'].forEach(selId => {
        const el = document.getElementById(selId);
        if (el && optHtml) el.innerHTML = optHtml;
      });

      // Atualiza HUD do Mapa
      if (sel) {
        const isOnline = !!sel.is_online;
        const fixBadge = document.getElementById('fixStatusBadge');
        if (fixBadge) {
          fixBadge.innerText = isOnline ? '● ONLINE' : '○ OFFLINE';
          fixBadge.style.color = isOnline ? 'var(--green)' : 'var(--red)';
          fixBadge.style.borderColor = isOnline ? 'var(--green)' : 'var(--red)';
        }

        const p = sel.battery_pct || 0;
        const batCol = p < 25 ? 'var(--red)' : (p < 50 ? 'var(--yellow)' : 'var(--green)');
        document.getElementById('batVal').innerText = p + '%';
        document.getElementById('speedVal').innerText = (sel.speed_knots || 0).toFixed(1) + ' kn';
        document.getElementById('headingVal').innerText = (sel.heading_deg || 0).toFixed(0) + '°';
        document.getElementById('rssiVal').innerText = (sel.rssi || 0) + ' dBm';

        // Coordenadas na barra inferior
        document.getElementById('curCoordTxt').innerText = `${(sel.lat || 0).toFixed(6)}, ${(sel.lng || 0).toFixed(6)}`;
        if (sel.target_lat && sel.target_lng && (sel.target_lat !== 0 || sel.target_lng !== 0)) {
          const dist = calcDistanceMeters(sel.lat, sel.lng, sel.target_lat, sel.target_lng);
          document.getElementById('tgtCoordTxt').innerText = `${sel.target_lat.toFixed(5)}, ${sel.target_lng.toFixed(5)} (${dist}m)`;
        } else {
          document.getElementById('tgtCoordTxt').innerText = 'Nenhum alvo definido';
        }

        // Atualiza Comandos
        document.getElementById('curMotor').innerText = sel.motor_status !== undefined ? sel.motor_status : 'idle';
        document.getElementById('curAnchor').innerText = sel.anchor_status !== undefined ? sel.anchor_status : 'retracted';
        document.getElementById('curDepth').innerText = (sel.anchor_depth_m || 0).toFixed(1) + ' m';
        document.getElementById('curRssi').innerText = (sel.rssi || 0) + ' dBm';

        // Atualiza Bateria
        const v = sel.battery_voltage || 0;
        const vc = (v / 4.0);
        document.getElementById('batPct').innerText = p + '%';
        document.getElementById('batPct').style.color = batCol;
        document.getElementById('batBar').style.width = p + '%';
        document.getElementById('batBar').style.background = batCol;
        document.getElementById('vTotVal').innerText = v.toFixed(2) + ' V';
        document.getElementById('vCellVal').innerText = vc.toFixed(2) + ' V/cel';
        document.querySelectorAll('.cell-v').forEach(el => el.innerText = vc.toFixed(2) + ' V');

        const h = document.getElementById('batHealth');
        if (h) {
          h.innerText = p < 20 ? 'CRÍTICO' : (p < 40 ? 'BAIXA' : 'NORMAL');
          h.style.color = batCol;
        }
      }

      // Resumo de Bateria de Toda a Frota
      let batListHtml = '';
      currentRoversData.forEach(r => {
        const bp = r.battery_pct || 0;
        const bCol = bp < 25 ? 'var(--red)' : (bp < 50 ? 'var(--yellow)' : 'var(--green)');
        const code = r.code ? r.code.replace('ROVER-', 'Rover ') : ('Rover ' + r.id);
        batListHtml += `
          <div style=\"display:flex;align-items:center;justify-content:space-between;padding:8px 12px;background:rgba(255,255,255,0.03);border:1px solid var(--card-border);border-radius:8px;\">
            <div style=\"display:flex;align-items:center;gap:8px;\">
              <span class=\"dot ${r.is_online ? 'online' : 'offline'}\"></span>
              <strong>${code}</strong>
              <span style=\"font-size:0.75rem;color:var(--muted);\">(${r.role || 'Boia'})</span>
            </div>
            <div style=\"display:flex;align-items:center;gap:12px;\">
              <span style=\"font-family:monospace;font-size:0.85rem;\">${(r.battery_voltage || 0).toFixed(2)}V</span>
              <strong style=\"color:${bCol};width:42px;text-align:right;\">${bp}%</strong>
              <div class=\"progress-bg\" style=\"width:60px;height:10px;margin:0;\">
                <div class=\"progress-bar\" style=\"width:${bp}%;background:${bCol};\"></div>
              </div>
            </div>
          </div>
        `;
      });
      if (batListHtml) document.getElementById('fleetBatteryList').innerHTML = batListHtml;

      // Marcadores do Mapa
      if (!isDragging && map) {
        let validCoords = [];
        currentRoversData.forEach(r => {
          const isSel = (r.id === selectedRoverId);
          const hasFix = (r.lat !== 0 || r.lng !== 0);
          if (!hasFix) return;

          const latLng = [r.lat, r.lng];
          validCoords.push(latLng);

          const icon = L.divIcon({
            html: createRoverIconHtml(r, isSel),
            className: 'custom-rover-icon',
            iconSize: [38, 38],
            iconAnchor: [19, 19]
          });

          if (!roverMarkers[r.id]) {
            const marker = L.marker(latLng, { icon: icon, draggable: true, autoPan: true, zIndexOffset: isSel ? 1000 : 500 }).addTo(map);

            marker.on('dragstart', () => {
              isDragging = true;
              selectedRoverId = r.id;
              document.getElementById('dragHintText').innerText = `Arrasta ${r.code || ('R' + r.id)} para novo alvo...`;
              document.getElementById('dragHint').style.display = 'block';
            });
            marker.on('drag', (e) => {
              const pos = e.target.getLatLng();
              document.getElementById('dragHintText').innerText = `${r.code || ('R' + r.id)} -> ${pos.lat.toFixed(5)}, ${pos.lng.toFixed(5)}`;
              document.getElementById('tgtCoordTxt').innerText = `${pos.lat.toFixed(5)}, ${pos.lng.toFixed(5)} (Novo)`;
              if (targetLines[r.id]) targetLines[r.id].setLatLngs([[r.lat, r.lng], pos]);
            });
            marker.on('dragend', (e) => {
              isDragging = false;
              document.getElementById('dragHint').style.display = 'none';
              const pos = e.target.getLatLng();
              marker.setLatLng([r.lat, r.lng]);
              sendNewCoordinates(r.id, pos.lat, pos.lng);
            });
            marker.on('click', () => {
              if (selectedRoverId !== r.id) changeRover(r.id);
            });

            roverMarkers[r.id] = marker;
          } else {
            roverMarkers[r.id].setLatLng(latLng);
            roverMarkers[r.id].setIcon(icon);
            roverMarkers[r.id].setZIndexOffset(isSel ? 1000 : 500);
          }

          updateRoverTargetDisplay(r);
        });

        if ((!hasCenteredInitial && validCoords.length > 0) || forceMapCenter) {
          centerMapOnRovers();
          hasCenteredInitial = true;
        }
      }
    })
    .catch(() => {});

  // 2. Carrega dados de missão persistentes (Regata e Estado)
  fetch('/api/mission')
    .then(r => r.json())
    .then(mdata => {
      currentMissionData = mdata;
      if (mdata.operation_state) {
        updateStateButtons(mdata.operation_state);
      }
      if (mdata.race_id && document.activeElement !== document.getElementById('raceIdInput')) {
        document.getElementById('raceIdInput').value = mdata.race_id;
      }
      if (mdata.mission_name && document.activeElement !== document.getElementById('missionNameInput')) {
        document.getElementById('missionNameInput').value = mdata.mission_name;
      }
      if (mdata.rovers && Array.isArray(mdata.rovers)) {
        renderRoversTable(mdata.rovers);
      }
    })
    .catch(() => {});
}

// Inicialização Geral
document.addEventListener('DOMContentLoaded', () => {
  const hash = (location.hash || '').replace('#', '').toLowerCase();
  if (hash === 'cmd' || hash === 'control' || hash === 'comandos') {
    switchMissionTab('cmd');
  } else if (hash === 'bat' || hash === 'battery' || hash === 'bateria') {
    switchMissionTab('bat');
  } else if (hash === 'setup' || hash === 'regata' || hash === 'fleet' || hash === 'boias') {
    switchMissionTab('setup');
  } else {
    switchMissionTab('map');
  }

  loadAllData(true);
  setInterval(() => {
    if (!isDragging) loadAllData();
  }, 1500);
});

window.addEventListener('hashchange', () => {
  const hash = (location.hash || '').replace('#', '').toLowerCase();
  if (hash === 'cmd' || hash === 'control' || hash === 'comandos') switchMissionTab('cmd');
  else if (hash === 'bat' || hash === 'battery' || hash === 'bateria') switchMissionTab('bat');
  else if (hash === 'setup' || hash === 'regata' || hash === 'fleet' || hash === 'boias') switchMissionTab('setup');
  else if (hash === 'map' || hash === 'gps') switchMissionTab('map');
});
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
    cfg_rover = generate_html_page(
        "Configuração do Rover",
        "config",
        CONFIG_CONTENT.replace("<!-- Secção OTA (Dual ESP32-C6) -->", ROVER_LORA_CARD + "\n" + ROVER_CONSOLE_CARD + "\n<!-- Secção OTA (Dual ESP32-C6) -->")
    )
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
    extra_head_mission = """
  <link rel="stylesheet" href="/leaflet/leaflet.css" onerror="this.onerror=null;this.href='https://unpkg.com/leaflet@1.9.4/dist/leaflet.css';">
  <script src="/leaflet/leaflet.js"></script>
  <script>
    if (typeof L === 'undefined') {
      document.write('<link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css">');
      document.write('<script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"><\\/script>');
    }
  </script>
"""
    mission_base = generate_base_html_page("Missão & Frota", "mission", BASE_MISSION_CONTENT, extra_head=extra_head_mission)
    portal_base = generate_base_html_page("Estado da Ligação ao Portal", "portal", BASE_PORTAL_CONTENT)
    config_base = generate_base_html_page("Configuração da Base Station", "config", BASE_CONFIG_CONTENT)
    wifi_base = generate_base_html_page("Configuração WiFi da Base", "wifi", BASE_WIFI_CONTENT)
    sd_base = generate_base_html_page("Explorador MicroSD da Base", "sd", BASE_SD_CONTENT)

    redirect_gps = """<!DOCTYPE html><html lang="pt"><head><meta charset="UTF-8"><meta http-equiv="refresh" content="0; url=/mission#map"><script>location.replace('/mission#map');</script></head><body style="background:#0b1329;color:#fff;font-family:sans-serif;text-align:center;padding:50px;">A redirecionar para Missão (Mapa)...</body></html>"""
    redirect_bat = """<!DOCTYPE html><html lang="pt"><head><meta charset="UTF-8"><meta http-equiv="refresh" content="0; url=/mission#battery"><script>location.replace('/mission#battery');</script></head><body style="background:#0b1329;color:#fff;font-family:sans-serif;text-align:center;padding:50px;">A redirecionar para Missão (Bateria)...</body></html>"""
    redirect_ctrl = """<!DOCTYPE html><html lang="pt"><head><meta charset="UTF-8"><meta http-equiv="refresh" content="0; url=/mission#control"><script>location.replace('/mission#control');</script></head><body style="background:#0b1329;color:#fff;font-family:sans-serif;text-align:center;padding:50px;">A redirecionar para Missão (Comandos)...</body></html>"""

    base_files = [
        ("index.html", mission_base),
        ("mission.html", mission_base),
        ("portal.html", portal_base),
        ("config.html", config_base),
        ("wifi.html", wifi_base),
        ("gps.html", redirect_gps),
        ("battery.html", redirect_bat),
        ("control.html", redirect_ctrl),
        ("sd.html", sd_base)
    ]

    for fname, content in base_files:
        with open(BASE_DIR / fname, "w", encoding="utf-8") as f:
            f.write(content.strip() + "\n")
        # Mantém cópia de compatibilidade em /www/ se for mission.html
        if fname == "mission.html":
            with open(WWW_DIR / fname, "w", encoding="utf-8") as f:
                f.write(content.strip() + "\n")
    print(f"  [+] 9 páginas especializadas geradas em {BASE_DIR}")

    print("\n[OK] Todas as páginas web do Rover e da Base Station foram geradas com sucesso!")

if __name__ == "__main__":
    main()

