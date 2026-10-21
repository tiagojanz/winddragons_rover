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
import urllib.parse
from pathlib import Path

import socket

DEFAULT_ROVER_IP = os.environ.get("ROVER_IP", "192.168.1.112")
DEFAULT_BASE_IP  = os.environ.get("BASE_IP", "192.168.1.66")
AP_IP            = "192.168.4.1"
DATA_DIR         = Path(__file__).resolve().parent.parent / "data"
WWW_DIR          = DATA_DIR / "www"
SYSTEM_DIR       = DATA_DIR / "system"

def is_reachable(ip, timeout=2.5):
    try:
        with socket.create_connection((ip, 80), timeout=timeout):
            return True
    except OSError:
        return False

def detect_device_type(ip):
    try:
        url = f"http://{ip}/api/status"
        req = urllib.request.Request(url)
        with urllib.request.urlopen(req, timeout=3.0) as resp:
            data = json.loads(resp.read().decode("utf-8"))
            if "station_id" in data or "station_name" in data or "operation_state" in data:
                return "base"
            if "rover_id" in data or "actuators" in data:
                return "rover"
            if "rovers" in data or "selected" in data:
                return "base"
            return "rover"
    except Exception:
        return "rover"

def make_sd_dir(ip, directory):
    try:
        url = f"http://{ip}/api/sd/mkdir"
        data = json.dumps({"dir": directory, "path": directory}).encode("utf-8")
        req = urllib.request.Request(url, data=data, headers={"Content-Type": "application/json"})
        with urllib.request.urlopen(req, timeout=4) as response:
            pass
    except Exception:
        pass

def upload_file(ip, local_path, remote_dir):
    fsize = local_path.stat().st_size
    print(f"  -> A carregar: {remote_dir}/{local_path.name} ({fsize} bytes)...", end="", flush=True)
    # Timeout dinâmico: pelo menos 30s + 1s por cada 15KB para transferências grandes (ex: fontes woff2)
    timeout_sec = max(30, int(30 + (fsize / 15000)))
    encoded_dir = urllib.parse.quote(remote_dir)
    cmd = [
        "curl", "-s", "-S",
        "--connect-timeout", "8",
        "--retry", "2",
        "--retry-delay", "1",
        "--max-time", str(timeout_sec),
        "-F", f"file=@{local_path}",
        f"http://{ip}/api/sd/upload?dir={encoded_dir}&ajax=1"
    ]
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode == 0 and ("true" in res.stdout.lower() or "sucesso" in res.stdout.lower() or "success" in res.stdout.lower()):
        print(" [OK]")
        return True
    else:
        print(f" [ERRO] {res.stdout.strip()} {res.stderr.strip()}")
        return False

def sync_device(ip, device_type):
    print(f"\n[*] A testar ligação a http://{ip}/ ...")
    if not is_reachable(ip, timeout=1.5):
        print(f"[!] Não foi possível contactar o dispositivo em http://{ip}/")
        print("    - Confirma se o ESP32 está ligado e conectado à rede.")
        print("    - Confirma se o endereço IP corresponde ao mostrado no ecrã OLED.")
        return 0

    print(f"[*] A sincronizar frontend e ficheiros de sistema para {device_type.upper()} em http://{ip}/ ...")
    make_sd_dir(ip, "/www")
    make_sd_dir(ip, "/www/rover")
    make_sd_dir(ip, "/www/base")
    make_sd_dir(ip, "/www/leaflet")
    make_sd_dir(ip, "/system")
    make_sd_dir(ip, "/system/fonts")

    total = 0
    success = 0

    # 1. Ficheiros de sistema (Fontes Font Awesome, etc.)
    if SYSTEM_DIR.exists():
        for root, dirs, files in os.walk(SYSTEM_DIR):
            rel_dir = Path(root).relative_to(DATA_DIR)
            remote_dir = "/" + str(rel_dir).replace("\\", "/")
            make_sd_dir(ip, remote_dir)
            for fname in sorted(files):
                if fname.startswith("."):
                    continue
                fpath = Path(root) / fname
                total += 1
                if upload_file(ip, fpath, remote_dir):
                    success += 1

    # 2. Ficheiros partilhados na raiz (/www/) e bibliotecas (/www/leaflet/)
    for fname in ["style.css", "app.js"]:
        f = WWW_DIR / fname
        if f.exists():
            total += 1
            if upload_file(ip, f, "/www"):
                success += 1

    leaflet_dir = WWW_DIR / "leaflet"
    if leaflet_dir.exists():
        for f in sorted(leaflet_dir.iterdir()):
            if f.is_file() and not f.name.startswith("."):
                total += 1
                if upload_file(ip, f, "/www/leaflet"):
                    success += 1

    assets_dir = WWW_DIR / "assets"
    if assets_dir.exists():
        make_sd_dir(ip, "/www/assets")
        for f in sorted(assets_dir.iterdir()):
            if f.is_file() and not f.name.startswith(".") and "original" not in f.name:
                total += 1
                if upload_file(ip, f, "/www/assets"):
                    success += 1

    # 3. Páginas especializadas
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

