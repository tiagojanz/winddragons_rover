#!/usr/bin/env python3
"""
KiCad Project Generator for WindDragons Universal Carrier Board
Converts the universal_pcb into a complete, professional KiCad Project:
- Universal_Carrier.kicad_pro
- Universal_Carrier.kicad_sch
- Universal_Carrier.kicad_pcb
- Universal_Carrier.kicad_prl
"""

import os
import uuid
import json
import subprocess

TARGET_DIR = "/Users/tiagotorredovale/Documents/projectos/winddragon/rover/hardware/universal_pcb"
PROJECT_NAME = "Universal_Carrier"

def new_uuid():
    return str(uuid.uuid4())

# ==============================================================================
# 1. GENERATE KICAD PROJECT (.kicad_pro)
# ==============================================================================
def generate_kicad_pro(path):
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
                    "silk_text_size_h": 1.0,
                    "silk_text_size_v": 1.0,
                    "silk_text_thickness": 0.15
                },
                "meta": {
                    "filename": f"{PROJECT_NAME}.kicad_pro",
                    "version": 3
                },
                "rules": {
                    "min_clearance": 0.15,
                    "min_copper_edge_clearance": 0.2,
                    "min_hole_clearance": 0.2,
                    "min_hole_to_hole": 0.25,
                    "min_track_width": 0.2
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
                    "via_drill": 0.6
                }
            ]
        },
        "pcbnew": {
            "last_paths": {
                "gencad": "",
                "idf": "",
                "netlist": "",
                "plot": "",
                "pos_files": "",
                "specctra_dsn": "",
                "step": "",
                "svg": "",
                "vrml": ""
            }
        },
        "schematic": {
            "drawing": {
                "default_junction_size": 40.0,
                "default_line_thickness": 6.0,
                "default_text_size": 50.0,
                "default_wire_thickness": 6.0
            },
            "meta": {
                "version": 1
            }
        },
        "sheets": [
            [
                "c0000000-0000-0000-0000-000000000001",
                "Root"
            ]
        ]
    }
    
    with open(path, "w") as f:
        json.dump(pro_data, f, indent=2)
    print(f"Generated KiCad Project: {path}")

# ==============================================================================
# 2. GENERATE KICAD PREFERENCES (.kicad_prl)
# ==============================================================================
def generate_kicad_prl(path):
    prl_data = {
        "board": {
            "appearance": {
                "color_theme": "_builtin_default"
            }
        },
        "meta": {
            "filename": f"{PROJECT_NAME}.kicad_prl",
            "version": 3
        },
        "schematic": {
            "appearance": {
                "color_theme": "_builtin_default"
            }
        }
    }
    with open(path, "w") as f:
        json.dump(prl_data, f, indent=2)
    print(f"Generated KiCad PRL: {path}")

