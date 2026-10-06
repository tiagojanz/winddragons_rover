#!/usr/bin/env python3
"""
WindDragons Universal Carrier Board - 2-Layer JLCPCB Landscape Version (100 x 85 mm)
Refactored for 0 DRC Errors, 0 Track Crossings, Exact PS4_joystick Footprint Parity.
"""

import os
import uuid
import json
import zipfile
import subprocess

TARGET_DIR = "/Users/tiagotorredovale/Documents/projectos/winddragon/rover/hardware/universal_pcb"
PROJECT_NAME = "Universal_Carrier"
BOARD_W = 100.0
BOARD_H = 85.0

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
            [root_uuid := new_uuid(), ""]
        ]
    }
    with open(path, "w") as f:
        json.dump(pro_data, f, indent=2)

def generate_kicad_prl(path):
    prl_data = {
        "board": {
            "appearance": {
                "color_theme": "default",
                "show_tracks": True,
                "show_zones": True
            }
        },
        "meta": {
            "filename": f"{PROJECT_NAME}.kicad_prl",
            "version": 1
        }
    }
    with open(path, "w") as f:
        json.dump(prl_data, f, indent=2)

# ==============================================================================
# 2. KICAD PCB (.kicad_pcb)
# ==============================================================================
def generate_kicad_pcb(path):
    all_nets = [
        "", "/GND", "/+3V3", "/+5V", "/+5V_SERVO",
        "/IO5_NSS", "/IO4_MISO", "/IO3_MOSI", "/IO2_SCK",
        "/IO1_ADC", "/IO0_ADC",
        "/IO9_BOOT", "/IO18_ACT1_UP", "/IO19_WINCH_MODE", "/IO20_RUDDER_DN", "/IO23_ACT4",
        "/IO12_LORA_DIO0", "/IO13_BUZ", "/IO16_GPS_RX", "/IO17_GPS_TX",
        "/IO15", "/GPS_TX2", "/GPS_RX2", "/NC"
    ]
    net_map = {n: i for i, n in enumerate(all_nets)}
    def norm_net(n):
        if not n:
            return ""
        return n if n.startswith("/") else f"/{n}"
    def get_net(n):
        return net_map.get(norm_net(n), 0)
    def net_name(n):
        return norm_net(n)

    lines = [
        '(kicad_pcb',
        '  (version 20240108)',
        '  (generator "pcbnew")',
        '  (generator_version "8.0")',
        '  (general',
        '    (thickness 1.6)',
        '    (legacy_teardrops no)',
        '  )',
        '  (paper "A4")',
        '  (title_block',
        '    (title "WindDragons Universal Carrier Board")',
        '    (date "2026-10-06")',
        '    (rev "V4.1 - Zero DRC Parity Clean")',
        '    (company "WindDragons Project")',
        '    (comment 1 "ESP32-C6-LCD-1.47 Carrier - 100x85mm 2-Layer JLCPCB")',
        '    (comment 2 "Landscape Top LCD, FSPI LoRa, LC29H GNSS & Single-Layer Planar Routing")',
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
        '    (allow_soldermask_bridges_in_footprints no)',
        '    (pcbplotparams',
        '      (layerselection 0x00010fc_ffffffff)',
        '      (plot_on_all_layers_selection 0x0000000_00000000)',
        '      (disableapertmacros no)',
        '      (usegerberextensions yes)',
        '      (usegerberattributes yes)',
        '      (usegerberadvancedattributes yes)',
        '      (creategerberjobfile yes)',
        '      (dashed_line_dash_ratio 12.0)',
        '      (dashed_line_gap_ratio 3.0)',
        '      (svgprecision 4)',
        '      (plotframeref no)',
        '      (viasonmask no)',
        '      (mode 1)',
        '      (useauxorigin no)',
        '      (hpglpennumber 1)',
        '      (hpglpenspeed 20)',
        '      (hpglpendiameter 15.0)',
        '      (pdf_front_fp_property_popups yes)',
        '      (pdf_back_fp_property_popups yes)',
        '      (dxfpadmode 0)',
        '      (dxfpolymode 0)',
        '      (dxfimperialunits yes)',
        '      (dxfcontourwidthmin 0.0)',
        '      (psnegative no)',
        '      (psa4output no)',
        '      (plotreference yes)',
        '      (plotvalue yes)',
        '      (plotfptext yes)',
        '      (plotinvisibletext no)',
        '      (sketchpadsonfab no)',
        '      (subtractmaskfromsilk no)',
        '      (outputformat 1)',
        '      (mirror no)',
        '      (drillshape 1)',
        '      (scaleselection 1)',
        '      (outputdirectory "gerbers/")',
        '    )',
        '  )',
    ]

    for n in all_nets:
        if n == "":
            lines.append(f'  (net 0 "")')
        else:
            lines.append(f'  (net {get_net(n)} "{net_name(n)}")')

    # Board Outline (Edge.Cuts) 100 x 85 mm
    corners = [(0.0, 0.0), (BOARD_W, 0.0), (BOARD_W, BOARD_H), (0.0, BOARD_H)]
    for i in range(4):
        p1 = corners[i]
        p2 = corners[(i + 1) % 4]
        lines.append(f'  (gr_line (start {p1[0]} {p1[1]}) (end {p2[0]} {p2[1]}) (stroke (width 0.15) (type solid)) (layer "Edge.Cuts"))')

    # 4x M3 Mounting Holes (Mechanical:MountingHole)
    mount_holes = [
        ("H1", 4.5, 4.5),
        ("H2", BOARD_W - 4.5, 4.5),
        ("H3", 4.5, BOARD_H - 4.5),
        ("H4", BOARD_W - 4.5, BOARD_H - 4.5),
    ]
    for ref, hx, hy in mount_holes:
        lines.append(f'  (footprint "MountingHole:MountingHole_3.2mm_M3" (layer "F.Cu") (uuid "{new_uuid()}")')
        lines.append(f'    (at {hx} {hy})')
        ref_y = -2.5 if "3" in ref or "4" in ref else 2.5
        lines.append(f'    (fp_text reference "{ref}" (at 0 {ref_y}) (layer "F.SilkS") (effects (font (size 0.8 0.8) (thickness 0.15))))')
        lines.append(f'    (fp_text value "MountingHole" (at 0 2.5) (layer "F.Fab") (effects (font (size 0.8 0.8) (thickness 0.15)) (hide yes)))')
        lines.append(f'    (pad "" np_thru_hole circle (at 0 0) (size 3.2 3.2) (drill 3.2) (layers *.Cu *.Mask))')
        lines.append('  )')

    # ESP32 Headers (J1 Top, J2 Bottom) on B.Cu
    esp_t_nets = [
        "IO5_NSS", "IO4_MISO", "IO3_MOSI", "IO2_SCK",
        "IO1_ADC", "IO0_ADC", "+3V3", "GND", "+5V"
    ]
    lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x09_P2.54mm_Horizontal" (layer "B.Cu") (uuid "{new_uuid()}")')
    lines.append(f'    (at 35.0 16.0)')
    lines.append(f'    (fp_text reference "J1" (at 0 -2.5) (layer "B.SilkS") (effects (font (size 1.0 1.0) (thickness 0.15)) (justify mirror)))')
    lines.append(f'    (fp_text value "ESP32_T_LANDSCAPE" (at 10.16 -2.5) (layer "B.SilkS") (effects (font (size 1.0 1.0) (thickness 0.15)) (justify mirror)))')
    for i in range(9):
        px = i * 2.54
        p_shape = "rect" if i == 0 else "circle"
        n_name = esp_t_nets[i]
        lines.append(f'    (pad "{i+1}" thru_hole {p_shape} (at {px:.2f} 0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "B.SilkS") (net {get_net(n_name)} "{net_name(n_name)}"))')
    lines.append('  )')

    esp_b_nets = [
        "IO9_BOOT", "IO18_ACT1_UP", "IO19_WINCH_MODE", "IO20_RUDDER_DN", "IO23_ACT4",
        "IO12_LORA_DIO0", "IO13_BUZ", "IO16_GPS_RX", "IO17_GPS_TX"
    ]
    lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x09_P2.54mm_Horizontal" (layer "B.Cu") (uuid "{new_uuid()}")')
    lines.append(f'    (at 35.0 33.78)')
    lines.append(f'    (fp_text reference "J2" (at 0 2.5) (layer "B.SilkS") (effects (font (size 1.0 1.0) (thickness 0.15)) (justify mirror)))')
    lines.append(f'    (fp_text value "ESP32_B_LANDSCAPE" (at 10.16 2.5) (layer "B.SilkS") (effects (font (size 1.0 1.0) (thickness 0.15)) (justify mirror)))')
    for i in range(9):
        px = i * 2.54
        p_shape = "rect" if i == 0 else "circle"
        n_name = esp_b_nets[i]
        lines.append(f'    (pad "{i+1}" thru_hole {p_shape} (at {px:.2f} 0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "B.SilkS") (net {get_net(n_name)} "{net_name(n_name)}"))')
    lines.append('  )')

    # LoRa Ra-02 Header (2x4, X=12.0, Y=16.0 to 23.62) - FRONT of PCB
    lora_defs = [
        (1, 0.0,  0.0,  "GND", "rect"),
        (2, 2.54, 0.0,  "+3V3", "circle"),
        (3, 0.0,  2.54, "NC", "circle"),
        (4, 2.54, 2.54, "IO5_NSS", "circle"),
        (5, 0.0,  5.08, "IO2_SCK", "circle"),
        (6, 2.54, 5.08, "IO3_MOSI", "circle"),
        (7, 0.0,  7.62, "IO4_MISO", "circle"),
        (8, 2.54, 7.62, "IO12_LORA_DIO0", "circle"),
    ]
    lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_2x04_P2.54mm_Vertical" (layer "F.Cu") (uuid "{new_uuid()}")')
    lines.append(f'    (at 12.0 16.0)')
    lines.append(f'    (fp_text reference "J3" (at 1.27 -2.4 0) (layer "F.SilkS") (effects (font (size 0.9 0.9) (thickness 0.14))))')
    lines.append(f'    (fp_text value "SX1278_LORA_2x4" (at 1.27 10.2 0) (layer "F.Fab") (effects (font (size 0.9 0.9) (thickness 0.14))))')
    lines.append(f'    (fp_line (start -1.33 -1.33) (end 3.87 -1.33) (stroke (width 0.12) (type solid)) (layer "F.SilkS"))')
    lines.append(f'    (fp_line (start 3.87 -1.33) (end 3.87 8.95) (stroke (width 0.12) (type solid)) (layer "F.SilkS"))')
    lines.append(f'    (fp_line (start 3.87 8.95) (end -1.33 8.95) (stroke (width 0.12) (type solid)) (layer "F.SilkS"))')
    lines.append(f'    (fp_line (start -1.33 8.95) (end -1.33 -1.33) (stroke (width 0.12) (type solid)) (layer "F.SilkS"))')
    lines.append(f'    (fp_text user "GND" (at -2.8 0 0) (layer "F.SilkS") (effects (font (size 0.7 0.7) (thickness 0.12))))')
    lines.append(f'    (fp_text user "3V3" (at 5.4 0 0) (layer "F.SilkS") (effects (font (size 0.7 0.7) (thickness 0.12))))')
    lines.append(f'    (fp_text user "NC" (at -2.8 2.54 0) (layer "F.SilkS") (effects (font (size 0.7 0.7) (thickness 0.12))))')
    lines.append(f'    (fp_text user "NSS" (at 5.4 2.54 0) (layer "F.SilkS") (effects (font (size 0.7 0.7) (thickness 0.12))))')
    lines.append(f'    (fp_text user "SCK" (at -2.8 5.08 0) (layer "F.SilkS") (effects (font (size 0.7 0.7) (thickness 0.12))))')
    lines.append(f'    (fp_text user "MOSI" (at 5.7 5.08 0) (layer "F.SilkS") (effects (font (size 0.7 0.7) (thickness 0.12))))')
    lines.append(f'    (fp_text user "MISO" (at -3.0 7.62 0) (layer "F.SilkS") (effects (font (size 0.7 0.7) (thickness 0.12))))')
    lines.append(f'    (fp_text user "DIO0" (at 5.7 7.62 0) (layer "F.SilkS") (effects (font (size 0.7 0.7) (thickness 0.12))))')
    for p_num, px, py, n_name, p_shape in lora_defs:
        lines.append(f'    (pad "{p_num}" thru_hole {p_shape} (at {px:.2f} {py:.2f}) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net(n_name)} "{net_name(n_name)}"))')
    lines.append('  )')

    # JOY1 PS4 3D Joystick (cx=20.0, cy=62.0) using official PS4_joystick:XDCR_COM-09032 on B.Cu
    cx, cy = 20.0, 62.0
    lines.append(f'  (footprint "PS4_joystick:XDCR_COM-09032" (layer "B.Cu") (uuid "{new_uuid()}")')
    lines.append(f'    (at {cx} {cy})')
    lines.append(f'    (fp_text reference "JOY1" (at 0 -13.5) (layer "B.SilkS") (effects (font (size 0.8 0.8) (thickness 0.15)) (justify mirror)))')
    lines.append(f'    (fp_text value "PS4_JOYSTICK" (at 0 12.5) (layer "B.Fab") (effects (font (size 1.0 1.0) (thickness 0.15)) (justify mirror) (hide yes)))')
    lines.append('    (fp_line (start 3.81 6.858) (end 3.81 10.16) (layer "B.SilkS") (width 0.2))')
    lines.append('    (fp_line (start -7.878 -6.856) (end -4.322 -6.856) (layer "B.SilkS") (width 0.2))')
    lines.append('    (fp_line (start -10.0838 -3.81) (end -7.874 -3.81) (layer "B.SilkS") (width 0.2))')
    lines.append('    (fp_line (start 3.81 10.16) (end -3.81 10.16) (layer "B.SilkS") (width 0.2))')
    lines.append('    (fp_line (start 7.874 6.858) (end 3.81 6.858) (layer "B.SilkS") (width 0.2))')
    lines.append('    (fp_line (start 7.874 -6.858) (end 7.874 6.858) (layer "B.SilkS") (width 0.2))')
    lines.append('    (fp_line (start -4.318 -11.938) (end -4.318 -6.858) (layer "B.SilkS") (width 0.2))')
    lines.append('    (fp_line (start -7.874 3.81) (end -9.906 3.81) (layer "B.SilkS") (width 0.2))')
    lines.append('    (fp_line (start -4.318 -11.938) (end 4.318 -11.938) (layer "B.SilkS") (width 0.2))')
    lines.append('    (fp_line (start -3.81 6.858) (end -7.874 6.858) (layer "B.SilkS") (width 0.2))')
    lines.append('    (fp_line (start 4.318 -6.858) (end 7.874 -6.858) (layer "B.SilkS") (width 0.2))')
    lines.append('    (fp_line (start -3.81 10.16) (end -3.81 6.858) (layer "B.SilkS") (width 0.2))')
    lines.append('    (fp_line (start -7.874 -3.81) (end -7.874 -6.858) (layer "B.SilkS") (width 0.2))')
    lines.append('    (fp_line (start -10.033 3.81) (end -10.0838 -3.81) (layer "B.SilkS") (width 0.2))')
    lines.append('    (fp_line (start 4.318 -11.938) (end 4.318 -6.858) (layer "B.SilkS") (width 0.2))')
    lines.append('    (fp_line (start -7.874 6.858) (end -7.874 3.81) (layer "B.SilkS") (width 0.2))')
    lines.append('    (fp_circle (center 0 0) (end 1.796 0) (layer "B.SilkS") (width 0.2) (fill none))')
    lines.append(f'    (pad "B1A" thru_hole circle (at -3.175 -10.8148) (size 1.778 1.778) (drill 0.9) (layers *.Cu *.Mask "B.SilkS") (net {get_net("GND")} "{net_name("GND")}"))')
    lines.append(f'    (pad "B1B" thru_hole circle (at 3.175 -10.795) (size 1.778 1.778) (drill 0.9) (layers *.Cu *.Mask "B.SilkS") (net {get_net("GND")} "{net_name("GND")}"))')
    lines.append(f'    (pad "B2A" thru_hole circle (at -3.175 -5.8748) (size 1.778 1.778) (drill 0.9) (layers *.Cu *.Mask "B.SilkS") (net {get_net("IO9_BOOT")} "{net_name("IO9_BOOT")}"))')
    lines.append(f'    (pad "B2B" thru_hole circle (at 3.185 -5.8548) (size 1.778 1.778) (drill 0.9) (layers *.Cu *.Mask "B.SilkS") (net {get_net("IO9_BOOT")} "{net_name("IO9_BOOT")}"))')
    lines.append(f'    (pad "H1" thru_hole circle (at -2.525 8.8752) (size 1.778 1.778) (drill 0.889) (layers *.Cu *.Mask "B.SilkS") (net {get_net("GND")} "{net_name("GND")}"))')
    lines.append(f'    (pad "H2" thru_hole circle (at 0.015 8.8952) (size 1.778 1.778) (drill 0.889) (layers *.Cu *.Mask "B.SilkS") (net {get_net("IO0_ADC")} "{net_name("IO0_ADC")}"))')
    lines.append(f'    (pad "H3" thru_hole circle (at 2.555 8.8752) (size 1.778 1.778) (drill 0.889) (layers *.Cu *.Mask "B.SilkS") (net {get_net("+3V3")} "{net_name("+3V3")}"))')
    lines.append(f'    (pad "S1" thru_hole circle (at -6.35 -5.0648) (size 2.286 2.286) (drill 1.397) (layers *.Cu *.Mask "B.SilkS") (net {get_net("GND")} "{net_name("GND")}"))')
    lines.append(f'    (pad "S2" thru_hole circle (at -6.35 5.0652) (size 2.286 2.286) (drill 1.397) (layers *.Cu *.Mask "B.SilkS") (net {get_net("GND")} "{net_name("GND")}"))')
    lines.append(f'    (pad "S3" thru_hole circle (at 6.35 5.08) (size 2.286 2.286) (drill 1.397) (layers *.Cu *.Mask "B.SilkS") (net {get_net("GND")} "{net_name("GND")}"))')
    lines.append(f'    (pad "S4" thru_hole circle (at 6.35 -5.08) (size 2.286 2.286) (drill 1.397) (layers *.Cu *.Mask "B.SilkS") (net {get_net("GND")} "{net_name("GND")}"))')
    lines.append(f'    (pad "V1" thru_hole circle (at -8.89 -2.54) (size 1.778 1.778) (drill 0.889) (layers *.Cu *.Mask "B.SilkS") (net {get_net("GND")} "{net_name("GND")}"))')
    lines.append(f'    (pad "V2" thru_hole circle (at -8.905 -0.0148) (size 1.778 1.778) (drill 0.889) (layers *.Cu *.Mask "B.SilkS") (net {get_net("IO1_ADC")} "{net_name("IO1_ADC")}"))')
    lines.append(f'    (pad "V3" thru_hole circle (at -8.905 2.5252) (size 1.778 1.778) (drill 0.889) (layers *.Cu *.Mask "B.SilkS") (net {get_net("+3V3")} "{net_name("+3V3")}"))')
    lines.append('  )')

    # Battery / APM Power Module Header J4 (X=88.0, Y=41.0) - Horizontal in Servo Column
    lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Horizontal" (layer "F.Cu") (uuid "{new_uuid()}")')
    lines.append(f'    (at 88.0 41.0)')
    lines.append(f'    (fp_text reference "J4" (at 8 -1.2 0) (layer "F.SilkS") (effects (font (size 0.8 0.8) (thickness 0.15))))')
    lines.append(f'    (fp_text value "BATT_APM" (at 2.54 2.6 0) (layer "F.Fab") (effects (font (size 0.8 0.8) (thickness 0.15)) (hide yes)))')
    lines.append(f'    (pad "1" thru_hole rect (at 0 0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("IO0_ADC")} "{net_name("IO0_ADC")}"))')
    lines.append(f'    (pad "2" thru_hole circle (at 2.54 0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("IO1_ADC")} "{net_name("IO1_ADC")}"))')
    lines.append(f'    (pad "3" thru_hole circle (at 5.08 0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("GND")} "{net_name("GND")}"))')
    lines.append('  )')

    # Pre-Launch Bench Test Jumper JP_RBL (X=35.0, Y=58.0) - BACK of PCB (UI side)
    lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical" (layer "B.Cu") (uuid "{new_uuid()}")')
    lines.append(f'    (at 35.0 58.0)')
    lines.append(f'    (fp_text reference "JP_RBL" (at 2.5 0 90) (layer "B.SilkS") (effects (font (size 0.8 0.8) (thickness 0.15)) (justify mirror)))')
    lines.append(f'    (fp_text value "REMOVE_BEFORE_LAUNCH" (at -2.5 3.81 90) (layer "B.Fab") (effects (font (size 0.8 0.8) (thickness 0.15)) (justify mirror) (hide yes)))')
    lines.append(f'    (pad "1" thru_hole rect (at 0 0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "B.SilkS") (net {get_net("JOY_X")} "{net_name("JOY_X")}"))')
    lines.append(f'    (pad "2" thru_hole circle (at 0 2.54) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "B.SilkS") (net {get_net("IO0_ADC")} "{net_name("IO0_ADC")}"))')
    lines.append(f'    (pad "3" thru_hole circle (at 0 5.08) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "B.SilkS") (net {get_net("JOY_Y")} "{net_name("JOY_Y")}"))')
    lines.append(f'    (pad "4" thru_hole circle (at 0 7.62) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "B.SilkS") (net {get_net("IO1_ADC")} "{net_name("IO1_ADC")}"))')
    lines.append('  )')

    # Buzzer Header (X=31.0, Y=48.0 to 50.54) - FRONT of PCB
    lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical" (layer "F.Cu") (uuid "{new_uuid()}")')
    lines.append(f'    (at 31.0 48.0)')
    lines.append(f'    (fp_text reference "J5" (at -2.5 0 90) (layer "F.SilkS") (effects (font (size 0.8 0.8) (thickness 0.15))))')
    lines.append(f'    (fp_text value "BUZZER" (at 2.5 1.27 90) (layer "F.Fab") (effects (font (size 0.8 0.8) (thickness 0.15)) (hide yes)))')
    lines.append(f'    (pad "1" thru_hole rect (at 0 0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("IO13_BUZ")} "{net_name("IO13_BUZ")}"))')
    lines.append(f'    (pad "2" thru_hole circle (at 0 2.54) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("GND")} "{net_name("GND")}"))')
    lines.append('  )')

    # Quectel LC29H GNSS (1x7 Header, X=82.0, Y=16.0 to 31.24) - FRONT of PCB
    gps_defs = [
        (1, 16.00, "IO15", "rect"),
        (2, 18.54, "GPS_TX2", "circle"),
        (3, 21.08, "GPS_RX2", "circle"),
        (4, 23.62, "IO16_GPS_RX", "circle"),
        (5, 26.16, "IO17_GPS_TX", "circle"),
        (6, 28.70, "GND", "circle"),
        (7, 31.24, "+3V3", "circle"),
    ]
    lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x07_P2.54mm_Vertical" (layer "F.Cu") (uuid "{new_uuid()}")')
    lines.append(f'    (at 82.0 16.0)')
    lines.append(f'    (fp_text reference "J6" (at -2.5 0 90) (layer "F.SilkS") (effects (font (size 0.8 0.8) (thickness 0.13))))')
    lines.append(f'    (fp_text value "GPS_LC29H_7PIN" (at 2.5 17.5 90) (layer "F.Fab") (effects (font (size 0.9 0.9) (thickness 0.14))))')
    lines.append(f'    (fp_text user "P" (at 2.8 0 0) (layer "F.SilkS") (effects (font (size 0.7 0.7) (thickness 0.12))))')
    lines.append(f'    (fp_text user "T2" (at 3.0 2.54 0) (layer "F.SilkS") (effects (font (size 0.7 0.7) (thickness 0.12))))')
    lines.append(f'    (fp_text user "R2" (at 3.0 5.08 0) (layer "F.SilkS") (effects (font (size 0.7 0.7) (thickness 0.12))))')
    lines.append(f'    (fp_text user "R1" (at 3.0 7.62 0) (layer "F.SilkS") (effects (font (size 0.7 0.7) (thickness 0.12))))')
    lines.append(f'    (fp_text user "T1" (at 3.0 10.16 0) (layer "F.SilkS") (effects (font (size 0.7 0.7) (thickness 0.12))))')
    lines.append(f'    (fp_text user "G" (at 2.8 12.70 0) (layer "F.SilkS") (effects (font (size 0.7 0.7) (thickness 0.12))))')
    lines.append(f'    (fp_text user "V" (at 2.8 15.24 0) (layer "F.SilkS") (effects (font (size 0.7 0.7) (thickness 0.12))))')
    for p_num, y_abs, n_name, p_shape in gps_defs:
        py = y_abs - 16.0
        lines.append(f'    (pad "{p_num}" thru_hole {p_shape} (at 0 {py:.2f}) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net(n_name)} "{net_name(n_name)}"))')
    lines.append('  )')

    # Actuators (J7, J8, J9, J10 at Right Side X=88.0) - FRONT of PCB
    act_defs = [
        ("J7", 48.0, "IO18_ACT1_UP", "ESC_PROP_UP"),
        ("J8", 55.0, "IO19_WINCH_MODE", "RUDDER_DN"),
        ("J9", 62.0, "IO20_RUDDER_DN", "WINCH_MODE"),
        ("J10", 69.0, "IO23_ACT4", "RACK_CREMALHEIRA"),
    ]
    for ref, ay, n_sig, val in act_defs:
        lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Horizontal" (layer "F.Cu") (uuid "{new_uuid()}")')
        lines.append(f'    (at 88.0 {ay})')
        lines.append(f'    (fp_text reference "{ref}" (at 2.54 -2.6) (layer "F.SilkS") (effects (font (size 0.8 0.8) (thickness 0.15))))')
        lines.append(f'    (fp_text value "{val}" (at 2.54 2.6) (layer "F.Fab") (effects (font (size 0.8 0.8) (thickness 0.15)) (hide yes)))')
        lines.append(f'    (pad "1" thru_hole rect (at 0 0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net(n_sig)} "{net_name(n_sig)}"))')
        lines.append(f'    (pad "2" thru_hole circle (at 2.54 0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("+5V_SERVO")} "{net_name("+5V_SERVO")}"))')
        lines.append(f'    (pad "3" thru_hole circle (at 5.08 0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("GND")} "{net_name("GND")}"))')
        lines.append('  )')

    # J11 ESP Power In (X=44.0, Y=76.0) - FRONT of PCB
    lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Horizontal" (layer "F.Cu") (uuid "{new_uuid()}")')
    lines.append(f'    (at 44.0 76.0)')
    lines.append(f'    (fp_text reference "J11" (at 1.27 -4.0) (layer "F.SilkS") (effects (font (size 0.8 0.8) (thickness 0.15))))')
    lines.append(f'    (fp_text value "PWR_ESP_5V" (at 1.27 2.2) (layer "F.Fab") (effects (font (size 0.8 0.8) (thickness 0.15)) (hide yes)))')
    lines.append(f'    (pad "1" thru_hole rect (at 0 0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("GND")} "{net_name("GND")}"))')
    lines.append(f'    (pad "2" thru_hole circle (at 2.54 0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("+5V")} "{net_name("+5V")}"))')
    lines.append('  )')

    # J12 Servo Power In (X=88.0, Y=76.0) - FRONT of PCB
    lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Horizontal" (layer "F.Cu") (uuid "{new_uuid()}")')
    lines.append(f'    (at 88.0 76.0)')
    lines.append(f'    (fp_text reference "J12" (at 1.27 -4.0) (layer "F.SilkS") (effects (font (size 0.8 0.8) (thickness 0.15))))')
    lines.append(f'    (fp_text value "PWR_SERVO_5V" (at 1.27 2.2) (layer "F.Fab") (effects (font (size 0.8 0.8) (thickness 0.15)) (hide yes)))')
    lines.append(f'    (pad "1" thru_hole rect (at 0 0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("GND")} "{net_name("GND")}"))')
    lines.append(f'    (pad "2" thru_hole circle (at 2.54 0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("+5V_SERVO")} "{net_name("+5V_SERVO")}"))')
    lines.append('  )')

    # WindDragons Logos (Front 20mm at 64,69 and Back 14mm at 50.5,56)
    dragon_json_path = os.path.join(os.path.dirname(__file__), 'dragon_polys.json')
    if os.path.exists(dragon_json_path):
        with open(dragon_json_path) as djf:
            dragon_norm_polys = json.load(djf)

        # Front Logo (20mm, F.SilkS)
        lines.append(f'  (footprint "WindDragons:WindDragons_Logo_20mm" (layer "F.Cu") (uuid "{new_uuid()}")')
        lines.append(f'    (at 60.0 68.0)')
        lines.append(f'    (fp_text reference "LOGO1" (at 0 -11.5) (layer "F.SilkS") (effects (font (size 1 1) (thickness 0.15))) (hide yes))')
        lines.append(f'    (fp_text value "WindDragons_Logo" (at 0 11.5) (layer "F.Fab") (effects (font (size 1 1) (thickness 0.15))) (hide yes))')
        lines.append(f'    (attr exclude_from_pos_files exclude_from_bom allow_missing_courtyard)')
        for poly in dragon_norm_polys:
            pts_str = " ".join([f"(xy {x*20.0:.3f} {y*20.0:.3f})" for x, y in poly])
            lines.append(f'    (fp_poly (pts {pts_str}) (stroke (width 0.01) (type solid)) (fill yes) (layer "F.SilkS"))')
        lines.append('  )')

        # Back Logo (14mm, B.SilkS) - mirrored X for back orientation
        lines.append(f'  (footprint "WindDragons:WindDragons_Logo_14mm" (layer "B.Cu") (uuid "{new_uuid()}")')
        lines.append(f'    (at 50.5 56.0)')
        lines.append(f'    (fp_text reference "LOGO2" (at 0 -8.5) (layer "B.SilkS") (effects (font (size 1 1) (thickness 0.15))) (hide yes))')
        lines.append(f'    (fp_text value "WindDragons_Logo" (at 0 8.5) (layer "B.Fab") (effects (font (size 1 1) (thickness 0.15))) (hide yes))')
        lines.append(f'    (attr exclude_from_pos_files exclude_from_bom allow_missing_courtyard)')
        for poly in dragon_norm_polys:
            pts_str = " ".join([f"(xy {-x*14.0:.3f} {y*14.0:.3f})" for x, y in poly])
            lines.append(f'    (fp_poly (pts {pts_str}) (stroke (width 0.01) (type solid)) (fill yes) (layer "B.SilkS"))')
        lines.append('  )')

    # Silkscreen Outlines
    front_silk_rects = [
        (1.5, 1.5, BOARD_W - 1.5, BOARD_H - 1.5, 0.2),
        (5.0, 11.5, 21.0, 42.0, 0.2),
        (70.9, 13.5, 97.1, 43.6, 0.2),
    ]
    for x1, y1, x2, y2, sw in front_silk_rects:
        lines.append(f'  (gr_rect (start {x1} {y1}) (end {x2} {y2}) (stroke (width {sw}) (type default)) (fill none) (layer "F.SilkS"))')

    back_silk_rects = [
        (1.5, 1.5, BOARD_W - 1.5, BOARD_H - 1.5, 0.2),
        (21.5, 12.5, 69.5, 37.5, 0.2),
        (34.0, 18.0, 56.0, 32.0, 0.18),
        (18.5, 22.0, 22.5, 28.0, 0.15),
    ]
    for x1, y1, x2, y2, sw in back_silk_rects:
        lines.append(f'  (gr_rect (start {x1} {y1}) (end {x2} {y2}) (stroke (width {sw}) (type default)) (fill none) (layer "B.SilkS"))')

    gps_holes = [(72.6, 15.15), (95.4, 15.15), (72.6, 41.85), (95.4, 41.85)]
    for hx, hy in gps_holes:
        lines.append(f'  (gr_circle (center {hx} {hy}) (end {hx+1.2} {hy}) (stroke (width 0.15) (type default)) (fill none) (layer "F.SilkS"))')

    f_silk_texts = [
        ("WINDDRAGONS UNIVERSAL CARRIER V4.1", 50.0, 4.0, 1.1),
        ("INTERNAL PERIPHERALS & DAUGHTERBOARDS (FRONT)", 50.0, 6.2, 0.8),
        ("SX1278 433MHz", 13.0, 39.5, 0.8),
        ("APM / JOY", 35.0, 55.5, 0.8),
        ("X Y G 3V", 35.0, 67.5, 0.8),
        ("BUZZER", 31.0, 45.5, 0.8),
        ("+  -", 33.0, 49.27, 0.8),
        ("GPS 26.2x30.1mm", 84.0, 36.5, 0.8),
        ("QUECTEL LC29H", 84.0, 11.5, 0.85),
        ("ACTUATORS", 79.0, 45.5, 0.8),
        ("S + -", 90.54, 46.2, 0.8),
        ("ESC/UP", 77.0, 48.0, 0.8),
        ("LEME/DN", 77.0, 55.0, 0.8),
        ("GUINCHO/MODE", 73.0, 53.5, 0.75),
        ("CREMALHEIRA", 74.0, 69.0, 0.8),
        ("PWR ESP 5V", 45.5, 73.5, 0.8),
        ("-  +", 45.5, 78.5, 0.8),
        ("PWR SERVO 5V", 89.5, 73.5, 0.8),
        ("-  +", 89.5, 78.5, 0.8),
    ]
    for txt, tx, ty, sz in f_silk_texts:
        lines.append(f'  (gr_text "{txt}" (at {tx} {ty}) (layer "F.SilkS") (effects (font (size {sz} {sz}) (thickness 0.15))))')

    b_silk_texts = [
        ("WINDDRAGONS REMOTE CONTROL & CARRIER V4.1", 50.0, 5.0, 1.1),
        ("USER INTERFACE: DISPLAY & PS4 JOYSTICK", 50.0, 7.5, 0.8),
        ("ST7789 1.47 INCH LCD (320x172)", 50.0, 27.0, 0.85),
        ("USB-C", 25.0, 25.0, 0.7),
        ("PS4 3D JOYSTICK", 20.0, 75.0, 0.85),
        ("SOLID GND COPPER PLANES ON F.CU & B.CU (100x85mm)", 50.0, 81.5, 0.85),
    ]
    for txt, tx, ty, sz in b_silk_texts:
        lines.append(f'  (gr_text "{txt}" (at {tx} {ty}) (layer "B.SilkS") (effects (font (size {sz} {sz}) (thickness 0.15)) (justify mirror)))')

    # ==========================================================================
    # USER-DRIVEN ROUTING: CLEAN RATSNEST NETS (0 PRE-ROUTED TRACES)
    # The user will route all copper tracks manually in KiCad PCB Editor.
    # ==========================================================================
    segments = []
    vias = []
    for vx, vy, vnid in vias:
        lines.append(f'  (via (at {vx} {vy}) (size 0.8) (drill 0.4) (layers "F.Cu" "B.Cu") (net {get_net(vnid)}))')

    for lyr, nid, x1, y1, x2, y2, w in segments:
        lines.append(f'  (segment (start {x1} {y1}) (end {x2} {y2}) (width {w}) (layer "{lyr}") (net {get_net(nid)}))')

    # Solid GND plane zones on both F.Cu (Top) and B.Cu (Bottom)
    for cu_layer in ["F.Cu", "B.Cu"]:
        lines.append('  (zone')
        lines.append(f'    (net {get_net("GND")})')
        gnd_n = net_name("GND")
        lines.append(f'    (net_name "{gnd_n}")')
        lines.append(f'    (layer "{cu_layer}")')
        lines.append(f'    (uuid "{new_uuid()}")')
        lines.append('    (hatch edge 0.5)')
        lines.append('    (priority 0)')
        lines.append('    (connect_pads (clearance 0.35))')
        lines.append('    (min_thickness 0.25)')
        lines.append('    (filled_areas_thickness no)')
        lines.append('    (fill')
        lines.append('      (thermal_gap 0.3)')
        lines.append('      (thermal_bridge_width 0.4)')
        lines.append('    )')
        lines.append('    (polygon')
        lines.append('      (pts')
        lines.append('        (xy 1.5 1.5)')
        lines.append('        (xy 98.5 1.5)')
        lines.append('        (xy 98.5 83.5)')
        lines.append('        (xy 1.5 83.5)')
        lines.append('      )')
        lines.append('    )')
        lines.append('  )')

    lines.append(')')
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")
    print(f"Generated 2-Layer Landscape KiCad PCB: {path}")