LAST_IP_FILE = Path(__file__).resolve().parent.parent / ".last_sd_ip"

def is_valid_ip_format(val):
    if not val:
        return False
    parts = val.split(".")
    return len(parts) == 4 and all(p.isdigit() and 0 <= int(p) <= 255 for p in parts)

def get_last_ip():
    if LAST_IP_FILE.exists():
        try:
            val = LAST_IP_FILE.read_text(encoding="utf-8").strip()
            if is_valid_ip_format(val):
                return val
        except Exception:
            return ""
    return ""

def save_last_ip(ip):
    if not is_valid_ip_format(ip):
        return
    try:
        LAST_IP_FILE.write_text(ip.strip(), encoding="utf-8")
    except Exception:
        pass

def prompt_for_ip():
    last_ip = get_last_ip()
    default_ip = last_ip if last_ip else DEFAULT_BASE_IP

    print("\n" + "=" * 58)
    print("  🛰️  WindDragons - Upload para o Cartão MicroSD (SDCard)")
    print("=" * 58)
    print(f"  [1] Base Station (padrão: {DEFAULT_BASE_IP})")
    print(f"  [2] Boia Rover   (padrão: {DEFAULT_ROVER_IP})")
    print(f"  [3] Ponto Acesso (AP: {AP_IP})")
    if last_ip and last_ip not in [DEFAULT_BASE_IP, DEFAULT_ROVER_IP, AP_IP]:
        print(f"  [4] Último IP usado ({last_ip})")
    print(f"  [a] Procurar automaticamente na rede local")
    print("-" * 58)

    prompt = f"Introduz o IP do dispositivo [Enter = {default_ip}]: "
    try:
        choice = input(prompt).strip()
    except (EOFError, KeyboardInterrupt):
        print("\n[!] Operação cancelada.")
        sys.exit(0)

    if not choice:
        return default_ip
    elif choice == "1":
        return DEFAULT_BASE_IP
    elif choice == "2":
        return DEFAULT_ROVER_IP
    elif choice == "3":
        return AP_IP
    elif choice == "4" and last_ip:
        return last_ip
    elif choice.lower() in ["a", "auto", "scan"]:
        return "auto"
    else:
        return choice

def main():
    parser = argparse.ArgumentParser(description="Upload de páginas web para o Cartão MicroSD do Rover ou da Base Station.")
    parser.add_argument("ip", nargs="?", default=None, help="Endereço IP do dispositivo (ex: 192.168.1.112)")
    parser.add_argument("--target", choices=["auto", "rover", "base", "all"], default="auto", help="Alvo de sincronização")
    args = parser.parse_args()

    if not WWW_DIR.exists():
        print(f"[ERR] Diretório {WWW_DIR} não encontrado.")
        sys.exit(1)

    ip = args.ip

    # Se não foi fornecido IP e não há alvo forçado, perguntar ao utilizador
    if not ip and args.target == "auto":
        ip = prompt_for_ip()

    # Caso IP fornecido explicitamente ou escolhido no prompt
    if ip and ip != "auto":
        save_last_ip(ip)
        target = args.target if args.target != "auto" else detect_device_type(ip)
        sync_device(ip, target)
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

    # Modo AUTO: detetar quem está online na rede
    print("[*] A detetar dispositivos WindDragons na rede...")
    found = False

    candidates = [
        ("Base Station", DEFAULT_BASE_IP, "base"),
        ("Rover", DEFAULT_ROVER_IP, "rover"),
        ("Ponto de Acesso AP", AP_IP, None)
    ]

    for name, candidate_ip, forced_type in candidates:
        if is_reachable(candidate_ip):
            dtype = forced_type if forced_type else detect_device_type(candidate_ip)
            print(f"  [+] Detetado {name} ({dtype.upper()}) em http://{candidate_ip}/")
            save_last_ip(candidate_ip)
            sync_device(candidate_ip, dtype)
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


