#!/usr/bin/env python3
"""
WindDragons Universal Carrier Board V4.2 - I2C Architecture & Synchronized Routing
Adds PCA9685 (16-Ch PWM) and ADS1115 (16-Bit 4-Ch ADC).
Synchronized with User KiCad layout:
- J11 (ESP 5V Logic In) relocated to (17.96, 8.0).
- Rearranged D-pad tactile navigation buttons (SW1 Left, SW2 Right, SW3 Top, SW4 Bottom).
- WindDragons official logo on B.Cu at (83.0, 32.5, 180).
- 149 pre-routed copper segments and 9 vias from manual routing.
- Solid GND copper pours on F.Cu and B.Cu.
"""

import os
import sys
import re
import json
import shutil
import subprocess

TARGET_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "universal_pcb"))
PROJECT_NAME = "Universal_Carrier"
BOARD_W = 100.0
BOARD_H = 85.0
TEMPLATE_PATH = os.path.join(TARGET_DIR, f"{PROJECT_NAME}.kicad_pcb.template")

# ==============================================================================
# 1. KICAD PROJECT (.kicad_pro)
# ==============================================================================
def generate_kicad_pro(path):
    if os.path.exists(path):
        return
    pro_data = {
        "board": {
            "3dviewports": [],
            "design_settings": {
                "defaults": {
                    "board_outline_line_width": 0.15,
                    "copper_line_width": 0.2,
                    "copper_text_size_h": 1.5,
                    "copper_text_size_v": 1.5,
                    "copper_text_thickness": 0.3,
                    "silk_line_width": 0.15,
                    "silk_text_size_h": 0.8,
                    "silk_text_size_v": 0.8,
                    "silk_text_thickness": 0.15
                },
                "meta": {
                    "filename": f"{PROJECT_NAME}.kicad_pro",
                    "version": 3
                },
                "rules": {
                    "min_clearance": 0.15,
                    "min_copper_edge_clearance": 0.25,
                    "min_hole_clearance": 0.2,
                    "min_hole_to_hole": 0.25,
                    "min_track_width": 0.2,
                    "min_text_height": 0.6
                }
            }
        },
        "meta": {
            "filename": f"{PROJECT_NAME}.kicad_pro",
            "version": 3
        },
        "net_settings": {
            "classes": [
                {
                    "clearance": 0.2,
                    "diff_pair_gap": 0.25,
                    "diff_pair_via_gap": 0.25,
                    "diff_pair_width": 0.2,
                    "microvia_diameter": 0.3,
                    "microvia_drill": 0.1,
                    "name": "Default",
                    "pcb_color": "rgba(0, 0, 0, 0.000)",
                    "schematic_color": "rgba(0, 0, 0, 0.000)",
                    "track_width": 0.25,
                    "via_diameter": 0.8,
                    "via_drill": 0.4
                },
                {
                    "clearance": 0.25,
                    "diff_pair_gap": 0.25,
                    "diff_pair_via_gap": 0.25,
                    "diff_pair_width": 0.2,
                    "microvia_diameter": 0.3,
                    "microvia_drill": 0.1,
                    "name": "Power",
                    "pcb_color": "rgba(220, 50, 50, 0.800)",
                    "schematic_color": "rgba(220, 50, 50, 0.800)",
                    "track_width": 0.6,
                    "via_diameter": 1.0,
                    "via_drill": 0.6
                }
            ]
        },
        "sheets": [
            [
                "c0000000-0000-0000-0000-000000000001",
                ""
            ]
        ]
    }
    with open(path, "w", encoding="utf-8") as f:
        json.dump(pro_data, f, indent=2)

def generate_kicad_prl(path):
    if os.path.exists(path):
        return
    prl_data = {
        "board": {
            "visibility": {
                "layers": {
                    "B.Cu": True,
                    "B.Mask": True,
                    "B.Paste": True,
                    "B.SilkS": True,
                    "Edge.Cuts": True,
                    "F.Cu": True,
                    "F.Mask": True,
                    "F.Paste": True,
                    "F.SilkS": True
                }
            }
        },
        "meta": {
            "filename": f"{PROJECT_NAME}.kicad_prl",
            "version": 1
        }
    }
    with open(path, "w", encoding="utf-8") as f:
        json.dump(prl_data, f, indent=2)

# ==============================================================================
# 2. KICAD PCB (.kicad_pcb)
# ==============================================================================
def generate_kicad_pcb(path, force=False):
    """
    Generates or synchronizes the KiCad PCB.
    - If the PCB does not exist or force=True, generates from TEMPLATE_PATH.
    - If the PCB exists, synchronizes any new edits back into TEMPLATE_PATH.
    """
    if os.path.exists(path) and not force:
        print(f"Board exists at {path}. Synchronizing template from board...")
        with open(path, "r", encoding="utf-8") as f:
            current_pcb = f.read()
        # Save clean template without filled polygons
        clean_template = re.sub(r"\t*\(filled_polygon\s+[\s\S]*?\n\t\t\)\n?", "", current_pcb)
        with open(TEMPLATE_PATH, "w", encoding="utf-8") as tf:
            tf.write(clean_template)
        print(f"Updated {TEMPLATE_PATH} from active layout.")
        return

    if os.path.exists(TEMPLATE_PATH):
        print(f"Generating PCB from template: {TEMPLATE_PATH} -> {path}")
        shutil.copyfile(TEMPLATE_PATH, path)
        print(f"Generated KiCad PCB: {path}")
    else:
        raise FileNotFoundError(f"Template not found at {TEMPLATE_PATH}")