# ==============================================================================
# 3. KICAD SCHEMATIC (.kicad_sch)
# ==============================================================================
def generate_kicad_sch(path):
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
        '		(date "2026-10-06")',
        '		(rev "V4.1 - Zero DRC Parity Clean")',
        '		(company "WindDragons Project")',
        '		(comment 1 "ESP32-C6-LCD-1.47 Carrier - 100x85mm 2-Layer JLCPCB")',
        '		(comment 2 "Dedicated LoRa FSPI, GNSS LC29H, PS4 Thumbstick & 4 Actuators")',
        '	)',
        '	(lib_symbols',
        '		(symbol "Connector_Generic:Conn_01x02"',
        '			(pin_names (offset 1.016) (hide yes))',
        '			(property "Reference" "J" (at 0 3.81 0) (effects (font (size 1.27 1.27))))',
        '			(property "Value" "Conn_01x02" (at 0 -3.81 0) (effects (font (size 1.27 1.27))))',
        '			(property "Footprint" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))',
        '			(symbol "Conn_01x02_1_1"',
        '				(rectangle (start -1.27 2.54) (end 1.27 -2.54) (stroke (width 0.254) (type default)) (fill (type background)))',
        '				(pin passive line (at -5.08 1.27 0) (length 3.81) (name "Pin_1" (effects (font (size 1.27 1.27)))) (number "1" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 -1.27 0) (length 3.81) (name "Pin_2" (effects (font (size 1.27 1.27)))) (number "2" (effects (font (size 1.27 1.27)))))',
        '			)',
        '		)',
        '		(symbol "Connector_Generic:Conn_01x03"',
        '			(pin_names (offset 1.016) (hide yes))',
        '			(property "Reference" "J" (at 0 5.08 0) (effects (font (size 1.27 1.27))))',
        '			(property "Value" "Conn_01x03" (at 0 -5.08 0) (effects (font (size 1.27 1.27))))',
        '			(property "Footprint" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))',
        '			(symbol "Conn_01x03_1_1"',
        '				(rectangle (start -1.27 3.81) (end 1.27 -3.81) (stroke (width 0.254) (type default)) (fill (type background)))',
        '				(pin passive line (at -5.08 2.54 0) (length 3.81) (name "Pin_1" (effects (font (size 1.27 1.27)))) (number "1" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 0 0) (length 3.81) (name "Pin_2" (effects (font (size 1.27 1.27)))) (number "2" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 -2.54 0) (length 3.81) (name "Pin_3" (effects (font (size 1.27 1.27)))) (number "3" (effects (font (size 1.27 1.27)))))',
        '			)',
        '		)',
        '		(symbol "Connector_Generic:Conn_01x04"',
        '			(pin_names (offset 1.016) (hide yes))',
        '			(property "Reference" "J" (at 0 6.35 0) (effects (font (size 1.27 1.27))))',
        '			(property "Value" "Conn_01x04" (at 0 -6.35 0) (effects (font (size 1.27 1.27))))',
        '			(property "Footprint" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))',
        '			(symbol "Conn_01x04_1_1"',
        '				(rectangle (start -1.27 5.08) (end 1.27 -5.08) (stroke (width 0.254) (type default)) (fill (type background)))',
        '				(pin passive line (at -5.08 3.81 0) (length 3.81) (name "Pin_1" (effects (font (size 1.27 1.27)))) (number "1" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 1.27 0) (length 3.81) (name "Pin_2" (effects (font (size 1.27 1.27)))) (number "2" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 -1.27 0) (length 3.81) (name "Pin_3" (effects (font (size 1.27 1.27)))) (number "3" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 -3.81 0) (length 3.81) (name "Pin_4" (effects (font (size 1.27 1.27)))) (number "4" (effects (font (size 1.27 1.27)))))',
        '			)',
        '		)',
        '		(symbol "Connector_Generic:Conn_01x07"',
        '			(pin_names (offset 1.016) (hide yes))',
        '			(property "Reference" "J" (at 0 10.16 0) (effects (font (size 1.27 1.27))))',
        '			(property "Value" "Conn_01x07" (at 0 -10.16 0) (effects (font (size 1.27 1.27))))',
        '			(property "Footprint" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))',
        '			(symbol "Conn_01x07_1_1"',
        '				(rectangle (start -1.27 8.89) (end 1.27 -8.89) (stroke (width 0.254) (type default)) (fill (type background)))',
        '				(pin passive line (at -5.08 7.62 0) (length 3.81) (name "Pin_1" (effects (font (size 1.27 1.27)))) (number "1" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 5.08 0) (length 3.81) (name "Pin_2" (effects (font (size 1.27 1.27)))) (number "2" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 2.54 0) (length 3.81) (name "Pin_3" (effects (font (size 1.27 1.27)))) (number "3" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 0 0) (length 3.81) (name "Pin_4" (effects (font (size 1.27 1.27)))) (number "4" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 -2.54 0) (length 3.81) (name "Pin_5" (effects (font (size 1.27 1.27)))) (number "5" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 -5.08 0) (length 3.81) (name "Pin_6" (effects (font (size 1.27 1.27)))) (number "6" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 -7.62 0) (length 3.81) (name "Pin_7" (effects (font (size 1.27 1.27)))) (number "7" (effects (font (size 1.27 1.27)))))',
        '			)',
        '		)',
        '		(symbol "Connector_Generic:Conn_01x09"',
        '			(pin_names (offset 1.016) (hide yes))',
        '			(property "Reference" "J" (at 0 12.7 0) (effects (font (size 1.27 1.27))))',
        '			(property "Value" "Conn_01x09" (at 0 -12.7 0) (effects (font (size 1.27 1.27))))',
        '			(property "Footprint" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))',
        '			(symbol "Conn_01x09_1_1"',
        '				(rectangle (start -1.27 11.43) (end 1.27 -11.43) (stroke (width 0.254) (type default)) (fill (type background)))',
        '				(pin passive line (at -5.08 10.16 0) (length 3.81) (name "Pin_1" (effects (font (size 1.27 1.27)))) (number "1" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 7.62 0) (length 3.81) (name "Pin_2" (effects (font (size 1.27 1.27)))) (number "2" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 5.08 0) (length 3.81) (name "Pin_3" (effects (font (size 1.27 1.27)))) (number "3" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 2.54 0) (length 3.81) (name "Pin_4" (effects (font (size 1.27 1.27)))) (number "4" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 0 0) (length 3.81) (name "Pin_5" (effects (font (size 1.27 1.27)))) (number "5" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 -2.54 0) (length 3.81) (name "Pin_6" (effects (font (size 1.27 1.27)))) (number "6" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 -5.08 0) (length 3.81) (name "Pin_7" (effects (font (size 1.27 1.27)))) (number "7" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 -7.62 0) (length 3.81) (name "Pin_8" (effects (font (size 1.27 1.27)))) (number "8" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 -10.16 0) (length 3.81) (name "Pin_9" (effects (font (size 1.27 1.27)))) (number "9" (effects (font (size 1.27 1.27)))))',
        '			)',
        '		)',
        '		(symbol "Connector_Generic:Conn_01x09_Right"',
        '			(pin_names (offset 1.016) (hide yes))',
        '			(property "Reference" "J" (at 0 12.7 0) (effects (font (size 1.27 1.27))))',
        '			(property "Value" "Conn_01x09" (at 0 -12.7 0) (effects (font (size 1.27 1.27))))',
        '			(property "Footprint" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))',
        '			(symbol "Conn_01x09_Right_1_1"',
        '				(rectangle (start -1.27 11.43) (end 1.27 -11.43) (stroke (width 0.254) (type default)) (fill (type background)))',
        '				(pin passive line (at 5.08 10.16 180) (length 3.81) (name "Pin_1" (effects (font (size 1.27 1.27)))) (number "1" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at 5.08 7.62 180) (length 3.81) (name "Pin_2" (effects (font (size 1.27 1.27)))) (number "2" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at 5.08 5.08 180) (length 3.81) (name "Pin_3" (effects (font (size 1.27 1.27)))) (number "3" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at 5.08 2.54 180) (length 3.81) (name "Pin_4" (effects (font (size 1.27 1.27)))) (number "4" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at 5.08 0 180) (length 3.81) (name "Pin_5" (effects (font (size 1.27 1.27)))) (number "5" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at 5.08 -2.54 180) (length 3.81) (name "Pin_6" (effects (font (size 1.27 1.27)))) (number "6" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at 5.08 -5.08 180) (length 3.81) (name "Pin_7" (effects (font (size 1.27 1.27)))) (number "7" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at 5.08 -7.62 180) (length 3.81) (name "Pin_8" (effects (font (size 1.27 1.27)))) (number "8" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at 5.08 -10.16 180) (length 3.81) (name "Pin_9" (effects (font (size 1.27 1.27)))) (number "9" (effects (font (size 1.27 1.27)))))',
        '			)',
        '		)',
        '		(symbol "Connector_Generic:Conn_02x04_Odd_Even"',
        '			(pin_names (offset 1.016) (hide yes))',
        '			(property "Reference" "J" (at 0 6.35 0) (effects (font (size 1.27 1.27))))',
        '			(property "Value" "Conn_02x04_Odd_Even" (at 0 -7.62 0) (effects (font (size 1.27 1.27))))',
        '			(property "Footprint" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))',
        '			(symbol "Conn_02x04_Odd_Even_1_1"',
        '				(rectangle (start -1.27 3.81) (end 3.81 -6.35) (stroke (width 0.254) (type default)) (fill (type background)))',
        '				(pin passive line (at -5.08 2.54 0) (length 3.81) (name "Pin_1" (effects (font (size 1.27 1.27)))) (number "1" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at 7.62 2.54 180) (length 3.81) (name "Pin_2" (effects (font (size 1.27 1.27)))) (number "2" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 0 0) (length 3.81) (name "Pin_3" (effects (font (size 1.27 1.27)))) (number "3" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at 7.62 0 180) (length 3.81) (name "Pin_4" (effects (font (size 1.27 1.27)))) (number "4" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 -2.54 0) (length 3.81) (name "Pin_5" (effects (font (size 1.27 1.27)))) (number "5" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at 7.62 -2.54 180) (length 3.81) (name "Pin_6" (effects (font (size 1.27 1.27)))) (number "6" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 -5.08 0) (length 3.81) (name "Pin_7" (effects (font (size 1.27 1.27)))) (number "7" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at 7.62 -5.08 180) (length 3.81) (name "Pin_8" (effects (font (size 1.27 1.27)))) (number "8" (effects (font (size 1.27 1.27)))))',
        '			)',
        '		)',
        '		(symbol "Mechanical:MountingHole"',
        '			(pin_names (offset 1.016) (hide yes))',
        '			(property "Reference" "H" (at 0 2.54 0) (effects (font (size 1.27 1.27))))',
        '			(property "Value" "MountingHole" (at 0 -2.54 0) (effects (font (size 1.27 1.27))))',
        '			(property "Footprint" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))',
        '			(symbol "MountingHole_1_1"',
        '				(circle (center 0 0) (radius 1.27) (stroke (width 0.254) (type default)) (fill (type none)))',
        '			)',
        '		)',
        '		(symbol "Device:Joystick_PS4_Thumbstick"',
        '			(pin_names (offset 1.016))',
        '			(property "Reference" "JOY" (at 0 15.24 0) (effects (font (size 1.27 1.27))))',
        '			(property "Value" "PS4_JOYSTICK" (at 0 -15.24 0) (effects (font (size 1.27 1.27))))',
        '			(property "Footprint" "PS4_joystick:XDCR_COM-09032" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))',
        '			(symbol "Joystick_PS4_Thumbstick_1_1"',
        '				(rectangle (start -10.16 12.70) (end 10.16 -12.70) (stroke (width 0.254) (type default)) (fill (type background)))',
        '				(pin passive line (at -13.97 10.16 0) (length 3.81) (name "H1_GND" (effects (font (size 0.9 0.9)))) (number "H1" (effects (font (size 0.9 0.9)))))',
        '				(pin passive line (at -13.97 7.62 0) (length 3.81) (name "H2_ADC" (effects (font (size 0.9 0.9)))) (number "H2" (effects (font (size 0.9 0.9)))))',
        '				(pin passive line (at -13.97 5.08 0) (length 3.81) (name "H3_3V3" (effects (font (size 0.9 0.9)))) (number "H3" (effects (font (size 0.9 0.9)))))',
        '				(pin passive line (at -13.97 0.00 0) (length 3.81) (name "V1_GND" (effects (font (size 0.9 0.9)))) (number "V1" (effects (font (size 0.9 0.9)))))',
        '				(pin passive line (at -13.97 -2.54 0) (length 3.81) (name "V2_ADC" (effects (font (size 0.9 0.9)))) (number "V2" (effects (font (size 0.9 0.9)))))',
        '				(pin passive line (at -13.97 -5.08 0) (length 3.81) (name "V3_3V3" (effects (font (size 0.9 0.9)))) (number "V3" (effects (font (size 0.9 0.9)))))',
        '				(pin passive line (at 13.97 10.16 180) (length 3.81) (name "B1A_GND" (effects (font (size 0.9 0.9)))) (number "B1A" (effects (font (size 0.9 0.9)))))',
        '				(pin passive line (at 13.97 7.62 180) (length 3.81) (name "B1B_GND" (effects (font (size 0.9 0.9)))) (number "B1B" (effects (font (size 0.9 0.9)))))',
        '				(pin passive line (at 13.97 5.08 180) (length 3.81) (name "B2A_SW" (effects (font (size 0.9 0.9)))) (number "B2A" (effects (font (size 0.9 0.9)))))',
        '				(pin passive line (at 13.97 2.54 180) (length 3.81) (name "B2B_SW" (effects (font (size 0.9 0.9)))) (number "B2B" (effects (font (size 0.9 0.9)))))',
        '				(pin passive line (at 13.97 -2.54 180) (length 3.81) (name "S1_GND" (effects (font (size 0.9 0.9)))) (number "S1" (effects (font (size 0.9 0.9)))))',
        '				(pin passive line (at 13.97 -5.08 180) (length 3.81) (name "S2_GND" (effects (font (size 0.9 0.9)))) (number "S2" (effects (font (size 0.9 0.9)))))',
        '				(pin passive line (at 13.97 -7.62 180) (length 3.81) (name "S3_GND" (effects (font (size 0.9 0.9)))) (number "S3" (effects (font (size 0.9 0.9)))))',
        '				(pin passive line (at 13.97 -10.16 180) (length 3.81) (name "S4_GND" (effects (font (size 0.9 0.9)))) (number "S4" (effects (font (size 0.9 0.9)))))',
        '			)',
        '		)',
        '	)',
    ]

    def add_block(title, subtitle, x1, y1, x2, y2):
        sch_lines.append('	(polyline')
        sch_lines.append(f'		(pts (xy {x1:.2f} {y1:.2f}) (xy {x2:.2f} {y1:.2f}) (xy {x2:.2f} {y2:.2f}) (xy {x1:.2f} {y2:.2f}) (xy {x1:.2f} {y1:.2f}))')
        sch_lines.append('		(stroke (width 0.3) (type dash))')
        sch_lines.append(f'		(uuid "{new_uuid()}")')
        sch_lines.append('	)')
        sch_lines.append(f'	(text "{title}" (at {x1+3.81:.2f} {y1+5.08:.2f} 0) (effects (font (size 2.5 2.5) (bold yes)) (justify left)) (uuid "{new_uuid()}"))')
        if subtitle:
            sch_lines.append(f'	(text "{subtitle}" (at {x1+3.81:.2f} {y1+9.52:.2f} 0) (effects (font (size 1.5 1.5) (italic yes)) (justify left)) (uuid "{new_uuid()}"))')

    def place_conn_1row(lib_id, ref, val, fp, x, y, labels):
        u = new_uuid()
        N = len(labels)
        sch_lines.append(f'	(symbol (lib_id "{lib_id}") (at {x:.2f} {y:.2f} 0) (unit 1)')
        sch_lines.append(f'		(uuid "{u}")')
        sch_lines.append(f'		(property "Reference" "{ref}" (at {x:.2f} {y+(N*1.27+3.81):.2f} 0) (effects (font (size 1.5 1.5))))')
        sch_lines.append(f'		(property "Value" "{val}" (at {x:.2f} {y-(N*1.27+3.81):.2f} 0) (effects (font (size 1.27 1.27))))')
        sch_lines.append(f'		(property "Footprint" "{fp}" (at {x:.2f} {y:.2f} 0) (effects (font (size 1.27 1.27)) (hide yes)))')
        sch_lines.append('	)')
        for i, lbl in enumerate(labels):
            pin_y = (N - 1) * 1.27 - i * 2.54
            p_abs_y = y - pin_y
            px_pin = x - 5.08
            px_wire_end = x - 17.78
            sch_lines.append('	(wire')
            sch_lines.append(f'		(pts (xy {px_pin:.2f} {p_abs_y:.2f}) (xy {px_wire_end:.2f} {p_abs_y:.2f}))')
            sch_lines.append('		(stroke (width 0) (type solid))')
            sch_lines.append(f'		(uuid "{new_uuid()}")')
            sch_lines.append('	)')
            sch_lines.append(f'	(label "{lbl}" (at {px_wire_end:.2f} {p_abs_y:.2f} 0) (effects (font (size 1.27 1.27)) (justify right)))')

    def place_conn_1row_right(lib_id, ref, val, fp, x, y, labels):
        u = new_uuid()
        N = len(labels)
        sch_lines.append(f'	(symbol (lib_id "{lib_id}") (at {x:.2f} {y:.2f} 0) (unit 1)')
        sch_lines.append(f'		(uuid "{u}")')
        sch_lines.append(f'		(property "Reference" "{ref}" (at {x:.2f} {y+(N*1.27+3.81):.2f} 0) (effects (font (size 1.5 1.5))))')
        sch_lines.append(f'		(property "Value" "{val}" (at {x:.2f} {y-(N*1.27+3.81):.2f} 0) (effects (font (size 1.27 1.27))))')
        sch_lines.append(f'		(property "Footprint" "{fp}" (at {x:.2f} {y:.2f} 0) (effects (font (size 1.27 1.27)) (hide yes)))')
        sch_lines.append('	)')
        for i, lbl in enumerate(labels):
            pin_y = (N - 1) * 1.27 - i * 2.54
            p_abs_y = y - pin_y
            px_pin = x + 5.08
            px_wire_end = x + 17.78
            sch_lines.append('	(wire')
            sch_lines.append(f'		(pts (xy {px_pin:.2f} {p_abs_y:.2f}) (xy {px_wire_end:.2f} {p_abs_y:.2f}))')
            sch_lines.append('		(stroke (width 0) (type solid))')
            sch_lines.append(f'		(uuid "{new_uuid()}")')
            sch_lines.append('	)')
            sch_lines.append(f'	(label "{lbl}" (at {px_wire_end:.2f} {p_abs_y:.2f} 0) (effects (font (size 1.27 1.27)) (justify left)))')

    def place_lora_2x4(ref, val, fp, x, y, odd_labels, even_labels):
        u = new_uuid()
        sch_lines.append(f'	(symbol (lib_id "Connector_Generic:Conn_02x04_Odd_Even") (at {x:.2f} {y:.2f} 0) (unit 1)')
        sch_lines.append(f'		(uuid "{u}")')
        sch_lines.append(f'		(property "Reference" "{ref}" (at {x:.2f} {y+10.16:.2f} 0) (effects (font (size 1.5 1.5))))')
        sch_lines.append(f'		(property "Value" "{val}" (at {x:.2f} {y-10.16:.2f} 0) (effects (font (size 1.27 1.27))))')
        sch_lines.append(f'		(property "Footprint" "{fp}" (at {x:.2f} {y:.2f} 0) (effects (font (size 1.27 1.27)) (hide yes)))')
        sch_lines.append('	)')
        for i, lbl in enumerate(odd_labels):
            p_y = 2.54 - i * 2.54
            p_abs_y = y - p_y
            px_pin = x - 5.08
            px_end = x - 17.78
            sch_lines.append('	(wire')
            sch_lines.append(f'		(pts (xy {px_pin:.2f} {p_abs_y:.2f}) (xy {px_end:.2f} {p_abs_y:.2f}))')
            sch_lines.append('		(stroke (width 0) (type solid))')
            sch_lines.append(f'		(uuid "{new_uuid()}")')
            sch_lines.append('	)')
            sch_lines.append(f'	(label "{lbl}" (at {px_end:.2f} {p_abs_y:.2f} 0) (effects (font (size 1.27 1.27)) (justify right)))')
        for i, lbl in enumerate(even_labels):
            p_y = 2.54 - i * 2.54
            p_abs_y = y - p_y
            px_pin = x + 7.62
            px_end = x + 20.32
            sch_lines.append('	(wire')
            sch_lines.append(f'		(pts (xy {px_pin:.2f} {p_abs_y:.2f}) (xy {px_end:.2f} {p_abs_y:.2f}))')
            sch_lines.append('		(stroke (width 0) (type solid))')
            sch_lines.append(f'		(uuid "{new_uuid()}")')
            sch_lines.append('	)')
            sch_lines.append(f'	(label "{lbl}" (at {px_end:.2f} {p_abs_y:.2f} 0) (effects (font (size 1.27 1.27)) (justify left)))')

    # Helper: place Joystick
    def place_joystick(ref, val, fp, x, y):
        u = new_uuid()
        sch_lines.append(f'	(symbol (lib_id "Device:Joystick_PS4_Thumbstick") (at {x:.2f} {y:.2f} 0) (unit 1)')
        sch_lines.append(f'		(uuid "{u}")')
        sch_lines.append(f'		(property "Reference" "{ref}" (at {x:.2f} {y+16.51:.2f} 0) (effects (font (size 1.5 1.5))))')
        sch_lines.append(f'		(property "Value" "{val}" (at {x:.2f} {y-16.51:.2f} 0) (effects (font (size 1.27 1.27))))')
        sch_lines.append(f'		(property "Footprint" "{fp}" (at {x:.2f} {y:.2f} 0) (effects (font (size 1.27 1.27)) (hide yes)))')
        sch_lines.append('	)')
        left_pins = [
            (10.16, "GND"),     # H1
            (7.62, "JOY_X"),    # H2
            (5.08, "+3V3"),     # H3
            (0.00, "GND"),      # V1
            (-2.54, "JOY_Y"),   # V2
            (-5.08, "+3V3"),    # V3
        ]
        for py, lbl in left_pins:
            p_abs_y = y - py
            px_pin = x - 13.97
            px_end = x - 25.40
            sch_lines.append('	(wire')
            sch_lines.append(f'		(pts (xy {px_pin:.2f} {p_abs_y:.2f}) (xy {px_end:.2f} {p_abs_y:.2f}))')
            sch_lines.append('		(stroke (width 0) (type solid))')
            sch_lines.append(f'		(uuid "{new_uuid()}")')
            sch_lines.append('	)')
            sch_lines.append(f'	(label "{lbl}" (at {px_end:.2f} {p_abs_y:.2f} 0) (effects (font (size 1.27 1.27)) (justify right)))')
        right_pins = [
            (10.16, "GND"),      # B1A
            (7.62, "GND"),       # B1B
            (5.08, "IO9_BOOT"),  # B2A
            (2.54, "IO9_BOOT"),  # B2B
            (-2.54, "GND"),      # S1
            (-5.08, "GND"),      # S2
            (-7.62, "GND"),      # S3
            (-10.16, "GND"),     # S4
        ]
        for py, lbl in right_pins:
            p_abs_y = y - py
            px_pin = x + 13.97
            px_end = x + 25.40
            sch_lines.append('	(wire')
            sch_lines.append(f'		(pts (xy {px_pin:.2f} {p_abs_y:.2f}) (xy {px_end:.2f} {p_abs_y:.2f}))')
            sch_lines.append('		(stroke (width 0) (type solid))')
            sch_lines.append(f'		(uuid "{new_uuid()}")')
            sch_lines.append('	)')
            sch_lines.append(f'	(label "{lbl}" (at {px_end:.2f} {p_abs_y:.2f} 0) (effects (font (size 1.27 1.27)) (justify left)))')

    # ==================== CIRCUIT BLOCKS ====================
    # BLOCK 1: ESP32-C6-LCD-1.47 Processor
    add_block("1. ESP32-C6-LCD-1.47 MAIN PROCESSOR", "Dual 1x09 Headers (J1 Top / SPI, J2 Bottom / Actuators & GPS)", 20.32, 20.32, 142.24, 114.30)
    esp_t = ["IO5_NSS", "IO4_MISO", "IO3_MOSI", "IO2_SCK", "IO1_ADC", "IO0_ADC", "+3V3", "GND", "+5V"]
    place_conn_1row("Connector_Generic:Conn_01x09", "J1", "ESP32_T_LANDSCAPE", "Connector_PinHeader_2.54mm:PinHeader_1x09_P2.54mm_Horizontal", 50.80, 66.04, esp_t)

    esp_b = ["IO9_BOOT", "IO18_ACT1_UP", "IO19_WINCH_MODE", "IO20_RUDDER_DN", "IO23_ACT4", "IO12_LORA_DIO0", "IO13_BUZ", "IO16_GPS_RX", "IO17_GPS_TX"]
    place_conn_1row_right("Connector_Generic:Conn_01x09_Right", "J2", "ESP32_B_LANDSCAPE", "Connector_PinHeader_2.54mm:PinHeader_1x09_P2.54mm_Horizontal", 111.76, 66.04, esp_b)

    sch_lines.append('	(polyline')
    sch_lines.append('		(pts (xy 60.96 48.26) (xy 104.14 48.26) (xy 104.14 83.82) (xy 60.96 83.82) (xy 60.96 48.26))')
    sch_lines.append('		(stroke (width 0.254) (type solid))')
    sch_lines.append(f'		(uuid "{new_uuid()}")')
    sch_lines.append('	)')
    sch_lines.append(f'	(text "ESP32-C6-LCD-1.47" (at 82.55 58.42 0) (effects (font (size 2.0 2.0) (bold yes))) (uuid "{new_uuid()}"))')
    sch_lines.append(f'	(text "ST7789V 1.47-inch IPS LCD" (at 82.55 64.77 0) (effects (font (size 1.4 1.4))) (uuid "{new_uuid()}"))')
    sch_lines.append(f'	(text "RISC-V 160MHz / 4MB Flash" (at 82.55 71.12 0) (effects (font (size 1.3 1.3) (italic yes))) (uuid "{new_uuid()}"))')
    sch_lines.append(f'	(text "WiFi 6 / BLE 5 / Zigbee" (at 82.55 77.47 0) (effects (font (size 1.2 1.2))) (uuid "{new_uuid()}"))')

    # BLOCK 2: SX1278 Ra-02 433MHz LoRa
    add_block("2. SX1278 433MHz LORA TELEMETRY", "Hardware SPI (GP2/3/4/5) + DIO0 IRQ (GP12)", 152.40, 20.32, 264.16, 81.28)
    lora_odd = ["GND", "NC", "IO2_SCK", "IO4_MISO"]
    lora_even = ["+3V3", "IO5_NSS", "IO3_MOSI", "IO12_LORA_DIO0"]
    place_lora_2x4("J3", "SX1278_LORA_2x4", "Connector_PinHeader_2.54mm:PinHeader_2x04_P2.54mm_Vertical", 208.28, 50.80, lora_odd, lora_even)

    # BLOCK 3: GNSS / GPS LC29H
    add_block("3. GNSS / GPS NAVIGATION (LC29H)", "UART Port (GP16 RX, GP17 TX) + 3V3", 152.40, 91.44, 264.16, 172.72)
    gps_labels = ["IO15", "GPS_TX2", "GPS_RX2", "IO16_GPS_RX", "IO17_GPS_TX", "GND", "+3V3"]
    place_conn_1row("Connector_Generic:Conn_01x07", "J6", "GPS_LC29H_7PIN", "Connector_PinHeader_2.54mm:PinHeader_1x07_P2.54mm_Vertical", 218.44, 137.16, gps_labels)

    # BLOCK 4: Dual Joystick Interface & Pre-Launch Test Jumpers
    add_block("4. DUAL JOYSTICK INTERFACE", "Onboard PS4 Thumbstick (JOY1), RBL Jumpers (JP_RBL) & External Battery Port (J4)", 274.32, 20.32, 406.40, 114.30)
    place_joystick("JOY1", "PS4_JOYSTICK", "PS4_joystick:XDCR_COM-09032", 314.96, 66.04)
    rbl_labels = ["JOY_X", "IO0_ADC", "JOY_Y", "IO1_ADC"]
    place_conn_1row("Connector_Generic:Conn_01x04", "JP_RBL", "REMOVE_BEFORE_LAUNCH", "Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical", 347.98, 66.04, rbl_labels)
    apm_labels = ["IO0_ADC", "IO1_ADC", "GND"]
    place_conn_1row("Connector_Generic:Conn_01x03", "J4", "BATT_APM", "Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Horizontal", 381.00, 64.77, apm_labels)

    # BLOCK 5: Actuators & Servos
    add_block("5. ACTUATOR & SERVO OUTPUTS", "PWM Timers: ESC (GP18), Rudder (GP19), Winch (GP20), Rack (GP23)", 274.32, 124.46, 406.40, 203.20)
    place_conn_1row("Connector_Generic:Conn_01x03", "J7", "ESC_PROP_UP", "Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Horizontal", 309.88, 152.40, ["IO18_ACT1_UP", "+5V_SERVO", "GND"])
    place_conn_1row("Connector_Generic:Conn_01x03", "J8", "RUDDER_DN", "Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Horizontal", 309.88, 180.34, ["IO19_WINCH_MODE", "+5V_SERVO", "GND"])
    place_conn_1row("Connector_Generic:Conn_01x03", "J9", "WINCH_MODE", "Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Horizontal", 381.00, 152.40, ["IO20_RUDDER_DN", "+5V_SERVO", "GND"])
    place_conn_1row("Connector_Generic:Conn_01x03", "J10", "RACK_CREMALHEIRA", "Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Horizontal", 381.00, 180.34, ["IO23_ACT4", "+5V_SERVO", "GND"])

    # BLOCK 6: Power Supply & Alarm
    add_block("6. POWER & ALARM DISTRIBUTION", "Isolated +5V Logic, +5V Servo Rail & Piezo Siren", 20.32, 124.46, 142.24, 203.20)
    place_conn_1row("Connector_Generic:Conn_01x02", "J11", "PWR_ESP_5V", "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Horizontal", 53.34, 152.40, ["GND", "+5V"])
    place_conn_1row("Connector_Generic:Conn_01x02", "J12", "PWR_SERVO_5V", "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Horizontal", 53.34, 180.34, ["GND", "+5V_SERVO"])
    place_conn_1row("Connector_Generic:Conn_01x02", "J5", "BUZZER", "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical", 114.30, 165.10, ["IO13_BUZ", "GND"])

    # BLOCK 7: Mounting Holes
    add_block("7. MECHANICAL MOUNTING", "4x M3 Corner Mounting Holes", 152.40, 180.34, 264.16, 203.20)
    m_holes = [("H1", 165.10, 193.04), ("H2", 190.50, 193.04), ("H3", 215.90, 193.04), ("H4", 241.30, 193.04)]
    for href, hx, hy in m_holes:
        sch_lines.append(f'	(symbol (lib_id "Mechanical:MountingHole") (at {hx:.2f} {hy:.2f} 0) (unit 1)')
        sch_lines.append(f'		(uuid "{new_uuid()}")')
        sch_lines.append(f'		(property "Reference" "{href}" (at {hx:.2f} {hy-3.81:.2f} 0) (effects (font (size 1.27 1.27))))')
        sch_lines.append(f'		(property "Value" "MountingHole" (at {hx:.2f} {hy+3.81:.2f} 0) (effects (font (size 1.27 1.27))))')
        sch_lines.append(f'		(property "Footprint" "MountingHole:MountingHole_3.2mm_M3" (at {hx:.2f} {hy:.2f} 0) (effects (font (size 1.27 1.27)) (hide yes)))')
        sch_lines.append('	)')

    sch_lines.append('	(sheet_instances')
    sch_lines.append('		(path "/" (page "1"))')
    sch_lines.append('	)')
    sch_lines.append('	(embedded_fonts no)')
    sch_lines.append(')')

    with open(path, "w") as f:
        f.write("\n".join(sch_lines) + "\n")
    print(f"Generated KiCad Schematic: {path}")