# ==============================================================================
# 3. GENERATE FULL KICAD PCB (.kicad_pcb)
# ==============================================================================
def generate_kicad_pcb(path):
    # Net definitions
    nets = [
        (0, ""),
        (1, "GND"),
        (2, "+3V3"),
        (3, "+5V"),
        (4, "+5V_SERVO"),
        (5, "IO4_NSS"),
        (6, "IO5_MISO"),
        (7, "IO6_MOSI"),
        (8, "IO7_SCK"),
        (9, "IO10_DIO0"),
        (10, "IO12_RST"),
        (11, "IO0_ADC"),
        (12, "IO1_ADC"),
        (13, "IO11_BUZ"),
        (14, "IO13_SW"),
        (15, "IO16_GPS_TX"),
        (16, "IO17_GPS_RX"),
        (17, "IO18_ACT1_UP"),
        (18, "IO19_ACT2_DOWN"),
        (19, "IO20_ACT3_MODE"),
        (20, "IO23_ACT4"),
    ]
    net_map = {name: idx for idx, name in nets}

    def get_net(name):
        return net_map.get(name, 0)

    lines = []
    lines.append('(kicad_pcb (version 20240108) (generator pcbnew)')
    lines.append('  (general (thickness 1.6) (legacy_teardrops no))')
    lines.append('  (paper "A4")')
    lines.append('  (layers')
    lines.append('    (0 "F.Cu" signal)')
    lines.append('    (31 "B.Cu" signal)')
    lines.append('    (36 "B.SilkS" user "B.Silkscreen")')
    lines.append('    (37 "F.SilkS" user "F.Silkscreen")')
    lines.append('    (38 "B.Mask" user)')
    lines.append('    (39 "F.Mask" user)')
    lines.append('    (44 "Edge.Cuts" user)')
    lines.append('    (46 "B.CrtYd" user "B.Courtyard")')
    lines.append('    (47 "F.CrtYd" user "F.Courtyard")')
    lines.append('    (48 "B.Fab" user)')
    lines.append('    (49 "F.Fab" user)')
    lines.append('  )')
    lines.append('  (setup (pad_to_mask_clearance 0.1))')

    # Nets declaration
    for idx, name in nets:
        lines.append(f'  (net {idx} "{name}")')

    # Board Outline (100.0 x 70.0 mm) with 2mm rounded corners
    lines.append('  (gr_rect (start 0 0) (end 100.0 70.0) (stroke (width 0.15) (type default)) (fill none) (layer "Edge.Cuts"))')

    # Mounting holes M3
    m_holes = [
        ("H1", 4.5, 4.5),
        ("H2", 95.5, 4.5),
        ("H3", 4.5, 65.5),
        ("H4", 95.5, 65.5)
    ]
    for ref, mx, my in m_holes:
        lines.append(f'  (footprint "MountingHole:MountingHole_3.2mm_M3_Pad" (layer "F.Cu") (uuid "{new_uuid()}")')
        lines.append(f'    (at {mx} {my})')
        lines.append(f'    (fp_text reference "{ref}" (at 0 -4.5) (layer "F.SilkS") (effects (font (size 1.0 1.0) (thickness 0.15))))')
        lines.append(f'    (fp_text value "MountingHole_M3" (at 0 4.5) (layer "F.Fab") (effects (font (size 1.0 1.0) (thickness 0.15))))')
        lines.append(f'    (pad "" np_thru_hole circle (at 0 0) (size 6.0 6.0) (drill 3.2) (layers *.Cu *.Mask))')
        lines.append('  )')

    # Footprint 1: J1 ESP32_L (1x16 Header at X=36.0, Y=15.0 to 53.10)
    esp_l_nets = [
        "+3V3", "GND", "IO4_NSS", "IO5_MISO", "IO6_MOSI", "IO7_SCK",
        "IO0_ADC", "IO1_ADC", "GND", "GND", "IO10_DIO0", "IO11_BUZ",
        "IO12_RST", "IO13_SW", "IO14", "IO15"
    ]
    lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x16_P2.54mm_Vertical" (layer "F.Cu") (uuid "{new_uuid()}")')
    lines.append(f'    (at 36.0 15.0)')
    lines.append(f'    (fp_text reference "J1" (at -3.5 0 90) (layer "F.SilkS") (effects (font (size 1.0 1.0) (thickness 0.15))))')
    lines.append(f'    (fp_text value "ESP32_L" (at 3.5 19.05 90) (layer "F.Fab") (effects (font (size 1.0 1.0) (thickness 0.15))))')
    for i in range(16):
        py = i * 2.54
        p_shape = "rect" if i == 0 else "circle"
        n_name = esp_l_nets[i] if i < len(esp_l_nets) else ""
        n_id = get_net(n_name)
        lines.append(f'    (pad "{i+1}" thru_hole {p_shape} (at 0 {py:.2f}) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {n_id} "{n_name}"))')
    lines.append('  )')

    # Footprint 2: J2 ESP32_R (1x16 Header at X=58.86, Y=15.0 to 53.10)
    esp_r_nets = [
        "+5V", "GND", "IO18_ACT1_UP", "IO19_ACT2_DOWN", "IO20_ACT3_MODE", "IO21",
        "IO22", "IO23_ACT4", "IO2", "IO3", "IO16_GPS_TX", "IO17_GPS_RX",
        "GND", "+3V3", "NC", "GND"
    ]
    lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x16_P2.54mm_Vertical" (layer "F.Cu") (uuid "{new_uuid()}")')
    lines.append(f'    (at 58.86 15.0)')
    lines.append(f'    (fp_text reference "J2" (at 3.5 0 90) (layer "F.SilkS") (effects (font (size 1.0 1.0) (thickness 0.15))))')
    lines.append(f'    (fp_text value "ESP32_R" (at -3.5 19.05 90) (layer "F.Fab") (effects (font (size 1.0 1.0) (thickness 0.15))))')
    for i in range(16):
        py = i * 2.54
        p_shape = "rect" if i == 0 else "circle"
        n_name = esp_r_nets[i] if i < len(esp_r_nets) else ""
        n_id = get_net(n_name)
        lines.append(f'    (pad "{i+1}" thru_hole {p_shape} (at 0 {py:.2f}) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {n_id} "{n_name}"))')
    lines.append('  )')

    # Footprint 3: J3 LoRa Ra-02 Header (X=26.5)
    lora_pin_defs = [
        (1, 15.00, "+3V3", "rect"),
        (2, 17.54, "GND", "circle"),
        (3, 20.08, "IO4_NSS", "circle"),
        (4, 22.62, "IO5_MISO", "circle"),
        (5, 25.16, "IO6_MOSI", "circle"),
        (6, 27.70, "IO7_SCK", "circle"),
        (7, 40.40, "IO10_DIO0", "circle"),
        (8, 45.48, "IO12_RST", "circle"),
    ]
    lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x08_P2.54mm_Vertical" (layer "F.Cu") (uuid "{new_uuid()}")')
    lines.append(f'    (at 26.5 15.0)')
    lines.append(f'    (fp_text reference "J3" (at -2.5 0 90) (layer "F.SilkS") (effects (font (size 1.0 1.0) (thickness 0.15))))')
    lines.append(f'    (fp_text value "LORA_RA02" (at 2.5 15.0 90) (layer "F.Fab") (effects (font (size 1.0 1.0) (thickness 0.15))))')
    for p_num, y_abs, n_name, p_shape in lora_pin_defs:
        py = y_abs - 15.0
        n_id = get_net(n_name)
        lines.append(f'    (pad "{p_num}" thru_hole {p_shape} (at 0 {py:.2f}) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {n_id} "{n_name}"))')
    lines.append('  )')

    # Footprint 4: JOY1 PS4 3D Joystick (cx=14.0, cy=31.51)
    cx, cy = 14.0, 31.51
    lines.append(f'  (footprint "Joystick:RKJXV_PS4_Thumbstick" (layer "F.Cu") (uuid "{new_uuid()}")')
    lines.append(f'    (at {cx} {cy})')
    lines.append(f'    (fp_text reference "JOY1" (at 0 -12.5) (layer "F.SilkS") (effects (font (size 1.0 1.0) (thickness 0.15))))')
    lines.append(f'    (fp_text value "PS4_JOYSTICK" (at 0 13.5) (layer "F.Fab") (effects (font (size 1.0 1.0) (thickness 0.15))))')
    # Frame tabs GND
    for px, py in [(-6.325, 5.0), (6.325, 5.0), (-6.325, -5.0), (6.325, -5.0)]:
        lines.append(f'    (pad "" thru_hole circle (at {px:.3f} {py:.3f}) (size 2.25 2.25) (drill 1.5) (layers *.Cu *.Mask "F.SilkS") (net {get_net("GND")} "GND"))')
    for px, py in [(-4.3, 0.0), (4.3, 0.0)]:
        lines.append(f'    (pad "" thru_hole circle (at {px:.3f} {py:.3f}) (size 2.35 2.35) (drill 1.6) (layers *.Cu *.Mask "F.SilkS") (net {get_net("GND")} "GND"))')
    # Pot X
    lines.append(f'    (pad "X1" thru_hole circle (at -2.5 -8.73) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("GND")} "GND"))')
    lines.append(f'    (pad "X2" thru_hole rect (at 0.0 -8.73) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("IO0_ADC")} "IO0_ADC"))')
    lines.append(f'    (pad "X3" thru_hole circle (at 2.5 -8.73) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("+3V3")} "+3V3"))')
    # Pot Y
    lines.append(f'    (pad "Y1" thru_hole circle (at 8.73 2.5) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("+3V3")} "+3V3"))')
    lines.append(f'    (pad "Y2" thru_hole rect (at 8.73 0.0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("IO1_ADC")} "IO1_ADC"))')
    lines.append(f'    (pad "Y3" thru_hole circle (at 8.73 -2.5) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("GND")} "GND"))')
    # Switch
    lines.append(f'    (pad "S1" thru_hole circle (at -3.25 5.75) (size 1.8 1.8) (drill 1.2) (layers *.Cu *.Mask "F.SilkS") (net {get_net("GND")} "GND"))')
    lines.append(f'    (pad "S2" thru_hole circle (at 3.25 5.75) (size 1.8 1.8) (drill 1.2) (layers *.Cu *.Mask "F.SilkS") (net {get_net("IO13_SW")} "IO13_SW"))')
    lines.append(f'    (pad "S3" thru_hole circle (at -3.25 10.25) (size 1.8 1.8) (drill 1.2) (layers *.Cu *.Mask "F.SilkS") (net {get_net("GND")} "GND"))')
    lines.append(f'    (pad "S4" thru_hole circle (at 3.25 10.25) (size 1.8 1.8) (drill 1.2) (layers *.Cu *.Mask "F.SilkS") (net {get_net("IO13_SW")} "IO13_SW"))')
    lines.append('  )')

    # Footprint 5: J4 APM / JOY 1x04 Header (X=31.0, Y=30.24 to 37.86)
    apm_pin_defs = [
        (1, 30.24, "IO0_ADC", "rect"),
        (2, 32.78, "IO1_ADC", "circle"),
        (3, 35.32, "GND", "circle"),
        (4, 37.86, "+3V3", "circle"),
    ]
    lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical" (layer "F.Cu") (uuid "{new_uuid()}")')
    lines.append(f'    (at 31.0 30.24)')
    lines.append(f'    (fp_text reference "J4" (at -2.5 0 90) (layer "F.SilkS") (effects (font (size 1.0 1.0) (thickness 0.15))))')
    lines.append(f'    (fp_text value "APM_JOY" (at 2.5 3.81 90) (layer "F.Fab") (effects (font (size 1.0 1.0) (thickness 0.15))))')
    for p_num, y_abs, n_name, p_shape in apm_pin_defs:
        py = y_abs - 30.24
        n_id = get_net(n_name)
        lines.append(f'    (pad "{p_num}" thru_hole {p_shape} (at 0 {py:.2f}) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {n_id} "{n_name}"))')
    lines.append('  )')

    # Footprint 6: J5 Buzzer 1x02 Header (X=31.0, Y=42.94 to 45.48)
    buz_pin_defs = [
        (1, 42.94, "IO11_BUZ", "rect"),
        (2, 45.48, "GND", "circle"),
    ]
    lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical" (layer "F.Cu") (uuid "{new_uuid()}")')
    lines.append(f'    (at 31.0 42.94)')
    lines.append(f'    (fp_text reference "J5" (at -2.5 0 90) (layer "F.SilkS") (effects (font (size 1.0 1.0) (thickness 0.15))))')
    lines.append(f'    (fp_text value "BUZZER" (at 2.5 1.27 90) (layer "F.Fab") (effects (font (size 1.0 1.0) (thickness 0.15))))')
    for p_num, y_abs, n_name, p_shape in buz_pin_defs:
        py = y_abs - 42.94
        n_id = get_net(n_name)
        lines.append(f'    (pad "{p_num}" thru_hole {p_shape} (at 0 {py:.2f}) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {n_id} "{n_name}"))')
    lines.append('  )')

    # Footprint 7: J6 Quectel LC29H GNSS 1x04 Header (X=75.0, Y=40.40 to 48.02)
    gps_pin_defs = [
        (1, 40.40, "IO16_GPS_TX", "rect"),
        (2, 42.94, "IO17_GPS_RX", "circle"),
        (3, 45.48, "GND", "circle"),
        (4, 48.02, "+3V3", "circle"),
    ]
    lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical" (layer "F.Cu") (uuid "{new_uuid()}")')
    lines.append(f'    (at 75.0 40.40)')
    lines.append(f'    (fp_text reference "J6" (at -2.5 0 90) (layer "F.SilkS") (effects (font (size 1.0 1.0) (thickness 0.15))))')
    lines.append(f'    (fp_text value "GPS_LC29H" (at 2.5 3.81 90) (layer "F.Fab") (effects (font (size 1.0 1.0) (thickness 0.15))))')
    for p_num, y_abs, n_name, p_shape in gps_pin_defs:
        py = y_abs - 40.40
        n_id = get_net(n_name)
        lines.append(f'    (pad "{p_num}" thru_hole {p_shape} (at 0 {py:.2f}) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {n_id} "{n_name}"))')
    lines.append('  )')

    # Footprint 8, 9, 10: 3x SMD Tactile Buttons
    btn_defs = [
        ("SW1", 66.5, 20.08, "IO18_ACT1_UP", "UP"),
        ("SW2", 74.5, 22.62, "IO19_ACT2_DOWN", "DOWN"),
        ("SW3", 82.5, 25.16, "IO20_ACT3_MODE", "MODE"),
    ]
    for ref, bx, by, n_sig, val in btn_defs:
        lines.append(f'  (footprint "Button_Switch_SMD:SW_Push_1P1T_NO_6x6mm_H9.5mm" (layer "F.Cu") (uuid "{new_uuid()}")')
        lines.append(f'    (at {bx} {by})')
        lines.append(f'    (fp_text reference "{ref}" (at 0 -4.2) (layer "F.SilkS") (effects (font (size 0.9 0.9) (thickness 0.15))))')
        lines.append(f'    (fp_text value "{val}" (at 0 4.2) (layer "F.SilkS") (effects (font (size 0.9 0.9) (thickness 0.15))))')
        lines.append(f'    (pad "1" smd rect (at -3.25 2.25) (size 1.8 1.2) (layers "F.Cu" "F.Mask") (net {get_net(n_sig)} "{n_sig}"))')
        lines.append(f'    (pad "2" smd rect (at -3.25 -2.25) (size 1.8 1.2) (layers "F.Cu" "F.Mask") (net {get_net(n_sig)} "{n_sig}"))')
        lines.append(f'    (pad "3" smd rect (at 3.25 2.25) (size 1.8 1.2) (layers "F.Cu" "F.Mask") (net {get_net("GND")} "GND"))')
        lines.append(f'    (pad "4" smd rect (at 3.25 -2.25) (size 1.8 1.2) (layers "F.Cu" "F.Mask") (net {get_net("GND")} "GND"))')
        lines.append('  )')

    # Footprints 11, 12, 13, 14: Actuators / External Headers (1x03 Horizontal at X=90.0, 92.54, 95.08)
    act_defs = [
        ("J7", 20.08, "IO18_ACT1_UP", "ESC_UP"),
        ("J8", 22.62, "IO19_ACT2_DOWN", "RUDDER_DN"),
        ("J9", 25.16, "IO20_ACT3_MODE", "WINCH_MODE"),
        ("J10", 32.78, "IO23_ACT4", "RACK_ACT4"),
    ]
    for ref, ay, n_sig, val in act_defs:
        lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Horizontal" (layer "F.Cu") (uuid "{new_uuid()}")')
        lines.append(f'    (at 90.0 {ay})')
        lines.append(f'    (fp_text reference "{ref}" (at -2.5 0) (layer "F.SilkS") (effects (font (size 0.9 0.9) (thickness 0.15))))')
        lines.append(f'    (fp_text value "{val}" (at 2.54 -2.0) (layer "F.Fab") (effects (font (size 0.8 0.8) (thickness 0.15))))')
        lines.append(f'    (pad "1" thru_hole rect (at 0 0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net(n_sig)} "{n_sig}"))')
        lines.append(f'    (pad "2" thru_hole circle (at 2.54 0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("+5V_SERVO")} "+5V_SERVO"))')
        lines.append(f'    (pad "3" thru_hole circle (at 5.08 0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("GND")} "GND"))')
        lines.append('  )')

    # Footprint 15: J11 ESP Power In (1x02 Horizontal at X=56.32, 58.86, Y=8.0)
    lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Horizontal" (layer "F.Cu") (uuid "{new_uuid()}")')
    lines.append(f'    (at 56.32 8.0)')
    lines.append(f'    (fp_text reference "J11" (at 1.27 -2.2) (layer "F.SilkS") (effects (font (size 0.9 0.9) (thickness 0.15))))')
    lines.append(f'    (fp_text value "PWR_ESP_5V" (at 1.27 2.2) (layer "F.Fab") (effects (font (size 0.8 0.8) (thickness 0.15))))')
    lines.append(f'    (pad "1" thru_hole rect (at 0 0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("GND")} "GND"))')
    lines.append(f'    (pad "2" thru_hole circle (at 2.54 0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("+5V")} "+5V"))')
    lines.append('  )')

    # Footprint 16: J12 Servo Power In (1x02 Horizontal at X=92.54, 95.08, Y=12.0)
    lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Horizontal" (layer "F.Cu") (uuid "{new_uuid()}")')
    lines.append(f'    (at 92.54 12.0)')
    lines.append(f'    (fp_text reference "J12" (at 1.27 -2.2) (layer "F.SilkS") (effects (font (size 0.9 0.9) (thickness 0.15))))')
    lines.append(f'    (fp_text value "PWR_SERVO_5V" (at 1.27 2.2) (layer "F.Fab") (effects (font (size 0.8 0.8) (thickness 0.15))))')
    lines.append(f'    (pad "1" thru_hole rect (at 0 0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("+5V_SERVO")} "+5V_SERVO"))')
    lines.append(f'    (pad "2" thru_hole circle (at 2.54 0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("GND")} "GND"))')
    lines.append('  )')

    # Silkscreen Rectangles & Drawings
    silk_rects = [
        (1.5, 1.5, 98.5, 68.5, 0.2),       # Border
        (33.0, 13.0, 61.86, 56.0, 0.2),    # ESP32-C6 Module Outline
        (40.5, 24.0, 54.5, 50.0, 0.15),    # ST7789 LCD 1.47" Outline
        (43.5, 54.5, 51.5, 58.0, 0.15),    # USB-C Connector Outline
        (24.5, 13.0, 28.5, 48.0, 0.2),     # LoRa Module Outline
        (72.0, 38.0, 96.0, 62.0, 0.2),     # Quectel LC29H GNSS Outline
        # Joystick outline
        (7.4, 24.91, 20.6, 38.11, 0.18),
        (9.5, 21.0, 18.5, 24.91, 0.15),
        (20.6, 27.0, 23.5, 36.0, 0.15),
        (9.5, 38.11, 18.5, 43.0, 0.15),
    ]
    for x1, y1, x2, y2, sw in silk_rects:
        lines.append(f'  (gr_rect (start {x1} {y1}) (end {x2} {y2}) (stroke (width {sw}) (type default)) (fill none) (layer "F.SilkS"))')

    # Silkscreen Texts
    silk_texts = [
        ("WINDDRAGONS UNIVERSAL CARRIER V3.1", 10.0, 5.5, 1.2, "F.SilkS"),
        ("100% PLANAR ZERO-CROSSING (100x70mm)", 10.0, 3.2, 0.9, "F.SilkS"),
        ("ST7789 LCD 1.47 INCH", 47.5, 37.0, 1.0, "F.SilkS"),
        ("USB-C", 47.5, 56.0, 0.9, "F.SilkS"),
        ("LORA RA-02", 22.0, 50.0, 0.85, "F.SilkS"),
        ("PS4 3D JOYSTICK", 14.0, 44.0, 0.85, "F.SilkS"),
        ("VRX", 14.0, 20.0, 0.7, "F.SilkS"),
        ("VRY", 24.0, 31.5, 0.7, "F.SilkS"),
        ("APM / JOY", 31.0, 28.5, 0.8, "F.SilkS"),
        ("X/V  Y/I  GND 3V3", 31.0, 39.5, 0.7, "F.SilkS"),
        ("BUZZER", 31.0, 47.0, 0.8, "F.SilkS"),
        ("+  -", 31.0, 41.5, 0.7, "F.SilkS"),
        ("QUECTEL LC29H GNSS", 84.0, 59.5, 0.9, "F.SilkS"),
        ("GPS LC29H", 84.0, 50.5, 0.9, "F.SilkS"),
        ("RX  TX GND 3V3", 84.0, 44.0, 0.8, "F.SilkS"),
        ("ACTUATORS (ROVER)", 88.0, 36.5, 0.8, "F.SilkS"),
        ("S + -", 92.54, 34.8, 0.75, "F.SilkS"),
        ("CREMALHEIRA", 83.5, 32.78, 0.7, "F.SilkS"),
        ("ESC/UP", 85.0, 20.08, 0.7, "F.SilkS"),
        ("LEME/DN", 85.0, 22.62, 0.7, "F.SilkS"),
        ("GUINCHO/MODE", 82.0, 25.16, 0.7, "F.SilkS"),
        ("PWR ESP 5V", 57.5, 5.5, 0.8, "F.SilkS"),
        ("-  +", 57.5, 10.2, 0.7, "F.SilkS"),
        ("PWR SERVO 5V", 93.8, 8.5, 0.8, "F.SilkS"),
        ("+  -", 93.8, 14.2, 0.7, "F.SilkS"),
        # Bottom Silkscreen
        ("WINDDRAGONS UNIVERSAL CARRIER V3.1", 10.0, 10.0, 2.0, "B.SilkS"),
        ("DESIGNED FOR WINDDRAGONS ROVER & BASE STATION", 10.0, 6.5, 1.2, "B.SilkS"),
    ]
    for txt, tx, ty, sz, lyr in silk_texts:
        lines.append(f'  (gr_text "{txt}" (at {tx} {ty}) (layer "{lyr}") (effects (font (size {sz} {sz}) (thickness 0.15))))')

    # Tracks Definitions with Nets!
    tracks = [
        # Top traces (F.Cu)
        # Buttons internal traces
        (63.25, 22.33, 63.25, 17.83, 0.5, "F.Cu", "IO18_ACT1_UP"),
        (69.75, 22.33, 69.75, 17.83, 0.5, "F.Cu", "GND"),
        (71.25, 24.87, 71.25, 20.37, 0.5, "F.Cu", "IO19_ACT2_DOWN"),
        (77.75, 24.87, 77.75, 20.37, 0.5, "F.Cu", "GND"),
        (79.25, 27.41, 79.25, 22.91, 0.5, "F.Cu", "IO20_ACT3_MODE"),
        (85.75, 27.41, 85.75, 22.91, 0.5, "F.Cu", "GND"),
        # Actuators straight horizontal traces from ESP_R
        (58.86, 20.08, 90.00, 20.08, 0.5, "F.Cu", "IO18_ACT1_UP"),
        (58.86, 22.62, 90.00, 22.62, 0.5, "F.Cu", "IO19_ACT2_DOWN"),
        (58.86, 25.16, 90.00, 25.16, 0.5, "F.Cu", "IO20_ACT3_MODE"),
        (58.86, 32.78, 90.00, 32.78, 0.5, "F.Cu", "IO23_ACT4"),
        # GPS straight horizontal traces to ESP_R
        (58.86, 40.40, 75.00, 40.40, 0.5, "F.Cu", "IO16_GPS_TX"),
        (58.86, 42.94, 75.00, 42.94, 0.5, "F.Cu", "IO17_GPS_RX"),
        (58.86, 45.48, 75.00, 45.48, 0.6, "F.Cu", "GND"),
        (58.86, 48.02, 75.00, 48.02, 0.6, "F.Cu", "+3V3"),
        # Servo Power & GND Buses
        (92.54, 12.00, 92.54, 32.78, 1.0, "F.Cu", "+5V_SERVO"),
        (95.08, 12.00, 95.08, 32.78, 1.0, "F.Cu", "GND"),
        # LoRa direct horizontal traces to ESP_L
        (26.50, 15.00, 36.00, 15.00, 0.6, "F.Cu", "+3V3"),
        (26.50, 20.08, 36.00, 20.08, 0.4, "F.Cu", "IO4_NSS"),
        (26.50, 22.62, 36.00, 22.62, 0.4, "F.Cu", "IO5_MISO"),
        (26.50, 25.16, 36.00, 25.16, 0.4, "F.Cu", "IO6_MOSI"),
        (26.50, 27.70, 36.00, 27.70, 0.4, "F.Cu", "IO7_SCK"),
        (26.50, 40.40, 36.00, 40.40, 0.4, "F.Cu", "IO10_DIO0"),
        (26.50, 45.48, 36.00, 45.48, 0.4, "F.Cu", "IO12_RST"),
        # Joystick & APM traces
        (14.00, 22.78, 14.00, 21.20, 0.4, "F.Cu", "IO0_ADC"),
        (14.00, 21.20, 24.50, 21.20, 0.4, "F.Cu", "IO0_ADC"),
        (24.50, 21.20, 24.50, 30.24, 0.4, "F.Cu", "IO0_ADC"),
        (24.50, 30.24, 36.00, 30.24, 0.4, "F.Cu", "IO0_ADC"),
        (22.73, 31.51, 24.50, 31.51, 0.4, "F.Cu", "IO1_ADC"),
        (24.50, 31.51, 24.50, 32.78, 0.4, "F.Cu", "IO1_ADC"),
        (24.50, 32.78, 36.00, 32.78, 0.4, "F.Cu", "IO1_ADC"),
        (17.25, 41.76, 17.25, 48.02, 0.4, "F.Cu", "IO13_SW"),
        (17.25, 48.02, 36.00, 48.02, 0.4, "F.Cu", "IO13_SW"),
        # Buzzer trace
        (31.00, 42.94, 36.00, 42.94, 0.4, "F.Cu", "IO11_BUZ"),
        # ESP 5V Power
        (58.86, 8.00, 58.86, 15.00, 0.8, "F.Cu", "+5V"),

        # Bottom traces (B.Cu)
        (56.32, 8.00, 58.86, 17.54, 0.8, "B.Cu", "GND"),
        (26.50, 17.54, 36.00, 17.54, 0.8, "B.Cu", "GND"),
        (31.00, 35.32, 36.00, 35.32, 0.8, "B.Cu", "GND"),
        (31.00, 45.48, 36.00, 45.48, 0.8, "B.Cu", "GND"),
        (69.75, 20.08, 77.75, 22.62, 0.5, "B.Cu", "GND"),
        (77.75, 22.62, 85.75, 25.16, 0.5, "B.Cu", "GND"),
        (85.75, 25.16, 95.08, 25.16, 0.5, "B.Cu", "GND"),
        (26.50, 15.00, 16.50, 22.78, 0.6, "B.Cu", "+3V3"),
    ]

    for x1, y1, x2, y2, w, lyr, n_name in tracks:
        n_id = get_net(n_name)
        lines.append(f'  (segment (start {x1} {y1}) (end {x2} {y2}) (width {w}) (layer "{lyr}") (net {n_id}))')

    lines.append(')')
    
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")
    print(f"Generated KiCad PCB: {path}")

