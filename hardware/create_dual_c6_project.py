#!/usr/bin/env python3
"""
WindDragons Dual ESP32-C6 Carrier Board - KiCad 10 Native Generator
Dimensions: 100.0 x 100.0 mm (2-Layer JLCPCB Standard)

FRONT (F.Cu / F.SilkS) - USER INTERFACE PANEL:
    * ESP32-C6 LCD 1.47" (LAFVIN Master) with EXACT footprint from Universal PCB:
        - J1 at (48.0, 13.5)
        - J2 at (48.0, 31.28)
        - Board outline: (34.5, 10.0) to (82.5, 35.0)
        - Active ST7789 1.47" LCD: (47.0, 15.5) to (69.0, 29.5)
        - USB-C connector on right: (81.5, 20.0) to (85.5, 26.0)
    * PS5 3D Analog Joystick (JOY1: Alps RKJXV1224005) at (79.25, 49.75)
    * 4x Alps SKRKAEE020 SMD Tactile Buttons at left (X=10.13..22.73, Y=45.22..57.22)
    * WindDragons Vector Logo

BACK (B.Cu / B.SilkS) - DAUGHTERBOARDS, RF & ACTUATORS:
    * ESP32-C6-DevKitC-1 V1.2 (Slave - LoRa & Servos - USB facing DOWN):
        - J_DK_R at (35.5, 52.25)
        - J_DK_L at (58.36, 52.25)
        - Board outline: (32.5, 50.25) to (61.5, 93.25)
        - USB-UART connector outline at bottom: (43.0, 91.75) to (51.0, 96.25)
    * SX1278 (Ra-02 433MHz) LoRa Module at (84.81, 70.23, -90 deg)
        - Outline: (71.19, 64.23) to (89.19, 87.23)
    * Quectel LC29H GNSS GPS Module at (11.88, 12.5)
        - Outline: (3.5, 10.0) to (30.5, 41.0)
    * 4x Direct RC Servo Headers (ESC, RUD, WIN, RCK) at Y=74.5 (X=6.0, 9.0, 12.0, 15.0)
    * Actuator & Telemetry Headers: STRB at (18.0, 74.46), J_BATT at (21.0, 74.46), J_LIMIT at (24.0, 74.46), J_WATER at (26.5, 74.5)
    * Dual Power Inputs: 5V Servo (J_PWR_SRV) at (14.5, 84.5, -90 deg), 5V ESP (J_PWR_ESP) at (95.5, 51.5)
"""

import os
import uuid
import json
import zipfile
import subprocess

TARGET_DIR = "/Users/tiagotorredovale/Documents/projectos/winddragon/rover/hardware/dual_esp_carrier"
PROJECT_NAME = "Dual_ESP_Carrier"
BOARD_W = 100.0
BOARD_H = 100.0

def new_uuid():
    return str(uuid.uuid4())

# ==============================================================================
# 1. KICAD PROJECT (.kicad_pro)
# ==============================================================================
def generate_kicad_pro(path):
    pro_data = {
        "board": {
            "3dviewports": [],
            "design_settings": {
                "defaults": {
                    "board_outline_line_width": 0.15,
                    "copper_line_width": 0.25,
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
                    "min_copper_edge_clearance": 0.3,
                    "min_hole_clearance": 0.25,
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
                    "name": "Default",
                    "track_width": 0.4,
                    "via_diameter": 0.8,
                    "via_drill": 0.4
                },
                {
                    "clearance": 0.25,
                    "name": "Power",
                    "track_width": 0.8,
                    "via_diameter": 1.0,
                    "via_drill": 0.5
                }
            ]
        },
        "pcbnew": {
            "last_paths": {
                "gencad": "", "idf": "", "netlist": "", "plot": "",
                "pos_files": "", "specctra_dsn": "", "step": "", "svg": "", "vrml": ""
            }
        },
        "schematic": {
            "drawing": {
                "default_junction_size": 40.0,
                "default_line_thickness": 6.0,
                "default_text_size": 50.0,
                "field_names": [],
                "intersheets_ref_own_page": False,
                "intersheets_ref_prefix": "",
                "intersheets_ref_short": False,
                "intersheets_ref_show": False,
                "intersheets_ref_suffix": "",
                "junction_size_choice": 3,
                "label_size_ratio": 0.25,
                "pin_symbol_size": 0.0,
                "text_offset_ratio": 0.2
            },
            "meta": {
                "filename": f"{PROJECT_NAME}.kicad_sch",
                "version": 1
            }
        },
        "sheets": [
            [str(uuid.uuid4()), ""]
        ]
    }
    with open(path, "w") as f:
        json.dump(pro_data, f, indent=2)

def generate_kicad_prl(path):
    prl_data = {
        "board": {
            "hidden_nets": [],
            "layer_presets": [],
            "visible_layers": "ffffffff_ffffffff",
            "visible_servers": "ffffffff"
        },
        "meta": {
            "filename": f"{PROJECT_NAME}.kicad_prl",
            "version": 1
        }
    }
    with open(path, "w") as f:
        json.dump(prl_data, f, indent=2)

# ==============================================================================
# 2. KICAD SCHEMATIC (.kicad_sch)
# ==============================================================================
def generate_kicad_sch(path):
    sch_lines = [
        '(kicad_sch',
        '  (version 20260306)',
        '  (generator "eeschema")',
        '  (generator_version "10.0")',
        '  (uuid "a0000000-0000-0000-0000-000000000001")',
        '  (paper "A3")',
        '  (title_block',
        '    (title "WindDragons Dual ESP32-C6 Carrier")',
        '    (date "2026-10-14")',
        '    (rev "V1.0 - Dual C6 100x100mm")',
        '    (company "WindDragons Autonomous Rover")',
        '    (comment 1 "Master (Front): ESP32-C6 LCD 1.47 (Universal Footprint), PS5 Joystick, 4x SKRK Buttons")',
        '    (comment 2 "Slave (Back): ESP32-C6 DevKitC-1, SX1278 LoRa, Quectel GPS, 4x LEDC Servos")',
        '  )',
        '  (text "FRONT PANEL: 1.47 INCH IPS LCD, PS5 JOYSTICK & 4x TACTILE BUTTONS" (at 20 20 0) (effects (font (size 2 2) (bold yes))))',
        '  (text "BACK PERIPHERALS: ESP32-C6 DEVKITC-1, SX1278 LORA & 4x DIRECT SERVOS" (at 180 20 0) (effects (font (size 2 2) (bold yes))))',
        '  (text "INTER-ESP UART: LAFVIN (TX=GPIO4, RX=GPIO2) <---> DEVKITC (TX=GPIO13, RX=GPIO22)" (at 100 80 0) (effects (font (size 2 2) (bold yes))))',
        ')'
    ]
    with open(path, "w") as f:
        f.write("\n".join(sch_lines) + "\n")