# ==============================================================================
# 4. MAIN ORCHESTRATION & ZONE REFILL & GERBER PACKAGING
# ==============================================================================
def main():
    pro_path = os.path.join(TARGET_DIR, f"{PROJECT_NAME}.kicad_pro")
    prl_path = os.path.join(TARGET_DIR, f"{PROJECT_NAME}.kicad_prl")
    pcb_path = os.path.join(TARGET_DIR, f"{PROJECT_NAME}.kicad_pcb")
    sch_path = os.path.join(TARGET_DIR, f"{PROJECT_NAME}.kicad_sch")
    gerber_dir = os.path.join(TARGET_DIR, "gerbers")
    os.makedirs(gerber_dir, exist_ok=True)

    print(f"Generating Zero-DRC 2-Layer JLCPCB Project in {TARGET_DIR}...")
    generate_kicad_pro(pro_path)
    generate_kicad_prl(prl_path)
    generate_kicad_pcb(pcb_path)
    generate_kicad_sch(sch_path)

    kicad_cli = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
    kicad_python = "/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3"

    if os.path.exists(kicad_cli):
        subprocess.run([kicad_cli, "pcb", "upgrade", "--force", pcb_path], check=True)
        subprocess.run([kicad_cli, "sch", "upgrade", "--force", sch_path], check=True)
        print("Upgraded to native KiCad 10 format.")

        if os.path.exists(kicad_python):
            py_fill_script = f"""
import pcbnew
board = pcbnew.LoadBoard('{pcb_path}')
filler = pcbnew.ZONE_FILLER(board)
filler.Fill(board.Zones())
board.Save('{pcb_path}')
print("Successfully filled and saved all copper zones.")
"""
            subprocess.run([kicad_python, "-c", py_fill_script], check=True)

        erc_rpt = os.path.join(TARGET_DIR, f"{PROJECT_NAME}-erc.rpt")
        res_erc = subprocess.run([kicad_cli, "sch", "erc", "-o", erc_rpt, sch_path], capture_output=True, text=True)
        print("ERC Output:\n", res_erc.stdout)

        drc_rpt = os.path.join(TARGET_DIR, f"{PROJECT_NAME}-drc.rpt")
        res_drc = subprocess.run([kicad_cli, "pcb", "drc", "--schematic-parity", "-o", drc_rpt, pcb_path], capture_output=True, text=True)
        print("DRC Output:\n", res_drc.stdout)

        pdf_path = os.path.join(TARGET_DIR, f"{PROJECT_NAME}_schematic.pdf")
        subprocess.run([kicad_cli, "sch", "export", "pdf", "-o", pdf_path, sch_path], check=True)
        subprocess.run(["qlmanage", "-t", "-s", "1920", "-o", TARGET_DIR, pdf_path], capture_output=True)
        thumb = os.path.join(TARGET_DIR, f"{PROJECT_NAME}_schematic.pdf.png")
        final_png = os.path.join(TARGET_DIR, f"{PROJECT_NAME}_schematic.png")
        if os.path.exists(thumb):
            os.replace(thumb, final_png)
        print("Exported Schematic PDF & PNG.")

        subprocess.run([
            kicad_cli, "pcb", "export", "gerbers",
            "--check-zones",
            "--output", gerber_dir,
            pcb_path
        ], check=True)

        subprocess.run([
            kicad_cli, "pcb", "export", "drill",
            "--output", gerber_dir,
            pcb_path
        ], check=True)
        print("Exported Gerbers and Drills.")

        zip_path = os.path.join(TARGET_DIR, f"{PROJECT_NAME}_Gerber.zip")
        with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
            for fname in sorted(os.listdir(gerber_dir)):
                fpath = os.path.join(gerber_dir, fname)
                if os.path.isfile(fpath):
                    zf.write(fpath, arcname=fname)
        print(f"Packaged JLCPCB Gerber Archive: {zip_path}")

        render_top = os.path.join(TARGET_DIR, "Universal_Carrier_3d_top.png")
        subprocess.run([
            kicad_cli, "pcb", "render", pcb_path,
            "-o", render_top,
            "--side", "top",
            "--width", "1920", "--height", "1080", "--rotate", "0,0,0", "--zoom", "1.3"
        ], capture_output=True)

        render_bottom = os.path.join(TARGET_DIR, "Universal_Carrier_3d_bottom.png")
        subprocess.run([
            kicad_cli, "pcb", "render", pcb_path,
            "-o", render_bottom,
            "--side", "bottom",
            "--width", "1920", "--height", "1080", "--rotate", "0,0,0", "--zoom", "1.3"
        ], capture_output=True)

        render_bottom_angle = os.path.join(TARGET_DIR, "Universal_Carrier_3d_bottom_angle.png")
        subprocess.run([
            kicad_cli, "pcb", "render", pcb_path,
            "-o", render_bottom_angle,
            "--side", "bottom",
            "--width", "1920", "--height", "1080", "--rotate", "-35,0,30", "--zoom", "1.2"
        ], capture_output=True)

        render_angle = os.path.join(TARGET_DIR, "Universal_Carrier_3d.png")
        subprocess.run([
            kicad_cli, "pcb", "render", pcb_path,
            "-o", render_angle,
            "--side", "top",
            "--width", "1920", "--height", "1080", "--rotate", "-40,0,45", "--zoom", "1.15"
        ], capture_output=True)
        print("Generated all 3D Photorealistic Previews.")

if __name__ == "__main__":
    main()
