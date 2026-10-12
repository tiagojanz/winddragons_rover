#!/usr/bin/env python3
"""
WindDragons Universal Carrier Board V4.2 - I2C Architecture Update
Adds PCA9685 (16-Ch PWM) and ADS1115 (16-Bit 4-Ch ADC).
Relocates tactile buttons to the left/center on B.Cu.
Removes obsolete individual servo headers J7-J10.
Updates J5 to DETECTOR_ANCORA on IO13.
"""

import os
import uuid
import json
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
    with open(path, "w") as f:
        json.dump(pro_data, f, indent=2)

def generate_kicad_prl(path):
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
    with open(path, "w") as f:
        json.dump(prl_data, f, indent=2)

# ==============================================================================
# 2. KICAD PCB (.kicad_pcb)
# ==============================================================================
def generate_kicad_pcb(path):
    all_nets = [
        "", "/GND", "/+3V3", "/+5V", "/+5V_SERVO",
        "/IO5_NSS", "/IO4_MISO", "/IO3_MOSI", "/IO2_SCK",
        "/I2C_SDA", "/I2C_SCL", "/JOY_X", "/JOY_Y",
        "/IO9_BOOT", "/IO18_UP", "/IO19_MODE", "/IO20_DN", "/IO23_SPARE",
        "/IO12_LORA_DIO0", "/IO13_ANCORA_DET", "/IO16_GPS_RX", "/IO17_GPS_TX",
        "/IO15", "/GPS_TX2", "/GPS_RX2",
        "/BATT_VOLT_APM", "/BATT_CURR_APM", "/WATER_SENS", "/AUX_ADC",
        "/NC"
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
        '    (date "2026-10-12")',
        '    (rev "V4.2 - I2C PCA9685 & ADS1115 Clean")',
        '    (company "WindDragons Project")',
        '    (comment 1 "ESP32-C6-LCD-1.47 Carrier - 100x85mm 2-Layer JLCPCB")',
        '    (comment 2 "I2C Expansion: PCA9685 16-Ch PWM & ADS1115 16-Bit ADC")',
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
            lines.append('  (net 0 "")')
        else:
            lines.append(f'  (net {get_net(n)} "{net_name(n)}")')

    # Board Outline (Edge.Cuts) 100 x 85 mm
    corners = [(0.0, 0.0), (BOARD_W, 0.0), (BOARD_W, BOARD_H), (0.0, BOARD_H)]
    for i in range(4):
        p1 = corners[i]
        p2 = corners[(i + 1) % 4]
        lines.append(f'  (gr_line (start {p1[0]} {p1[1]}) (end {p2[0]} {p2[1]}) (stroke (width 0.15) (type solid)) (layer "Edge.Cuts"))')

    # 4x M3 Mounting Holes
    mount_holes = [
        ("H1", 4.5, 4.5),
        ("H2", BOARD_W - 4.5, 4.5),
        ("H3", 4.5, BOARD_H - 4.5),
        ("H4", BOARD_W - 4.5, BOARD_H - 4.5),
    ]
    for href, hx, hy in mount_holes:
        lines.append(f'  (footprint "MountingHole:MountingHole_3.2mm_M3" (layer "F.Cu") (uuid "{new_uuid()}")')
        lines.append(f'    (at {hx} {hy})')
        lines.append(f'    (fp_text reference "{href}" (at 0 -2.5) (layer "F.SilkS") (effects (font (size 0.8 0.8) (thickness 0.15))))')
        lines.append(f'    (fp_text value "MountingHole" (at 0 2.5) (layer "F.Fab") (effects (font (size 0.8 0.8) (thickness 0.15)) (hide yes)))')
        lines.append('    (pad "" np_thru_hole circle (at 0 0) (size 3.2 3.2) (drill 3.2) (layers *.Cu *.Mask))')
        lines.append('  )')

    # ESP32 Headers (J1 Top, J2 Bottom) on B.Cu
    esp_t_nets = [
        "IO5_NSS", "IO4_MISO", "IO3_MOSI", "IO2_SCK",
        "I2C_SCL", "I2C_SDA", "+3V3", "GND", "+5V"
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
        "IO9_BOOT", "IO18_UP", "IO19_MODE", "IO20_DN", "IO23_SPARE",
        "IO12_LORA_DIO0", "IO13_ANCORA_DET", "IO16_GPS_RX", "IO17_GPS_TX"
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

    # LoRa Ra-02 Header J3 (2x4, X=12.0, Y=16.0 to 23.62) - FRONT of PCB
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
    lines.append(f'    (at 17.08 18.5 -90)')
    lines.append(f'    (fp_text reference "J3" (at 1.27 -2.4 90) (layer "F.SilkS") (effects (font (size 0.9 0.9) (thickness 0.14))))')
    lines.append(f'    (fp_text value "SX1278_LORA_2x4" (at 1.27 10.2 90) (layer "F.Fab") (effects (font (size 0.9 0.9) (thickness 0.14))))')
    lines.append(f'    (fp_line (start -1.33 -1.33) (end 3.87 -1.33) (stroke (width 0.12) (type solid)) (layer "F.SilkS"))')
    lines.append(f'    (fp_line (start 3.87 -1.33) (end 3.87 8.95) (stroke (width 0.12) (type solid)) (layer "F.SilkS"))')
    lines.append(f'    (fp_line (start 3.87 8.95) (end -1.33 8.95) (stroke (width 0.12) (type solid)) (layer "F.SilkS"))')
    lines.append(f'    (fp_line (start -1.33 8.95) (end -1.33 -1.33) (stroke (width 0.12) (type solid)) (layer "F.SilkS"))')
    lines.append(f'    (fp_text user "GND" (at -2.8 0 90) (layer "F.SilkS") (effects (font (size 0.7 0.7) (thickness 0.12))))')
    lines.append(f'    (fp_text user "3V3" (at 5.4 0 90) (layer "F.SilkS") (effects (font (size 0.7 0.7) (thickness 0.12))))')
    lines.append(f'    (fp_text user "NC" (at -2.8 2.54 90) (layer "F.SilkS") (effects (font (size 0.7 0.7) (thickness 0.12))))')
    lines.append(f'    (fp_text user "NSS" (at 5.4 2.54 90) (layer "F.SilkS") (effects (font (size 0.7 0.7) (thickness 0.12))))')
    lines.append(f'    (fp_text user "SCK" (at -2.8 5.08 90) (layer "F.SilkS") (effects (font (size 0.7 0.7) (thickness 0.12))))')
    lines.append(f'    (fp_text user "MOSI" (at 5.7 5.08 90) (layer "F.SilkS") (effects (font (size 0.7 0.7) (thickness 0.12))))')
    lines.append(f'    (fp_text user "MISO" (at -3.0 7.62 90) (layer "F.SilkS") (effects (font (size 0.7 0.7) (thickness 0.12))))')
    lines.append(f'    (fp_text user "DIO0" (at 5.7 7.62 90) (layer "F.SilkS") (effects (font (size 0.7 0.7) (thickness 0.12))))')
    for p_num, px, py, n_name, p_shape in lora_defs:
        lines.append(f'    (pad "{p_num}" thru_hole {p_shape} (at {px:.2f} {py:.2f}) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net(n_name)} "{net_name(n_name)}"))')
    lines.append('  )')

    # JOY1 PS5 3D Joystick (cx=20.0, cy=62.0) Alps RKJXV1224005 on B.Cu
    cx, cy = 20.0, 62.0
    lines.append(f'  (footprint "PS5_joystick:Alps_RKJXV1224005" (layer "B.Cu") (uuid "{new_uuid()}")')
    lines.append(f'    (at {cx} {cy})')
    lines.append(f'    (fp_text reference "JOY1" (at 0 -11.5) (layer "B.SilkS") (effects (font (size 0.8 0.8) (thickness 0.15)) (justify mirror)))')
    lines.append(f'    (fp_text value "PS5_JOYSTICK" (at 0 13.0) (layer "B.Fab") (effects (font (size 1.0 1.0) (thickness 0.15)) (justify mirror) (hide yes)))')
    lines.append('    (fp_line (start -8.0 -8.0) (end -5.0 -8.0) (layer "B.SilkS") (width 0.15))')
    lines.append('    (fp_line (start -8.0 -8.0) (end -8.0 -5.0) (layer "B.SilkS") (width 0.15))')
    lines.append('    (fp_line (start 8.0 -8.0) (end 5.0 -8.0) (layer "B.SilkS") (width 0.15))')
    lines.append('    (fp_line (start 8.0 -8.0) (end 8.0 -5.0) (layer "B.SilkS") (width 0.15))')
    lines.append('    (fp_line (start -8.0 8.0) (end -5.0 8.0) (layer "B.SilkS") (width 0.15))')
    lines.append('    (fp_line (start -8.0 8.0) (end -8.0 5.0) (layer "B.SilkS") (width 0.15))')
    lines.append('    (fp_line (start 8.0 8.0) (end 5.0 8.0) (layer "B.SilkS") (width 0.15))')
    lines.append('    (fp_line (start 8.0 8.0) (end 8.0 5.0) (layer "B.SilkS") (width 0.15))')
    lines.append('    (fp_circle (center 0 0) (end 2.0 0) (layer "B.SilkS") (width 0.15) (fill none))')
    lines.append('    (fp_rect (start -8.0 -8.0) (end 8.0 8.0) (layer "B.Fab") (width 0.15) (fill none))')
    lines.append('    (fp_rect (start -4.7 -6.5) (end 4.7 -9.5) (layer "B.Fab") (width 0.15) (fill none))')
    lines.append('    (fp_rect (start -6.5 -4.7) (end -9.5 4.7) (layer "B.Fab") (width 0.15) (fill none))')
    lines.append('    (fp_rect (start -4.5 6.5) (end 4.5 11.5) (layer "B.Fab") (width 0.15) (fill none))')
    lines.append(f'    (pad "1" thru_hole circle (at -8.73 -2.5) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "B.SilkS") (net {get_net("GND")} "{net_name("GND")}"))')
    lines.append(f'    (pad "2" thru_hole circle (at -8.73 0.0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "B.SilkS") (net {get_net("JOY_Y")} "{net_name("JOY_Y")}"))')
    lines.append(f'    (pad "3" thru_hole circle (at -8.73 2.5) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "B.SilkS") (net {get_net("+3V3")} "{net_name("+3V3")}"))')
    lines.append(f'    (pad "4" thru_hole circle (at -2.5 -8.73) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "B.SilkS") (net {get_net("GND")} "{net_name("GND")}"))')
    lines.append(f'    (pad "5" thru_hole circle (at 0.0 -8.73) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "B.SilkS") (net {get_net("JOY_X")} "{net_name("JOY_X")}"))')
    lines.append(f'    (pad "6" thru_hole circle (at 2.5 -8.73) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "B.SilkS") (net {get_net("+3V3")} "{net_name("+3V3")}"))')
    lines.append(f'    (pad "7" thru_hole circle (at -3.25 5.75) (size 1.8 1.8) (drill 1.2) (layers *.Cu *.Mask "B.SilkS") (net {get_net("GND")} "{net_name("GND")}"))')
    lines.append(f'    (pad "8" thru_hole circle (at -3.25 10.25) (size 1.8 1.8) (drill 1.2) (layers *.Cu *.Mask "B.SilkS") (net {get_net("GND")} "{net_name("GND")}"))')
    lines.append(f'    (pad "9" thru_hole circle (at 3.25 5.75) (size 1.8 1.8) (drill 1.2) (layers *.Cu *.Mask "B.SilkS") (net {get_net("IO9_BOOT")} "{net_name("IO9_BOOT")}"))')
    lines.append(f'    (pad "10" thru_hole circle (at 3.25 10.25) (size 1.8 1.8) (drill 1.2) (layers *.Cu *.Mask "B.SilkS") (net {get_net("IO9_BOOT")} "{net_name("IO9_BOOT")}"))')
    lines.append(f'    (pad "11" thru_hole circle (at -6.325 -5.0) (size 2.3 2.3) (drill 1.5) (layers *.Cu *.Mask "B.SilkS") (net {get_net("GND")} "{net_name("GND")}"))')
    lines.append(f'    (pad "12" thru_hole circle (at 6.325 -5.0) (size 2.3 2.3) (drill 1.5) (layers *.Cu *.Mask "B.SilkS") (net {get_net("GND")} "{net_name("GND")}"))')
    lines.append(f'    (pad "13" thru_hole circle (at -6.325 5.0) (size 2.3 2.3) (drill 1.5) (layers *.Cu *.Mask "B.SilkS") (net {get_net("GND")} "{net_name("GND")}"))')
    lines.append(f'    (pad "14" thru_hole circle (at 6.325 5.0) (size 2.3 2.3) (drill 1.5) (layers *.Cu *.Mask "B.SilkS") (net {get_net("GND")} "{net_name("GND")}"))')
    lines.append('    (pad "" np_thru_hole circle (at -4.3 0.0) (size 1.6 1.6) (drill 1.6) (layers *.Cu *.Mask))')
    lines.append('    (pad "" np_thru_hole circle (at 4.3 0.0) (size 1.6 1.6) (drill 1.6) (layers *.Cu *.Mask))')
    lines.append('  )')

    # Battery / APM Power Module Header J4 (X=90.5, Y=49.5, rot=180) - FRONT of PCB
    lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Horizontal" (layer "F.Cu") (uuid "{new_uuid()}")')
    lines.append(f'    (at 90.5 49.5 180)')
    lines.append(f'    (fp_text reference "J4" (at 2.54 -2.6 0) (layer "F.SilkS") (effects (font (size 0.8 0.8) (thickness 0.15))))')
    lines.append(f'    (fp_text value "BATT_APM" (at 2.54 2.6 0) (layer "F.Fab") (effects (font (size 0.8 0.8) (thickness 0.15)) (hide yes)))')
    lines.append(f'    (pad "1" thru_hole rect (at 0 0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("BATT_VOLT_APM")} "{net_name("BATT_VOLT_APM")}"))')
    lines.append(f'    (pad "2" thru_hole circle (at 2.54 0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("BATT_CURR_APM")} "{net_name("BATT_CURR_APM")}"))')
    lines.append(f'    (pad "3" thru_hole circle (at 5.08 0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("GND")} "{net_name("GND")}"))')
    lines.append('  )')

    # Detector Âncora Header J5 (X=65.0, Y=42.5) - FRONT of PCB
    lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical" (layer "F.Cu") (uuid "{new_uuid()}")')
    lines.append(f'    (at 65.0 42.5)')
    lines.append(f'    (fp_text reference "J5" (at -2.5 0 90) (layer "F.SilkS") (effects (font (size 0.8 0.8) (thickness 0.15)) (hide yes)))')
    lines.append(f'    (fp_text value "DETECTOR_ANCORA" (at 2.5 1.27 90) (layer "F.Fab") (effects (font (size 0.8 0.8) (thickness 0.15)) (hide yes)))')
    lines.append(f'    (pad "1" thru_hole rect (at 0 0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("IO13_ANCORA_DET")} "{net_name("IO13_ANCORA_DET")}"))')
    lines.append(f'    (pad "2" thru_hole circle (at 0 2.54) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("GND")} "{net_name("GND")}"))')
    lines.append('  )')

    # Quectel LC29H GNSS (1x7 Header, Horizontal, X=76.38 to 91.62, Y=18.50) - FRONT of PCB
    gps_defs = [
        (1, 0.00, "IO15", "rect", "P"),
        (2, 2.54, "GPS_TX2", "circle", "T2"),
        (3, 5.08, "GPS_RX2", "circle", "R2"),
        (4, 7.62, "IO17_GPS_TX", "circle", "R1"),
        (5, 10.16, "IO16_GPS_RX", "circle", "T1"),
        (6, 12.70, "GND", "circle", "G"),
        (7, 15.24, "+3V3", "circle", "V"),
    ]
    lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x07_P2.54mm_Horizontal" (layer "F.Cu") (uuid "{new_uuid()}")')
    lines.append(f'    (at 76.38 18.50)')
    lines.append(f'    (fp_text reference "J6" (at 0 3.0 0) (layer "F.SilkS") (effects (font (size 0.8 0.8) (thickness 0.13))))')
    lines.append(f'    (fp_text value "GPS_LC29H_7PIN" (at 7.62 9.5 0) (layer "F.Fab") (effects (font (size 0.9 0.9) (thickness 0.14))))')
    for p_num, px_rel, n_name, p_shape, p_lbl in gps_defs:
        lines.append(f'    (fp_text user "{p_lbl}" (at {px_rel:.2f} -2.7 0) (layer "F.SilkS") (effects (font (size 0.7 0.7) (thickness 0.12))))')
        lines.append(f'    (pad "{p_num}" thru_hole {p_shape} (at {px_rel:.2f} 0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net(n_name)} "{net_name(n_name)}"))')
    lines.append('  )')

    # ==========================================================================
    # NEW MODULE 1: ADS1115 (1x10 Header, Horizontal at X=8.0, Y=44.0) - FRONT
    # Matches user rectangle (7.0, 38.0) to (35.0, 50.0)
    # ==========================================================================
    ads_defs = [
        (1, 0.00,  "+3V3", "rect", "VDD"),
        (2, 2.54,  "GND", "circle", "GND"),
        (3, 5.08,  "I2C_SCL", "circle", "SCL"),
        (4, 7.62,  "I2C_SDA", "circle", "SDA"),
        (5, 10.16, "GND", "circle", "ADDR"),
        (6, 12.70, "NC", "circle", "ALRT"),
        (7, 15.24, "BATT_VOLT_APM", "circle", "A0"),
        (8, 17.78, "BATT_CURR_APM", "circle", "A1"),
        (9, 20.32, "WATER_SENS", "circle", "A2"),
        (10, 22.86, "AUX_ADC", "circle", "A3"),
    ]
    lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x10_P2.54mm_Horizontal" (layer "F.Cu") (uuid "{new_uuid()}")')
    lines.append(f'    (at 34.64 49.0)')
    lines.append(f'    (fp_text reference "J_ADS" (at 11.43 -2.2 0) (layer "F.SilkS") (effects (font (size 0.75 0.75) (thickness 0.13)) (hide yes)))')
    lines.append(f'    (fp_text value "ADS1115_ADC" (at 11.43 3.5 0) (layer "F.Fab") (effects (font (size 0.8 0.8) (thickness 0.15)) (hide yes)))')
    for p_num, px_rel, n_name, p_shape, p_lbl in ads_defs:
        lines.append(f'    (fp_text user "{p_lbl}" (at {px_rel:.2f} 2.0 90) (layer "F.SilkS") (effects (font (size 0.55 0.55) (thickness 0.10))))')
        lines.append(f'    (pad "{p_num}" thru_hole {p_shape} (at {px_rel:.2f} 0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net(n_name)} "{net_name(n_name)}"))')
    lines.append('  )')

    # ==========================================================================
    # NEW MODULE 2: PCA9685 (62.5 x 25.4 mm Horizontal at X: 32.0..94.5, Y: 53.0..78.4)
    # Replaces old J7-J10 servo headers!
    # ==========================================================================
    # Input Header (1x06 Vertical at X=33.5, Y=65.7)
    pca_in_defs = [
        (1, 0.0, -6.35, "GND", "rect", "GND"),
        (2, 0.0, -3.81, "GND", "circle", "OE"),
        (3, 0.0, -1.27, "I2C_SCL", "circle", "SCL"),
        (4, 0.0,  1.27, "I2C_SDA", "circle", "SDA"),
        (5, 0.0,  3.81, "+3V3", "circle", "VCC"),
        (6, 0.0,  6.35, "+5V_SERVO", "circle", "V+"),
    ]
    lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x06_P2.54mm_Vertical" (layer "F.Cu") (uuid "{new_uuid()}")')
    lines.append(f'    (at 33.5 65.7)')
    lines.append(f'    (fp_text reference "J_PCA_IN" (at -2.5 0 90) (layer "F.SilkS") (effects (font (size 0.8 0.8) (thickness 0.15))))')
    lines.append(f'    (fp_text value "PCA9685_IN" (at 2.5 0 90) (layer "F.Fab") (effects (font (size 0.8 0.8) (thickness 0.15)) (hide yes)))')
    for p_num, px, py, n_name, p_shape, p_lbl in pca_in_defs:
        lines.append(f'    (fp_text user "{p_lbl}" (at 2.2 {py:.2f} 0) (layer "F.SilkS") (effects (font (size 0.6 0.6) (thickness 0.11))))')
        lines.append(f'    (pad "{p_num}" thru_hole {p_shape} (at {px:.2f} {py:.2f}) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net(n_name)} "{net_name(n_name)}"))')
    lines.append('  )')

    # Cascade Output Header (1x06 Vertical at X=93.0, Y=65.7) for Mechanical Support
    lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x06_P2.54mm_Vertical" (layer "F.Cu") (uuid "{new_uuid()}")')
    lines.append(f'    (at 93.0 65.7)')
    lines.append(f'    (fp_text reference "J_PCA_OUT" (at 2.5 0 90) (layer "F.SilkS") (effects (font (size 0.8 0.8) (thickness 0.15))))')
    lines.append(f'    (fp_text value "PCA9685_OUT" (at -2.5 0 90) (layer "F.Fab") (effects (font (size 0.8 0.8) (thickness 0.15)) (hide yes)))')
    for p_num, px, py, n_name, p_shape, p_lbl in pca_in_defs:
        lines.append(f'    (pad "{p_num}" thru_hole {p_shape} (at {px:.2f} {py:.2f}) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net(n_name)} "{net_name(n_name)}"))')
    lines.append('  )')

    # J11 ESP Logic Power In (X=11.46, Y=11.5) - FRONT of PCB
    lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Horizontal" (layer "F.Cu") (uuid "{new_uuid()}")')
    lines.append(f'    (at 11.46 11.5)')
    lines.append(f'    (fp_text reference "J11" (at 1.27 -4.0) (layer "F.SilkS") (effects (font (size 0.8 0.8) (thickness 0.15))))')
    lines.append(f'    (fp_text value "PWR_ESP_5V" (at 1.27 2.2) (layer "F.Fab") (effects (font (size 0.8 0.8) (thickness 0.15)) (hide yes)))')
    lines.append(f'    (pad "1" thru_hole rect (at 0 0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("GND")} "{net_name("GND")}"))')
    lines.append(f'    (pad "2" thru_hole circle (at 2.54 0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("+5V")} "{net_name("+5V")}"))')
    lines.append('  )')

    # J12 High-Power Servo Power In (X=24.96, Y=79.0) - FRONT of PCB
    lines.append(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Horizontal" (layer "F.Cu") (uuid "{new_uuid()}")')
    lines.append(f'    (at 24.96 79.0)')
    lines.append(f'    (fp_text reference "J12" (at 1.27 -3.5) (layer "F.SilkS") (effects (font (size 0.8 0.8) (thickness 0.15))) (hide yes))')
    lines.append(f'    (fp_text value "PWR_SERVO_5V" (at 1.27 2.2) (layer "F.Fab") (effects (font (size 0.8 0.8) (thickness 0.15)) (hide yes)))')
    lines.append(f'    (pad "1" thru_hole rect (at 0 0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("GND")} "{net_name("GND")}"))')
    lines.append(f'    (pad "2" thru_hole circle (at 2.54 0) (size 1.7 1.7) (drill 1.0) (layers *.Cu *.Mask "F.SilkS") (net {get_net("+5V_SERVO")} "{net_name("+5V_SERVO")}"))')
    lines.append('  )')

    # 4x Tactile Navigation Pushbuttons (SW1-SW4) - Relocated to Center-Left on BACK (B.Cu)
    buttons = [
        ("SW1", "UP", 62.0, 61.5, "IO18_UP"),
        ("SW2", "MODE", 74.6, 61.5, "IO19_MODE"),
        ("SW3", "DOWN", 68.4, 56.5, "IO20_DN"),
        ("SW4", "BOOT", 68.5, 66.0, "IO9_BOOT"),
    ]
    for ref, val, bx, by, sig_net in buttons:
        lines.append(f'  (footprint "PS4_joystick:SKRKAEE020" (layer "B.Cu") (uuid "{new_uuid()}")')
        lines.append(f'    (at {bx} {by})')
        lines.append(f'    (property "Reference" "{ref}" (at 0 -2.3 0) (layer "B.SilkS") (uuid "{new_uuid()}") (effects (font (size 0.8 0.8) (thickness 0.15)) (justify mirror)))')
        lines.append(f'    (property "Value" "{val}" (at 0 2.3 0) (layer "B.SilkS") (uuid "{new_uuid()}") (effects (font (size 0.7 0.7) (thickness 0.15)) (justify mirror)))')
        lines.append(f'    (property "Datasheet" "" (at 0 0 0) (layer "F.Fab") (hide yes) (uuid "{new_uuid()}"))')
        lines.append(f'    (property "Description" "" (at 0 0 0) (layer "F.Fab") (hide yes) (uuid "{new_uuid()}"))')
        lines.append(f'    (attr smd)')
        lines.append(f'    (duplicate_pad_numbers_are_jumpers no)')
        lines.append(f'    (fp_line (start -2.05 -1.55) (end 2.05 -1.55) (stroke (width 0.12) (type solid)) (layer "B.SilkS") (uuid "{new_uuid()}"))')
        lines.append(f'    (fp_line (start 2.05 1.55) (end -2.05 1.55) (stroke (width 0.12) (type solid)) (layer "B.SilkS") (uuid "{new_uuid()}"))')
        lines.append(f'    (fp_line (start -2.75 -1.75) (end 2.75 -1.75) (stroke (width 0.05) (type solid)) (layer "B.CrtYd") (uuid "{new_uuid()}"))')
        lines.append(f'    (fp_line (start -2.75 1.75) (end -2.75 -1.75) (stroke (width 0.05) (type solid)) (layer "B.CrtYd") (uuid "{new_uuid()}"))')
        lines.append(f'    (fp_line (start 2.75 -1.75) (end 2.75 1.75) (stroke (width 0.05) (type solid)) (layer "B.CrtYd") (uuid "{new_uuid()}"))')
        lines.append(f'    (fp_line (start 2.75 1.75) (end -2.75 1.75) (stroke (width 0.05) (type solid)) (layer "B.CrtYd") (uuid "{new_uuid()}"))')
        lines.append(f'    (pad "1" smd roundrect (at -2.1 0) (size 0.8 2) (layers "B.Cu" "B.Mask" "B.Paste") (roundrect_rratio 0.25) (net {get_net(sig_net)} "{net_name(sig_net)}") (uuid "{new_uuid()}"))')
        lines.append(f'    (pad "2" smd roundrect (at 2.1 0) (size 0.8 2) (layers "B.Cu" "B.Mask" "B.Paste") (roundrect_rratio 0.25) (net {get_net("GND")} "{net_name("GND")}") (uuid "{new_uuid()}"))')
        lines.append(f'    (embedded_fonts no)')
        lines.append('  )')

    # Silkscreen Outlines (Front & Back)
    front_silk_rects = [
        (1.5, 1.5, BOARD_W - 1.5, BOARD_H - 1.5, 0.2),
        (5.0, 16.0, 21.5, 45.5, 0.2),     # SX1278 (Vertical)
        (70.9, 13.5, 97.1, 43.6, 0.2),    # GPS LC29H
        (32.0, 40.0, 60.0, 52.0, 0.2),    # ADS1115
        (32.0, 53.0, 94.5, 78.4, 0.25),   # PCA9685 (62.5 x 25.4mm)
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

    pca_holes = [(33.5, 55.0), (93.0, 55.0), (33.5, 76.4), (93.0, 76.4)]
    for hx, hy in pca_holes:
        lines.append(f'  (gr_circle (center {hx} {hy}) (end {hx+1.5} {hy}) (stroke (width 0.15) (type default)) (fill none) (layer "F.SilkS"))')

    f_silk_texts = [
        ("WINDDRAGONS UNIVERSAL CARRIER V4.2", 50.0, 4.0, 1.1),
        ("INTERNAL PERIPHERALS & DAUGHTERBOARDS (FRONT)", 50.0, 6.2, 0.8),
        ("SX1278 433MHz", 12.5, 40.0, 0.85),
        ("ADS1115 16-BIT ADC (0x48)", 46.0, 38.5, 0.85),
        ("PCA9685 16-CH 12-BIT PWM (I2C: 0x40)", 65.75, 57.0, 0.85),
        ("CH0:ESC  CH1:LEME  CH2:GUINCHO  CH3:RACK  CH4-15:LEDS", 65.5, 80.5, 0.7),
        ("DETECTOR ANCORA", 69.0, 47.0, 0.8),
        ("+", 67.5, 42.5, 0.8),
        ("-", 67.5, 45.0, 0.8),
        ("PWR SERVO 5V", 25.5, 76.5, 0.8),
        ("-  +", 26.23, 81.5, 0.8),
        ("GPS 26.2x30.1mm", 84.0, 36.5, 0.8),
        ("QUECTEL LC29H", 84.0, 11.5, 0.85),
        ("TELEMETRIA BAT.", 88.0, 47.0, 0.8),
        ("PWR ESP 5V", 6.5, 9.5, 0.8),
        ("-  +", 12.5, 9.5, 0.8),
    ]
    for txt, tx, ty, sz in f_silk_texts:
        lines.append(f'  (gr_text "{txt}" (at {tx} {ty}) (layer "F.SilkS") (effects (font (size {sz} {sz}) (thickness 0.15))))')

    b_silk_texts = [
        ("WINDDRAGONS REMOTE CONTROL & CARRIER V4.2", 50.0, 5.0, 1.1),
        ("USER INTERFACE: DISPLAY & PS5 JOYSTICK", 50.0, 7.5, 0.8),
        ("ST7789 1.47 INCH LCD (320x172)", 50.0, 27.0, 0.85),
        ("USB-C", 25.0, 25.0, 0.7),
        ("PS5 3D JOYSTICK (ALPS RKJXV)", 20.0, 76.5, 0.85),
        ("SOLID GND COPPER PLANES ON F.CU & B.CU (100x85mm)", 50.0, 81.5, 0.85),
    ]
    for txt, tx, ty, sz in b_silk_texts:
        lines.append(f'  (gr_text "{txt}" (at {tx} {ty}) (layer "B.SilkS") (effects (font (size {sz} {sz}) (thickness 0.15)) (justify mirror)))')

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
        '		(date "2026-10-12")',
        '		(rev "V4.2 - I2C PCA9685 & ADS1115 Clean")',
        '		(company "WindDragons Project")',
        '		(comment 1 "ESP32-C6-LCD-1.47 Carrier - 100x85mm 2-Layer JLCPCB")',
        '		(comment 2 "I2C Expansion: PCA9685 16-Ch PWM & ADS1115 16-Bit ADC")',
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
        '		(symbol "Connector_Generic:Conn_01x06"',
        '			(pin_names (offset 1.016) (hide yes))',
        '			(property "Reference" "J" (at 0 8.89 0) (effects (font (size 1.27 1.27))))',
        '			(property "Value" "Conn_01x06" (at 0 -8.89 0) (effects (font (size 1.27 1.27))))',
        '			(property "Footprint" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))',
        '			(symbol "Conn_01x06_1_1"',
        '				(rectangle (start -1.27 7.62) (end 1.27 -7.62) (stroke (width 0.254) (type default)) (fill (type background)))',
        '				(pin passive line (at -5.08 6.35 0) (length 3.81) (name "Pin_1" (effects (font (size 1.27 1.27)))) (number "1" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 3.81 0) (length 3.81) (name "Pin_2" (effects (font (size 1.27 1.27)))) (number "2" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 1.27 0) (length 3.81) (name "Pin_3" (effects (font (size 1.27 1.27)))) (number "3" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 -1.27 0) (length 3.81) (name "Pin_4" (effects (font (size 1.27 1.27)))) (number "4" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 -3.81 0) (length 3.81) (name "Pin_5" (effects (font (size 1.27 1.27)))) (number "5" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 -6.35 0) (length 3.81) (name "Pin_6" (effects (font (size 1.27 1.27)))) (number "6" (effects (font (size 1.27 1.27)))))',
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
        '		(symbol "Connector_Generic:Conn_01x10"',
        '			(pin_names (offset 1.016) (hide yes))',
        '			(property "Reference" "J" (at 0 13.97 0) (effects (font (size 1.27 1.27))))',
        '			(property "Value" "Conn_01x10" (at 0 -13.97 0) (effects (font (size 1.27 1.27))))',
        '			(property "Footprint" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))',
        '			(symbol "Conn_01x10_1_1"',
        '				(rectangle (start -1.27 12.7) (end 1.27 -12.7) (stroke (width 0.254) (type default)) (fill (type background)))',
        '				(pin passive line (at -5.08 11.43 0) (length 3.81) (name "Pin_1" (effects (font (size 1.27 1.27)))) (number "1" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 8.89 0) (length 3.81) (name "Pin_2" (effects (font (size 1.27 1.27)))) (number "2" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 6.35 0) (length 3.81) (name "Pin_3" (effects (font (size 1.27 1.27)))) (number "3" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 3.81 0) (length 3.81) (name "Pin_4" (effects (font (size 1.27 1.27)))) (number "4" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 1.27 0) (length 3.81) (name "Pin_5" (effects (font (size 1.27 1.27)))) (number "5" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 -1.27 0) (length 3.81) (name "Pin_6" (effects (font (size 1.27 1.27)))) (number "6" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 -3.81 0) (length 3.81) (name "Pin_7" (effects (font (size 1.27 1.27)))) (number "7" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 -6.35 0) (length 3.81) (name "Pin_8" (effects (font (size 1.27 1.27)))) (number "8" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 -8.89 0) (length 3.81) (name "Pin_9" (effects (font (size 1.27 1.27)))) (number "9" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at -5.08 -11.43 0) (length 3.81) (name "Pin_10" (effects (font (size 1.27 1.27)))) (number "10" (effects (font (size 1.27 1.27)))))',
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
        '		(symbol "Device:Joystick_PS5_Thumbstick"',
        '			(pin_names (offset 1.016))',
        '			(property "Reference" "JOY" (at 0 15.24 0) (effects (font (size 1.27 1.27))))',
        '			(property "Value" "PS5_JOYSTICK" (at 0 -15.24 0) (effects (font (size 1.27 1.27))))',
        '			(property "Footprint" "PS5_joystick:Alps_RKJXV1224005" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))',
        '			(symbol "Joystick_PS5_Thumbstick_1_1"',
        '				(rectangle (start -10.16 12.70) (end 10.16 -15.24) (stroke (width 0.254) (type default)) (fill (type background)))',
        '				(pin passive line (at -13.97 10.16 0) (length 3.81) (name "Y_GND" (effects (font (size 0.9 0.9)))) (number "1" (effects (font (size 0.9 0.9)))))',
        '				(pin passive line (at -13.97 7.62 0) (length 3.81) (name "JOY_Y" (effects (font (size 0.9 0.9)))) (number "2" (effects (font (size 0.9 0.9)))))',
        '				(pin passive line (at -13.97 5.08 0) (length 3.81) (name "Y_3V3" (effects (font (size 0.9 0.9)))) (number "3" (effects (font (size 0.9 0.9)))))',
        '				(pin passive line (at -13.97 0.00 0) (length 3.81) (name "X_GND" (effects (font (size 0.9 0.9)))) (number "4" (effects (font (size 0.9 0.9)))))',
        '				(pin passive line (at -13.97 -2.54 0) (length 3.81) (name "JOY_X" (effects (font (size 0.9 0.9)))) (number "5" (effects (font (size 0.9 0.9)))))',
        '				(pin passive line (at -13.97 -5.08 0) (length 3.81) (name "X_3V3" (effects (font (size 0.9 0.9)))) (number "6" (effects (font (size 0.9 0.9)))))',
        '				(pin passive line (at 13.97 10.16 180) (length 3.81) (name "SW_GND1" (effects (font (size 0.9 0.9)))) (number "7" (effects (font (size 0.9 0.9)))))',
        '				(pin passive line (at 13.97 7.62 180) (length 3.81) (name "SW_GND2" (effects (font (size 0.9 0.9)))) (number "8" (effects (font (size 0.9 0.9)))))',
        '				(pin passive line (at 13.97 5.08 180) (length 3.81) (name "SW_BOOT1" (effects (font (size 0.9 0.9)))) (number "9" (effects (font (size 0.9 0.9)))))',
        '				(pin passive line (at 13.97 2.54 180) (length 3.81) (name "SW_BOOT2" (effects (font (size 0.9 0.9)))) (number "10" (effects (font (size 0.9 0.9)))))',
        '				(pin passive line (at 13.97 -2.54 180) (length 3.81) (name "SHIELD1" (effects (font (size 0.9 0.9)))) (number "11" (effects (font (size 0.9 0.9)))))',
        '				(pin passive line (at 13.97 -5.08 180) (length 3.81) (name "SHIELD2" (effects (font (size 0.9 0.9)))) (number "12" (effects (font (size 0.9 0.9)))))',
        '				(pin passive line (at 13.97 -7.62 180) (length 3.81) (name "SHIELD3" (effects (font (size 0.9 0.9)))) (number "13" (effects (font (size 0.9 0.9)))))',
        '				(pin passive line (at 13.97 -10.16 180) (length 3.81) (name "SHIELD4" (effects (font (size 0.9 0.9)))) (number "14" (effects (font (size 0.9 0.9)))))',
        '			)',
        '		)',
        '		(symbol "Switch:SW_Push"',
        '			(pin_names (offset 1.016) (hide yes))',
        '			(property "Reference" "SW" (at 0 3.81 0) (effects (font (size 1.27 1.27))))',
        '			(property "Value" "SW_Push" (at 0 -3.81 0) (effects (font (size 1.27 1.27))))',
        '			(property "Footprint" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))',
        '			(symbol "SW_Push_1_1"',
        '				(circle (center -2.032 0) (radius 0.508) (stroke (width 0) (type default)) (fill (type none)))',
        '				(circle (center 2.032 0) (radius 0.508) (stroke (width 0) (type default)) (fill (type none)))',
        '				(polyline (pts (xy -1.524 0.254) (xy 1.524 1.778)) (stroke (width 0.254) (type default)) (fill (type none)))',
        '				(polyline (pts (xy 0 1.016) (xy 0 3.048)) (stroke (width 0.254) (type default)) (fill (type none)))',
        '				(pin passive line (at -5.08 0 0) (length 3.048) (name "1" (effects (font (size 1.27 1.27)))) (number "1" (effects (font (size 1.27 1.27)))))',
        '				(pin passive line (at 5.08 0 180) (length 3.048) (name "2" (effects (font (size 1.27 1.27)))) (number "2" (effects (font (size 1.27 1.27)))))',
        '			)',
        '		)',
        '	)',
    ]

    def add_block(title, subtitle, x1, y1, x2, y2):
        sch_lines.append('	(polyline')
        sch_lines.append(f'		(pts (xy {x1:.2f} {y1:.2f}) (xy {x2:.2f} {y1:.2f}) (xy {x2:.2f} {y2:.2f}) (xy {x1:.2f} {y2:.2f}) (xy {x1:.2f} {y1:.2f}))')
        sch_lines.append('		(stroke (width 0.3) (type solid))')
        sch_lines.append(f'		(uuid "{new_uuid()}")')
        sch_lines.append('	)')
        tx = x1 + 2.54
        ty_title = y1 + 3.81
        ty_sub = y1 + 7.62
        sch_lines.append(f'	(text "{title}" (at {tx:.2f} {ty_title:.2f} 0) (effects (font (size 1.8 1.8) (bold yes))) (uuid "{new_uuid()}"))')
        sch_lines.append(f'	(text "{subtitle}" (at {tx:.2f} {ty_sub:.2f} 0) (effects (font (size 1.2 1.2) (italic yes))) (uuid "{new_uuid()}"))')

    def place_conn_1row(lib_id, ref, val, fp, x, y, labels):
        n = len(labels)
        sch_lines.append(f'	(symbol (lib_id "{lib_id}") (at {x:.2f} {y:.2f} 0) (unit 1)')
        sch_lines.append(f'		(uuid "{new_uuid()}")')
        sch_lines.append(f'		(property "Reference" "{ref}" (at {x:.2f} {y-n*1.27-1.5:.2f} 0) (effects (font (size 1.27 1.27))))')
        sch_lines.append(f'		(property "Value" "{val}" (at {x:.2f} {y+n*1.27+1.5:.2f} 0) (effects (font (size 1.27 1.27))))')
        sch_lines.append(f'		(property "Footprint" "{fp}" (at {x:.2f} {y:.2f} 0) (effects (font (size 1.27 1.27)) (hide yes)))')
        sch_lines.append('	)')
        for i, lbl in enumerate(labels):
            py = y + (n - 1) * 1.27 - i * 2.54
            p_x = x - 5.08
            p_end = x - 15.24
            sch_lines.append('	(wire')
            sch_lines.append(f'		(pts (xy {p_x:.2f} {py:.2f}) (xy {p_end:.2f} {py:.2f}))')
            sch_lines.append('		(stroke (width 0) (type solid))')
            sch_lines.append(f'		(uuid "{new_uuid()}")')
            sch_lines.append('	)')
            sch_lines.append(f'	(label "{lbl}" (at {p_end:.2f} {py:.2f} 0) (effects (font (size 1.27 1.27)) (justify right)))')

    def place_conn_1row_right(lib_id, ref, val, fp, x, y, labels):
        n = len(labels)
        sch_lines.append(f'	(symbol (lib_id "{lib_id}") (at {x:.2f} {y:.2f} 0) (unit 1)')
        sch_lines.append(f'		(uuid "{new_uuid()}")')
        sch_lines.append(f'		(property "Reference" "{ref}" (at {x:.2f} {y-n*1.27-1.5:.2f} 0) (effects (font (size 1.27 1.27))))')
        sch_lines.append(f'		(property "Value" "{val}" (at {x:.2f} {y+n*1.27+1.5:.2f} 0) (effects (font (size 1.27 1.27))))')
        sch_lines.append(f'		(property "Footprint" "{fp}" (at {x:.2f} {y:.2f} 0) (effects (font (size 1.27 1.27)) (hide yes)))')
        sch_lines.append('	)')
        for i, lbl in enumerate(labels):
            py = y + (n - 1) * 1.27 - i * 2.54
            p_x = x + 5.08
            p_end = x + 15.24
            sch_lines.append('	(wire')
            sch_lines.append(f'		(pts (xy {p_x:.2f} {py:.2f}) (xy {p_end:.2f} {py:.2f}))')
            sch_lines.append('		(stroke (width 0) (type solid))')
            sch_lines.append(f'		(uuid "{new_uuid()}")')
            sch_lines.append('	)')
            sch_lines.append(f'	(label "{lbl}" (at {p_end:.2f} {py:.2f} 0) (effects (font (size 1.27 1.27)) (justify left)))')

    def place_lora_2x4(ref, val, fp, x, y, labels_odd, labels_even):
        sch_lines.append(f'	(symbol (lib_id "Connector_Generic:Conn_02x04_Odd_Even") (at {x:.2f} {y:.2f} 0) (unit 1)')
        sch_lines.append(f'		(uuid "{new_uuid()}")')
        sch_lines.append(f'		(property "Reference" "{ref}" (at {x:.2f} {y-7.62:.2f} 0) (effects (font (size 1.27 1.27))))')
        sch_lines.append(f'		(property "Value" "{val}" (at {x:.2f} {y+7.62:.2f} 0) (effects (font (size 1.27 1.27))))')
        sch_lines.append(f'		(property "Footprint" "{fp}" (at {x:.2f} {y:.2f} 0) (effects (font (size 1.27 1.27)) (hide yes)))')
        sch_lines.append('	)')
        for i, lbl in enumerate(labels_odd):
            py = y + 2.54 - i * 2.54
            p_x = x - 5.08
            p_end = x - 15.24
            sch_lines.append('	(wire')
            sch_lines.append(f'		(pts (xy {p_x:.2f} {py:.2f}) (xy {p_end:.2f} {py:.2f}))')
            sch_lines.append('		(stroke (width 0) (type solid))')
            sch_lines.append(f'		(uuid "{new_uuid()}")')
            sch_lines.append('	)')
            sch_lines.append(f'	(label "{lbl}" (at {p_end:.2f} {py:.2f} 0) (effects (font (size 1.27 1.27)) (justify right)))')
        for i, lbl in enumerate(labels_even):
            py = y + 2.54 - i * 2.54
            p_x = x + 7.62
            p_end = x + 17.78
            sch_lines.append('	(wire')
            sch_lines.append(f'		(pts (xy {p_x:.2f} {py:.2f}) (xy {p_end:.2f} {py:.2f}))')
            sch_lines.append('		(stroke (width 0) (type solid))')
            sch_lines.append(f'		(uuid "{new_uuid()}")')
            sch_lines.append('	)')
            sch_lines.append(f'	(label "{lbl}" (at {p_end:.2f} {py:.2f} 0) (effects (font (size 1.27 1.27)) (justify left)))')

    def place_joystick(ref, val, fp, x, y):
        sch_lines.append(f'	(symbol (lib_id "Device:Joystick_PS5_Thumbstick") (at {x:.2f} {y:.2f} 0) (unit 1)')
        sch_lines.append(f'		(uuid "{new_uuid()}")')
        sch_lines.append(f'		(property "Reference" "{ref}" (at {x:.2f} {y-17.78:.2f} 0) (effects (font (size 1.27 1.27))))')
        sch_lines.append(f'		(property "Value" "{val}" (at {x:.2f} {y+17.78:.2f} 0) (effects (font (size 1.27 1.27))))')
        sch_lines.append(f'		(property "Footprint" "{fp}" (at {x:.2f} {y:.2f} 0) (effects (font (size 1.27 1.27)) (hide yes)))')
        sch_lines.append('	)')
        pins_left = [(1, "GND", 10.16), (2, "JOY_Y", 7.62), (3, "+3V3", 5.08), (4, "GND", 0.00), (5, "JOY_X", -2.54), (6, "+3V3", -5.08)]
        for pnum, lbl, py_off in pins_left:
            py = y + py_off
            px = x - 13.97
            pend = x - 25.40
            sch_lines.append('	(wire')
            sch_lines.append(f'		(pts (xy {px:.2f} {py:.2f}) (xy {pend:.2f} {py:.2f}))')
            sch_lines.append('		(stroke (width 0) (type solid))')
            sch_lines.append(f'		(uuid "{new_uuid()}")')
            sch_lines.append('	)')
            sch_lines.append(f'	(label "{lbl}" (at {pend:.2f} {py:.2f} 0) (effects (font (size 1.27 1.27)) (justify right)))')
        pins_right = [(7, "GND", 10.16), (8, "GND", 7.62), (9, "IO9_BOOT", 5.08), (10, "IO9_BOOT", 2.54), (11, "GND", -2.54), (12, "GND", -5.08), (13, "GND", -7.62), (14, "GND", -10.16)]
        for pnum, lbl, py_off in pins_right:
            py = y + py_off
            px = x + 13.97
            pend = x + 25.40
            sch_lines.append('	(wire')
            sch_lines.append(f'		(pts (xy {px:.2f} {py:.2f}) (xy {pend:.2f} {py:.2f}))')
            sch_lines.append('		(stroke (width 0) (type solid))')
            sch_lines.append(f'		(uuid "{new_uuid()}")')
            sch_lines.append('	)')
            sch_lines.append(f'	(label "{lbl}" (at {pend:.2f} {py:.2f} 0) (effects (font (size 1.27 1.27)) (justify left)))')

    def place_push_button(ref, val, fp, x, y, sig_label, gnd_label="GND"):
        sch_lines.append(f'	(symbol (lib_id "Switch:SW_Push") (at {x:.2f} {y:.2f} 0) (unit 1)')
        sch_lines.append(f'		(uuid "{new_uuid()}")')
        sch_lines.append(f'		(property "Reference" "{ref}" (at {x:.2f} {y-4.0:.2f} 0) (effects (font (size 1.5 1.5))))')
        sch_lines.append(f'		(property "Value" "{val}" (at {x:.2f} {y+4.0:.2f} 0) (effects (font (size 1.27 1.27))))')
        sch_lines.append(f'		(property "Footprint" "{fp}" (at {x:.2f} {y:.2f} 0) (effects (font (size 1.27 1.27)) (hide yes)))')
        sch_lines.append('	)')
        p1_x = x - 5.08
        p1_end = x - 17.78
        sch_lines.append('	(wire')
        sch_lines.append(f'		(pts (xy {p1_x:.2f} {y:.2f}) (xy {p1_end:.2f} {y:.2f}))')
        sch_lines.append('		(stroke (width 0) (type solid))')
        sch_lines.append(f'		(uuid "{new_uuid()}")')
        sch_lines.append('	)')
        sch_lines.append(f'	(label "{sig_label}" (at {p1_end:.2f} {y:.2f} 0) (effects (font (size 1.27 1.27)) (justify right)))')
        p2_x = x + 5.08
        p2_end = x + 17.78
        sch_lines.append('	(wire')
        sch_lines.append(f'		(pts (xy {p2_x:.2f} {y:.2f}) (xy {p2_end:.2f} {y:.2f}))')
        sch_lines.append('		(stroke (width 0) (type solid))')
        sch_lines.append(f'		(uuid "{new_uuid()}")')
        sch_lines.append('	)')
        sch_lines.append(f'	(label "{gnd_label}" (at {p2_end:.2f} {y:.2f} 0) (effects (font (size 1.27 1.27)) (justify left)))')

    # ==================== CIRCUIT BLOCKS ====================
    # BLOCK 1: ESP32-C6-LCD-1.47 Processor
    add_block("1. ESP32-C6-LCD-1.47 MAIN PROCESSOR", "Dual 1x09 Headers (J1 Top / SPI & I2C, J2 Bottom / UART & Signals)", 20.32, 20.32, 142.24, 114.30)
    esp_t = ["IO5_NSS", "IO4_MISO", "IO3_MOSI", "IO2_SCK", "I2C_SCL", "I2C_SDA", "+3V3", "GND", "+5V"]
    place_conn_1row("Connector_Generic:Conn_01x09", "J1", "ESP32_T_LANDSCAPE", "Connector_PinHeader_2.54mm:PinHeader_1x09_P2.54mm_Horizontal", 50.80, 66.04, esp_t)

    esp_b = ["IO9_BOOT", "IO18_UP", "IO19_MODE", "IO20_DN", "IO23_SPARE", "IO12_LORA_DIO0", "IO13_ANCORA_DET", "IO16_GPS_RX", "IO17_GPS_TX"]
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
    gps_labels = ["IO15", "GPS_TX2", "GPS_RX2", "IO17_GPS_TX", "IO16_GPS_RX", "GND", "+3V3"]
    place_conn_1row("Connector_Generic:Conn_01x07", "J6", "GPS_LC29H_7PIN", "Connector_PinHeader_2.54mm:PinHeader_1x07_P2.54mm_Horizontal", 218.44, 137.16, gps_labels)

    # BLOCK 4: Joystick & Battery Port
    add_block("4. JOYSTICK & BATTERY INTERFACE", "Onboard PS5 Thumbstick (JOY1) & External Battery Port (J4)", 274.32, 20.32, 406.40, 114.30)
    place_joystick("JOY1", "PS5_JOYSTICK", "PS5_joystick:Alps_RKJXV1224005", 314.96, 66.04)
    apm_labels = ["BATT_VOLT_APM", "BATT_CURR_APM", "GND"]
    place_conn_1row("Connector_Generic:Conn_01x03", "J4", "BATT_APM", "Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Horizontal", 381.00, 64.77, apm_labels)

    # BLOCK 5: PCA9685 16-Channel 12-Bit PWM Driver (I2C: 0x40)
    add_block("5. PCA9685 16-CH PWM CONTROLLER (I2C: 0x40)", "16 Hardware Servos: Ch0 ESC, Ch1 Leme, Ch2 Guincho, Ch3 Rack, Ch4-15 LEDs", 274.32, 124.46, 406.40, 203.20)
    pca_in_labels = ["GND", "GND", "I2C_SCL", "I2C_SDA", "+3V3", "+5V_SERVO"]
    place_conn_1row("Connector_Generic:Conn_01x06", "J_PCA_IN", "PCA9685_INPUT", "Connector_PinHeader_2.54mm:PinHeader_1x06_P2.54mm_Vertical", 309.88, 160.00, pca_in_labels)
    place_conn_1row_right("Connector_Generic:Conn_01x06", "J_PCA_OUT", "PCA9685_CASCADE", "Connector_PinHeader_2.54mm:PinHeader_1x06_P2.54mm_Vertical", 370.00, 160.00, pca_in_labels)
    sch_lines.append(f'	(text "CH0: ESC Propulsão | CH1: Servo Leme | CH2: Guincho Âncora" (at 280.00 188.00 0) (effects (font (size 1.2 1.2) (bold yes))) (uuid "{new_uuid()}"))')
    sch_lines.append(f'	(text "CH3: Cremalheira Âncora | CH4-15: LEDs, Faróis de Navegação e Auxiliares" (at 280.00 194.00 0) (effects (font (size 1.2 1.2) (italic yes))) (uuid "{new_uuid()}"))')

    # BLOCK 6: Power Supply & Anchor Detector
    add_block("6. POWER & SENSOR DISTRIBUTION", "Isolated +5V Logic, +5V Servo Rail & Anchor End-Stop Sensor", 20.32, 124.46, 142.24, 203.20)
    place_conn_1row("Connector_Generic:Conn_01x02", "J11", "PWR_ESP_5V", "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Horizontal", 53.34, 152.40, ["GND", "+5V"])
    place_conn_1row("Connector_Generic:Conn_01x02", "J12", "PWR_SERVO_5V", "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Horizontal", 53.34, 180.34, ["GND", "+5V_SERVO"])
    place_conn_1row("Connector_Generic:Conn_01x02", "J5", "DETECTOR_ANCORA", "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical", 114.30, 165.10, ["IO13_ANCORA_DET", "GND"])

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

    # BLOCK 8: Push Buttons SW1-SW4
    add_block("8. USER BUTTONS (SW1-SW4)", "Tactile Push Buttons on Back (UI Side) for Menu Navigation & Boot", 274.32, 215.90, 406.40, 280.00)
    place_push_button("SW1", "UP", "PS4_joystick:SKRKAEE020", 309.88, 235.00, "IO18_UP")
    place_push_button("SW2", "MODE", "PS4_joystick:SKRKAEE020", 370.00, 235.00, "IO19_MODE")
    place_push_button("SW3", "DOWN", "PS4_joystick:SKRKAEE020", 309.88, 260.00, "IO20_DN")
    place_push_button("SW4", "BOOT", "PS4_joystick:SKRKAEE020", 370.00, 260.00, "IO9_BOOT")

    # BLOCK 9: ADS1115 16-Bit 4-Channel I2C ADC (0x48)
    add_block("9. ADS1115 16-BIT 4-CH ADC (I2C: 0x48)", "High Precision Telemetry: A0 V_Batt, A1 I_Batt, A2 Water Sensor, A3 Aux", 20.32, 215.90, 264.16, 280.00)
    ads_sch_labels = [
        "+3V3", "GND", "I2C_SCL", "I2C_SDA", "GND",
        "NC", "BATT_VOLT_APM", "BATT_CURR_APM", "WATER_SENS", "AUX_ADC"
    ]
    place_conn_1row("Connector_Generic:Conn_01x10", "J_ADS", "ADS1115_MODULE", "Connector_PinHeader_2.54mm:PinHeader_1x10_P2.54mm_Horizontal", 140.00, 248.00, ads_sch_labels)

    sch_lines.append('	(sheet_instances')
    sch_lines.append('		(path "/" (page "1"))')
    sch_lines.append('	)')
    sch_lines.append('	(embedded_fonts no)')
    sch_lines.append(')')

    with open(path, "w") as f:
        f.write("\n".join(sch_lines) + "\n")
    print(f"Generated KiCad Schematic: {path}")

# ==============================================================================
# 4. MAIN ORCHESTRATION & ZONE REFILL
# ==============================================================================
def main():
    pro_path = os.path.join(TARGET_DIR, f"{PROJECT_NAME}.kicad_pro")
    prl_path = os.path.join(TARGET_DIR, f"{PROJECT_NAME}.kicad_prl")
    pcb_path = os.path.join(TARGET_DIR, f"{PROJECT_NAME}.kicad_pcb")
    sch_path = os.path.join(TARGET_DIR, f"{PROJECT_NAME}.kicad_sch")
    gerber_dir = os.path.join(TARGET_DIR, "gerbers")
    os.makedirs(gerber_dir, exist_ok=True)

    print(f"Generating Zero-DRC V4.2 I2C Carrier Project in {TARGET_DIR}...")
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

        # Refill zones and run DRC check
        try:
            drc_report = os.path.join(TARGET_DIR, "drc_report.json")
            subprocess.run([
                kicad_cli, "pcb", "drc",
                "--refill-zones", "--save-board",
                "--output", drc_report,
                "--format", "json",
                pcb_path
            ], check=True)
            print("Refilled zones and ran DRC check.")
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
            print("Rendered 3D Top view.")

            bottom_png = os.path.join(TARGET_DIR, "Universal_Carrier_3d_bottom.png")
            subprocess.run([
                kicad_cli, "pcb", "render",
                "--side", "bottom", "-w", "1600", "-h", "1200",
                "-o", bottom_png, pcb_path
            ], check=True)
            print("Rendered 3D Bottom view.")
        except Exception as e:
            print("3D Render warning:", e)

        # Export high-res SVG
        try:
            subprocess.run([
                kicad_cli, "pcb", "export", "svg",
                "-o", os.path.join(TARGET_DIR, "Universal_Carrier_V4_2_top.svg"),
                "--layers", "F.SilkS,F.Cu,Edge.Cuts",
                "--page-size-mode", "2",
                pcb_path
            ], check=True)
            print("Exported Top SVG.")
            subprocess.run([
                kicad_cli, "pcb", "export", "svg",
                "-o", os.path.join(TARGET_DIR, "Universal_Carrier_V4_2_bottom.svg"),
                "--layers", "B.SilkS,B.Cu,Edge.Cuts",
                "--page-size-mode", "2",
                pcb_path
            ], check=True)
            print("Exported Bottom SVG.")
        except Exception as e:
            print("SVG Export warning:", e)

if __name__ == "__main__":
    main()