# ==============================================================================
# 3. KICAD PCB (.kicad_pcb)
# ==============================================================================
def generate_kicad_pcb(path, force=False):
    template_path = os.path.join(os.path.dirname(path), f"{PROJECT_NAME}_template.kicad_pcb")
    if os.path.exists(path):
        if os.path.exists(template_path):
            with open(path, "r") as f_act, open(template_path, "r") as f_tpl:
                c_act = f_act.read()
                c_tpl = f_tpl.read()
            if c_act != c_tpl:
                print(f"Detected user PCB changes! Synchronizing active board to master template: {template_path}")
                with open(template_path, "w") as f_tpl:
                    f_tpl.write(c_act)
        else:
            print(f"Creating initial master template from active board: {template_path}")
            with open(path, "r") as f_act, open(template_path, "w") as f_tpl:
                f_tpl.write(f_act.read())
        if not force:
            print(f"Preserving existing user-modified PCB: {path}")
            return
    if os.path.exists(template_path):
        print(f"Writing PCB from master user-calibrated template: {template_path}")
        with open(template_path, "r") as tf:
            content = tf.read()
        with open(path, "w") as f:
            f.write(content)
        return

    all_nets = [
        "", "/GND", "/+3V3", "/+5V", "/+5V_SERVO",
        "/INTER_TX_M2S", "/INTER_RX_S2M",
        "/GPS_RX", "/GPS_TX", "/GPS_PPS",
        "/LORA_NSS", "/LORA_SCK", "/LORA_MOSI", "/LORA_MISO", "/LORA_RST", "/LORA_DIO0",
        "/SERVO_ESC", "/SERVO_RUDDER", "/SERVO_WINCH", "/SERVO_RACK",
        "/BATT_VOLT", "/BATT_CURR", "/WATER_SENS",
        "/JOY_X", "/JOY_Y", "/IO9_BOOT",
        "/IO18_UP", "/IO19_MODE", "/IO20_DN",
        "/IO3_FREE", "/IO4_FREE", "/IO5_FREE", "/IO12_FREE", "/IO13_FREE"
    ]
    net_map = {n: i for i, n in enumerate(all_nets)}
    def get_net(n):
        if not n:
            return 0
        norm = n if n.startswith("/") else f"/{n}"
        return net_map.get(norm, 0)

    lines = [
        '(kicad_pcb',
        '  (version 20260206)',
        '  (generator "pcbnew")',
        '  (generator_version "10.0")',
        '  (general',
        '    (thickness 1.6)',
        '    (legacy_teardrops no)',
        '  )',
        '  (paper "A4")',
        '  (title_block',
        '    (title "WindDragons Dual ESP32-C6 Carrier")',
        '    (date "2026-10-14")',
        '    (rev "V1.0 - Dual-C6 100x100mm")',
        '    (company "WindDragons Project")',
        '    (comment 1 "Front: LCD 1.47 (Universal Footprint), PS5 Joystick, 4x SKRK Buttons")',
        '    (comment 2 "Back: DevKitC, SX1278 LoRa, Quectel GPS, 4x Servos | Max: 100 x 100 mm")',
        '  )',
        '  (layers',
        '    (0 "F.Cu" signal)',
        '    (31 "B.Cu" signal)',
        '    (32 "B.Adhes" user "B.Adhesive")',
        '    (33 "F.Adhes" user "F.Adhesive")',
        '    (34 "B.Paste" user)',
        '    (35 "F.Paste" user)',
        '    (36 "B.SilkS" user "B.Silkscreen")',
        '    (37 "F.SilkS" user "F.Silkscreen")',
        '    (38 "B.Mask" user)',
        '    (39 "F.Mask" user)',
        '    (40 "Dwgs.User" user "User.Drawings")',
        '    (41 "Cmts.User" user "User.Comments")',
        '    (42 "Eco1.User" user "User.Eco1")',
        '    (43 "Eco2.User" user "User.Eco2")',
        '    (44 "Edge.Cuts" user)',
        '    (45 "Margin" user)',
        '    (46 "B.CrtYd" user "B.Courtyard")',
        '    (47 "F.CrtYd" user "F.Courtyard")',
        '    (48 "B.Fab" user)',
        '    (49 "F.Fab" user)',
        '  )',
        '  (setup',
        '    (pad_to_mask_clearance 0.05)',
        '    (pcbplotparams',
        '      (layerselection 0x00010fc_ffffffff)',
        '      (usegerberextensions yes)',
        '      (usegerberattributes yes)',
        '      (creategerberjobfile yes)',
        '      (outputdirectory "gerbers/")',
        '    )',
        '  )',
    ]

    for n in all_nets:
        if n == "":
            lines.append('  (net 0 "")')
        else:
            lines.append(f'  (net {get_net(n)} "{n}")')

    # 1. Edge.Cuts (100.0 x 100.0 mm)
    corners = [(0.0, 0.0), (BOARD_W, 0.0), (BOARD_W, BOARD_H), (0.0, BOARD_H)]
    for i in range(4):
        p1 = corners[i]
        p2 = corners[(i + 1) % 4]
        lines.append(f'  (gr_line (start {p1[0]} {p1[1]}) (end {p2[0]} {p2[1]}) (stroke (width 0.15) (type solid)) (layer "Edge.Cuts"))')

    # 2. 4x M3 Mounting Holes
    mholes = [("H1", 4.5, 4.5), ("H2", 95.5, 4.5), ("H3", 4.5, 95.5), ("H4", 95.5, 95.5)]
    for h_name, hx, hy in mholes:
        lines.append(f'  (footprint "MountingHole:MountingHole_3.2mm_M3" (layer "F.Cu") (uuid "{new_uuid()}")')
        lines.append(f'    (at {hx} {hy})')
        lines.append(f'    (fp_text reference "{h_name}" (at 0 2.5) (layer "F.SilkS") (effects (font (size 0.8 0.8) (thickness 0.15))))')
        lines.append(f'    (pad "" np_thru_hole circle (at 0 0) (size 3.2 3.2) (drill 3.2) (layers *.Cu *.Mask))')
        lines.append('  )')

    # Silkscreen Titles & Decorative Text
    lines.append('  (gr_text "WINDDRAGONS DUAL ESP32-C6 CARRIER" (at 50.0 4.5 0) (layer "F.SilkS") (effects (font (size 1.2 1.2) (thickness 0.18) (bold yes))))')
    lines.append('  (gr_text "FRONT: DISPLAY & CONTROL PANEL" (at 50.0 7.5 0) (layer "F.SilkS") (effects (font (size 0.8 0.8) (thickness 0.14))))')

    lines.append('  (gr_text "WINDDRAGONS DUAL ESP32-C6 CARRIER" (at 50.0 4.5 0) (layer "B.SilkS") (effects (font (size 1.2 1.2) (thickness 0.18) (bold yes)) (justify mirror)))')
    lines.append('  (gr_text "BACK: DAUGHTERBOARDS, RF & ACTUATORS" (at 50.0 7.5 0) (layer "B.SilkS") (effects (font (size 0.8 0.8) (thickness 0.14)) (justify mirror)))')

    # ==============================================================================
    # FRONT SIDE (F.Cu / F.SilkS):
    # 1. LAFVIN ESP32-C6 1.47" LCD (Master) - EXACT UNIVERSAL PCB FOOTPRINT
    # 2. PS5 3D Analog Joystick (JOY1: Alps RKJXV1224005)
    # 3. 4x Alps SKRKAEE020 SMD Tactile Buttons (SW1-SW4)
    # ==============================================================================
    # FRONT SIDE (F.Cu / F.SilkS):
    # 1. LAFVIN ESP32-C6 1.47" LCD (Master)
    # 2. PS5 3D Analog Joystick (JOY1: Alps RKJXV1224005 on right)
    # 3. 4x Alps SKRKAEE020 SMD Tactile Buttons (SW1-SW4 on left)
    # 4. WindDragons Vector Logo
    # ==============================================================================

    # --- 1. LAFVIN ESP32-C6 1.47" LCD ---
    # Exact Board Outline from Universal PCB (shifted to center):
    lines.append('  (gr_rect (start 34.5 10.0) (end 82.5 35.0) (stroke (width 0.2) (type default)) (fill none) (layer "F.SilkS"))')
    # Exact Active ST7789 1.47" LCD Outline:
    lines.append('  (gr_rect (start 47.0 15.5) (end 69.0 29.5) (stroke (width 0.18) (type default)) (fill none) (layer "F.SilkS"))')
    # Exact USB-C Connector Outline on right edge:
    lines.append('  (gr_rect (start 81.5 20.0) (end 85.5 26.0) (stroke (width 0.15) (type default)) (fill none) (layer "F.SilkS"))')
    lines.append('  (gr_text "USB-C" (at 79.0 23.0 0) (layer "F.SilkS") (effects (font (size 0.7 0.7) (thickness 0.15))))')
    lines.append('  (gr_text "ST7789 1.47 INCH LCD (320x172)" (at 58.0 24.5 0) (layer "F.SilkS") (effects (font (size 0.8 0.8) (thickness 0.15))))')

    # Exact J1: ESP32_T_LANDSCAPE at (48.0, 13.5) on F.Cu
    # Pin 1: IO5, 2: IO4(M2S TX), 3: IO3(FREE/TF-MISO), 4: IO2(S2M RX), 5: IO1(JOY_Y), 6: IO0(JOY_X), 7: +3V3, 8: GND, 9: +5V
    j1_nets = [
        "/IO5_FREE", "/INTER_TX_M2S", "/IO3_FREE", "/INTER_RX_S2M",
        "/JOY_Y", "/JOY_X", "/+3V3", "/GND", "/+5V"
    ]
    lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x09_P2.54mm_Horizontal" (layer "F.Cu") (uuid "{new_uuid()}")')
    lines.append('    (at 48.0 13.5)')
    lines.append('    (property "Reference" "J1" (at 0 -2.5 0) (layer "F.SilkS") (effects (font (size 0.8 0.8) (thickness 0.15))))')
    lines.append('    (property "Value" "ESP32_T_LANDSCAPE" (at 10.16 -2.5 0) (layer "F.SilkS") (effects (font (size 0.8 0.8) (thickness 0.15))))')
    for i in range(9):
        px = i * 2.54
        n = j1_nets[i]
        p_shape = "rect" if i == 0 else "circle"
        lines.append(f'    (pad "{i+1}" thru_hole {p_shape} (at {px:.2f} 0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net(n)} "{n}"))')
    lines.append('  )')

    # Exact J2: ESP32_B_LANDSCAPE at (48.0, 31.28) on F.Cu
    # Pin 1: IO9(BOOT/CLICK), 2: IO18(UP), 3: IO19(MODE), 4: IO20(DN), 5: IO23(PPS), 6: IO12, 7: IO13, 8: IO16(GPS_RX), 9: IO17(GPS_TX)
    j2_nets = [
        "/IO9_BOOT", "/IO18_UP", "/IO19_MODE", "/IO20_DN",
        "/GPS_PPS", "/IO12_FREE", "/IO13_FREE", "/GPS_RX", "/GPS_TX"
    ]
    lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x09_P2.54mm_Horizontal" (layer "F.Cu") (uuid "{new_uuid()}")')
    lines.append('    (at 48.0 31.28)')
    lines.append('    (property "Reference" "J2" (at 0 2.5 0) (layer "F.SilkS") (effects (font (size 0.8 0.8) (thickness 0.15))))')
    lines.append('    (property "Value" "ESP32_B_LANDSCAPE" (at 10.16 2.5 0) (layer "F.SilkS") (effects (font (size 0.8 0.8) (thickness 0.15))))')
    for i in range(9):
        px = i * 2.54
        n = j2_nets[i]
        p_shape = "rect" if i == 0 else "circle"
        lines.append(f'    (pad "{i+1}" thru_hole {p_shape} (at {px:.2f} 0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net(n)} "{n}"))')
    lines.append('  )')

    # --- 2. PS5 3D ANALOG JOYSTICK (JOY1: Alps RKJXV1224005 on F.Cu at right) ---
    joy_cx, joy_cy = 78.2525, 51.715
    lines.append('  (gr_text "PS5 3D JOYSTICK (ALPS RKJXV)" (at 78.5 37.5 0) (layer "F.SilkS") (effects (font (size 0.8 0.8) (thickness 0.14))))')
    lines.append(f'  (footprint "PS5_joystick:Alps_RKJXV1224005" (layer "F.Cu") (uuid "{new_uuid()}")')
    lines.append(f'    (at {joy_cx} {joy_cy})')
    lines.append('    (fp_text reference "JOY1" (at 0 -11.5) (layer "F.SilkS") (effects (font (size 0.8 0.8) (thickness 0.15))))')
    lines.append('    (fp_text value "PS5_JOYSTICK" (at 0 13.0) (layer "F.Fab") (effects (font (size 0.8 0.8) (thickness 0.15)) (hide yes)))')
    lines.append('    (fp_line (start -8.0 -8.0) (end -5.0 -8.0) (stroke (width 0.15) (type solid)) (layer "F.SilkS"))')
    lines.append('    (fp_line (start -8.0 -8.0) (end -8.0 -5.0) (stroke (width 0.15) (type solid)) (layer "F.SilkS"))')
    lines.append('    (fp_line (start 8.0 -8.0) (end 5.0 -8.0) (stroke (width 0.15) (type solid)) (layer "F.SilkS"))')
    lines.append('    (fp_line (start 8.0 -8.0) (end 8.0 -5.0) (stroke (width 0.15) (type solid)) (layer "F.SilkS"))')
    lines.append('    (fp_line (start -8.0 8.0) (end -5.0 8.0) (stroke (width 0.15) (type solid)) (layer "F.SilkS"))')
    lines.append('    (fp_line (start -8.0 8.0) (end -8.0 5.0) (stroke (width 0.15) (type solid)) (layer "F.SilkS"))')
    lines.append('    (fp_line (start 8.0 8.0) (end 5.0 8.0) (stroke (width 0.15) (type solid)) (layer "F.SilkS"))')
    lines.append('    (fp_line (start 8.0 8.0) (end 8.0 5.0) (stroke (width 0.15) (type solid)) (layer "F.SilkS"))')
    lines.append('    (fp_circle (center 0 0) (end 2.0 0) (stroke (width 0.15) (type solid)) (fill none) (layer "F.SilkS"))')
    lines.append('    (fp_rect (start -8.0 -8.0) (end 8.0 8.0) (stroke (width 0.15) (type solid)) (fill none) (layer "F.Fab"))')
    # Pins
    lines.append(f'    (pad "1" thru_hole circle (at -8.73 -2.5) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("/GND")} "/GND"))')
    lines.append(f'    (pad "2" thru_hole circle (at -8.73 0.0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("/JOY_Y")} "/JOY_Y"))')
    lines.append(f'    (pad "3" thru_hole circle (at -8.73 2.5) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("/+3V3")} "/+3V3"))')
    lines.append(f'    (pad "4" thru_hole circle (at -2.5 -8.73) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("/GND")} "/GND"))')
    lines.append(f'    (pad "5" thru_hole circle (at 0.0 -8.73) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("/JOY_X")} "/JOY_X"))')
    lines.append(f'    (pad "6" thru_hole circle (at 2.5 -8.73) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("/+3V3")} "/+3V3"))')
    lines.append(f'    (pad "7" thru_hole circle (at -3.25 5.75) (size 1.8 1.8) (drill 1.2) (layers *.Cu *.Mask "F.SilkS") (net {get_net("/GND")} "/GND"))')
    lines.append(f'    (pad "8" thru_hole circle (at -3.25 10.25) (size 1.8 1.8) (drill 1.2) (layers *.Cu *.Mask "F.SilkS") (net {get_net("/GND")} "/GND"))')
    lines.append(f'    (pad "9" thru_hole circle (at 3.25 5.75) (size 1.8 1.8) (drill 1.2) (layers *.Cu *.Mask "F.SilkS") (net {get_net("/IO9_BOOT")} "/IO9_BOOT"))')
    lines.append(f'    (pad "10" thru_hole circle (at 3.25 10.25) (size 1.8 1.8) (drill 1.2) (layers *.Cu *.Mask "F.SilkS") (net {get_net("/IO9_BOOT")} "/IO9_BOOT"))')
    lines.append(f'    (pad "11" thru_hole circle (at -6.325 -5.0) (size 2.3 2.3) (drill 1.5) (layers *.Cu *.Mask "F.SilkS") (net {get_net("/GND")} "/GND"))')
    lines.append(f'    (pad "12" thru_hole circle (at 6.325 -5.0) (size 2.3 2.3) (drill 1.5) (layers *.Cu *.Mask "F.SilkS") (net {get_net("/GND")} "/GND"))')
    lines.append(f'    (pad "13" thru_hole circle (at -6.325 5.0) (size 2.3 2.3) (drill 1.5) (layers *.Cu *.Mask "F.SilkS") (net {get_net("/GND")} "/GND"))')
    lines.append(f'    (pad "14" thru_hole circle (at 6.325 5.0) (size 2.3 2.3) (drill 1.5) (layers *.Cu *.Mask "F.SilkS") (net {get_net("/GND")} "/GND"))')
    lines.append('    (pad "" np_thru_hole circle (at -4.3 0.0) (size 1.6 1.6) (drill 1.6) (layers *.Cu *.Mask))')
    lines.append('    (pad "" np_thru_hole circle (at 4.3 0.0) (size 1.6 1.6) (drill 1.6) (layers *.Cu *.Mask))')
    lines.append('  )')

    # --- 3. 4x ALPS SKRKAEE020 SMD TACTILE SWITCHES (F.Cu at left) ---
    buttons = [
        ("SW4", "BOOT", 16.43, 45.22, "/IO9_BOOT"),
        ("SW1", "subir ancora", 10.13, 51.22, "/IO18_UP"),
        ("SW2", "MODE", 22.73, 51.22, "/IO19_MODE"),
        ("SW3", "largar ancora", 16.43, 57.22, "/IO20_DN"),
    ]
    for ref, val, bx, by, sig_net in buttons:
        lines.append(f'  (footprint "PS4_joystick:SKRKAEE020" (layer "F.Cu") (uuid "{new_uuid()}")')
        lines.append(f'    (at {bx} {by})')
        lines.append(f'    (property "Reference" "{ref}" (at 0 -2.3 0) (layer "F.SilkS") (uuid "{new_uuid()}") (effects (font (size 0.75 0.75) (thickness 0.14))))')
        lines.append(f'    (property "Value" "{val}" (at 0 2.3 0) (layer "F.SilkS") (uuid "{new_uuid()}") (effects (font (size 0.65 0.65) (thickness 0.12))))')
        lines.append('    (attr smd)')
        lines.append(f'    (fp_line (start -2.05 -1.55) (end 2.05 -1.55) (stroke (width 0.12) (type solid)) (layer "F.SilkS") (uuid "{new_uuid()}"))')
        lines.append(f'    (fp_line (start 2.05 1.55) (end -2.05 1.55) (stroke (width 0.12) (type solid)) (layer "F.SilkS") (uuid "{new_uuid()}"))')
        lines.append(f'    (fp_line (start -2.75 -1.75) (end 2.75 -1.75) (stroke (width 0.05) (type solid)) (layer "F.CrtYd") (uuid "{new_uuid()}"))')
        lines.append(f'    (fp_line (start -2.75 1.75) (end -2.75 -1.75) (stroke (width 0.05) (type solid)) (layer "F.CrtYd") (uuid "{new_uuid()}"))')
        lines.append(f'    (fp_line (start 2.75 -1.75) (end 2.75 1.75) (stroke (width 0.05) (type solid)) (layer "F.CrtYd") (uuid "{new_uuid()}"))')
        lines.append(f'    (fp_line (start 2.75 1.75) (end -2.75 1.75) (stroke (width 0.05) (type solid)) (layer "F.CrtYd") (uuid "{new_uuid()}"))')
        lines.append(f'    (pad "1" smd roundrect (at -2.1 0) (size 0.8 2) (layers "F.Cu" "F.Mask" "F.Paste") (roundrect_rratio 0.25) (net {get_net(sig_net)} "{sig_net}") (uuid "{new_uuid()}"))')
        lines.append(f'    (pad "2" smd roundrect (at 2.1 0) (size 0.8 2) (layers "F.Cu" "F.Mask" "F.Paste") (roundrect_rratio 0.25) (net {get_net("/GND")} "/GND") (uuid "{new_uuid()}"))')
        lines.append('  )')

    # ==============================================================================
    # BACK SIDE (B.Cu / B.SilkS):
    # ==============================================================================

    # --- 1. ESP32-C6 DEVKITC-1 V1.2 (B.Cu - USB FACING DOWN) ---
    lines.append('  (gr_text "ESP32-C6 DEVKITC-1 (SLAVE: RF & SERVOS)" (at 17.5 86.5 0) (layer "B.SilkS") (effects (font (size 0.85 0.85) (thickness 0.14) (bold yes)) (justify mirror)))')
    lines.append('  (gr_rect (start 32.5 50.25) (end 61.5 93.25) (stroke (width 0.15) (type default)) (fill none) (layer "B.SilkS"))')
    lines.append('  (gr_rect (start 43.0 91.75) (end 51.0 96.25) (stroke (width 0.15) (type default)) (fill none) (layer "B.SilkS"))')
    lines.append('  (gr_text "USB-UART" (at 47.0 93.75 180) (layer "B.SilkS") (effects (font (size 0.6 0.6) (thickness 0.1)) (justify mirror)))')
    lines.append('  (gr_text "! J5 CLOSED !" (at 47.0 51.25 180) (layer "B.SilkS") (effects (font (size 0.65 0.65) (thickness 0.12) (bold yes)) (justify mirror)))')
    lines.append('  (gr_text "DIRECT LEDC PWM SERVOS" (at 46.0 97.75 180) (layer "B.SilkS") (effects (font (size 0.75 0.75) (thickness 0.12) (bold yes)) (justify mirror)))')

    # Right Header (J_DK_R at 35.5, 52.25)
    # Pin 1: GND, 3: ESC(IO17), 4: RUD(IO15), 5: WINCH(IO23), 6: INTER_RX(IO22), 7: RACK(IO21), 12: GND, 13: INTER_TX(IO13), 14: IO12, 15: GND
    dk_r_nets = [
        "/GND", "", "/SERVO_ESC", "/SERVO_RUDDER", "/SERVO_WINCH", "/INTER_TX_M2S",
        "/SERVO_RACK", "", "", "", "", "/GND",
        "/INTER_RX_S2M", "/IO12_FREE", "/GND", ""
    ]
    lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x16_P2.54mm_Vertical" (layer "B.Cu") (uuid "{new_uuid()}")')
    lines.append('    (at 35.5 52.25)')
    lines.append('    (property "Reference" "J_DK_R" (at -2.5 19.05 90) (layer "B.SilkS") (effects (font (size 0.8 0.8) (thickness 0.15)) (justify mirror)) (hide yes))')
    for i in range(16):
        py = i * 2.54
        n = dk_r_nets[i]
        p_shape = "rect" if i == 0 else "circle"
        lines.append(f'    (pad "{i+1}" thru_hole {p_shape} (at 0 {py:.2f}) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "B.SilkS") (net {get_net(n)} "{n}"))')
    lines.append('  )')

    # Left Header (J_DK_L at 58.36, 52.25)
    dk_l_nets = [
        "/+3V3", "/LORA_RST", "/LORA_MISO", "/LORA_SCK", "/LORA_NSS",
        "/LORA_MOSI", "/LORA_DIO0", "", "", "/WATER_SENS", "", "/BATT_VOLT",
        "/BATT_CURR", "/+5V", "/GND", ""
    ]
    lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x16_P2.54mm_Vertical" (layer "B.Cu") (uuid "{new_uuid()}")')
    lines.append('    (at 58.36 52.25)')
    lines.append('    (property "Reference" "J_DK_L" (at 2.5 19.05 90) (layer "B.SilkS") (effects (font (size 0.8 0.8) (thickness 0.15)) (justify mirror)) (hide yes))')
    for i in range(16):
        py = i * 2.54
        n = dk_l_nets[i]
        p_shape = "rect" if i == 0 else "circle"
        lines.append(f'    (pad "{i+1}" thru_hole {p_shape} (at 0 {py:.2f}) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "B.SilkS") (net {get_net(n)} "{n}"))')
    lines.append('  )')

    # --- 2. SX1278 (Ra-02 433MHz) LORA MODULE (B.Cu) ---
    lines.append('  (gr_text "SX1278 LORA 2x4" (at 67.5 79.46 90) (layer "B.SilkS") (effects (font (size 0.75 0.75) (thickness 0.13) (bold yes)) (justify mirror)))')
    lines.append('  (gr_rect (start 64.46 69.96) (end 82.46 92.96) (stroke (width 0.2) (type default)) (fill none) (layer "B.SilkS"))')
    lines.append('  (gr_text "SX1278 433MHz" (at 74.96 90.96 0) (layer "B.SilkS") (effects (font (size 0.85 0.85) (thickness 0.15)) (justify mirror)))')

    lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_2x04_P2.54mm_Vertical" (layer "B.Cu") (uuid "{new_uuid()}")')
    lines.append('    (at 77.54 72.0 -90)')
    lines.append('    (property "Reference" "J3" (at 1.27 -2.4 90) (layer "B.SilkS") (effects (font (size 0.9 0.9) (thickness 0.14)) (justify mirror)))')
    lines.append('    (property "Value" "SX1278_LORA_2x4" (at 11.54 -2.92 90) (layer "B.Fab") (effects (font (size 0.9 0.9) (thickness 0.14)) (justify mirror)))')
    lines.append('    (fp_line (start -1.33 8.95) (end -1.33 -1.33) (stroke (width 0.12) (type solid)) (layer "B.SilkS"))')
    lines.append('    (fp_line (start 3.87 8.95) (end -1.33 8.95) (stroke (width 0.12) (type solid)) (layer "B.SilkS"))')
    lines.append('    (fp_line (start -1.33 -1.33) (end 3.87 -1.33) (stroke (width 0.12) (type solid)) (layer "B.SilkS"))')
    lines.append('    (fp_line (start 3.87 -1.33) (end 3.87 8.95) (stroke (width 0.12) (type solid)) (layer "B.SilkS"))')
    lines.append('    (fp_text user "GND" (at -4.0 7.7 90) (layer "B.SilkS") (effects (font (size 0.7 0.7) (thickness 0.12)) (justify mirror)))')
    lines.append('    (fp_text user "3V3" (at 5.4 7.62 90) (layer "B.SilkS") (effects (font (size 0.7 0.7) (thickness 0.12)) (justify mirror)))')
    lines.append('    (fp_text user "NC" (at -4.0 5.16 90) (layer "B.SilkS") (effects (font (size 0.7 0.7) (thickness 0.12)) (justify mirror)))')
    lines.append('    (fp_text user "NSS" (at 5.4 5.08 90) (layer "B.SilkS") (effects (font (size 0.7 0.7) (thickness 0.12)) (justify mirror)))')
    lines.append('    (fp_text user "SCK" (at -4.0 2.62 90) (layer "B.SilkS") (effects (font (size 0.7 0.7) (thickness 0.12)) (justify mirror)))')
    lines.append('    (fp_text user "MOSI" (at 5.7 2.54 90) (layer "B.SilkS") (effects (font (size 0.7 0.7) (thickness 0.12)) (justify mirror)))')
    lines.append('    (fp_text user "MISO" (at -4.2 0.08 90) (layer "B.SilkS") (effects (font (size 0.7 0.7) (thickness 0.12)) (justify mirror)))')
    lines.append('    (fp_text user "DIO0" (at 5.7 0.0 90) (layer "B.SilkS") (effects (font (size 0.7 0.7) (thickness 0.12)) (justify mirror)))')
    lines.append(f'    (pad "1" thru_hole rect (at 0 7.62 270) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "B.SilkS") (net {get_net("/GND")} "/GND"))')
    lines.append(f'    (pad "2" thru_hole circle (at 2.54 7.62 270) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "B.SilkS") (net {get_net("/+3V3")} "/+3V3"))')
    lines.append(f'    (pad "3" thru_hole circle (at 0 5.08 270) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "B.SilkS") (net {get_net("/LORA_RST")} "/LORA_RST"))')
    lines.append(f'    (pad "4" thru_hole circle (at 2.54 5.08 270) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "B.SilkS") (net {get_net("/LORA_NSS")} "/LORA_NSS"))')
    lines.append(f'    (pad "5" thru_hole circle (at 0 2.54 270) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "B.SilkS") (net {get_net("/LORA_SCK")} "/LORA_SCK"))')
    lines.append(f'    (pad "6" thru_hole circle (at 2.54 2.54 270) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "B.SilkS") (net {get_net("/LORA_MOSI")} "/LORA_MOSI"))')
    lines.append(f'    (pad "7" thru_hole circle (at 0 0 270) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "B.SilkS") (net {get_net("/LORA_MISO")} "/LORA_MISO"))')
    lines.append(f'    (pad "8" thru_hole circle (at 2.54 0 270) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "B.SilkS") (net {get_net("/LORA_DIO0")} "/LORA_DIO0"))')
    lines.append('  )')

    # --- 3. QUECTEL LC29H GNSS GPS MODULE (B.Cu) ---
    lines.append('  (gr_text "QUECTEL LC29H" (at 17.0 7.5 0) (layer "B.SilkS") (effects (font (size 0.85 0.85) (thickness 0.14) (bold yes)) (justify mirror)))')
    lines.append('  (gr_rect (start 3.5 10.0) (end 30.5 41.0) (stroke (width 0.2) (type default)) (fill none) (layer "B.SilkS"))')
    lines.append('  (gr_text "GPS 26.2x30.1mm" (at 19.5 38.0 0) (layer "B.SilkS") (effects (font (size 0.8 0.8) (thickness 0.15)) (justify mirror)))')
    
    gps_nets = ["/GPS_PPS", "", "", "/GPS_TX", "/GPS_RX", "/GND", "/+3V3"]
    gps_labels = ["P", "T2", "R2", "R1", "T1", "G", "V"]
    lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x07_P2.54mm_Horizontal" (layer "B.Cu") (uuid "{new_uuid()}")')
    lines.append('    (at 11.88 12.5)')
    lines.append('    (property "Reference" "J6" (at 0 3.0 0) (layer "B.SilkS") (effects (font (size 0.8 0.8) (thickness 0.13)) (justify mirror)))')
    lines.append('    (property "Value" "GPS_LC29H_7PIN" (at 7.62 9.5 0) (layer "B.Fab") (effects (font (size 0.9 0.9) (thickness 0.14)) (justify mirror)))')
    for i in range(7):
        px = i * 2.54
        n = gps_nets[i]
        lbl = gps_labels[i]
        p_shape = "rect" if i == 0 else "circle"
        lines.append(f'    (fp_text user "{lbl}" (at {px:.2f} -2.7 0) (layer "B.SilkS") (effects (font (size 0.7 0.7) (thickness 0.12)) (justify mirror)))')
        lines.append(f'    (pad "{i+1}" thru_hole {p_shape} (at {px:.2f} 0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "B.SilkS") (net {get_net(n)} "{n}"))')
    lines.append('  )')

    # --- 4. DIRECT RC SERVO HEADERS (4 CHANNELS) (F.Cu & F.SilkS) ---
    servos = [
        ("ESC", "ESC", 10.0, 73.58, "/SERVO_ESC"),
        ("RUD", "RUDDER", 13.0, 73.58, "/SERVO_RUDDER"),
        ("WIN", "WINCH", 16.0, 73.5, "/SERVO_WINCH"),
        ("RCK", "RACK", 19.0, 73.5, "/SERVO_RACK"),
    ]
    lines.append('  (gr_text "RC SERVOS" (at 14.5 66.0 0) (layer "F.SilkS") (effects (font (size 0.85 0.85) (thickness 0.14) (bold yes))))')
    lines.append('  (gr_text "SIG" (at 7.6 68.5 0) (layer "F.SilkS") (effects (font (size 0.65 0.65) (thickness 0.12))))')
    lines.append('  (gr_text "+5V" (at 7.6 71.04 0) (layer "F.SilkS") (effects (font (size 0.65 0.65) (thickness 0.12))))')
    lines.append('  (gr_text "GND" (at 7.6 73.58 0) (layer "F.SilkS") (effects (font (size 0.65 0.65) (thickness 0.12))))')
    for ref, lbl, sx, sy, sig_net in servos:
        lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical" (layer "F.Cu") (uuid "{new_uuid()}")')
        lines.append(f'    (at {sx} {sy} 180)')
        ref_x = -0.5 if ref == "WIN" else 0
        lines.append(f'    (property "Reference" "{ref}" (at {ref_x} -2.4 0) (layer "F.SilkS") (effects (font (size 0.7 0.7) (thickness 0.12) (bold yes))))')
        lines.append(f'    (property "Value" "{lbl}" (at 0 7.5 90) (layer "F.SilkS") (effects (font (size 0.6 0.6) (thickness 0.1))))')
        lines.append(f'    (pad "1" thru_hole rect (at 0 0 180) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("/GND")} "/GND"))')
        lines.append(f'    (pad "2" thru_hole circle (at 0 2.54 180) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("/+5V_SERVO")} "/+5V_SERVO"))')
        lines.append(f'    (pad "3" thru_hole circle (at 0 5.08 180) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net(sig_net)} "{sig_net}"))')
        lines.append('  )')

    # --- 5. POWER & TELEMETRY HEADERS (B.Cu) ---
    # BATT_SENSE (APM V/I at 22.0, 73.5, rot 180)
    lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical" (layer "B.Cu") (uuid "{new_uuid()}")')
    lines.append('    (at 22.0 73.5 180)')
    lines.append('    (property "Reference" "J_BATT" (at 0 -2.4 0) (layer "B.SilkS") (effects (font (size 0.65 0.65) (thickness 0.1)) (justify mirror)))')
    lines.append('    (property "Value" "BATT V/I/G" (at 0 7.5 90) (layer "B.SilkS") (effects (font (size 0.6 0.6) (thickness 0.1)) (justify mirror)))')
    lines.append(f'    (pad "1" thru_hole rect (at 0 0 180) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "B.SilkS") (net {get_net("/GND")} "/GND"))')
    lines.append(f'    (pad "2" thru_hole circle (at 0 2.54 180) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "B.SilkS") (net {get_net("/BATT_CURR")} "/BATT_CURR"))')
    lines.append(f'    (pad "3" thru_hole circle (at 0 5.08 180) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "B.SilkS") (net {get_net("/BATT_VOLT")} "/BATT_VOLT"))')
    lines.append('  )')

    # WATER / ANCHOR SENSOR (at 25.0, 73.5, rot 180)
    lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical" (layer "B.Cu") (uuid "{new_uuid()}")')
    lines.append('    (at 25.0 73.5 180)')
    lines.append('    (property "Reference" "J_WATER" (at -1 -2.5 0) (layer "B.SilkS") (effects (font (size 0.65 0.65) (thickness 0.1)) (justify mirror)))')
    lines.append('    (property "Value" "WATER/ANC" (at 0 7.0 90) (layer "B.SilkS") (effects (font (size 0.6 0.6) (thickness 0.1)) (justify mirror)))')
    lines.append(f'    (pad "1" thru_hole rect (at 0 0 180) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "B.SilkS") (net {get_net("/GND")} "/GND"))')
    lines.append(f'    (pad "2" thru_hole circle (at 0 2.54 180) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "B.SilkS") (net {get_net("/WATER_SENS")} "/WATER_SENS"))')
    lines.append('  )')

    # PWR_SERVO_5V (at 5.0, 77.73)
    lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical" (layer "B.Cu") (uuid "{new_uuid()}")')
    lines.append('    (at 5.0 77.73)')
    lines.append('    (property "Reference" "J_PWR_SRV" (at 0 -2.4 0) (layer "B.SilkS") (effects (font (size 0.65 0.65) (thickness 0.1)) (justify mirror)))')
    lines.append('    (property "Value" "5V_SERVO" (at 0 5.0 90) (layer "B.SilkS") (effects (font (size 0.6 0.6) (thickness 0.1)) (justify mirror)))')
    lines.append(f'    (pad "1" thru_hole rect (at 0 0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "B.SilkS") (net {get_net("/GND")} "/GND"))')
    lines.append(f'    (pad "2" thru_hole circle (at 0 2.54) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "B.SilkS") (net {get_net("/+5V_SERVO")} "/+5V_SERVO"))')
    lines.append('  )')

    # PWR_ESP_5V (at 95.5, 51.5)
    lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical" (layer "B.Cu") (uuid "{new_uuid()}")')
    lines.append('    (at 95.5 51.5)')
    lines.append('    (property "Reference" "J_PWR_ESP" (at 0 -2.4 0) (layer "B.SilkS") (effects (font (size 0.65 0.65) (thickness 0.1)) (justify mirror)))')
    lines.append('    (property "Value" "5V_ESP" (at 0 5.0 90) (layer "B.SilkS") (effects (font (size 0.6 0.6) (thickness 0.1)) (justify mirror)))')
    lines.append(f'    (pad "1" thru_hole rect (at 0 0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "B.SilkS") (net {get_net("/GND")} "/GND"))')
    lines.append(f'    (pad "2" thru_hole circle (at 0 2.54) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "B.SilkS") (net {get_net("/+5V")} "/+5V"))')
    lines.append('  )')

    # ==============================================================================
    # 6. VECTORIZED WINDDRAGONS LOGOS (F.SilkS & B.SilkS)
    # ==============================================================================
    dragon_json_path = os.path.join(os.path.dirname(__file__), "dragon_polys.json")
    if not os.path.exists(dragon_json_path):
        dragon_json_path = "/Users/tiagotorredovale/Documents/projectos/winddragon/rover/hardware/dragon_polys.json"
    
    if os.path.exists(dragon_json_path):
        with open(dragon_json_path) as djf:
            dragon_norm_polys = json.load(djf)

        # FRONT: Perfectly centered inside DevKit header channel (X=44.93, Y=65.0, 16mm)
        logo_f_x, logo_f_y = 44.93, 65.0
        logo_f_sz = 16.0
        for poly in dragon_norm_polys:
            pts = " ".join([f"(xy {logo_f_x + pt[0]*logo_f_sz:.2f} {logo_f_y + pt[1]*logo_f_sz:.2f})" for pt in poly])
            lines.append(f'  (gr_poly (pts {pts}) (stroke (width 0.1) (type solid)) (fill solid) (layer "F.SilkS"))')

        # BACK: Placed on Back open area (X=75.0, Y=70.0, 16mm mirrored)
        logo_b_x, logo_b_y = 75.0, 70.0
        logo_b_sz = 16.0
        for poly in dragon_norm_polys:
            pts = " ".join([f"(xy {logo_b_x - pt[0]*logo_b_sz:.2f} {logo_b_y + pt[1]*logo_b_sz:.2f})" for pt in poly])
            lines.append(f'  (gr_poly (pts {pts}) (stroke (width 0.1) (type solid)) (fill solid) (layer "B.SilkS"))')

    # ==============================================================================
    # 7. SOLID COPPER GND ZONES (F.Cu & B.Cu)
    # ==============================================================================
    z_box = [(1.0, 1.0), (BOARD_W - 1.0, 1.0), (BOARD_W - 1.0, BOARD_H - 1.0), (1.0, BOARD_H - 1.0)]
    pts_str = " ".join([f"(xy {p[0]} {p[1]})" for p in z_box])
    gnd_idx = get_net("/GND")

    for layer in ["F.Cu", "B.Cu"]:
        lines.append('  (zone (net ' + str(gnd_idx) + ') (net_name "/GND") (layers "' + layer + '") (uuid "' + new_uuid() + '")')
        lines.append('    (hatch edge 0.5)')
        lines.append('    (connect_pads (clearance 0.25))')
        lines.append('    (min_thickness 0.2)')
        lines.append('    (filled_areas_thickness no)')
        lines.append('    (fill yes (thermal_gap 0.25) (thermal_bridge_width 0.4))')
        lines.append('    (polygon')
        lines.append(f'      (pts {pts_str})')
        lines.append('    )')
        lines.append('  )')

    lines.append(')')

    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")