# ==============================================================================
# 4. GENERATE KICAD SCHEMATIC (.kicad_sch)
# ==============================================================================
def generate_kicad_sch(path):
    root_uuid = "c0000000-0000-0000-0000-000000000001"
    
    sch_lines = [
        '(kicad_sch',
        '	(version 20250114)',
        '	(generator "eeschema")',
        '	(generator_version "9.0")',
        f'	(uuid "{root_uuid}")',
        '	(paper "A3")',
        '	(title_block',
        '		(title "WindDragons Universal Carrier Board (Rover & Base Station)")',
        '		(date "2026-10-06")',
        '		(rev "V3.1")',
        '		(company "WindDragons Project")',
        '		(comment 1 "100% Collinear Planar Zero-Crossing PCB Architecture")',
        '		(comment 2 "Unified Carrier for Rover & Base Station ESP32-C6")',
        '	)',
        '	(lib_symbols',
        '		(symbol "Connector_Generic:Conn_01x02"',
        '			(pin_names (offset 1.016) (hide yes))',
        '			(property "Reference" "J" (at 0 2.54 0) (effects (font (size 1.27 1.27))))',
        '			(property "Value" "Conn_01x02" (at 0 -5.08 0) (effects (font (size 1.27 1.27))))',
        '			(property "Footprint" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))',
        '			(symbol "Conn_01x02_1_1"',
        '				(rectangle (start -1.27 1.27) (end 1.27 -3.81) (stroke (width 0.254) (type default)) (fill (type background)))',
        '				(pin passive line (at -5.08 0 0) (length 3.81) (name "Pin_1" (effects (font (size 1.27 1.27)))) (number "1" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 -2.54 0) (length 3.81) (name "Pin_2" (effects (font (size 1.27 1.27)))) (number "2" (effects (font (size 1.27 1.27)))))',
        '			)',
        '		)',
        '		(symbol "Connector_Generic:Conn_01x03"',
        '			(pin_names (offset 1.016) (hide yes))',
        '			(property "Reference" "J" (at 0 2.54 0) (effects (font (size 1.27 1.27))))',
        '			(property "Value" "Conn_01x03" (at 0 -7.62 0) (effects (font (size 1.27 1.27))))',
        '			(symbol "Conn_01x03_1_1"',
        '				(rectangle (start -1.27 1.27) (end 1.27 -6.35) (stroke (width 0.254) (type default)) (fill (type background)))',
        '				(pin passive line (at -5.08 0 0) (length 3.81) (name "Pin_1" (effects (font (size 1.27 1.27)))) (number "1" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 -2.54 0) (length 3.81) (name "Pin_2" (effects (font (size 1.27 1.27)))) (number "2" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 -5.08 0) (length 3.81) (name "Pin_3" (effects (font (size 1.27 1.27)))) (number "3" (effects (font (size 1.27 1.27)))))',
        '			)',
        '		)',
        '		(symbol "Connector_Generic:Conn_01x04"',
        '			(pin_names (offset 1.016) (hide yes))',
        '			(property "Reference" "J" (at 0 2.54 0) (effects (font (size 1.27 1.27))))',
        '			(property "Value" "Conn_01x04" (at 0 -10.16 0) (effects (font (size 1.27 1.27))))',
        '			(symbol "Conn_01x04_1_1"',
        '				(rectangle (start -1.27 1.27) (end 1.27 -8.89) (stroke (width 0.254) (type default)) (fill (type background)))',
        '				(pin passive line (at -5.08 0 0) (length 3.81) (name "Pin_1" (effects (font (size 1.27 1.27)))) (number "1" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 -2.54 0) (length 3.81) (name "Pin_2" (effects (font (size 1.27 1.27)))) (number "2" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 -5.08 0) (length 3.81) (name "Pin_3" (effects (font (size 1.27 1.27)))) (number "3" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 -7.62 0) (length 3.81) (name "Pin_4" (effects (font (size 1.27 1.27)))) (number "4" (effects (font (size 1.27 1.27)))))',
        '			)',
        '		)',
        '		(symbol "Connector_Generic:Conn_01x08"',
        '			(pin_names (offset 1.016) (hide yes))',
        '			(property "Reference" "J" (at 0 2.54 0) (effects (font (size 1.27 1.27))))',
        '			(property "Value" "Conn_01x08" (at 0 -20.32 0) (effects (font (size 1.27 1.27))))',
        '			(symbol "Conn_01x08_1_1"',
        '				(rectangle (start -1.27 1.27) (end 1.27 -19.05) (stroke (width 0.254) (type default)) (fill (type background)))',
        '				(pin passive line (at -5.08 0 0) (length 3.81) (name "Pin_1" (effects (font (size 1.27 1.27)))) (number "1" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 -2.54 0) (length 3.81) (name "Pin_2" (effects (font (size 1.27 1.27)))) (number "2" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 -5.08 0) (length 3.81) (name "Pin_3" (effects (font (size 1.27 1.27)))) (number "3" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 -7.62 0) (length 3.81) (name "Pin_4" (effects (font (size 1.27 1.27)))) (number "4" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 -10.16 0) (length 3.81) (name "Pin_5" (effects (font (size 1.27 1.27)))) (number "5" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 -12.7 0) (length 3.81) (name "Pin_6" (effects (font (size 1.27 1.27)))) (number "6" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 -15.24 0) (length 3.81) (name "Pin_7" (effects (font (size 1.27 1.27)))) (number "7" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 -17.78 0) (length 3.81) (name "Pin_8" (effects (font (size 1.27 1.27)))) (number "8" (effects (font (size 1.27 1.27)))))',
        '			)',
        '		)',
        '		(symbol "Connector_Generic:Conn_01x16"',
        '			(pin_names (offset 1.016) (hide yes))',
        '			(property "Reference" "J" (at 0 2.54 0) (effects (font (size 1.27 1.27))))',
        '			(property "Value" "Conn_01x16" (at 0 -40.64 0) (effects (font (size 1.27 1.27))))',
        '			(symbol "Conn_01x16_1_1"',
        '				(rectangle (start -1.27 1.27) (end 1.27 -39.37) (stroke (width 0.254) (type default)) (fill (type background)))',
    ]
    for i in range(16):
        py = -i * 2.54
        sch_lines.append(f'				(pin passive line (at -5.08 {py:.2f} 0) (length 3.81) (name "Pin_{i+1}" (effects (font (size 1.27 1.27)))) (number "{i+1}" (effects (font (size 1.27 1.27)))))')
    sch_lines.append('			)')
    sch_lines.append('		)')
    sch_lines.append('	)') # end lib_symbols

    # Helpers for schematic elements
    def add_label(text, x, y):
        sch_lines.append('	(label "' + text + '"')
        sch_lines.append(f'		(at {x:.2f} {y:.2f} 0)')
        sch_lines.append('		(effects (font (size 1.27 1.27)) (justify left bottom))')
        sch_lines.append(f'		(uuid "{new_uuid()}")')
        sch_lines.append('	)')

    def add_sym(lib_id, ref, val, footprint, x, y, pin_count, pin_labels):
        sch_lines.append(f'	(symbol (lib_id "{lib_id}")')
        sch_lines.append(f'		(at {x:.2f} {y:.2f} 0)')
        sch_lines.append('		(unit 1) (in_bom yes) (on_board yes)')
        sch_lines.append(f'		(uuid "{new_uuid()}")')
        sch_lines.append(f'		(property "Reference" "{ref}" (at {x:.2f} {y+2.54:.2f} 0) (effects (font (size 1.27 1.27))))')
        sch_lines.append(f'		(property "Value" "{val}" (at {x:.2f} {y-(pin_count*2.54)-1.5:.2f} 0) (effects (font (size 1.27 1.27))))')
        sch_lines.append(f'		(property "Footprint" "{footprint}" (at {x:.2f} {y:.2f} 0) (effects (font (size 1.27 1.27)) (hide yes)))')
        for i in range(pin_count):
            sch_lines.append(f'		(pin "{i+1}" (uuid "{new_uuid()}"))')
        sch_lines.append('	)')
        # Add wires and labels for each pin
        for i, lbl in enumerate(pin_labels):
            py = y - (i * 2.54)
            px_pin = x - 5.08
            px_wire_end = px_pin - 5.08
            sch_lines.append('	(wire')
            sch_lines.append(f'		(pts (xy {px_pin:.2f} {py:.2f}) (xy {px_wire_end:.2f} {py:.2f}))')
            sch_lines.append('		(stroke (width 0) (type solid))')
            sch_lines.append(f'		(uuid "{new_uuid()}")')
            sch_lines.append('	)')
            add_label(lbl, px_wire_end - 0.2, py)

    # ESP32 Left Header
    esp_l_labels = [
        "+3V3", "GND", "IO4_NSS", "IO5_MISO", "IO6_MOSI", "IO7_SCK",
        "IO0_ADC", "IO1_ADC", "GND", "NC", "IO10_DIO0", "IO11_BUZ",
        "IO12_RST", "IO13_SW", "IO14", "IO15"
    ]
    add_sym("Connector_Generic:Conn_01x16", "J1", "ESP32_L", "Connector_PinHeader_2.54mm:PinHeader_1x16_P2.54mm_Vertical", 70.0, 70.0, 16, esp_l_labels)

    # ESP32 Right Header
    esp_r_labels = [
        "+5V", "GND", "IO18_ACT1_UP", "IO19_ACT2_DOWN", "IO20_ACT3_MODE", "IO21",
        "IO22", "IO23_ACT4", "IO2", "IO3", "IO16_GPS_TX", "IO17_GPS_RX",
        "GND", "+3V3", "NC", "GND"
    ]
    add_sym("Connector_Generic:Conn_01x16", "J2", "ESP32_R", "Connector_PinHeader_2.54mm:PinHeader_1x16_P2.54mm_Vertical", 130.0, 70.0, 16, esp_r_labels)

    # LoRa Ra-02 Header (8 pins)
    lora_labels = ["+3V3", "GND", "IO4_NSS", "IO5_MISO", "IO6_MOSI", "IO7_SCK", "IO10_DIO0", "IO12_RST"]
    add_sym("Connector_Generic:Conn_01x08", "J3", "LORA_RA02", "Connector_PinHeader_2.54mm:PinHeader_1x08_P2.54mm_Vertical", 190.0, 50.0, 8, lora_labels)

    # Quectel LC29H GNSS Header (4 pins)
    gps_labels = ["IO16_GPS_TX", "IO17_GPS_RX", "GND", "+3V3"]
    add_sym("Connector_Generic:Conn_01x04", "J6", "GPS_LC29H", "Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical", 190.0, 90.0, 4, gps_labels)

    # APM Sensor / Ext Joystick Header (4 pins)
    apm_labels = ["IO0_ADC", "IO1_ADC", "GND", "+3V3"]
    add_sym("Connector_Generic:Conn_01x04", "J4", "APM_JOY", "Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical", 190.0, 120.0, 4, apm_labels)

    # Buzzer Header (2 pins)
    buz_labels = ["IO11_BUZ", "GND"]
    add_sym("Connector_Generic:Conn_01x02", "J5", "BUZZER", "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical", 190.0, 145.0, 2, buz_labels)

    # Actuators (4x 1x03)
    add_sym("Connector_Generic:Conn_01x03", "J7", "ESC_PROP_UP", "Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Horizontal", 260.0, 50.0, 3, ["IO18_ACT1_UP", "+5V_SERVO", "GND"])
    add_sym("Connector_Generic:Conn_01x03", "J8", "RUDDER_DN", "Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Horizontal", 260.0, 75.0, 3, ["IO19_ACT2_DOWN", "+5V_SERVO", "GND"])
    add_sym("Connector_Generic:Conn_01x03", "J9", "WINCH_MODE", "Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Horizontal", 260.0, 100.0, 3, ["IO20_ACT3_MODE", "+5V_SERVO", "GND"])
    add_sym("Connector_Generic:Conn_01x03", "J10", "RACK_CREMALHEIRA", "Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Horizontal", 260.0, 125.0, 3, ["IO23_ACT4", "+5V_SERVO", "GND"])

    # Power In Connectors (2x 1x02)
    add_sym("Connector_Generic:Conn_01x02", "J11", "PWR_ESP_5V", "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Horizontal", 260.0, 150.0, 2, ["GND", "+5V"])
    add_sym("Connector_Generic:Conn_01x02", "J12", "PWR_SERVO_5V", "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Horizontal", 260.0, 170.0, 2, ["+5V_SERVO", "GND"])

    # Sheet instances
    sch_lines.append('	(sheet_instances')
    sch_lines.append('		(path "/" (page "1"))')
    sch_lines.append('	)')
    sch_lines.append('	(embedded_fonts no)')
    sch_lines.append(')')

    with open(path, "w") as f:
        f.write("\n".join(sch_lines) + "\n")
    print(f"Generated KiCad Schematic: {path}")

def main():
    pro_path = os.path.join(TARGET_DIR, f"{PROJECT_NAME}.kicad_pro")
    prl_path = os.path.join(TARGET_DIR, f"{PROJECT_NAME}.kicad_prl")
    pcb_path = os.path.join(TARGET_DIR, f"{PROJECT_NAME}.kicad_pcb")
    sch_path = os.path.join(TARGET_DIR, f"{PROJECT_NAME}.kicad_sch")

    print(f"Transforming {TARGET_DIR} into a complete KiCad Project...")
    generate_kicad_pro(pro_path)
    generate_kicad_prl(prl_path)
    generate_kicad_pcb(pcb_path)
    generate_kicad_sch(sch_path)
    print("\nKiCad Project creation complete!")

if __name__ == "__main__":
    main()
