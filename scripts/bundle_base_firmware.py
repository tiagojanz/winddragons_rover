#!/usr/bin/env python3
"""
WindDragons Base Station - Firmware Bundler
Packages dual ESP32-C6 binaries (base_c6 and base_lafvin) and web assets into a single distribution ZIP.
Output: dist/base_firmware.zip
"""

import os
import sys
import zipfile
import json
import time
import subprocess

PROJECT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
BUILD_DIR = os.path.join(PROJECT_DIR, ".pio", "build")
DIST_DIR = os.path.join(PROJECT_DIR, "dist")

C6_BIN = os.path.join(BUILD_DIR, "base_c6", "firmware.bin")
LAFVIN_BIN = os.path.join(BUILD_DIR, "base_lafvin", "firmware.bin")
OUTPUT_ZIP = os.path.join(DIST_DIR, "base_firmware.zip")

DATA_DIR = os.path.join(PROJECT_DIR, "data")
WWW_DIR = os.path.join(DATA_DIR, "www")
SYSTEM_DIR = os.path.join(DATA_DIR, "system")

def ensure_web_assets():
    script = os.path.join(PROJECT_DIR, "scripts", "build_web_assets.py")
    if os.path.isfile(script):
        print("[BUNDLE BASE] A gerar/atualizar ficheiros web frontend (build_web_assets.py)...")
        res = subprocess.run([sys.executable, script], cwd=PROJECT_DIR, capture_output=True, text=True)
        if res.returncode != 0:
            print(f"[BUNDLE BASE] AVISO ao gerar ficheiros web: {res.stderr}")

def ensure_binaries():
    needs_build = []
    if not os.path.isfile(C6_BIN):
        needs_build.append("base_c6")
    if not os.path.isfile(LAFVIN_BIN):
        needs_build.append("base_lafvin")
    
    if needs_build:
        print(f"[BUNDLE BASE] A compilar ambientes em falta: {', '.join(needs_build)}...")
        cmd = ["pio", "run"]
        for env in needs_build:
            cmd.extend(["-e", env])
        res = subprocess.run(cmd, cwd=PROJECT_DIR)
        if res.returncode != 0:
            print("[BUNDLE BASE] ERRO: Falha na compilação do firmware.")
            sys.exit(1)

def get_base_version():
    ver_file = os.path.join(PROJECT_DIR, "include", "base_version.h")
    if os.path.isfile(ver_file):
        with open(ver_file, "r", encoding="utf-8") as f:
            for line in f:
                if "#define BASE_FIRMWARE_VERSION_STR" in line or "#define BASE_VERSION_STR" in line:
                    parts = line.split('"')
                    if len(parts) >= 2:
                        return parts[1]
    return "v2.1.0"

def format_size(bytes_num):
    for unit in ['B', 'KB', 'MB']:
        if bytes_num < 1024.0:
            return f"{bytes_num:.1f} {unit}"
        bytes_num /= 1024.0
    return f"{bytes_num:.1f} GB"

def main():
    version_str = get_base_version()
    print("\n=======================================================")
    print(f"  WINDDRAGONS BASE STATION - FULL BUNDLER (FW + WEB {version_str})")
    print("=======================================================")
    
    ensure_web_assets()
    ensure_binaries()

    if not os.path.isfile(C6_BIN) or not os.path.isfile(LAFVIN_BIN):
        print("[BUNDLE BASE] ERRO: Binários não encontrados após compilação.")
        sys.exit(1)

    os.makedirs(DIST_DIR, exist_ok=True)

    c6_size = os.path.getsize(C6_BIN)
    lafvin_size = os.path.getsize(LAFVIN_BIN)

    manifest = {
        "project": "WindDragons Base Station",
        "version": version_str,
        "created_at": time.strftime("%Y-%m-%d %H:%M:%S"),
        "timestamp": int(time.time()),
        "firmwares": {
            "c6": {
                "file": "c6.bin",
                "version": version_str,
                "role": "Coprocessor (LoRa SX1278 433MHz via FSPI dedicado)",
                "size_bytes": c6_size
            },
            "lafvin": {
                "file": "lafvin.bin",
                "version": version_str,
                "role": "Master (ST7789 Display 1.47\", MicroSD TF, WebServer, WiFi)",
                "size_bytes": lafvin_size
            }
        },
        "web_assets": {
            "version": version_str,
            "files_count": 0,
            "total_bytes": 0
        }
    }

    print(f"[BUNDLE BASE] Versão Pacote    : {version_str}")
    print(f"[BUNDLE BASE] Coprocessador C6 : {C6_BIN} ({format_size(c6_size)})")
    print(f"[BUNDLE BASE] Master Lafvin    : {LAFVIN_BIN} ({format_size(lafvin_size)})")

    web_file_count = 0
    web_total_bytes = 0

    with zipfile.ZipFile(OUTPUT_ZIP, "w", zipfile.ZIP_DEFLATED) as z:
        # 1. Firmware Binaries (nomes padrão c6.bin e lafvin.bin + aliases)
        z.write(C6_BIN, arcname="c6.bin")
        z.write(C6_BIN, arcname="base_c6.bin")
        z.write(LAFVIN_BIN, arcname="lafvin.bin")
        z.write(LAFVIN_BIN, arcname="base_lafvin.bin")

        # 2. Web Assets (/www/)
        if os.path.isdir(WWW_DIR):
            for root, dirs, files in os.walk(WWW_DIR):
                for f in files:
                    if f.startswith("."):
                        continue
                    full_path = os.path.join(root, f)
                    rel_to_www = os.path.relpath(full_path, WWW_DIR)
                    arcname = "www/" + rel_to_www.replace("\\", "/")
                    z.write(full_path, arcname=arcname)
                    web_file_count += 1
                    web_total_bytes += os.path.getsize(full_path)

        # 3. System Assets (/system/)
        if os.path.isdir(SYSTEM_DIR):
            for root, dirs, files in os.walk(SYSTEM_DIR):
                for f in files:
                    if f.startswith("."):
                        continue
                    full_path = os.path.join(root, f)
                    rel_to_sys = os.path.relpath(full_path, SYSTEM_DIR)
                    arcname = "system/" + rel_to_sys.replace("\\", "/")
                    z.write(full_path, arcname=arcname)
                    web_file_count += 1
                    web_total_bytes += os.path.getsize(full_path)

        manifest["web_assets"]["files_count"] = web_file_count
        manifest["web_assets"]["total_bytes"] = web_total_bytes

        # 4. Manifest
        z.writestr("manifest.json", json.dumps(manifest, indent=2))

    zip_size = os.path.getsize(OUTPUT_ZIP)
    print(f"[BUNDLE BASE] Ficheiros Web    : {web_file_count} ficheiros ({format_size(web_total_bytes)})")
    print("-------------------------------------------------------")
    print(f"[BUNDLE BASE] ★ Pacote completo Base OTA gerado com sucesso!")
    print(f"[BUNDLE BASE] Destino  : {OUTPUT_ZIP}")
    print(f"[BUNDLE BASE] Tamanho  : {format_size(zip_size)} (comprimido)")
    print("=======================================================\n")

if __name__ == "__main__":
    main()
