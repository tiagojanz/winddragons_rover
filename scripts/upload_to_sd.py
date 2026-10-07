#!/usr/bin/env python3
"""
upload_to_sd.py
Uploads all files from data/www to the ESP32-C6 MicroSD card via HTTP.
"""

import sys
import os
import subprocess
from pathlib import Path

ROVER_IP = os.environ.get("ROVER_IP", "192.168.1.112")
WWW_DIR = Path(__file__).resolve().parent.parent / "data" / "www"

def main():
    if not WWW_DIR.exists():
        print(f"[ERR] Diretório {WWW_DIR} não encontrado.")
        sys.exit(1)

    files = [f for f in WWW_DIR.iterdir() if f.is_file() and not f.name.startswith(".")]
    print(f"[*] A iniciar sincronização de {len(files)} ficheiros para http://{ROVER_IP}/www/ ...\n")

    success_count = 0
    for f in sorted(files):
        print(f"  -> A carregar: {f.name} ({f.stat().st_size} bytes)...", end="", flush=True)
        cmd = [
            "curl", "-s", "-S",
            "-F", f"file=@{f}",
            f"http://{ROVER_IP}/api/sd/upload?dir=/www"
        ]
        res = subprocess.run(cmd, capture_output=True, text=True)
        if res.returncode == 0 and ("sucesso" in res.stdout.lower() or "success" in res.stdout.lower() or res.stdout == ""):
            print(" [OK]")
            success_count += 1
        else:
            print(f" [ERRO] {res.stdout.strip()} {res.stderr.strip()}")

    print(f"\n[OK] Concluído: {success_count}/{len(files)} ficheiros enviados com sucesso para o MicroSD!")

if __name__ == "__main__":
    main()
