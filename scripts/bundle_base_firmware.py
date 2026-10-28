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

def ensure_binaries(force_rebuild=True, rollback_fn=None):
    needs_build = []
    if force_rebuild or not os.path.isfile(C6_BIN):
        needs_build.append("base_c6")
    if force_rebuild or not os.path.isfile(LAFVIN_BIN):
        needs_build.append("base_lafvin")
    
    if needs_build:
        print(f"[BUNDLE BASE] A compilar ambientes ({', '.join(needs_build)}) com a nova versão...")
        cmd = ["pio", "run"]
        for env in needs_build:
            cmd.extend(["-e", env])
        try:
            res = subprocess.run(cmd, cwd=PROJECT_DIR)
            if res.returncode != 0:
                raise RuntimeError(f"Exit code {res.returncode}")
        except Exception as e:
            print(f"[BUNDLE BASE] ERRO: Falha na compilação do firmware ({e}).")
            if rollback_fn:
                rollback_fn()
            sys.exit(1)

def get_base_version():
    ver_file = os.path.join(PROJECT_DIR, "include", "base_version.h")
    if os.path.isfile(ver_file):
        with open(ver_file, "r", encoding="utf-8") as f:
            for line in f:
                if "#define BASE_FIRMWARE_VERSION_STR" in line or "#define BASE_RAW_VERSION_STR" in line:
                    parts = line.split('"')
                    if len(parts) >= 2:
                        v = parts[1].lstrip("v")
                        return f"v{v}"
    return "v2.1.0"

def write_base_version(major, minor, patch):
    ver_raw = f"{major}.{minor}.{patch}"
    ver_str = f"v{ver_raw}"
    ver_file = os.path.join(PROJECT_DIR, "include", "base_version.h")
    content = f"""#pragma once

// =========================================================================
// WINDDRAGONS BASE STATION - DEFINIÇÕES DE VERSÃO DE FIRMWARE
// =========================================================================

#define BASE_VERSION_MAJOR {major}
#define BASE_VERSION_MINOR {minor}
#define BASE_VERSION_PATCH {patch}

#define BASE_RAW_VERSION_STR      "{ver_raw}"
#define BASE_FIRMWARE_VERSION_STR "{ver_str}"
#define BASE_LAFVIN_VERSION       "{ver_str}"
#define BASE_C6_VERSION           "{ver_str}"
#define BASE_BUILD_DATE           __DATE__ " " __TIME__

// Aliases para compatibilidade
#define BASE_VERSION_STR          BASE_FIRMWARE_VERSION_STR
"""
    with open(ver_file, "w", encoding="utf-8") as f:
        f.write(content)
    return ver_str

def bump_base_version(bump_type="patch", explicit_ver=None):
    old_ver = get_base_version().lstrip("v")
    parts = [int(x) for x in old_ver.split(".") if x.isdigit()]
    while len(parts) < 3:
        parts.append(0)
    major, minor, patch = parts[0], parts[1], parts[2]

    if explicit_ver:
        eparts = [int(x) for x in explicit_ver.lstrip("v").split(".") if x.isdigit()]
        while len(eparts) < 3:
            eparts.append(0)
        major, minor, patch = eparts[0], eparts[1], eparts[2]
    elif bump_type == "major":
        major += 1
        minor = 0
        patch = 0
    elif bump_type == "minor":
        minor += 1
        patch = 0
    elif bump_type == "patch":
        patch += 1

    new_ver = write_base_version(major, minor, patch)
    print(f"[BUNDLE BASE] Bump automático de versão Base: v{old_ver} -> {new_ver}")
    return new_ver

def format_size(bytes_num):
    for unit in ['B', 'KB', 'MB']:
        if bytes_num < 1024.0:
            return f"{bytes_num:.1f} {unit}"
        bytes_num /= 1024.0
    return f"{bytes_num:.1f} GB"

def main():
    bump_type = "patch"
    explicit_ver = None
    do_bump = True
    force_rebuild = True

    args = sys.argv[1:]
    i = 0
    while i < len(args):
        a = args[i]
        if a == "--no-bump":
            do_bump = False
        elif a == "--skip-build":
            force_rebuild = False
        elif a == "--major":
            bump_type = "major"
        elif a == "--minor":
            bump_type = "minor"
        elif a == "--patch":
            bump_type = "patch"
        elif a == "--set" and i + 1 < len(args):
            explicit_ver = args[i + 1]
            i += 1
        i += 1

    prev_ver = get_base_version().lstrip("v")
    prev_parts = [int(x) for x in prev_ver.split(".") if x.isdigit()]
    while len(prev_parts) < 3:
        prev_parts.append(0)

    def rollback_version():
        if do_bump:
            write_base_version(prev_parts[0], prev_parts[1], prev_parts[2])
            ensure_web_assets()
            print(f"[BUNDLE BASE] Versão revertida para v{prev_ver} devido a falha na compilação.")

    if do_bump:
        version_str = bump_base_version(bump_type=bump_type, explicit_ver=explicit_ver)
    else:
        version_str = f"v{prev_ver}"

    print("\n=======================================================")
    print(f"  WINDDRAGONS BASE STATION - FULL BUNDLER (FW + WEB {version_str})")
    print("=======================================================")
    
    ensure_web_assets()
    ensure_binaries(force_rebuild=force_rebuild, rollback_fn=rollback_version)

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
                    if f.startswith(".") or "original" in f:
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