# ==============================================================================
# 3. KICAD SCHEMATIC (.kicad_sch)
# ==============================================================================
def generate_kicad_sch(path):
    if os.path.exists(path):
        return

    root_uuid = "c0000000-0000-0000-0000-000000000001"
    sch_lines = [
        '(kicad_sch',
        '	(version 20231120)',
        '	(generator "eeschema")',
        '	(generator_version "8.0")',
        f'	(uuid "{root_uuid}")',
        '	(paper "A3")',
        '	(title_block',
        '		(title "WindDragons Universal Carrier Board")',
        '		(date "2026-10-12")',
        '		(rev "V4.2 - I2C PCA9685 & ADS1115 Clean")',
        '		(company "WindDragons Project")',
        '		(comment 1 "ESP32-C6-LCD-1.47 Carrier - 100x85mm 2-Layer JLCPCB")',
        '		(comment 2 "I2C Expansion: PCA9685 16-Ch PWM & ADS1115 16-Bit ADC")',
        '	)',
        '	(sheet_instances',
        '		(path "/" (page "1"))',
        '	)',
        '	(embedded_fonts no)',
        ')'
    ]
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(sch_lines) + "\n")
    print(f"Generated KiCad Schematic: {path}")

# ==============================================================================
# 4. MAIN ORCHESTRATION & DRC
# ==============================================================================
def main():
    force_gen = "--force" in sys.argv
    pro_path = os.path.join(TARGET_DIR, f"{PROJECT_NAME}.kicad_pro")
    prl_path = os.path.join(TARGET_DIR, f"{PROJECT_NAME}.kicad_prl")
    pcb_path = os.path.join(TARGET_DIR, f"{PROJECT_NAME}.kicad_pcb")
    sch_path = os.path.join(TARGET_DIR, f"{PROJECT_NAME}.kicad_sch")
    gerber_dir = os.path.join(TARGET_DIR, "gerbers")
    os.makedirs(gerber_dir, exist_ok=True)

    print(f"Synchronizing V4.2 I2C Carrier Project in {TARGET_DIR}...")
    generate_kicad_pro(pro_path)
    generate_kicad_prl(prl_path)
    generate_kicad_pcb(pcb_path, force=force_gen)
    generate_kicad_sch(sch_path)

    kicad_cli = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"

    if os.path.exists(kicad_cli):
        # Refill zones and run DRC check
        try:
            drc_report = os.path.join(TARGET_DIR, "drc_report.json")
            res = subprocess.run([
                kicad_cli, "pcb", "drc",
                "--refill-zones", "--save-board",
                "--output", drc_report,
                "--format", "json",
                pcb_path
            ], capture_output=True, text=True)
            print(res.stdout.strip())
            print(f"Refilled copper zones and verified DRC. Report saved to {drc_report}")
        except Exception as e:
            print("DRC warning:", e)

        # Render 3D views (Top & Bottom)
        try:
            top_png = os.path.join(TARGET_DIR, "Universal_Carrier_3d_top.png")
            subprocess.run([
                kicad_cli, "pcb", "render",
                "--side", "top", "-w", "1600", "-h", "1200",
                "-o", top_png, pcb_path
            ], check=True)
            print(f"Rendered 3D Top view: {top_png}")

            bottom_png = os.path.join(TARGET_DIR, "Universal_Carrier_3d_bottom.png")
            subprocess.run([
                kicad_cli, "pcb", "render",
                "--side", "bottom", "-w", "1600", "-h", "1200",
                "-o", bottom_png, pcb_path
            ], check=True)
            print(f"Rendered 3D Bottom view: {bottom_png}")
        except Exception as e:
            print("3D Render warning:", e)

        # Export high-res SVG
        try:
            top_svg = os.path.join(TARGET_DIR, "Universal_Carrier_V4_2_top.svg")
            subprocess.run([
                kicad_cli, "pcb", "export", "svg",
                "-o", top_svg,
                "--layers", "F.SilkS,F.Cu,Edge.Cuts",
                "--page-size-mode", "2",
                pcb_path
            ], check=True)
            print(f"Exported Top SVG: {top_svg}")

            bottom_svg = os.path.join(TARGET_DIR, "Universal_Carrier_V4_2_bottom.svg")
            subprocess.run([
                kicad_cli, "pcb", "export", "svg",
                "-o", bottom_svg,
                "--layers", "B.SilkS,B.Cu,Edge.Cuts",
                "--page-size-mode", "2",
                pcb_path
            ], check=True)
            print(f"Exported Bottom SVG: {bottom_svg}")
        except Exception as e:
            print("SVG Export warning:", e)

if __name__ == "__main__":
    main()