# ==============================================================================
# MAIN EXECUTION
# ==============================================================================
def main():
    os.makedirs(TARGET_DIR, exist_ok=True)
    gerber_dir = os.path.join(TARGET_DIR, "gerbers")
    os.makedirs(gerber_dir, exist_ok=True)

    pro_path = os.path.join(TARGET_DIR, f"{PROJECT_NAME}.kicad_pro")
    prl_path = os.path.join(TARGET_DIR, f"{PROJECT_NAME}.kicad_prl")
    sch_path = os.path.join(TARGET_DIR, f"{PROJECT_NAME}.kicad_sch")
    pcb_path = os.path.join(TARGET_DIR, f"{PROJECT_NAME}.kicad_pcb")

    print(f"Generating WindDragons Dual ESP32-C6 KiCad project in: {TARGET_DIR}")
    generate_kicad_pro(pro_path)
    generate_kicad_prl(prl_path)
    generate_kicad_sch(sch_path)
    generate_kicad_pcb(pcb_path)
    print("Base project files generated successfully.")

    kicad_cli = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
    if os.path.exists(kicad_cli):
        print("Upgrading board and schematic to native KiCad 10 format...")
        subprocess.run([kicad_cli, "pcb", "upgrade", "--force", pcb_path], capture_output=True)
        subprocess.run([kicad_cli, "sch", "upgrade", "--force", sch_path], capture_output=True)

        print("Rendering 3D preview images...")
        render_top = os.path.join(TARGET_DIR, f"{PROJECT_NAME}_3d_top.png")
        subprocess.run([
            kicad_cli, "pcb", "render", pcb_path,
            "-o", render_top,
            "--side", "top",
            "--width", "1920", "--height", "1080", "--zoom", "1.25"
        ], capture_output=True)

        render_bottom = os.path.join(TARGET_DIR, f"{PROJECT_NAME}_3d_bottom.png")
        subprocess.run([
            kicad_cli, "pcb", "render", pcb_path,
            "-o", render_bottom,
            "--side", "bottom",
            "--width", "1920", "--height", "1080", "--zoom", "1.25"
        ], capture_output=True)

        print("Exporting Gerbers and Drill files...")
        subprocess.run([kicad_cli, "pcb", "export", "gerbers", "--output", gerber_dir, pcb_path], capture_output=True)
        subprocess.run([kicad_cli, "pcb", "export", "drill", "--output", gerber_dir, pcb_path], capture_output=True)

        zip_path = os.path.join(TARGET_DIR, f"{PROJECT_NAME}_Gerber.zip")
        with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
            for fname in sorted(os.listdir(gerber_dir)):
                fpath = os.path.join(gerber_dir, fname)
                if os.path.isfile(fpath):
                    zf.write(fpath, arcname=fname)
        print(f"Generated Gerbers and packaged: {zip_path}")
    else:
        print("kicad-cli not found at standard path, skipped 3D rendering and gerbers.")

if __name__ == "__main__":
    main()
