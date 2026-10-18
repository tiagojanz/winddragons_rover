#!/usr/bin/env python3
"""
WindDragons Rover - Firmware Bundler
Packages dual ESP32-C6 binaries (rover_c6 and rover_lafvin) into a single distribution ZIP.
Output: dist/rover_firmware.zip
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

C6_BIN = os.path.join(BUILD_DIR, "rover_c6", "firmware.bin")
LAFVIN_BIN = os.path.join(BUILD_DIR, "rover_lafvin", "firmware.bin")
OUTPUT_ZIP = os.path.join(DIST_DIR, "rover_firmware.zip")

def ensure_binaries():
    needs_build = []
    if not os.path.isfile(C6_BIN):
        needs_build.append("rover_c6")
    if not os.path.isfile(LAFVIN_BIN):
        needs_build.append("rover_lafvin")
    
    if needs_build:
        print(f"[BUNDLE] A compilar ambientes em falta: {', '.join(needs_build)}...")
        cmd = ["pio", "run"]
        for env in needs_build:
            cmd.extend(["-e", env])
        res = subprocess.run(cmd, cwd=PROJECT_DIR)
        if res.returncode != 0:
            print("[BUNDLE] ERRO: Falha na compilação do firmware.")
            sys.exit(1)

def format_size(bytes_num):
    for unit in ['B', 'KB', 'MB']:
        if bytes_num < 1024.0:
            return f"{bytes_num:.1f} {unit}"
        bytes_num /= 1024.0
    return f"{bytes_num:.1f} GB"

def main():
    print("\n=======================================================")
    print("  WINDDRAGONS ROVER - DUAL FIRMWARE BUNDLER (OTA)")
    print("=======================================================")
    
    ensure_binaries()

    if not os.path.isfile(C6_BIN) or not os.path.isfile(LAFVIN_BIN):
        print("[BUNDLE] ERRO: Binários não encontrados após compilação.")
        sys.exit(1)

    os.makedirs(DIST_DIR, exist_ok=True)

    c6_size = os.path.getsize(C6_BIN)
    lafvin_size = os.path.getsize(LAFVIN_BIN)

    manifest = {
        "project": "WindDragons Rover",
        "created_at": time.strftime("%Y-%m-%d %H:%M:%S"),
        "timestamp": int(time.time()),
        "firmwares": {
            "c6": {
                "file": "c6.bin",
                "role": "Coprocessor (LoRa, Actuators, GPS, Power)",
                "size_bytes": c6_size
            },
            "lafvin": {
                "file": "lafvin.bin",
                "role": "Master (LCD ST7789, MicroSD, WebServer, WiFi)",
                "size_bytes": lafvin_size
            }
        }
    }

    print(f"[BUNDLE] Coprocessador C6 : {C6_BIN} ({format_size(c6_size)})")
    print(f"[BUNDLE] Master Lafvin    : {LAFVIN_BIN} ({format_size(lafvin_size)})")

    with zipfile.ZipFile(OUTPUT_ZIP, "w", zipfile.ZIP_DEFLATED) as z:
        z.write(C6_BIN, arcname="c6.bin")
        z.write(LAFVIN_BIN, arcname="lafvin.bin")
        z.writestr("manifest.json", json.dumps(manifest, indent=2))

    zip_size = os.path.getsize(OUTPUT_ZIP)
    print("-------------------------------------------------------")
    print(f"[BUNDLE] ★ Pacote OTA gerado com sucesso!")
    print(f"[BUNDLE] Destino  : {OUTPUT_ZIP}")
    print(f"[BUNDLE] Tamanho  : {format_size(zip_size)} (comprimido)")
    print("=======================================================\n")

if __name__ == "__main__":
    main()
