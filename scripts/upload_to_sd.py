#!/usr/bin/env python3
"""
upload_to_sd.py
Uploads web assets from data/www to the ESP32-C6 MicroSD card via HTTP.
Supports uploading dedicated files for Rover (/www/rover/) and Base Station (/www/base/).

Usage:
    python3 scripts/upload_to_sd.py [--target rover|base|all] [IP_ADDRESS]
Examples:
    python3 scripts/upload_to_sd.py --target rover 192.168.1.112
    python3 scripts/upload_to_sd.py --target base 192.168.1.120
    python3 scripts/upload_to_sd.py --target all
"""

import sys
import os
import argparse
import subprocess
import json
import urllib.request
from pathlib import Path

import socket

DEFAULT_ROVER_IP = os.environ.get("ROVER_IP", "192.168.1.112")
DEFAULT_BASE_IP  = os.environ.get("BASE_IP", "192.168.1.66")
AP_IP            = "192.168.4.1"
WWW_DIR = Path(__file__).resolve().parent.parent / "data" / "www"

def is_reachable(ip, timeout=0.8):
    try:
        with socket.create_connection((ip, 80), timeout=timeout):
            return True
    except OSError:
        return False

def detect_device_type(ip):
    try:
        url = f"http://{ip}/api/status"
        req = urllib.request.Request(url)
        with urllib.request.urlopen(req, timeout=1.2) as resp:
            data = json.loads(resp.read().decode("utf-8"))
            if "selected" in data or "rover_id" not in data:
                return "base"
            return "rover"
    except Exception:
        return "rover"

def make_sd_dir(ip, directory):
    try:
        url = f"http://{ip}/api/sd/mkdir"
        data = json.dumps({"dir": directory}).encode("utf-8")
        req = urllib.request.Request(url, data=data, headers={"Content-Type": "application/json"})
        with urllib.request.urlopen(req, timeout=4) as response:
            pass
    except Exception:
        pass

def upload_file(ip, local_path, remote_dir):
    print(f"  -> A carregar: {remote_dir}/{local_path.name} ({local_path.stat().st_size} bytes)...", end="", flush=True)
    cmd = [
        "curl", "-s", "-S",
        "-F", f"file=@{local_path}",
        f"http://{ip}/api/sd/upload?dir={remote_dir}"
    ]
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode == 0 and ("sucesso" in res.stdout.lower() or "success" in res.stdout.lower() or res.stdout == ""):
        print(" [OK]")
        return True
    else:
        print(f" [ERRO] {res.stdout.strip()} {res.stderr.strip()}")
        return False

def sync_device(ip, device_type):
    print(f"\n[*] A sincronizar frontend para {device_type.upper()} em http://{ip}/ ...")
    make_sd_dir(ip, "/www")
    make_sd_dir(ip, "/www/rover")
    make_sd_dir(ip, "/www/base")

    total = 0
    success = 0

    # 1. Ficheiros partilhados na raiz (/www/)
    for fname in ["style.css", "app.js"]:
        f = WWW_DIR / fname
        if f.exists():
            total += 1
            if upload_file(ip, f, "/www"):
                success += 1

    # 2. Páginas especializadas
    subfolder = "base" if device_type == "base" else "rover"
    src_dir = WWW_DIR / subfolder
    if src_dir.exists():
        files = [f for f in src_dir.iterdir() if f.is_file() and not f.name.startswith(".")]
        for f in sorted(files):
            total += 1
            if upload_file(ip, f, f"/www/{subfolder}"):
                success += 1
            # Se for rover, espelha também na raiz
            if device_type == "rover":
                upload_file(ip, f, "/www")

    print(f"[OK] {device_type.upper()}: {success}/{total} ficheiros enviados com sucesso.")
    return success

def main():
    parser = argparse.ArgumentParser(description="Upload de páginas web para o Cartão MicroSD do Rover ou da Base Station.")
    parser.add_argument("ip", nargs="?", default=None, help="Endereço IP do dispositivo (ex: 192.168.1.112)")
    parser.add_argument("--target", choices=["auto", "rover", "base", "all"], default="auto", help="Alvo de sincronização")
    args = parser.parse_args()

    if not WWW_DIR.exists():
        print(f"[ERR] Diretório {WWW_DIR} não encontrado.")
        sys.exit(1)

    # Caso IP fornecido explicitamente
    if args.ip:
        target = args.target if args.target != "auto" else detect_device_type(args.ip)
        sync_device(args.ip, target)
        return

    # Modo explícito com IP default
    if args.target == "rover":
        sync_device(DEFAULT_ROVER_IP, "rover")
        return
    elif args.target == "base":
        sync_device(DEFAULT_BASE_IP, "base")
        return
    elif args.target == "all":
        sync_device(DEFAULT_ROVER_IP, "rover")
        sync_device(DEFAULT_BASE_IP, "base")
        return

    # Modo AUTO: detectar quem está online na rede
    print("[*] A detetar dispositivos WindDragons na rede...")
    found = False

    candidates = [
        ("Base Station", DEFAULT_BASE_IP, "base"),
        ("Rover", DEFAULT_ROVER_IP, "rover"),
        ("Ponto de Acesso AP", AP_IP, None)
    ]

    for name, ip, forced_type in candidates:
        if is_reachable(ip):
            dtype = forced_type if forced_type else detect_device_type(ip)
            print(f"  [+] Detetado {name} ({dtype.upper()}) em http://{ip}/")
            sync_device(ip, dtype)
            found = True

    if not found:
        print("[!] Nenhum dispositivo respondeu aos IPs padrão:")
        print(f"    - Base:  {DEFAULT_BASE_IP}")
        print(f"    - Rover: {DEFAULT_ROVER_IP}")
        print(f"    - AP:    {AP_IP}")
        print("\nDica: Podes especificar o IP diretamente:")
        print("    python3 scripts/upload_to_sd.py <IP_DO_DISPOSITIVO>")

if __name__ == "__main__":
    main()

