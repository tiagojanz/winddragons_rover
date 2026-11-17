#!/usr/bin/env python3
"""
WindDragons Dual ESP32-C6 Carrier V2 - KiCad 10 Generator & PCBA Exporter
Dimensions: 100.0 x 100.0 mm (2-Layer JLCPCB Single-Sided Top-SMT PCBA)

New in V2 (preserving 100% V1 firmware/pinout compatibility):
  1. Integrated 2S LiPo (7.4V / 8.4V) USB-C + 5V/6V Solar Boost-Balance Charger (IP2326)
     and 5.0V 2A Synchronous Step-Down Buck Converter (MT2492) on Front (F.Cu).
  2. Extended Breakaway Winch + Anchor Hall Sub-Board (76.0 x 7.2 mm, bottom edge):
     - Keeps exact mechanical fit (M3 at 54.0,96.4 and DRV5032 at 86.5,96.4)
     - Adds SRV_WIN & SRV_RCK headers + single 6-pin harness (GND, 5V, 3V3, HALL, WIN, RCK).
  3. Full Double-Sided Silkscreen (F.SilkS + B.SilkS) with solid GND polarity bars
     and clear separation between 5V RC Servos and ADC/Digital Sensor headers.
"""

import csv
import json
import math
import os
import re
import subprocess
import uuid
import zipfile

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
V1_DIR = "/Users/tiagotorredovale/Documents/projectos/winddragon/rover/hardware/dual_esp_carrier"
V1_PCB = os.path.join(V1_DIR, "Dual_ESP_Carrier.kicad_pcb")
V1_PRO = os.path.join(V1_DIR, "Dual_ESP_Carrier.kicad_pro")
V1_PRL = os.path.join(V1_DIR, "Dual_ESP_Carrier.kicad_prl")
KICAD_FP_DIR = "/Applications/KiCad/KiCad.app/Contents/SharedSupport/footprints"
KICAD_CLI = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"

PROJECT_NAME = "Dual_ESP_Carrier_V2"
PCB_PATH = os.path.join(BASE_DIR, f"{PROJECT_NAME}.kicad_pcb")
PRO_PATH = os.path.join(BASE_DIR, f"{PROJECT_NAME}.kicad_pro")
PRL_PATH = os.path.join(BASE_DIR, f"{PROJECT_NAME}.kicad_prl")
SCH_PATH = os.path.join(BASE_DIR, f"{PROJECT_NAME}.kicad_sch")
GERBER_DIR = os.path.join(BASE_DIR, "gerbers")
PCBA_DIR = os.path.join(BASE_DIR, "jlcpcb_pcba")

# Global registry populated by load_std_footprint: PAD_POS[(ref, pad_num)] = (gx, gy)
PAD_POS = {}
# BOM registry populated by load_std_footprint for parts with an LCSC code
BOM_ITEMS = []


def P(ref, pad_num):
    """Return exact global (x, y) coordinates of a placed footprint pad."""
    return PAD_POS[(ref, str(pad_num))]


def new_uuid():
    return str(uuid.uuid4())


def extract_top_blocks(txt):
    """Split a KiCad s-expression file into header and list of top-level blocks."""
    lines = txt.splitlines()
    blocks = []
    header_lines = []
    i = 0
    in_header = True
    while i < len(lines):
        line = lines[i]
        if in_header and re.match(r"^\t\((footprint|gr_|segment|via|zone)\b", line):
            in_header = False
        if in_header:
            header_lines.append(line)
            i += 1
            continue
        if line == ")":
            break
        if line.startswith("\t("):
            block = [line]
            depth = line.count("(") - line.count(")")
            i += 1
            while i < len(lines) and depth > 0:
                block.append(lines[i])
                depth += lines[i].count("(") - lines[i].count(")")
                i += 1
            blocks.append("\n".join(block))
        else:
            i += 1
    return "\n".join(header_lines), blocks


def get_fp_ref(block):
    m = re.search(r'\(property\s+"Reference"\s+"([^"]*)"', block)
    return m.group(1) if m else ""


def get_seg_net(block):
    m = re.search(r'\(net\s+"([^"]*)"\)', block)
    return m.group(1) if m else ""


def get_seg_coords(block):
    ms = re.search(r'\(start\s+([0-9.-]+)\s+([0-9.-]+)\)', block)
    me = re.search(r'\(end\s+([0-9.-]+)\s+([0-9.-]+)\)', block)
    if not ms or not me:
        return None
    return (float(ms.group(1)), float(ms.group(2)), float(me.group(1)), float(me.group(2)))


def load_std_footprint(
    rel_path,
    ref,
    val,
    x,
    y,
    rot=0,
    pad_nets=None,
    layer="F.Cu",
    ref_at=(0, -2.0, 0),
    ref_size=0.65,
    ref_thick=0.12,
    hide_ref=False,
    hide_val=True,
    lcsc="",
    bom_fp_name=None,
    exclude_bom=False,
    exclude_pos=False,
    pad_shapes=None,
    model_path=None,
    model_offset=(0, 0, 0),
    model_rot=(0, 0, 0),
):
    """Load an official KiCad 10 .kicad_mod and adapt it as an embedded PCB footprint."""
    fp_file = os.path.join(KICAD_FP_DIR, rel_path)
    with open(fp_file, "r") as f:
        raw = f.read()

    pad_nets = pad_nets or {}
    pad_shapes = pad_shapes or {}

    # Strip outer (footprint "..." ... ) wrapper
    lines = raw.splitlines()
    m_head = re.match(r'\(footprint\s+"([^"]+)"', lines[0])
    fp_name = m_head.group(1) if m_head else rel_path
    lib_prefix = rel_path.split(".pretty/")[0]
    full_fp_id = f"{lib_prefix}:{fp_name}"

    inner = []
    skip_props = False
    for line in lines[1:-1]:
        s = line.strip()
        if line.startswith("\t(version ") or line.startswith("\t(generator") or line.startswith('\t(layer "') or line.startswith("\t(descr ") or line.startswith("\t(tags "):
            continue
        if s.startswith('(property "Reference"') or s.startswith('(property "Value"') or s.startswith('(property "KiLib_Generator"') or s.startswith('(property "Datasheet"') or s.startswith('(property "Description"'):
            skip_props = True
            depth = line.count("(") - line.count(")")
            continue
        if skip_props:
            depth += line.count("(") - line.count(")")
            if depth <= 0:
                skip_props = False
            continue
        inner.append(line)

    body = "\n".join(inner)

    # Set attrs cleanly while preserving mount type (smd vs through_hole)
    if exclude_bom or exclude_pos:
        def _upd_attr(m):
            toks = m.group(1).split()
            mtype = toks[0] if toks and toks[0] in ("smd", "through_hole") else ("smd" if " smd " in body else "through_hole")
            flags = [mtype]
            if exclude_pos or "exclude_from_pos_files" in toks:
                flags.append("exclude_from_pos_files")
            if exclude_bom or "exclude_from_bom" in toks:
                flags.append("exclude_from_bom")
            return "\t(attr " + " ".join(flags) + ")"

        if re.search(r'\t\(attr\s+[^\n]+\)', body):
            body = re.sub(r'\t\(attr\s+([^\n]+)\)', _upd_attr, body)
        else:
            mtype = "smd" if " smd " in body else "through_hole"
            flags = [mtype]
            if exclude_pos:
                flags.append("exclude_from_pos_files")
            if exclude_bom:
                flags.append("exclude_from_bom")
            body = f"\t(attr {' '.join(flags)})\n" + body

    if lcsc and not exclude_bom:
        BOM_ITEMS.append((ref, val, bom_fp_name or fp_name, lcsc))

    # Inject pad nets, absolute pad rotation (KiCad stores pad_local_rot + fp_rot),
    # optional pad shapes, and record global pad coordinates
    rad = math.radians(rot)
    cos_r, sin_r = math.cos(rad), math.sin(rad)

    def replace_pad(m):
        pad_txt = m.group(0)
        pnum_m = re.match(r'\(pad\s+"([^"]*)"\s+(thru_hole|smd|np_thru_hole)\s+(\w+)', pad_txt)
        if not pnum_m:
            return pad_txt
        pnum = pnum_m.group(1)
        pat_m = re.search(r'\(at\s+([0-9.-]+)\s+([0-9.-]+)(?:\s+([0-9.-]+))?\)', pad_txt)
        if pat_m:
            px, py = float(pat_m.group(1)), float(pat_m.group(2))
            prot = float(pat_m.group(3)) if pat_m.group(3) else 0.0
            if pnum:
                gx = x + px * cos_r + py * sin_r
                gy = y - px * sin_r + py * cos_r
                PAD_POS[(ref, pnum)] = (gx, gy)
            new_prot = (prot + rot) % 360.0
            if abs(new_prot - prot) > 1e-6 or (rot != 0 and pat_m.group(3) is None):
                ang_s = f" {int(round(new_prot))}" if abs(new_prot - round(new_prot)) < 1e-6 else f" {new_prot:g}"
                if abs(new_prot) < 1e-6:
                    ang_s = ""
                pad_txt = pad_txt[:pat_m.start()] + f"(at {pat_m.group(1)} {pat_m.group(2)}{ang_s})" + pad_txt[pat_m.end():]
        if pnum in pad_shapes:
            pad_txt = pad_txt.replace(f"{pnum_m.group(2)} {pnum_m.group(3)}", f"{pnum_m.group(2)} {pad_shapes[pnum]}", 1)
        net_name = pad_nets.get(pnum, "")
        if net_name and "(net " not in pad_txt:
            pad_txt = re.sub(
                r'\n\t\)$',
                f'\n\t\t(net "{net_name}")\n\t\t(uuid "{new_uuid()}")\n\t)',
                pad_txt,
            )
        elif "(uuid " not in pad_txt:
            pad_txt = re.sub(r'\n\t\)$', f'\n\t\t(uuid "{new_uuid()}")\n\t)', pad_txt)
        return pad_txt

    body = re.sub(r'\(pad\s+"[^"]*"\s+.*?\n\t\)', replace_pad, body, flags=re.S)
    if model_path:
        ox, oy, oz = model_offset
        rx, ry, rz = model_rot
        new_model = (
            f'\t(model "{model_path}"\n'
            f'\t\t(offset\n\t\t\t(xyz {ox} {oy} {oz})\n\t\t)\n'
            f'\t\t(scale\n\t\t\t(xyz 1 1 1)\n\t\t)\n'
            f'\t\t(rotate\n\t\t\t(xyz {rx} {ry} {rz})\n\t\t)\n'
            f'\t)'
        )
        if re.search(r'\t\(model\s+"[^"]+".*?\n\t\)', body, flags=re.S):
            body = re.sub(r'\t\(model\s+"[^"]+".*?\n\t\)', lambda _m: new_model, body, count=1, flags=re.S)
        else:
            body = body + "\n" + new_model
    # Remove any old uuids in properties/text if needed, keep geometry clean
    body = "\n".join("\t" + ln for ln in body.splitlines())

    hide_ref_str = "\n\t\t\t(hide yes)" if hide_ref else ""
    hide_val_str = "\n\t\t\t(hide yes)" if hide_val else ""
    lcsc_prop = ""
    if lcsc:
        lcsc_prop = f"""\t\t(property "LCSC" "{lcsc}"
\t\t\t(at 0 0 0)
\t\t\t(layer "F.Fab"){"" if False else ""}
\t\t\t(hide yes)
\t\t\t(uuid "{new_uuid()}")
\t\t\t(effects (font (size 1 1)))
\t\t)"""

    out = f"""\t(footprint "{full_fp_id}"
\t\t(layer "{layer}")
\t\t(uuid "{new_uuid()}")
\t\t(at {x:.4f} {y:.4f}{f" {rot}" if rot else ""})
\t\t(property "Reference" "{ref}"
\t\t\t(at {ref_at[0]} {ref_at[1]} {ref_at[2]})
\t\t\t(layer "F.SilkS"){hide_ref_str}
\t\t\t(uuid "{new_uuid()}")
\t\t\t(effects
\t\t\t\t(font
\t\t\t\t\t(size {ref_size} {ref_size})
\t\t\t\t\t(thickness {ref_thick})
\t\t\t\t\t(bold yes)
\t\t\t\t)
\t\t\t)
\t\t)
\t\t(property "Value" "{val}"
\t\t\t(at 0 2.0 0)
\t\t\t(layer "F.Fab"){hide_val_str}
\t\t\t(uuid "{new_uuid()}")
\t\t\t(effects
\t\t\t\t(font
\t\t\t\t\t(size 0.7 0.7)
\t\t\t\t\t(thickness 0.12)
\t\t\t\t)
\t\t\t)
\t\t)
{lcsc_prop}
{body}
\t)"""
    return out


def make_seg(x1, y1, x2, y2, width, layer, net):
    return f"""\t(segment
\t\t(start {x1:.4f} {y1:.4f})
\t\t(end {x2:.4f} {y2:.4f})
\t\t(width {width:.3f})
\t\t(layer "{layer}")
\t\t(net "{net}")
\t\t(uuid "{new_uuid()}")
\t)"""


def make_route(pts, width, layer, net):
    segs = []
    for i in range(len(pts) - 1):
        if abs(pts[i][0] - pts[i + 1][0]) < 1e-4 and abs(pts[i][1] - pts[i + 1][1]) < 1e-4:
            continue
        segs.append(make_seg(pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1], width, layer, net))
    return segs


def make_via(x, y, net, size=0.65, drill=0.35):
    return f"""\t(via
\t\t(at {x:.4f} {y:.4f})
\t\t(size {size:.3f})
\t\t(drill {drill:.3f})
\t\t(layers "F.Cu" "B.Cu")
\t\t(net "{net}")
\t\t(uuid "{new_uuid()}")
\t)"""


def make_edge_line(x1, y1, x2, y2, width=0.15):
    return f"""\t(gr_line
\t\t(start {x1:.4f} {y1:.4f})
\t\t(end {x2:.4f} {y2:.4f})
\t\t(stroke
\t\t\t(width {width:.3f})
\t\t\t(type solid)
\t\t)
\t\t(layer "Edge.Cuts")
\t\t(uuid "{new_uuid()}")
\t)"""


def make_silk_text(text, x, y, rot=0, layer="F.SilkS", size=0.70, thick=0.13, bold=False):
    mirror = "\n\t\t\t(justify mirror)" if layer == "B.SilkS" else ""
    bold_str = "\n\t\t\t\t\t(bold yes)" if bold else ""
    return f"""\t(gr_text "{text}"
\t\t(at {x:.4f} {y:.4f} {rot})
\t\t(layer "{layer}")
\t\t(uuid "{new_uuid()}")
\t\t(effects
\t\t\t(font
\t\t\t\t(size {size:.3f} {size:.3f})
\t\t\t\t(thickness {thick:.3f}){bold_str}
\t\t\t){mirror}
\t\t)
\t)"""


def make_silk_rect(x1, y1, x2, y2, layer="F.SilkS", width=0.15, fill=False):
    fill_str = "yes" if fill else "no"
    return f"""\t(gr_rect
\t\t(start {x1:.4f} {y1:.4f})
\t\t(end {x2:.4f} {y2:.4f})
\t\t(stroke
\t\t\t(width {width:.3f})
\t\t\t(type solid)
\t\t)
\t\t(fill {fill_str})
\t\t(layer "{layer}")
\t\t(uuid "{new_uuid()}")
\t)"""


def make_silk_line(x1, y1, x2, y2, layer="F.SilkS", width=0.15):
    return f"""\t(gr_line
\t\t(start {x1:.4f} {y1:.4f})
\t\t(end {x2:.4f} {y2:.4f})
\t\t(stroke
\t\t\t(width {width:.3f})
\t\t\t(type solid)
\t\t)
\t\t(layer "{layer}")
\t\t(uuid "{new_uuid()}")
\t)"""


def make_gnd_bar_both(cx, cy, vertical=False):
    """Draw a crisp solid white GND bar on both F.SilkS and B.SilkS."""
    if vertical:
        dx, dy = 0.25, 0.85
    else:
        dx, dy = 0.85, 0.25
    return [
        make_silk_rect(cx - dx, cy - dy, cx + dx, cy + dy, layer="F.SilkS", width=0.05, fill=True),
        make_silk_rect(cx - dx, cy - dy, cx + dx, cy + dy, layer="B.SilkS", width=0.05, fill=True),
    ]


def make_zone(net_name, layer, pts):
    pts_str = " ".join(f"(xy {x:.4f} {y:.4f})" for x, y in pts)
    return f"""\t(zone
\t\t(net "{net_name}")
\t\t(layer "{layer}")
\t\t(uuid "{new_uuid()}")
\t\t(hatch edge 0.5)
\t\t(connect_pads
\t\t\t(clearance 0.20)
\t\t)
\t\t(min_thickness 0.2)
\t\t(fill yes
\t\t\t(thermal_gap 0.20)
\t\t\t(thermal_bridge_width 0.25)
\t\t\t(island_removal_mode 0)
\t\t)
\t\t(polygon
\t\t\t(pts {pts_str})
\t\t)
\t)"""


def make_keepout_zone(pts, layers=("F.Cu", "B.Cu")):
    """Create a copper/via/track/pad keepout zone (e.g. under ESP32-C6 PCB antenna)."""
    pts_str = " ".join(f"(xy {x:.4f} {y:.4f})" for x, y in pts)
    layers_str = " ".join(f'"{lyr}"' for lyr in layers)
    return f"""\t(zone
\t\t(layers {layers_str})
\t\t(uuid "{new_uuid()}")
\t\t(hatch full 0.5)
\t\t(connect_pads
\t\t\t(clearance 0)
\t\t)
\t\t(min_thickness 0.2)
\t\t(keepout
\t\t\t(tracks not_allowed)
\t\t\t(vias not_allowed)
\t\t\t(pads not_allowed)
\t\t\t(copperpour not_allowed)
\t\t)
\t\t(fill
\t\t\t(thermal_gap 0.5)
\t\t\t(thermal_bridge_width 0.5)
\t\t\t(island_removal_mode 0)
\t\t)
\t\t(polygon
\t\t\t(pts {pts_str})
\t\t)
\t)"""


def make_esp32_c6_wroom1_footprint(
    ref,
    val,
    x,
    y,
    rot=180,
    pad_nets=None,
    lcsc="C5366877",
    ref_at=(0, 14.5, 0),
    ref_size=0.70,
    hide_ref=False,
):
    """Build Espressif ESP32-C6-WROOM-1-N8 footprint (28 edge pads + center EPAD 29)."""
    pad_nets = pad_nets or {}
    rad = math.radians(rot)
    cos_r, sin_r = math.cos(rad), math.sin(rad)

    def to_global(px, py):
        return (x + px * cos_r + py * sin_r, y - px * sin_r + py * cos_r)

    BOM_ITEMS.append((ref, val, "ESP32-C6-WROOM-1", lcsc))

    lines = [
        f'\t(footprint "RF_Module:ESP32-C6-WROOM-1"',
        '\t\t(layer "F.Cu")',
        f'\t\t(uuid "{new_uuid()}")',
        f'\t\t(at {x:.4f} {y:.4f}{f" {rot}" if rot else ""})',
        '\t\t(attr smd)',
        f'\t\t(property "Reference" "{ref}"\n\t\t\t(at {ref_at[0]} {ref_at[1]} {ref_at[2]})\n\t\t\t(layer "F.SilkS"){"\n\t\t\t(hide yes)" if hide_ref else ""}\n\t\t\t(uuid "{new_uuid()}")\n\t\t\t(effects (font (size {ref_size} {ref_size}) (thickness 0.12) (bold yes)))\n\t\t)',
        f'\t\t(property "Value" "{val}"\n\t\t\t(at 0 0 0)\n\t\t\t(layer "F.Fab")\n\t\t\t(hide yes)\n\t\t\t(uuid "{new_uuid()}")\n\t\t\t(effects (font (size 0.7 0.7) (thickness 0.12)))\n\t\t)',
        f'\t\t(property "LCSC" "{lcsc}"\n\t\t\t(at 0 0 0)\n\t\t\t(layer "F.Fab")\n\t\t\t(hide yes)\n\t\t\t(uuid "{new_uuid()}")\n\t\t\t(effects (font (size 1 1)))\n\t\t)',
    ]

    # Body F.Fab outline (18.0 x 25.5 mm) & RF shield line (y = -6.45)
    fab_rects = [(-9.0, -12.75, 9.0, 12.75)]
    for x1, y1, x2, y2 in fab_rects:
        lines.append(
            f'\t\t(fp_rect (start {x1} {y1}) (end {x2} {y2}) (stroke (width 0.12) (type solid)) (fill no) (layer "F.Fab") (uuid "{new_uuid()}"))'
        )
    lines.append(
        f'\t\t(fp_line (start -9.0 -6.45) (end 9.0 -6.45) (stroke (width 0.12) (type solid)) (layer "F.Fab") (uuid "{new_uuid()}"))'
    )
    # Courtyard (-9.75..9.75 x -13.45..13.45)
    lines.append(
        f'\t\t(fp_rect (start -9.75 -13.45) (end 9.75 13.45) (stroke (width 0.05) (type solid)) (fill no) (layer "F.CrtYd") (uuid "{new_uuid()}"))'
    )
    # Silkscreen outline + Pin 1 chamfer marker
    silk_segs = [
        (-9.2, -12.95, 9.2, -12.95),
        (-9.2, -12.95, -9.2, -6.45),
        (9.2, -12.95, 9.2, -6.45),
        (-9.2, 11.95, -9.2, 12.95),
        (-9.2, 12.95, 9.2, 12.95),
        (9.2, 11.95, 9.2, 12.95),
        (-9.7, -5.26, -9.2, -5.76),
    ]
    for x1, y1, x2, y2 in silk_segs:
        lines.append(
            f'\t\t(fp_line (start {x1} {y1}) (end {x2} {y2}) (stroke (width 0.12) (type solid)) (layer "F.SilkS") (uuid "{new_uuid()}"))'
        )

    # 28 edge castellated SMD pads: 1..14 Left (-8.75), 15..28 Right (+8.75)
    for i in range(1, 15):
        px, py = -8.75, -5.26 + (i - 1) * 1.27
        pnum = str(i)
        PAD_POS[(ref, pnum)] = to_global(px, py)
        net = pad_nets.get(pnum, "")
        net_s = f'\n\t\t\t(net "{net}")' if net else ""
        rot_s = f" {rot}" if rot else ""
        lines.append(
            f'\t\t(pad "{pnum}" smd rect (at {px:.4f} {py:.4f}{rot_s}) (size 1.5 0.9) (layers "F.Cu" "F.Mask" "F.Paste"){net_s}\n\t\t\t(uuid "{new_uuid()}")\n\t\t)'
        )
    for i in range(15, 29):
        px, py = 8.75, 11.25 - (i - 15) * 1.27
        pnum = str(i)
        PAD_POS[(ref, pnum)] = to_global(px, py)
        net = pad_nets.get(pnum, "")
        net_s = f'\n\t\t\t(net "{net}")' if net else ""
        rot_s = f" {rot}" if rot else ""
        lines.append(
            f'\t\t(pad "{pnum}" smd rect (at {px:.4f} {py:.4f}{rot_s}) (size 1.5 0.9) (layers "F.Cu" "F.Mask" "F.Paste"){net_s}\n\t\t\t(uuid "{new_uuid()}")\n\t\t)'
        )

    # Pad 29: Center Thermal / GND Exposed Pad (4.4 x 4.4 mm at (0, 2.46)) + segmented F.Paste
    PAD_POS[(ref, "29")] = to_global(0.0, 2.46)
    epad_net = pad_nets.get("29", "/GND")
    rot_s = f" {rot}" if rot else ""
    lines.append(
        f'\t\t(pad "29" smd rect (at 0.0000 2.4600{rot_s}) (size 4.4 4.4) (layers "F.Cu" "F.Mask")\n\t\t\t(zone_connect 2)\n\t\t\t(net "{epad_net}")\n\t\t\t(uuid "{new_uuid()}")\n\t\t)'
    )
    for dx in (-1.1, 1.1):
        for dy in (-1.1, 1.1):
            lines.append(
                f'\t\t(pad "" smd rect (at {dx:.4f} {2.46 + dy:.4f}{rot_s}) (size 1.6 1.6) (layers "F.Paste")\n\t\t\t(uuid "{new_uuid()}")\n\t\t)'
            )

    lines.append(
        '\t\t(model "${KICAD10_3DMODEL_DIR}/RF_Module.3dshapes/ESP32-S3-WROOM-1.step"\n'
        '\t\t\t(offset (xyz 0 0 0))\n'
        '\t\t\t(scale (xyz 1 1 1))\n'
        '\t\t\t(rotate (xyz 0 0 0))\n'
        '\t\t)'
    )
    lines.append("\t)")
    return "\n".join(lines)


def write_joy1_vrml_model():
    """Generate VRML 3D model for Alps RKJXV122400R joystick in 3dmodels/."""
    models_dir = os.path.join(BASE_DIR, "3dmodels")
    os.makedirs(models_dir, exist_ok=True)
    wrl_path = os.path.join(models_dir, "Alps_RKJXV122400R.wrl")

    def box_node(cx, cy, cz, sx, sy, sz, color):
        r, g, b = color
        return (
            f"Transform {{ translation {cx:.3f} {cy:.3f} {cz:.3f} children [\n"
            f"  Shape {{ appearance Appearance {{ material Material {{ diffuseColor {r:.2f} {g:.2f} {b:.2f} specularColor 0.35 0.35 0.35 shininess 0.4 }} }}\n"
            f"    geometry Box {{ size {sx:.3f} {sy:.3f} {sz:.3f} }}\n"
            f"  }}\n"
            f"] }}\n"
        )

    def cyl_node(cx, cy, cz, radius, height, color):
        r, g, b = color
        return (
            f"Transform {{ translation {cx:.3f} {cy:.3f} {cz:.3f} rotation 1 0 0 1.5707963 children [\n"
            f"  Shape {{ appearance Appearance {{ material Material {{ diffuseColor {r:.2f} {g:.2f} {b:.2f} specularColor 0.25 0.25 0.25 shininess 0.3 }} }}\n"
            f"    geometry Cylinder {{ radius {radius:.3f} height {height:.3f} }}\n"
            f"  }}\n"
            f"] }}\n"
        )

    # VRML uses mm coordinates scaled in KiCad via (scale (xyz 0.393701 0.393701 0.393701))
    # Footprint X=right, Y=down -> KiCad 3D model Y is inverted relative to PCB Y (up = +Y)
    wrl = "#VRML V2.0 utf8\n"
    # Main stamped metal cage (16.0 x 16.0 x 12.2 mm)
    wrl += box_node(0.0, 0.0, 6.1, 16.0, 16.0, 12.2, (0.72, 0.74, 0.77))
    # X potentiometer housing (top edge, orange/amber)
    wrl += box_node(0.0, 8.8, 5.2, 9.4, 2.2, 10.4, (0.85, 0.42, 0.12))
    # Y potentiometer housing (left edge, dark green/blue)
    wrl += box_node(-8.8, 0.0, 5.2, 2.2, 9.4, 10.4, (0.12, 0.45, 0.55))
    # Center tactile switch module (bottom edge, black body + white plunger)
    wrl += box_node(0.0, -8.8, 3.0, 8.8, 4.2, 6.0, (0.15, 0.15, 0.16))
    # Stem & thumbstick shaft
    wrl += cyl_node(0.0, 0.0, 14.5, 4.6, 5.0, (0.12, 0.12, 0.13))
    wrl += cyl_node(0.0, 0.0, 18.0, 2.2, 4.0, (0.18, 0.18, 0.20))

    with open(wrl_path, "w") as f:
        f.write(wrl)
    return wrl_path


def generate_project_files():
    os.makedirs(BASE_DIR, exist_ok=True)
    os.makedirs(GERBER_DIR, exist_ok=True)
    os.makedirs(PCBA_DIR, exist_ok=True)

    with open(V1_PRO, "r") as f:
        pro = json.load(f)
    pro["meta"]["filename"] = f"{PROJECT_NAME}.kicad_pro"
    pro["pcbnew"]["last_paths"]["plot"] = "gerbers/"
    pro["pcbnew"]["last_paths"]["step"] = f"{PROJECT_NAME}.step"
    pro["schematic"]["meta"]["filename"] = f"{PROJECT_NAME}.kicad_sch"
    pro["schematic"]["top_level_sheets"] = [
        {"filename": f"{PROJECT_NAME}.kicad_sch", "name": PROJECT_NAME, "uuid": "00000000-0000-0000-0000-000000000000"}
    ]
    # JLCPCB Standard/Economic capabilities: 0.15mm clearance, 0.18mm hole clearance, 0.08mm mask bridge
    pro["board"]["design_settings"]["rules"]["min_clearance"] = 0.15
    pro["net_settings"]["classes"][0]["clearance"] = 0.15
    pro["board"]["design_settings"]["rules"]["min_track_width"] = 0.2
    pro["board"]["design_settings"]["rules"]["min_hole_clearance"] = 0.18
    pro["board"]["design_settings"]["rules"]["min_via_diameter"] = 0.5
    pro["board"]["design_settings"]["rules"]["min_via_annular_width"] = 0.1
    pro["board"]["design_settings"]["rules"]["min_through_hole_diameter"] = 0.3
    pro["board"]["design_settings"]["rules"]["min_resolved_spokes"] = 1
    pro["board"]["design_settings"]["rules"]["solder_mask_to_copper_clearance"] = 0.0
    pro["board"]["design_settings"]["rule_severities"]["solder_mask_bridge"] = "warning"
    with open(PRO_PATH, "w") as f:
        json.dump(pro, f, indent=2)

    with open(V1_PRL, "r") as f:
        prl = json.load(f)
    prl["meta"]["filename"] = f"{PROJECT_NAME}.kicad_prl"
    with open(PRL_PATH, "w") as f:
        json.dump(prl, f, indent=2)

    sch_txt = f"""(kicad_sch
\t(version 20260306)
\t(generator "eeschema")
\t(generator_version "10.0")
\t(uuid "a0000000-0000-0000-0000-000000000002")
\t(paper "A3")
\t(title_block
\t\t(title "WindDragons Dual ESP32-C6 Carrier V2")
\t\t(date "2026-11-17")
\t\t(rev "V2.0 - 2S Solar/USB-C + Winch Hub")
\t\t(company "WindDragons Autonomous Rover")
\t\t(comment 1 "Power: USB-C + 5-6V Solar -> IP2326 2S Balance Charger -> MT2492 5.0V Buck")
\t\t(comment 2 "Breakaway Winch Hub: Single 6-pin cable (GND, 5V, 3V3, HALL, WIN, RCK) to DRV5032 + 2 Servos")
\t)
\t(lib_symbols)
\t(text "POWER ARCHITECTURE V2: USB-C (5V) + SOLAR (5-6V) -> SCHOTTKY OR -> IP2326 2S CHARGER (8.4V BALANCED) -> PTC + P-MOS -> SW_PWR -> MT2492 5.0V BUCK"
\t\t(exclude_from_sim no)
\t\t(at 20 25 0)
\t\t(effects (font (size 2 2) (bold yes)))
\t\t(uuid "{new_uuid()}")
\t)
\t(text "WINCH + HALL BREAKAWAY HUB: J_WINCH_HUB (1:GND 2:5V 3:3V3 4:HALL 5:WIN 6:RCK) <---> J_WINCH_SUB -> SRV_WIN + SRV_RCK + DRV5032"
\t\t(exclude_from_sim no)
\t\t(at 20 55 0)
\t\t(effects (font (size 2 2) (bold yes)))
\t\t(uuid "{new_uuid()}")
\t)
\t(text "INTER-ESP UART: LAFVIN (TX=IO2, RX=IO1) <---> ONBOARD ESP32-C6-WROOM-1 (TX=IO3, RX=IO22) @ 115200 bps"
\t\t(exclude_from_sim no)
\t\t(at 20 85 0)
\t\t(effects (font (size 2 2) (bold yes)))
\t\t(uuid "{new_uuid()}")
\t)
)
"""
    with open(SCH_PATH, "w") as f:
        f.write(sch_txt)


def main():
    generate_project_files()
    joy1_wrl = write_joy1_vrml_model()

    with open(V1_PCB, "r") as f:
        v1_txt = f.read()

    header, v1_blocks = extract_top_blocks(v1_txt)
    # Update title block in header
    header = header.replace(
        '(title "WindDragons Dual ESP32-C6 Carrier")',
        '(title "WindDragons Dual ESP32-C6 Carrier V2")',
    ).replace(
        '(date "2026-10-14")',
        '(date "2026-11-17")',
    ).replace(
        '(rev "V1.0 - Dual-C6 100x100mm")',
        '(rev "V2.0 - 2S Solar/USB-C + Onboard C6 + JOY1 PCBA")',
    ).replace(
        '(comment 2 "Back: DevKitC, SX1278 LoRa, Quectel GPS, 4x Servos | Max: 100 x 100 mm")',
        '(comment 2 "Back: SX1278, GPS, IMU | Front PCBA: C6-WROOM-1, JOY1, 2xADS1115, IP2326, MT2492, AMS1117")',
    ).replace(
        '(pad_to_mask_clearance 0.05)',
        '(pad_to_mask_clearance 0.025)',
    ).replace(
        '(allow_soldermask_bridges_in_footprints no)',
        '(allow_soldermask_bridges_in_footprints yes)',
    )

    PAD_POS.clear()
    BOM_ITEMS.clear()

    # Footprints to remove and recreate cleanly
    remove_refs = {
        "HALL", "J_BATT", "J_WATER", "J_LIMIT", "J_PWR_SRV", "J_PWR_ESP",
        "J_HALL", "C_HALL", "U_HALL", "H_HALL", "STAMP", "J_EXP",
        "J_DK_L", "J_DK_R",
    }
    # Nets whose V1 segments/vias are replaced cleanly (keep /+3V3 <= 64 and /IO9_BOOT <= 48.5)
    reroute_nets = {
        "/+5V", "/+5V_SERVO", "/+3V3_C6",
        "/WATER_SENS", "/BATT_CURR", "/BATT_VOLT", "/ANCHOR_HALL", "/ANCHOR_LIMIT",
        "/HALL_GND", "/HALL_VCC", "/HALL_OUT",
        "/JOY_X", "/JOY_Y", "/GPS_PPS", "/EXP_IO1", "/EXP_IO11",
        "/LORA_RST", "/LORA_MISO", "/LORA_SCK", "/LORA_NSS", "/LORA_MOSI", "/LORA_DIO0",
        "/SERVO_ESC", "/SERVO_RUDDER", "/SERVO_WINCH", "/SERVO_RACK", "/STROBE_DIN",
    }
    # Old Edge.Cuts and bottom/old silkscreen texts to refresh
    remove_text_prefixes = (
        "WINDDRAGONS VIRTUAL REGATTA SYSTEM V1.0",
        "FRONT: DISPLAY & CONTROL PANEL",
        "BACK: DAUGHTERBOARDS, RF & ACTUATORS",
        "RC SERVOS", "SIG", "+5V", "GND", "GND VCC OUT",
        "DRV5032 HALL SENSOR", "WINCH SPOOL", "USB-C",
        "ESP32-C6 DEVKITC-1", "SX1278 LORA", "SX1278 433MHz",
        "QUECTEL LC29H", "GPS 26.2x30.1mm", "PS5 3D JOYSTICK",
        "ST7789 1.47 INCH LCD", "IO1", "IO11", "3V3", "5V",
        "! J5 CLOSED !", "USB-UART",
    )

    kept_blocks = []
    for blk in v1_blocks:
        if blk.startswith("\t(zone"):
            continue
        if blk.startswith("\t(gr_") and '"Edge.Cuts"' in blk:
            continue
        if blk.startswith("\t(gr_rect") and "(start 60 75)" in blk:
            continue
        if blk.startswith("\t(footprint"):
            ref = get_fp_ref(blk)
            if ref in remove_refs:
                continue
            if ref == "J2":
                blk = blk.replace('(net "/GPS_PPS")', '(net "/JOY_SW")', 1)
                PAD_POS[("J2", "5")] = (58.1600, 31.2800)
            if ref == "J6":
                blk = blk.replace('\n\t\t\t(net "/GPS_PPS")', '', 1)
            if ref == "JOY1":
                blk = blk.replace("(at 78.2525 51.715)", "(at 72.7525 51.715)", 1)
                blk = blk.replace('(net "/IO9_BOOT")', '(net "/JOY_SW")')
                PAD_POS[("JOY1", "2")] = (64.0225, 51.7150)
                PAD_POS[("JOY1", "3")] = (64.0225, 54.2150)
                PAD_POS[("JOY1", "5")] = (72.7525, 42.9850)
                PAD_POS[("JOY1", "6")] = (75.2525, 42.9850)
                PAD_POS[("JOY1", "9")] = (76.0025, 57.4650)
                PAD_POS[("JOY1", "10")] = (76.0025, 61.9650)
                joy_model = (
                    f'\t\t(model "{joy1_wrl}"\n'
                    '\t\t\t(offset (xyz 0 0 0))\n'
                    '\t\t\t(scale (xyz 0.393701 0.393701 0.393701))\n'
                    '\t\t\t(rotate (xyz 0 0 0))\n'
                    '\t\t)\n\t)'
                )
                blk = blk[:-2] + joy_model
            if ref in {"SW1", "SW2", "SW3", "SW4"} and "(model " not in blk:
                sw_model = (
                    '\t\t(model "${KICAD10_3DMODEL_DIR}/Button_Switch_SMD.3dshapes/SW_Push_SPST_NO_Alps_SKRK.step"\n'
                    '\t\t\t(offset (xyz 0 0 0))\n'
                    '\t\t\t(scale (xyz 1 1 1))\n'
                    '\t\t\t(rotate (xyz 0 0 0))\n'
                    '\t\t)\n\t)'
                )
                blk = blk[:-2] + sw_model
            if ref == "J3":
                for idx, sx in enumerate((78.38, 80.92, 83.46, 86.00)):
                    PAD_POS[("J3", str(2 * idx + 1))] = (sx, 71.96)
                    PAD_POS[("J3", str(2 * idx + 2))] = (sx, 74.50)
            servo_x = {"ESC": 6.00, "RUD": 8.54, "WIN": 11.08, "RCK": 13.62, "STRB": 16.16}
            if ref in servo_x:
                PAD_POS[(ref, "3")] = (servo_x[ref], 69.42)
            # Hide overlapping V1 labels cleanly on multi-line KiCad 10 property blocks
            hide_both = {"ESC", "RUD", "WIN", "RCK", "STRB", "H1", "H2", "H3", "H4"}
            hide_val_only = {"J1", "J2", "J3", "J6", "JOY1", "SW1", "SW2", "SW3", "SW4"}
            target_props = {"Reference", "Value"} if ref in hide_both else ({"Value"} if ref in hide_val_only else set())
            if target_props:
                for prop_name in target_props:
                    pat = rf'(\(property "{prop_name}"[^\n]*\n(?:\t+\([^\n]+\n)*?\t+\(layer [^\n]+\))'
                    blk = re.sub(
                        pat,
                        lambda m: m.group(1) if "(hide yes)" in m.group(1) else m.group(1) + "\n\t\t\t(hide yes)",
                        blk,
                        count=1,
                    )
            kept_blocks.append(blk)
            continue
        if blk.startswith("\t(via"):
            vnet = get_seg_net(blk)
            if vnet in reroute_nets:
                continue
            if vnet == "/+3V3":
                vm = re.search(r'\(at\s+([0-9.-]+)\s+([0-9.-]+)\)', blk)
                if vm and float(vm.group(1)) > 64.0:
                    continue
            if vnet == "/IO9_BOOT":
                vm = re.search(r'\(at\s+([0-9.-]+)\s+([0-9.-]+)\)', blk)
                if vm and float(vm.group(1)) > 48.5:
                    continue
            kept_blocks.append(blk)
            continue
        if blk.startswith("\t(segment"):
            snet = get_seg_net(blk)
            if snet in reroute_nets:
                continue
            if snet == "/+3V3":
                sc = get_seg_coords(blk)
                if sc and max(sc[0], sc[2]) > 64.0:
                    continue
            if snet == "/IO9_BOOT":
                sc = get_seg_coords(blk)
                if sc and max(sc[0], sc[2]) > 48.5:
                    continue
            if snet == "/INTER_TX_M2S":
                sc = get_seg_coords(blk)
                if sc and min(sc[0], sc[2]) < 38.9:
                    continue
            if snet == "/INTER_RX_S2M":
                sc = get_seg_coords(blk)
                if sc and max(sc[1], sc[3]) > 65.30:
                    continue
            kept_blocks.append(blk)
            continue
        if blk.startswith("\t(gr_text"):
            tm = re.search(r'\(gr_text\s+"([^"]*)"', blk)
            tval = tm.group(1) if tm else ""
            if any(tval.startswith(p) for p in remove_text_prefixes):
                continue
            kept_blocks.append(blk)
            continue
        kept_blocks.append(blk)

    new_blocks = []

    # ==========================================================================
    # A. EDGE.CUTS (100x100mm + Extended 76.0 x 7.2mm Breakaway Winch+Hall Stick)
    # ==========================================================================
    # Main outline:
    # Top (0,0)-(100,0), Right (100,0)-(100,100), Left (0,100)-(0,0)
    # Bottom Left (0,100)-(12.5,100)->(12.5,91.6)
    # Bottom Right (100,100)-(91.5,100)->(91.5,91.6)
    # Milled slot along Y=91.6..92.8 with 3x 4.0mm tabs:
    #   Tab 1: X=22.0..26.0, Tab 2: X=46.0..50.0, Tab 3: X=76.0..80.0
    edge_lines = [
        (0.0, 0.0, 100.0, 0.0),
        (100.0, 0.0, 100.0, 100.0),
        (100.0, 100.0, 91.5, 100.0),
        (91.5, 100.0, 91.5, 91.6),
        (91.5, 91.6, 80.0, 91.6),
        (80.0, 91.6, 80.0, 92.8),
        (80.0, 92.8, 90.0, 92.8),
        (90.0, 92.8, 90.0, 100.0),
        (90.0, 100.0, 14.0, 100.0),
        (14.0, 100.0, 14.0, 92.8),
        (14.0, 92.8, 22.0, 92.8),
        (22.0, 92.8, 22.0, 91.6),
        (22.0, 91.6, 12.5, 91.6),
        (12.5, 91.6, 12.5, 100.0),
        (12.5, 100.0, 0.0, 100.0),
        (0.0, 100.0, 0.0, 0.0),
        # Slot between Tab 1 and Tab 2 (X=26.0..46.0)
        (26.0, 91.6, 46.0, 91.6),
        (46.0, 91.6, 46.0, 92.8),
        (46.0, 92.8, 26.0, 92.8),
        (26.0, 92.8, 26.0, 91.6),
        # Slot between Tab 2 and Tab 3 (X=50.0..76.0)
        (50.0, 91.6, 76.0, 91.6),
        (76.0, 91.6, 76.0, 92.8),
        (76.0, 92.8, 50.0, 92.8),
        (50.0, 92.8, 50.0, 91.6),
    ]
    for x1, y1, x2, y2 in edge_lines:
        new_blocks.append(make_edge_line(x1, y1, x2, y2))

    # Stamp holes (5 per bridge along Y=92.2)
    stamp_xs = [
        22.4, 23.2, 24.0, 24.8, 25.6,
        46.4, 47.2, 48.0, 48.8, 49.6,
        76.4, 77.2, 78.0, 78.8, 79.6,
    ]
    for sx in stamp_xs:
        new_blocks.append(f"""\t(footprint "MountingHole:MountingHole_0.5mm"
\t\t(layer "F.Cu")
\t\t(uuid "{new_uuid()}")
\t\t(at {sx:.2f} 92.20)
\t\t(property "Reference" "STAMP" (at 0 0 0) (layer "F.SilkS") (hide yes) (uuid "{new_uuid()}") (effects (font (size 0.5 0.5))))
\t\t(property "Value" "" (at 0 0 0) (layer "F.Fab") (hide yes) (uuid "{new_uuid()}") (effects (font (size 0.5 0.5))))
\t\t(attr exclude_from_pos_files exclude_from_bom)
\t\t(pad "" np_thru_hole circle (at 0 0) (size 0.5 0.5) (drill 0.5) (layers "*.Cu" "*.Mask") (uuid "{new_uuid()}"))
\t)""")

    # ==========================================================================
    # B. REORGANIZED LEFT SENSOR HEADERS, SERVO POWER & WINCH HUB (6P)
    # ==========================================================================
    new_blocks.append(load_std_footprint(
        "Connector_PinHeader_2.54mm.pretty/PinHeader_1x02_P2.54mm_Vertical.kicad_mod",
        "J_WATER", "WATER/ANC", 21.50, 74.50, rot=180,
        pad_nets={"1": "/GND", "2": "/WATER_SENS"},
        ref_at=(0, 5.2, 90), ref_size=0.65, hide_ref=True, exclude_bom=True, exclude_pos=True,
    ))
    new_blocks.append(load_std_footprint(
        "Connector_PinHeader_2.54mm.pretty/PinHeader_1x02_P2.54mm_Vertical.kicad_mod",
        "J_LIMIT", "ANCHOR LIMIT", 26.80, 74.50, rot=180,
        pad_nets={"1": "/GND", "2": "/ANCHOR_LIMIT"},
        ref_at=(0, 5.2, 90), ref_size=0.65, hide_ref=True, exclude_bom=True, exclude_pos=True,
    ))

    # J_PWR_SRV at (7.50, 84.50, rot=270): Pad 1 (rect) at (7.50, 84.50)=GND, Pad 2 at (4.96, 84.50)=+5V_SERVO
    new_blocks.append(load_std_footprint(
        "Connector_PinHeader_2.54mm.pretty/PinHeader_1x02_P2.54mm_Vertical.kicad_mod",
        "J_PWR_SRV", "5V_6V_BEC", 7.50, 84.50, rot=270,
        pad_nets={"1": "/GND", "2": "/+5V_SERVO"},
        ref_at=(0, -3.5, 0), ref_size=0.65, hide_ref=True, exclude_bom=True, exclude_pos=True,
    ))

    # J_WINCH_HUB (1x06 @ 14.50, 84.50, rot=90):
    # 1:GND, 2:+5V_SERVO, 3:+3V3_C6, 4:ANCHOR_HALL, 5:SERVO_WINCH, 6:SERVO_RACK
    new_blocks.append(load_std_footprint(
        "Connector_PinHeader_2.54mm.pretty/PinHeader_1x06_P2.54mm_Vertical.kicad_mod",
        "J_WINCH_HUB", "WINCH_HUB_6P", 14.50, 84.50, rot=90,
        pad_nets={
            "1": "/GND",
            "2": "/+5V_SERVO",
            "3": "/+3V3_C6",
            "4": "/ANCHOR_HALL",
            "5": "/SERVO_WINCH",
            "6": "/SERVO_RACK",
        },
        ref_at=(6.35, -3.5, 90), ref_size=0.70, hide_ref=True, exclude_bom=True, exclude_pos=True,
    ))

    # ==========================================================================
    # C. EXTENDED BREAKAWAY WINCH + ANCHOR HALL SUB-BOARD (X=14..90, Y=92.8..100)
    # ==========================================================================
    new_blocks.append(load_std_footprint(
        "Connector_PinHeader_2.54mm.pretty/PinHeader_1x06_P2.54mm_Vertical.kicad_mod",
        "J_WINCH_SUB", "WINCH_6P_IN", 16.00, 96.40, rot=90,
        pad_nets={
            "1": "/HALL_GND",
            "2": "/SUB_5V",
            "3": "/HALL_VCC",
            "4": "/HALL_OUT",
            "5": "/SUB_WIN",
            "6": "/SUB_RCK",
        },
        ref_at=(6.35, -2.2, 90), ref_size=0.60, hide_ref=True, exclude_bom=True, exclude_pos=True,
    ))
    new_blocks.append(load_std_footprint(
        "Capacitor_SMD.pretty/C_0805_2012Metric.kicad_mod",
        "C_SRV", "22uF", 31.70, 96.40, rot=90,
        pad_nets={"1": "/SUB_5V", "2": "/HALL_GND"},
        ref_at=(0, -2.0, 90), ref_size=0.55, hide_ref=True, lcsc="C45783",
    ))
    new_blocks.append(load_std_footprint(
        "Connector_PinHeader_2.54mm.pretty/PinHeader_1x03_P2.54mm_Vertical.kicad_mod",
        "SRV_WIN", "WINCH_SRV", 34.70, 96.40, rot=90,
        pad_nets={"1": "/HALL_GND", "2": "/SUB_5V", "3": "/SUB_WIN"},
        ref_at=(2.54, -2.2, 90), ref_size=0.60, hide_ref=True, exclude_bom=True, exclude_pos=True,
    ))
    new_blocks.append(load_std_footprint(
        "Connector_PinHeader_2.54mm.pretty/PinHeader_1x03_P2.54mm_Vertical.kicad_mod",
        "SRV_RCK", "RACK_SRV", 43.60, 96.40, rot=90,
        pad_nets={"1": "/HALL_GND", "2": "/SUB_5V", "3": "/SUB_RCK"},
        ref_at=(2.54, -2.2, 90), ref_size=0.60, hide_ref=True, exclude_bom=True, exclude_pos=True,
    ))
    # H_HALL M3 mounting hole @ exact (54.0, 96.4)
    new_blocks.append(f"""\t(footprint "MountingHole:MountingHole_3.2mm_M3"
\t\t(layer "F.Cu")
\t\t(uuid "{new_uuid()}")
\t\t(at 54.00 96.40)
\t\t(property "Reference" "H_HALL" (at 0 -2.4 0) (layer "F.SilkS") (hide yes) (uuid "{new_uuid()}") (effects (font (size 0.65 0.65) (thickness 0.12))))
\t\t(property "Value" "M3" (at 0 2.4 0) (layer "F.Fab") (hide yes) (uuid "{new_uuid()}") (effects (font (size 0.65 0.65))))
\t\t(attr exclude_from_pos_files exclude_from_bom)
\t\t(pad "" np_thru_hole circle (at 0 0) (size 3.2 3.2) (drill 3.2) (layers "*.Cu" "*.Mask") (uuid "{new_uuid()}"))
\t)""")
    # C_HALL rot=90: Pad 1 (BOT, 97.35)=/HALL_GND, Pad 2 (TOP, 95.45)=/HALL_VCC
    new_blocks.append(load_std_footprint(
        "Capacitor_SMD.pretty/C_0805_2012Metric.kicad_mod",
        "C_HALL", "100nF", 75.00, 96.40, rot=90,
        pad_nets={"1": "/HALL_GND", "2": "/HALL_VCC"},
        ref_at=(0, -2.0, 90), ref_size=0.55, hide_ref=True, lcsc="C49678",
    ))
    # DRV5032FADBZR SOT-23 (TI DBZ package): Pin 1=VCC, Pin 2=OUT, Pin 3=GND
    new_blocks.append(load_std_footprint(
        "Package_TO_SOT_SMD.pretty/SOT-23.kicad_mod",
        "U_HALL", "DRV5032FADBZR", 86.50, 96.40, rot=0,
        pad_nets={"1": "/HALL_VCC", "2": "/HALL_OUT", "3": "/HALL_GND"},
        ref_at=(0, -2.2, 0), ref_size=0.60, hide_ref=True, lcsc="C140921",
    ))

    # ==========================================================================
    # D. INTEGRATED 2S LIPO CHARGER (USB-C + SOLAR) & 5.0V BUCK (F.Cu Top-SMT)
    # ==========================================================================
    # 1. Solar 5V-6V Input
    new_blocks.append(load_std_footprint(
        "Connector_JST.pretty/JST_XH_B2B-XH-A_1x02_P2.50mm_Vertical.kicad_mod",
        "J_SOLAR", "SOLAR_5_6V", 92.50, 11.00, rot=0,
        pad_nets={"1": "/GND", "2": "/VSOLAR_IN"},
        ref_at=(1.25, -3.5, 0), ref_size=0.65, hide_ref=True, exclude_bom=True, exclude_pos=True,
    ))
    # D_SOL rot=90: Pad 2 Anode TOP=/VSOLAR_IN, Pad 1 Cathode BOT=/VIN_CHG
    new_blocks.append(load_std_footprint(
        "Diode_SMD.pretty/D_SOD-123.kicad_mod",
        "D_SOL", "B5819W", 95.60, 17.80, rot=90,
        pad_nets={"1": "/VIN_CHG", "2": "/VSOLAR_IN"},
        ref_at=(0, -2.0, 90), ref_size=0.55, hide_ref=True, lcsc="C8598",
    ))

    # 2. USB-C Receptacle (flush with X=100.00)
    new_blocks.append(load_std_footprint(
        "Connector_USB.pretty/USB_C_Receptacle_HRO_TYPE-C-31-M-12.kicad_mod",
        "J_USBC", "TYPE-C-31-M-12", 96.35, 36.50, rot=90,
        pad_nets={
            "A1": "/GND", "B12": "/GND", "B1": "/GND", "A12": "/GND", "SH": "/GND",
            "A4": "/VBUS_USB", "B9": "/VBUS_USB", "B4": "/VBUS_USB", "A9": "/VBUS_USB",
            "A5": "/USB_CC1", "B5": "/USB_CC2",
            "A6": "/USB_DP", "B6": "/USB_DP",
            "A7": "/USB_DN", "B7": "/USB_DN",
        },
        ref_at=(-1.5, -5.5, 90), ref_size=0.60, hide_ref=True, lcsc="C165948",
        model_path="${KICAD10_3DMODEL_DIR}/Connector_USB.3dshapes/USB_C_Receptacle_GCT_USB4105-xx-A_16P_TopMnt_Horizontal.step",
    ))
    # R_CC1 rot=90 (Pad 2 TOP=/USB_CC1, Pad 1 BOT=/GND), R_CC2 rot=270 (Pad 1 TOP=/GND, Pad 2 BOT=/USB_CC2)
    new_blocks.append(load_std_footprint(
        "Resistor_SMD.pretty/R_0603_1608Metric.kicad_mod",
        "R_CC1", "5.1k", 89.70, 39.20, rot=90,
        pad_nets={"1": "/GND", "2": "/USB_CC1"},
        ref_at=(0, -1.3, 0), ref_size=0.50, hide_ref=True, lcsc="C23186",
    ))
    new_blocks.append(load_std_footprint(
        "Resistor_SMD.pretty/R_0603_1608Metric.kicad_mod",
        "R_CC2", "5.1k", 89.60, 33.40, rot=270,
        pad_nets={"1": "/GND", "2": "/USB_CC2"},
        ref_at=(0, -1.3, 0), ref_size=0.50, hide_ref=True, lcsc="C23186",
    ))
    # D_USB rot=0: Pad 1 LEFT=/VIN_CHG, Pad 2 RIGHT=/VBUS_USB
    new_blocks.append(load_std_footprint(
        "Diode_SMD.pretty/D_SOD-123.kicad_mod",
        "D_USB", "B5819W", 93.50, 29.80, rot=0,
        pad_nets={"1": "/VIN_CHG", "2": "/VBUS_USB"},
        ref_at=(0, -2.0, 0), ref_size=0.55, hide_ref=True, lcsc="C8598",
    ))

    # 3. IP2326 2S Balance Charger
    new_blocks.append(load_std_footprint(
        "Package_DFN_QFN.pretty/QFN-24-1EP_4x4mm_P0.5mm_EP2.6x2.6mm.kicad_mod",
        "U_CHG", "IP2326", 85.20, 37.50, rot=90,
        pad_nets={
            "4": "/CHG_NTC",
            "6": "/CHG_LED",
            "8": "/CHG_UV",
            "11": "/CHG_ISET", "12": "/CHG_EN",
            "13": "/CHG_VIN", "14": "/CHG_BST",
            "15": "/CHG_LX", "16": "/CHG_LX", "17": "/CHG_LX",
            "18": "/GND",
            "19": "/CHG_VSYS", "20": "/CHG_VSYS",
            "21": "/VBAT_2S", "22": "/VBAT_2S",
            "23": "/CHG_VBATM", "24": "/GND", "25": "/GND",
        },
        ref_at=(0, -3.2, 0), ref_size=0.60, hide_ref=True, lcsc="C2832094",
    ))

    # L1 rot=180: Pad 1 RIGHT (97.35)=/VIN_CHG, Pad 2 LEFT (93.65)=/CHG_LX
    new_blocks.append(load_std_footprint(
        "Inductor_SMD.pretty/L_Sunlord_MWSA0402S.kicad_mod",
        "L1", "2.2uH", 95.50, 22.80, rot=180,
        pad_nets={"1": "/VIN_CHG", "2": "/CHG_LX"},
        ref_at=(0, -3.0, 0), ref_size=0.60, hide_ref=True, lcsc="C408335",
    ))
    new_blocks.append(load_std_footprint(
        "Capacitor_SMD.pretty/C_0805_2012Metric.kicad_mod",
        "C_CHGIN", "22uF", 90.10, 24.40, rot=270,
        pad_nets={"1": "/VIN_CHG", "2": "/GND"},
        ref_at=(0, -1.6, 0), ref_size=0.50, hide_ref=True, lcsc="C45783",
    ))
    new_blocks.append(load_std_footprint(
        "Capacitor_SMD.pretty/C_0805_2012Metric.kicad_mod",
        "C_BST1", "100nF", 93.50, 26.80, rot=180,
        pad_nets={"1": "/CHG_LX", "2": "/CHG_BST"},
        ref_at=(0, -1.6, 0), ref_size=0.50, hide_ref=True, lcsc="C49678",
    ))
    new_blocks.append(load_std_footprint(
        "Resistor_SMD.pretty/R_0805_2012Metric.kicad_mod",
        "R_VIN", "2R", 90.50, 16.90, rot=270,
        pad_nets={"1": "/VIN_CHG", "2": "/CHG_VIN"},
        ref_at=(0, -1.5, 0), ref_size=0.50, hide_ref=True, lcsc="C17606",
    ))
    new_blocks.append(load_std_footprint(
        "Capacitor_SMD.pretty/C_0805_2012Metric.kicad_mod",
        "C_VIN", "100nF", 90.50, 20.60, rot=270,
        pad_nets={"1": "/CHG_VIN", "2": "/GND"},
        ref_at=(0, -1.5, 0), ref_size=0.50, hide_ref=True, lcsc="C49678",
    ))
    # R_CEN rot=270: Pad 1 TOP=/VIN_CHG, Pad 2 BOT=/CHG_EN
    new_blocks.append(load_std_footprint(
        "Resistor_SMD.pretty/R_0805_2012Metric.kicad_mod",
        "R_CEN", "10k", 97.50, 27.20, rot=270,
        pad_nets={"1": "/VIN_CHG", "2": "/CHG_EN"},
        ref_at=(0, -1.5, 0), ref_size=0.50, hide_ref=True, lcsc="C17414",
    ))
    new_blocks.append(load_std_footprint(
        "Resistor_SMD.pretty/R_0805_2012Metric.kicad_mod",
        "R_ISET", "100k", 89.50, 28.90, rot=90,
        pad_nets={"1": "/CHG_ISET", "2": "/GND"},
        ref_at=(0, -1.5, 0), ref_size=0.50, hide_ref=True, lcsc="C17407",
    ))
    # R_UV rot=180: Pad 1 RIGHT=/CHG_UV, Pad 2 LEFT=/GND
    new_blocks.append(load_std_footprint(
        "Resistor_SMD.pretty/R_0805_2012Metric.kicad_mod",
        "R_UV", "120k", 87.70, 42.10, rot=180,
        pad_nets={"1": "/CHG_UV", "2": "/GND"},
        ref_at=(0, -1.5, 0), ref_size=0.50, hide_ref=True, lcsc="C17436",
    ))
    new_blocks.append(load_std_footprint(
        "Resistor_SMD.pretty/R_0805_2012Metric.kicad_mod",
        "R_NTC", "51k", 84.10, 42.70, rot=180,
        pad_nets={"1": "/CHG_NTC", "2": "/GND"},
        ref_at=(0, -1.5, 0), ref_size=0.50, hide_ref=True, lcsc="C17737",
    ))
    new_blocks.append(load_std_footprint(
        "Resistor_SMD.pretty/R_0805_2012Metric.kicad_mod",
        "R_LED1", "1k", 83.20, 25.40, rot=180,
        pad_nets={"1": "/CHG_LED", "2": "/CHG_LED_K"},
        ref_at=(0, -1.5, 0), ref_size=0.50, hide_ref=True, lcsc="C17513",
    ))
    new_blocks.append(load_std_footprint(
        "LED_SMD.pretty/LED_0805_2012Metric.kicad_mod",
        "D_CHG", "RED_CHG", 79.00, 25.40, rot=0,
        pad_nets={"1": "/GND", "2": "/CHG_LED_K"},
        ref_at=(0, -1.5, 0), ref_size=0.50, hide_ref=True, lcsc="C2295",
    ))
    new_blocks.append(load_std_footprint(
        "Capacitor_SMD.pretty/C_0805_2012Metric.kicad_mod",
        "C_SYS1", "22uF", 81.40, 32.80, rot=270,
        pad_nets={"1": "/CHG_VSYS", "2": "/GND"},
        ref_at=(0, -1.6, 0), ref_size=0.50, hide_ref=True, lcsc="C45783",
    ))
    new_blocks.append(load_std_footprint(
        "Capacitor_SMD.pretty/C_0805_2012Metric.kicad_mod",
        "C_SYS2", "22uF", 79.20, 32.80, rot=270,
        pad_nets={"1": "/CHG_VSYS", "2": "/GND"},
        ref_at=(0, -1.6, 0), ref_size=0.50, hide_ref=True, lcsc="C45783",
    ))
    new_blocks.append(load_std_footprint(
        "Capacitor_SMD.pretty/C_0805_2012Metric.kicad_mod",
        "C_BAT1", "22uF", 77.00, 32.80, rot=270,
        pad_nets={"1": "/VBAT_2S", "2": "/GND"},
        ref_at=(0, -1.6, 0), ref_size=0.50, hide_ref=True, lcsc="C45783",
    ))
    # R_BAL placed cleanly right of J_LIPO_2S (rot=270: Pad 1 TOP=/CHG_VBATM, Pad 2 BOT=/VBAT_MID right beside Pad 2 of J_LIPO_2S!)
    new_blocks.append(load_std_footprint(
        "Resistor_SMD.pretty/R_0805_2012Metric.kicad_mod",
        "R_BAL", "10R", 97.60, 46.50, rot=270,
        pad_nets={"1": "/CHG_VBATM", "2": "/VBAT_MID"},
        ref_at=(0, -1.5, 0), ref_size=0.50, hide_ref=True, lcsc="C17415",
    ))

    # 4. 2S LiPo Balance Port, PTC Fuse, Reverse-Polarity MOSFET & Power Switch
    new_blocks.append(load_std_footprint(
        "Connector_JST.pretty/JST_XH_B3B-XH-A_1x03_P2.50mm_Vertical.kicad_mod",
        "J_LIPO_2S", "2S_LIPO_BAL", 93.40, 45.40, rot=270,
        pad_nets={"1": "/GND", "2": "/VBAT_MID", "3": "/VBAT_2S"},
        ref_at=(2.5, -4.0, 90), ref_size=0.65, hide_ref=True, exclude_bom=True, exclude_pos=True,
    ))
    new_blocks.append(load_std_footprint(
        "Fuse.pretty/Fuse_1206_3216Metric.kicad_mod",
        "F1", "1.5A_PTC", 90.80, 55.20, rot=0,
        pad_nets={"1": "/VBAT_2S", "2": "/VBAT_FUSED"},
        ref_at=(0, -1.8, 0), ref_size=0.55, hide_ref=True, lcsc="C2845946",
    ))
    new_blocks.append(load_std_footprint(
        "Package_TO_SOT_SMD.pretty/SOT-23.kicad_mod",
        "Q_PROT", "AO3401A", 91.20, 58.60, rot=90,
        pad_nets={"1": "/Q_GATE", "2": "/VBAT_PROT", "3": "/VBAT_FUSED"},
        ref_at=(0, -2.2, 0), ref_size=0.55, hide_ref=True, lcsc="C15127",
    ))
    new_blocks.append(load_std_footprint(
        "Resistor_SMD.pretty/R_0805_2012Metric.kicad_mod",
        "R_QG", "100k", 90.00, 62.60, rot=90,
        pad_nets={"1": "/GND", "2": "/Q_GATE"},
        ref_at=(0, -1.5, 0), ref_size=0.50, hide_ref=True, lcsc="C17407",
    ))
    new_blocks.append(load_std_footprint(
        "Button_Switch_SMD.pretty/SW_SPDT_Shouhan_MSK12C02.kicad_mod",
        "SW_PWR", "PWR_ON_OFF", 97.70, 58.00, rot=90,
        pad_nets={"2": "/BUCK_EN_SW", "3": "/VBAT_PROT", "SH": "/GND"},
        ref_at=(-3.2, 0, 90), ref_size=0.60, hide_ref=True, lcsc="C431540",
        model_path="${KICAD10_3DMODEL_DIR}/Button_Switch_SMD.3dshapes/SW_SPDT_PCM12.step",
    ))
    new_blocks.append(load_std_footprint(
        "Connector_PinHeader_2.54mm.pretty/PinHeader_1x02_P2.54mm_Vertical.kicad_mod",
        "J_SW_EXT", "EXT_SW", 93.80, 64.60, rot=90,
        pad_nets={"1": "/VBAT_PROT", "2": "/BUCK_EN_SW"},
        ref_at=(1.27, -2.2, 90), ref_size=0.55, hide_ref=True, exclude_bom=True, exclude_pos=True,
    ))
    new_blocks.append(load_std_footprint(
        "Resistor_SMD.pretty/R_0805_2012Metric.kicad_mod",
        "R_SW_PU", "10k", 92.40, 67.80, rot=0,
        pad_nets={"1": "/BUCK_EN_SW", "2": "/BUCK_EN"},
        ref_at=(0, -1.5, 0), ref_size=0.50, hide_ref=True, lcsc="C17414",
    ))
    new_blocks.append(load_std_footprint(
        "Resistor_SMD.pretty/R_0805_2012Metric.kicad_mod",
        "R_SW_PD", "100k", 97.00, 67.80, rot=180,
        pad_nets={"1": "/GND", "2": "/BUCK_EN"},
        ref_at=(0, -1.5, 0), ref_size=0.50, hide_ref=True, lcsc="C17407",
    ))

    # 5. MT2492 5.0V 2A Synchronous Buck
    new_blocks.append(load_std_footprint(
        "Package_TO_SOT_SMD.pretty/SOT-23-6.kicad_mod",
        "U_BUCK", "MT2492", 93.00, 71.80, rot=180,
        pad_nets={
            "1": "/BUCK_BST", "2": "/GND", "3": "/BUCK_FB",
            "4": "/BUCK_EN", "5": "/VBAT_PROT", "6": "/BUCK_LX",
        },
        ref_at=(0, -2.4, 0), ref_size=0.60, hide_ref=True, lcsc="C89358",
    ))
    new_blocks.append(load_std_footprint(
        "Capacitor_SMD.pretty/C_0805_2012Metric.kicad_mod",
        "C_BIN", "22uF", 89.20, 71.80, rot=270,
        pad_nets={"1": "/VBAT_PROT", "2": "/GND"},
        ref_at=(0, -1.6, 0), ref_size=0.50, hide_ref=True, lcsc="C45783",
    ))
    new_blocks.append(load_std_footprint(
        "Capacitor_SMD.pretty/C_0805_2012Metric.kicad_mod",
        "C_BST2", "100nF", 93.00, 75.00, rot=180,
        pad_nets={"1": "/BUCK_BST", "2": "/BUCK_LX"},
        ref_at=(0, -1.5, 0), ref_size=0.50, hide_ref=True, lcsc="C49678",
    ))
    new_blocks.append(load_std_footprint(
        "Inductor_SMD.pretty/L_Sunlord_MWSA0402S.kicad_mod",
        "L2", "4.7uH", 92.00, 79.20, rot=0,
        pad_nets={"1": "/BUCK_LX", "2": "/+5V"},
        ref_at=(0, -3.0, 0), ref_size=0.60, hide_ref=True, lcsc="C408337",
    ))
    new_blocks.append(load_std_footprint(
        "Resistor_SMD.pretty/R_0805_2012Metric.kicad_mod",
        "R_FB1", "110k", 97.50, 71.20, rot=270,
        pad_nets={"1": "/+5V", "2": "/BUCK_FB"},
        ref_at=(0, -1.5, 0), ref_size=0.50, hide_ref=True, lcsc="C17422",
    ))
    new_blocks.append(load_std_footprint(
        "Resistor_SMD.pretty/R_0805_2012Metric.kicad_mod",
        "R_FB2", "15k", 97.50, 75.00, rot=270,
        pad_nets={"1": "/BUCK_FB", "2": "/GND"},
        ref_at=(0, -1.5, 0), ref_size=0.50, hide_ref=True, lcsc="C17475",
    ))
    new_blocks.append(load_std_footprint(
        "Capacitor_SMD.pretty/C_0805_2012Metric.kicad_mod",
        "C_BOUT1", "22uF", 97.50, 78.80, rot=270,
        pad_nets={"1": "/+5V", "2": "/GND"},
        ref_at=(0, -1.5, 0), ref_size=0.50, hide_ref=True, lcsc="C45783",
    ))
    new_blocks.append(load_std_footprint(
        "Capacitor_SMD.pretty/C_0805_2012Metric.kicad_mod",
        "C_BOUT2", "22uF", 97.50, 82.60, rot=270,
        pad_nets={"1": "/+5V", "2": "/GND"},
        ref_at=(0, -1.5, 0), ref_size=0.50, hide_ref=True, lcsc="C45783",
    ))
    new_blocks.append(load_std_footprint(
        "Resistor_SMD.pretty/R_0805_2012Metric.kicad_mod",
        "R_LED2", "2.2k", 90.00, 83.40, rot=180,
        pad_nets={"1": "/+5V", "2": "/LED_5V_A"},
        ref_at=(0, -1.5, 0), ref_size=0.50, hide_ref=True, lcsc="C17520",
    ))
    new_blocks.append(load_std_footprint(
        "LED_SMD.pretty/LED_0805_2012Metric.kicad_mod",
        "D_5V", "GRN_5V", 93.80, 83.40, rot=180,
        pad_nets={"1": "/GND", "2": "/LED_5V_A"},
        ref_at=(0, -1.5, 0), ref_size=0.50, hide_ref=True, lcsc="C2297",
    ))

    # Auxiliary 5V ESP Header
    new_blocks.append(load_std_footprint(
        "Connector_PinHeader_2.54mm.pretty/PinHeader_1x02_P2.54mm_Vertical.kicad_mod",
        "J_PWR_ESP", "5V_ESP_AUX", 94.50, 87.80, rot=90,
        pad_nets={"1": "/GND", "2": "/+5V"},
        ref_at=(1.27, -2.2, 90), ref_size=0.60, hide_ref=True, exclude_bom=True, exclude_pos=True,
    ))

    # Dedicated 4-pin JST-XH IMU Connector (GY-BNO055: 1=GND, 2=3V3, 3=SDA/IO1, 4=SCL/IO11)
    new_blocks.append(load_std_footprint(
        "Connector_JST.pretty/JST_XH_B4B-XH-A_1x04_P2.50mm_Vertical.kicad_mod",
        "J_IMU", "GY_BNO055_I2C", 63.20, 80.50, rot=0,
        pad_nets={"1": "/GND", "2": "/+3V3_C6", "3": "/EXP_IO1", "4": "/EXP_IO11"},
        ref_at=(3.75, -3.5, 0), ref_size=0.60, hide_ref=True, exclude_bom=True, exclude_pos=True,
    ))

    # Optional Base-Station 2S Voltage Monitor Divider + Solder Jumper
    new_blocks.append(load_std_footprint(
        "Resistor_SMD.pretty/R_0805_2012Metric.kicad_mod",
        "R_2S_TOP", "100k", 68.50, 87.50, rot=180,
        pad_nets={"1": "/BUCK_EN_SW", "2": "/ADC_2S_DIV"},
        ref_at=(0, -1.5, 0), ref_size=0.50, hide_ref=True, lcsc="C17407",
    ))
    new_blocks.append(load_std_footprint(
        "Resistor_SMD.pretty/R_0805_2012Metric.kicad_mod",
        "R_2S_BOT", "33k", 68.50, 89.80, rot=180,
        pad_nets={"1": "/GND", "2": "/ADC_2S_DIV"},
        ref_at=(0, 1.5, 0), ref_size=0.50, hide_ref=True, lcsc="C17633",
    ))
    new_blocks.append(load_std_footprint(
        "Jumper.pretty/SolderJumper-2_P1.3mm_Open_Pad1.0x1.5mm.kicad_mod",
        "JP_2S_ADC", "2S_MON", 64.20, 88.00, rot=180,
        pad_nets={"1": "/ADC_2S_DIV", "2": "/BATT_VOLT"},
        ref_at=(0, -1.8, 0), ref_size=0.55, hide_ref=True, exclude_bom=True, exclude_pos=True,
    ))

    # --- D.6 3S-6S Motor LiPo Balance Monitor (J_BAL_MOT + F_BAL + 2x ADS1115) ---
    j_bal_blk = load_std_footprint(
        "Connector_PinHeader_2.54mm.pretty/PinHeader_1x07_P2.54mm_Horizontal.kicad_mod",
        "J_BAL_MOT", "LIPO_3S_6S_BAL", 4.40, 58.62, rot=180,
        pad_nets={
            "1": "/BAL_GND", "2": "/BAL_C1", "3": "/BAL_C2", "4": "/BAL_C3",
            "5": "/BAL_C4", "6": "/BAL_C5", "7": "/BAL_C6",
        },
        ref_at=(0, -2.5, 0), ref_size=0.60, hide_ref=True, exclude_bom=True, exclude_pos=True,
    )
    j_bal_blk = re.sub(
        r'\t\t\(fp_(?:line|rect)\n\t\t\t\(start (?:4\.15|10\.15)[\s\S]*?\(layer "F\.SilkS"\)\n\t\t\)\n?',
        '', j_bal_blk,
    )
    new_blocks.append(j_bal_blk)
    new_blocks.append(load_std_footprint(
        "Fuse.pretty/Fuse_1206_3216Metric.kicad_mod",
        "F_BAL", "1.5A_PTC", 8.60, 61.20, rot=0,
        pad_nets={"1": "/BAL_GND", "2": "/GND"},
        ref_at=(0, 2.2, 0), ref_size=0.55, hide_ref=True, lcsc="C2845946",
    ))

    bal_top_dividers = [
        ("R_C1T", "51k", 39.34, 43.20, "C17737", "/BAL_C1", "/ADC_C1"),
        ("R_C2T", "100k", 41.88, 43.20, "C17407", "/BAL_C2", "/ADC_C2"),
        ("R_C3T", "120k", 44.42, 43.20, "C17436", "/BAL_C3", "/ADC_C3"),
        ("R_C4T", "100k", 46.96, 43.20, "C17407", "/BAL_C4", "/ADC_C4"),
        ("R_C5T", "110k", 49.50, 43.20, "C17422", "/BAL_C5", "/ADC_C5"),
        ("R_C6T", "100k", 52.04, 43.20, "C17407", "/BAL_C6", "/ADC_C6"),
    ]
    for ref, val, x, y, lcsc, n1, n2 in bal_top_dividers:
        new_blocks.append(load_std_footprint(
            "Resistor_SMD.pretty/R_0805_2012Metric.kicad_mod",
            ref, val, x, y, rot=270,
            pad_nets={"1": n1, "2": n2},
            ref_at=(0, -1.8, 0), ref_size=0.55, hide_ref=True, lcsc=lcsc,
        ))

    bal_bot_dividers = [
        ("R_C1B", "100k", 35.20, 57.20, 180, "C17407", "/ADC_C1"),
        ("R_C2B", "51k", 35.20, 52.00, 180, "C17737", "/ADC_C2"),
        ("R_C3B", "33k", 41.80, 52.80, 180, "C17633", "/ADC_C3"),
        ("R_C4B", "15k", 46.00, 48.20, 0, "C17475", "/ADC_C4"),
        ("R_C5B", "15k", 49.80, 52.80, 180, "C17475", "/ADC_C5"),
        ("R_C6B", "10k", 53.40, 48.20, 0, "C17414", "/ADC_C6"),
    ]
    for ref, val, x, y, rot, lcsc, n1 in bal_bot_dividers:
        new_blocks.append(load_std_footprint(
            "Resistor_SMD.pretty/R_0805_2012Metric.kicad_mod",
            ref, val, x, y, rot=rot,
            pad_nets={"1": n1, "2": "/GND"},
            ref_at=(0, -1.6, 0), ref_size=0.55, hide_ref=True, lcsc=lcsc,
        ))

    bal_filter_caps = [
        ("C_F1", 35.20, 59.40, 180, "/ADC_C1"),
        ("C_F2", 35.20, 54.20, 180, "/ADC_C2"),
        ("C_F3", 41.80, 55.00, 180, "/ADC_C3"),
        ("C_F4", 46.00, 50.40, 0, "/ADC_C4"),
        ("C_F5", 49.80, 55.00, 180, "/ADC_C5"),
        ("C_F6", 53.40, 50.40, 0, "/ADC_C6"),
    ]
    for ref, x, y, rot, n1 in bal_filter_caps:
        new_blocks.append(load_std_footprint(
            "Capacitor_SMD.pretty/C_0805_2012Metric.kicad_mod",
            ref, "100nF", x, y, rot=rot,
            pad_nets={"1": n1, "2": "/GND"},
            ref_at=(0, -1.6, 0), ref_size=0.55, hide_ref=True, lcsc="C49678",
        ))

    new_blocks.append(load_std_footprint(
        "Package_SO.pretty/MSOP-10_3x3mm_P0.5mm.kicad_mod",
        "U_ADS1", "ADS1115", 44.20, 60.50, rot=270,
        pad_nets={
            "1": "/GND", "3": "/GND", "4": "/ADC_C4", "5": "/ADC_C3",
            "6": "/ADC_C2", "7": "/ADC_C1", "8": "/+3V3_C6",
            "9": "/EXP_IO1", "10": "/EXP_IO11",
        },
        ref_at=(0, -3.0, 0), ref_size=0.55, hide_ref=True, lcsc="C37593",
    ))
    new_blocks.append(load_std_footprint(
        "Package_SO.pretty/MSOP-10_3x3mm_P0.5mm.kicad_mod",
        "U_ADS2", "ADS1115", 51.80, 60.50, rot=270,
        pad_nets={
            "1": "/+3V3_C6", "3": "/GND", "4": "/ADC_C6", "5": "/ADC_C5",
            "6": "/GND", "7": "/GND", "8": "/+3V3_C6",
            "9": "/EXP_IO1", "10": "/EXP_IO11",
        },
        ref_at=(0, -3.0, 0), ref_size=0.55, hide_ref=True, lcsc="C37593",
    ))
    new_blocks.append(load_std_footprint(
        "Capacitor_SMD.pretty/C_0805_2012Metric.kicad_mod",
        "C_ADS1", "100nF", 47.60, 58.40, rot=90,
        pad_nets={"1": "/+3V3_C6", "2": "/GND"},
        ref_at=(0, -1.6, 0), ref_size=0.55, hide_ref=True, lcsc="C49678",
    ))
    new_blocks.append(load_std_footprint(
        "Capacitor_SMD.pretty/C_0805_2012Metric.kicad_mod",
        "C_ADS2", "100nF", 55.40, 59.50, rot=90,
        pad_nets={"1": "/+3V3_C6", "2": "/GND"},
        ref_at=(0, -1.6, 0), ref_size=0.55, hide_ref=True, lcsc="C49678",
    ))
    new_blocks.append(load_std_footprint(
        "Resistor_SMD.pretty/R_0805_2012Metric.kicad_mod",
        "R_I2C_SDA", "10k", 58.80, 56.80, rot=90,
        pad_nets={"1": "/+3V3_C6", "2": "/EXP_IO1"},
        ref_at=(0, -1.6, 0), ref_size=0.55, hide_ref=True, lcsc="C17414",
    ))
    new_blocks.append(load_std_footprint(
        "Resistor_SMD.pretty/R_0805_2012Metric.kicad_mod",
        "R_I2C_SCL", "10k", 58.80, 61.50, rot=90,
        pad_nets={"1": "/+3V3_C6", "2": "/EXP_IO11"},
        ref_at=(0, -1.6, 0), ref_size=0.55, hide_ref=True, lcsc="C17414",
    ))

    # --- D.7 Onboard ESP32-C6-WROOM-1-N8 + AMS1117-3.3 Dual-OR LDO + Boot/Reset ---
    new_blocks.append(make_esp32_c6_wroom1_footprint(
        "U_C6", "ESP32-C6-WROOM-1-N8", 46.50, 78.30, rot=180,
        pad_nets={
            "1": "/GND", "2": "/+3V3_C6", "3": "/C6_EN",
            "4": "/LORA_MISO", "5": "/LORA_SCK", "6": "/LORA_NSS", "7": "/LORA_MOSI",
            "8": "/LORA_DIO0", "9": "/EXP_IO1", "10": "/C6_IO8", "11": "/WATER_SENS",
            "12": "/EXP_IO11", "13": "/USB_DN", "14": "/USB_DP",
            "15": "/C6_BOOT", "16": "/ANCHOR_LIMIT", "17": "/STROBE_DIN", "18": "/ANCHOR_HALL",
            "19": "/SERVO_RACK", "20": "/INTER_TX_M2S", "21": "/SERVO_WINCH",
            "23": "/SERVO_RUDDER", "24": "/SERVO_ESC",
            "26": "/INTER_RX_S2M", "27": "/BATT_VOLT", "28": "/GND", "29": "/GND",
        },
        lcsc="C5366877", ref_at=(0, 4.5, 0), ref_size=0.70, hide_ref=True,
    ))
    new_blocks.append(load_std_footprint(
        "Package_TO_SOT_SMD.pretty/SOT-223-3_TabPin2.kicad_mod",
        "U_LDO", "AMS1117-3.3", 66.50, 70.00, rot=0,
        pad_nets={"1": "/GND", "2": "/+3V3_C6", "3": "/LDO_VIN"},
        ref_at=(0, -3.5, 0), ref_size=0.55, hide_ref=True, lcsc="C6186",
        model_path="${KICAD10_3DMODEL_DIR}/Package_TO_SOT_SMD.3dshapes/SOT-223.step",
    ))
    # Dual Schottky OR into LDO input (USB-C VBUS or Main +5V Buck)
    new_blocks.append(load_std_footprint(
        "Diode_SMD.pretty/D_SOD-123.kicad_mod",
        "D_LDO_USB", "B5819W", 73.60, 66.20, rot=0,
        pad_nets={"1": "/LDO_VIN", "2": "/VBUS_USB"},
        ref_at=(0, -1.6, 0), ref_size=0.50, hide_ref=True, lcsc="C8598",
    ))
    new_blocks.append(load_std_footprint(
        "Diode_SMD.pretty/D_SOD-123.kicad_mod",
        "D_LDO_5V", "B5819W", 72.60, 75.50, rot=0,
        pad_nets={"1": "/LDO_VIN", "2": "/+5V"},
        ref_at=(0, -1.6, 0), ref_size=0.50, hide_ref=True, lcsc="C8598",
    ))
    new_blocks.append(load_std_footprint(
        "Capacitor_SMD.pretty/C_0805_2012Metric.kicad_mod",
        "C_LDO_IN", "22uF", 63.60, 75.50, rot=90,
        pad_nets={"1": "/LDO_VIN", "2": "/GND"},
        ref_at=(0, -1.6, 0), ref_size=0.50, hide_ref=True, lcsc="C45783",
    ))
    new_blocks.append(load_std_footprint(
        "Capacitor_SMD.pretty/C_0805_2012Metric.kicad_mod",
        "C_LDO_OUT", "22uF", 74.40, 70.40, rot=90,
        pad_nets={"1": "/+3V3_C6", "2": "/GND"},
        ref_at=(0, -1.6, 0), ref_size=0.50, hide_ref=True, lcsc="C45783",
    ))
    new_blocks.append(load_std_footprint(
        "Capacitor_SMD.pretty/C_0805_2012Metric.kicad_mod",
        "C_C6_VDD", "100nF", 58.00, 82.30, rot=270,
        pad_nets={"1": "/+3V3_C6", "2": "/GND"},
        ref_at=(0, -1.6, 0), ref_size=0.50, hide_ref=True, lcsc="C49678",
    ))
    new_blocks.append(load_std_footprint(
        "Resistor_SMD.pretty/R_0805_2012Metric.kicad_mod",
        "R_C6_EN", "10k", 58.00, 77.60, rot=90,
        pad_nets={"1": "/C6_EN", "2": "/+3V3_C6"},
        ref_at=(0, -1.6, 0), ref_size=0.50, hide_ref=True, lcsc="C17414",
    ))
    new_blocks.append(load_std_footprint(
        "Resistor_SMD.pretty/R_0805_2012Metric.kicad_mod",
        "R_IO8_PU", "10k", 58.00, 72.13, rot=90,
        pad_nets={"1": "/C6_IO8", "2": "/+3V3_C6"},
        ref_at=(0, -1.6, 0), ref_size=0.50, hide_ref=True, lcsc="C17414",
    ))
    new_blocks.append(load_std_footprint(
        "Button_Switch_SMD.pretty/SW_Push_SPST_NO_Alps_SKRK.kicad_mod",
        "SW_C6_BOOT", "SKRKAEE020", 33.20, 67.20, rot=90,
        pad_nets={"1": "/GND", "2": "/C6_BOOT"},
        ref_at=(0, -2.5, 0), ref_size=0.55, hide_ref=True, lcsc="C115357",
        bom_fp_name="SKRKAEE020",
    ))
    new_blocks.append(load_std_footprint(
        "Capacitor_SMD.pretty/C_0805_2012Metric.kicad_mod",
        "C_C6_EN", "100nF", 33.20, 81.20, rot=90,
        pad_nets={"1": "/C6_EN", "2": "/GND"},
        ref_at=(0, -1.6, 0), ref_size=0.50, hide_ref=True, lcsc="C49678",
    ))
    new_blocks.append(load_std_footprint(
        "Button_Switch_SMD.pretty/SW_Push_SPST_NO_Alps_SKRK.kicad_mod",
        "SW_C6_RST", "SKRKAEE020", 33.20, 86.20, rot=90,
        pad_nets={"1": "/GND", "2": "/C6_EN"},
        ref_at=(0, -2.5, 0), ref_size=0.55, hide_ref=True, lcsc="C115357",
        bom_fp_name="SKRKAEE020",
    ))

    # ==========================================================================
    # E. COPPER ROUTING (Clean Orthogonal Traces + Power Tracks using P())
    # ==========================================================================
    routes = []

    # 1. Right half of /+3V3 & Joystick nets (/JOY_X, /JOY_Y, /JOY_SW) for inward-shifted JOY1 (72.7525, 51.715)
    routes += make_route([(63.24, 13.50), (74.80, 25.06), (74.80, 42.5325), P("JOY1", 6)], 0.30, "F.Cu", "/+3V3")
    routes += make_route([P("JOY1", 6), P("JOY1", 3)], 0.30, "B.Cu", "/+3V3")

    routes += make_route([
        (53.08, 13.50), (55.58, 11.00), (70.65, 11.00), (75.15, 15.50),
        (75.15, 40.5875), P("JOY1", 5),
    ], 0.30, "B.Cu", "/JOY_X")
    routes += make_route([
        (60.70, 13.50), (62.20, 15.00), (73.00, 15.00), (74.50, 16.50),
        (74.50, 31.10), (68.00, 37.60), (62.50, 43.10), (62.50, 50.1925), P("JOY1", 2),
    ], 0.30, "B.Cu", "/JOY_Y")
    routes += make_route([
        P("J2", 5), (58.16, 29.80), (58.96, 29.00), (69.50, 29.00), (71.50, 31.00),
        (71.50, 39.50), (68.50, 42.50), (68.50, 46.50), (71.50, 49.50),
        (71.50, 52.9625), P("JOY1", 9), P("JOY1", 10),
    ], 0.30, "F.Cu", "/JOY_SW")

    # 2. Sensor Bus (/WATER_SENS -> U_C6:11 & optional 2S /BATT_VOLT -> U_C6:27, outside antenna keepout)
    routes += make_route([
        P("J_WATER", 2), (24.15, 69.31), (24.15, 61.20), (37.40, 61.20), (37.40, 63.00),
    ], 0.25, "F.Cu", "/WATER_SENS")
    routes.append(make_via(37.40, 63.00, "/WATER_SENS", size=0.55, drill=0.30))
    routes += make_route([
        (37.40, 63.00), (37.40, 67.20), (53.40, 67.20),
    ], 0.25, "B.Cu", "/WATER_SENS")
    routes.append(make_via(53.40, 67.20, "/WATER_SENS", size=0.55, drill=0.30))
    routes += make_route([(53.40, 67.20), (53.40, 70.86), P("U_C6", 11)], 0.25, "F.Cu", "/WATER_SENS")

    routes += make_route([P("JP_2S_ADC", 2), (61.20, 88.00), (61.20, 86.80)], 0.25, "F.Cu", "/BATT_VOLT")
    routes.append(make_via(61.20, 86.80, "/BATT_VOLT", size=0.55, drill=0.30))
    routes += make_route([
        (61.20, 86.80), (61.20, 84.20), (39.60, 84.20), (39.60, 82.29),
    ], 0.25, "B.Cu", "/BATT_VOLT")
    routes.append(make_via(39.60, 82.29, "/BATT_VOLT", size=0.55, drill=0.30))
    routes += make_route([(39.60, 82.29), P("U_C6", 27)], 0.25, "F.Cu", "/BATT_VOLT")

    # 3. Actuators & Sensors (Left Edge -> U_C6:16..24, 15, 20, 26)
    # 3a. Planar non-crossing U-turn from Y=69.42 (ESC..RCK) -> B.Cu Y=64.6..66.4 -> X=31.6..34.0 -> F.Cu stubs
    servo_left_map = [
        # (ref, u_pin, y_bus, x_via, y_pin, net)
        ("RCK", 19, 66.40, 31.60, 72.13, "/SERVO_RACK"),
        ("WIN", 21, 65.80, 32.40, 74.67, "/SERVO_WINCH"),
        ("RUD", 23, 65.20, 33.20, 77.21, "/SERVO_RUDDER"),
        ("ESC", 24, 64.60, 34.00, 78.48, "/SERVO_ESC"),
    ]
    for sref, upin, yb, xv, yp, net in servo_left_map:
        sx, sy = P(sref, 3)
        routes += make_route([
            (sx, sy), (sx, yb), (xv, yb), (xv, yp),
        ], 0.25, "B.Cu", net)
        routes.append(make_via(xv, yp, net, size=0.55, drill=0.30))
        routes += make_route([(xv, yp), P("U_C6", upin)], 0.25, "F.Cu", net)

    # 3b. /ANCHOR_LIMIT (Y=67.60), /STROBE_DIN (Y=70.35), /ANCHOR_HALL (Y=71.15)
    routes += make_route([P("J_LIMIT", 2), (29.50, 69.26), (29.50, 67.60)], 0.25, "B.Cu", "/ANCHOR_LIMIT")
    routes.append(make_via(29.50, 67.60, "/ANCHOR_LIMIT", size=0.55, drill=0.30))
    routes += make_route([
        (29.50, 67.60), (35.60, 67.60), (36.32, 68.32), P("U_C6", 16),
    ], 0.25, "F.Cu", "/ANCHOR_LIMIT")

    routes += make_route([
        P("STRB", 3), (18.40, 71.66), (18.40, 76.50), (29.50, 76.50), (29.50, 70.35),
    ], 0.25, "B.Cu", "/STROBE_DIN")
    routes.append(make_via(29.50, 70.35, "/STROBE_DIN", size=0.55, drill=0.30))
    routes += make_route([
        (29.50, 70.35), (35.80, 70.35), (36.56, 69.59), P("U_C6", 17),
    ], 0.25, "F.Cu", "/STROBE_DIN")

    routes += make_route([
        P("J_WINCH_HUB", 4), (22.12, 80.20), (30.50, 80.20), (30.50, 71.15),
    ], 0.25, "B.Cu", "/ANCHOR_HALL")
    routes.append(make_via(30.50, 71.15, "/ANCHOR_HALL", size=0.55, drill=0.30))
    routes += make_route([
        (30.50, 71.15), (35.80, 71.15), (36.09, 70.86), P("U_C6", 18),
    ], 0.25, "F.Cu", "/ANCHOR_HALL")

    # 3c. Inter-ESP UART from kept B.Cu V1 endpoints (39.05,64.95) & (39.50,65.207) -> U_C6:20 & U_C6:26
    routes += make_route([(39.05, 64.95), (38.55, 65.25)], 0.25, "B.Cu", "/INTER_TX_M2S")
    routes.append(make_via(38.55, 65.25, "/INTER_TX_M2S", size=0.50, drill=0.30))
    routes += make_route([
        (38.55, 65.25), (39.45, 66.15), (39.45, 73.40), P("U_C6", 20),
    ], 0.25, "F.Cu", "/INTER_TX_M2S")

    routes += make_route([
        (39.50, 65.207106), (40.80, 66.507106),
    ], 0.25, "B.Cu", "/INTER_RX_S2M")
    routes.append(make_via(40.80, 66.51, "/INTER_RX_S2M", size=0.55, drill=0.30))
    routes += make_route([(40.80, 66.51), (40.80, 81.02), P("U_C6", 26)], 0.25, "F.Cu", "/INTER_RX_S2M")

    # 4. /+3V3_C6, I2C (/EXP_IO1 SDA, /EXP_IO11 SCL), 2x ADS1115 3S-6S Balance Monitor & U_LDO
    # U_LDO:2 & Tab -> (62.80, 70.00) -> B.Cu -> (59.40, 71.18) -> F.Cu spine X=59.40 -> U_C6:2 & B.Cu Y=78.60 -> J_IMU:2
    routes += make_route([P("U_LDO", 2), (69.65, 70.00)], 0.50, "F.Cu", "/+3V3_C6")
    routes += make_route([P("U_LDO", 2), (62.80, 70.00)], 0.45, "F.Cu", "/+3V3_C6")
    routes.append(make_via(62.80, 70.00, "/+3V3_C6", size=0.60, drill=0.30))
    routes += make_route([(62.80, 70.00), (61.62, 71.18), (59.40, 71.18)], 0.45, "B.Cu", "/+3V3_C6")
    routes.append(make_via(59.40, 71.18, "/+3V3_C6", size=0.60, drill=0.30))
    routes += make_route([
        (59.40, 71.18), (59.40, 81.35), P("C_C6_VDD", 1), (56.20, 82.29), P("U_C6", 2),
    ], 0.40, "F.Cu", "/+3V3_C6")
    routes += make_route([(59.40, 81.35), (60.40, 80.35)], 0.35, "F.Cu", "/+3V3_C6")
    routes.append(make_via(60.40, 80.35, "/+3V3_C6", size=0.55, drill=0.30))
    routes += make_route([
        (60.40, 80.35), (60.40, 78.60), (65.70, 78.60), P("J_IMU", 2),
    ], 0.35, "B.Cu", "/+3V3_C6")
    routes += make_route([(59.40, 71.18), P("R_IO8_PU", 2)], 0.30, "F.Cu", "/+3V3_C6")
    routes += make_route([(59.40, 76.65), P("R_C6_EN", 2)], 0.30, "F.Cu", "/+3V3_C6")

    # U_LDO:Tab -> B.Cu bridge -> C_LDO_OUT:1 -> J3:2 & upper B.Cu Y=65.00 -> R_I2C_*, ADS1115 -> F.Cu bridge -> J_WINCH_HUB:3
    routes += make_route([(69.65, 70.00), (70.40, 71.20)], 0.45, "F.Cu", "/+3V3_C6")
    routes.append(make_via(70.40, 71.20, "/+3V3_C6", size=0.60, drill=0.30))
    routes += make_route([(70.40, 71.20), (73.40, 71.20)], 0.45, "B.Cu", "/+3V3_C6")
    routes.append(make_via(73.40, 71.20, "/+3V3_C6", size=0.60, drill=0.30))
    routes += make_route([(73.40, 71.20), P("C_LDO_OUT", 1), (76.40, 71.35), (76.40, 74.50), P("J3", 2)], 0.40, "F.Cu", "/+3V3_C6")
    routes += make_route([(76.40, 71.35), (76.40, 65.00)], 0.35, "F.Cu", "/+3V3_C6")
    routes.append(make_via(76.40, 65.00, "/+3V3_C6", size=0.60, drill=0.32))
    routes += make_route([(76.40, 65.00), (59.40, 65.00), (59.40, 62.45)], 0.35, "B.Cu", "/+3V3_C6")
    routes.append(make_via(59.40, 62.45, "/+3V3_C6", size=0.60, drill=0.32))
    routes += make_route([(59.40, 62.45), P("R_I2C_SCL", 1)], 0.30, "F.Cu", "/+3V3_C6")
    routes += make_route([(59.40, 62.45), (59.40, 57.75)], 0.35, "B.Cu", "/+3V3_C6")
    routes.append(make_via(59.40, 57.75, "/+3V3_C6", size=0.55, drill=0.30))
    routes += make_route([(59.40, 57.75), P("R_I2C_SDA", 1)], 0.30, "F.Cu", "/+3V3_C6")

    # Compact B.Cu Y=64.45 /+3V3_C6 rail feeding U_ADS2:8, C_ADS1:1, U_ADS1:8, then F.Cu bridge -> J_WINCH_HUB:3
    routes += make_route([P("R_I2C_SDA", 1), (56.60, 57.7125), (56.60, 60.45), P("C_ADS2", 1)], 0.25, "F.Cu", "/+3V3_C6")
    routes += make_route([(59.40, 64.45), (43.40, 64.45)], 0.30, "B.Cu", "/+3V3_C6")
    routes.append(make_via(51.80, 64.45, "/+3V3_C6", size=0.55, drill=0.30))
    routes += make_route([(51.80, 64.45), P("U_ADS2", 8)], 0.20, "F.Cu", "/+3V3_C6")
    routes.append(make_via(47.60, 64.45, "/+3V3_C6", size=0.55, drill=0.30))
    routes += make_route([(47.60, 64.45), P("C_ADS1", 1)], 0.22, "F.Cu", "/+3V3_C6")
    routes.append(make_via(44.20, 64.45, "/+3V3_C6", size=0.55, drill=0.30))
    routes += make_route([(44.20, 64.45), P("U_ADS1", 8)], 0.22, "F.Cu", "/+3V3_C6")
    routes += make_route([P("U_ADS2", 1), (53.80, 58.40), (53.80, 60.45), P("C_ADS2", 1)], 0.22, "F.Cu", "/+3V3_C6")
    routes.append(make_via(43.40, 64.45, "/+3V3_C6", size=0.55, drill=0.30))
    routes += make_route([(43.40, 64.45), (36.40, 64.45), (34.80, 62.20)], 0.35, "F.Cu", "/+3V3_C6")
    routes.append(make_via(34.80, 62.20, "/+3V3_C6", size=0.55, drill=0.30))
    routes += make_route([(34.80, 62.20), (28.10, 62.20)], 0.35, "B.Cu", "/+3V3_C6")
    routes.append(make_via(28.10, 62.20, "/+3V3_C6", size=0.60, drill=0.32))
    routes += make_route([(28.10, 62.20), (28.10, 79.10)], 0.35, "F.Cu", "/+3V3_C6")
    routes.append(make_via(28.10, 79.10, "/+3V3_C6", size=0.60, drill=0.32))
    routes += make_route([(28.10, 79.10), (19.58, 79.10), P("J_WINCH_HUB", 3)], 0.40, "B.Cu", "/+3V3_C6")

    # I2C SDA (/EXP_IO1): U_ADS1:9 -> via (46.00,63.85) -> B.Cu Y=63.65 -> (49.60,63.65) -> (52.55,63.75) -> R_I2C_SDA:2
    routes += make_route([P("U_ADS1", 9), (44.70, 63.85), (46.00, 63.85)], 0.20, "F.Cu", "/EXP_IO1")
    routes.append(make_via(46.00, 63.85, "/EXP_IO1", size=0.50, drill=0.30))
    routes += make_route([
        (46.00, 63.85), (46.20, 63.65), (49.60, 63.65), (52.55, 63.75), (57.20, 63.75), (57.20, 55.85),
    ], 0.22, "B.Cu", "/EXP_IO1")
    routes.append(make_via(52.55, 63.75, "/EXP_IO1", size=0.50, drill=0.30))
    routes += make_route([(52.55, 63.75), (52.30, 63.50), P("U_ADS2", 9)], 0.20, "F.Cu", "/EXP_IO1")
    routes.append(make_via(57.20, 55.85, "/EXP_IO1", size=0.55, drill=0.30))
    routes += make_route([(57.20, 55.85), P("R_I2C_SDA", 2)], 0.25, "F.Cu", "/EXP_IO1")
    routes.append(make_via(49.60, 63.65, "/EXP_IO1", size=0.50, drill=0.30))
    routes += make_route([
        (49.60, 63.65), (49.60, 73.40), P("U_C6", 9), (52.50, 73.40), (52.50, 82.80),
    ], 0.25, "F.Cu", "/EXP_IO1")
    routes.append(make_via(52.50, 82.80, "/EXP_IO1", size=0.55, drill=0.30))
    routes += make_route([(52.50, 82.80), (68.20, 82.80), P("J_IMU", 3)], 0.25, "B.Cu", "/EXP_IO1")

    # I2C SCL (/EXP_IO11): U_ADS1:10 -> B.Cu Y=60.40 -> U_ADS2:10 -> F.Cu R_I2C_SCL:2 -> F.Cu X=61.75 -> U_C6:12 & J_IMU:4
    routes += make_route([P("U_ADS1", 10), (46.20, 62.60), (46.20, 60.40)], 0.20, "F.Cu", "/EXP_IO11")
    routes.append(make_via(46.20, 60.40, "/EXP_IO11", size=0.50, drill=0.30))
    routes += make_route([(46.20, 60.40), (53.80, 60.40), (53.80, 62.20)], 0.25, "B.Cu", "/EXP_IO11")
    routes.append(make_via(53.80, 62.20, "/EXP_IO11", size=0.50, drill=0.30))
    routes += make_route([(53.80, 62.20), (52.80, 62.20), P("U_ADS2", 10)], 0.20, "F.Cu", "/EXP_IO11")
    routes += make_route([
        (53.80, 62.20), (57.40, 62.20), (57.40, 60.5875), P("R_I2C_SCL", 2),
        (61.75, 60.5875), (61.75, 69.59), P("U_C6", 12),
        (61.75, 69.59), (61.75, 84.50), (70.70, 84.50), P("J_IMU", 4),
    ], 0.25, "F.Cu", "/EXP_IO11")

    # J_BAL_MOT (Left Edge 90-deg lateral) & 6-channel analog front-end
    routes += make_route([
        P("J_BAL_MOT", 1), (4.40, 60.40), (5.20, 61.20), P("F_BAL", 1),
    ], 0.50, "F.Cu", "/BAL_GND")

    bal_bus_lanes = [
        # (k, xv, yh, x_dst)
        (6, 5.80, 37.50, 52.04),
        (5, 6.40, 38.10, 49.50),
        (4, 7.00, 38.70, 46.96),
        (3, 7.60, 39.30, 44.42),
        (2, 8.20, 39.90, 41.88),
        (1, 8.80, 40.50, 39.34),
    ]
    for k, xv, yh, x_dst in bal_bus_lanes:
        px, py = P("J_BAL_MOT", k + 1)
        net = f"/BAL_C{k}"
        routes += make_route([
            (px, py), (xv, py - (xv - px)), (xv, yh), (x_dst, yh), (x_dst, 41.00),
        ], 0.25, "B.Cu", net)
        routes.append(make_via(x_dst, 41.00, net, size=0.55, drill=0.30))
        routes += make_route([(x_dst, 41.00), P(f"R_C{k}T", 1)], 0.30, "F.Cu", net)

    routes += make_route([
        P("R_C1T", 2), (37.80, 45.76), (37.80, 57.20),
        P("R_C1B", 1), P("C_F1", 1), (38.80, 59.40), (38.80, 63.60), (43.70, 63.60), P("U_ADS1", 7),
    ], 0.20, "F.Cu", "/ADC_C1")
    routes += make_route([P("R_C2T", 2), (41.88, 46.20)], 0.20, "F.Cu", "/ADC_C2")
    routes.append(make_via(41.88, 46.20, "/ADC_C2", size=0.50, drill=0.30))
    routes += make_route([(41.88, 46.20), (39.60, 48.48), (36.15, 48.48), (36.15, 50.00)], 0.20, "B.Cu", "/ADC_C2")
    routes.append(make_via(36.15, 50.00, "/ADC_C2", size=0.50, drill=0.30))
    routes += make_route([
        (36.15, 50.00), P("R_C2B", 1), P("C_F2", 1), (36.95, 54.20),
    ], 0.20, "F.Cu", "/ADC_C2")
    routes.append(make_via(36.95, 54.20, "/ADC_C2", size=0.50, drill=0.30))
    routes += make_route([(36.95, 54.20), (40.20, 57.45), (40.20, 61.60)], 0.20, "B.Cu", "/ADC_C2")
    routes.append(make_via(40.20, 61.60, "/ADC_C2", size=0.50, drill=0.30))
    routes += make_route([(40.20, 61.60), (42.20, 61.60), P("U_ADS1", 6)], 0.20, "F.Cu", "/ADC_C2")
    routes += make_route([
        P("R_C3T", 2), (42.75, 45.78), P("R_C3B", 1), P("C_F3", 1),
        (43.20, 55.45), P("U_ADS1", 5),
    ], 0.20, "F.Cu", "/ADC_C3")
    routes += make_route([
        P("R_C4T", 2), (45.05, 46.02), P("R_C4B", 1), P("C_F4", 1),
        (45.05, 54.00), (43.70, 55.35), P("U_ADS1", 4),
    ], 0.20, "F.Cu", "/ADC_C4")
    routes += make_route([
        P("R_C5T", 2), (50.75, 45.47), P("R_C5B", 1), P("C_F5", 1),
        (50.80, 55.05), P("U_ADS2", 5),
    ], 0.20, "F.Cu", "/ADC_C5")
    routes += make_route([
        P("R_C6T", 2), (52.45, 44.52), P("R_C6B", 1), P("C_F6", 1),
        (52.45, 55.00), (51.30, 56.15), P("U_ADS2", 4),
    ], 0.20, "F.Cu", "/ADC_C6")

    # Local /GND ties & stitching vias for analog matrix & U_C6
    routes += make_route([
        P("U_ADS1", 3), (44.20, 56.90), (45.20, 56.90), P("U_ADS1", 1),
    ], 0.20, "F.Cu", "/GND")
    routes.append(make_via(45.20, 56.90, "/GND", size=0.50, drill=0.30))

    routes += make_route([P("U_ADS2", 3), (51.80, 57.20)], 0.20, "F.Cu", "/GND")
    routes.append(make_via(51.80, 56.90, "/GND", size=0.50, drill=0.30))

    routes += make_route([P("U_ADS2", 7), P("U_ADS2", 6), (50.60, 62.60), (50.60, 61.90)], 0.20, "F.Cu", "/GND")
    routes.append(make_via(50.60, 61.90, "/GND", size=0.50, drill=0.30))

    routes += make_route([P("R_C1B", 2), P("C_F1", 2), (33.50, 58.30)], 0.30, "F.Cu", "/GND")
    routes += make_route([P("R_C2B", 2), P("C_F2", 2), (33.50, 53.10)], 0.30, "F.Cu", "/GND")
    routes += make_route([P("R_C5B", 2), P("C_F5", 2), (48.00, 53.90)], 0.30, "F.Cu", "/GND")

    bal_gnd_ties = [
        (11.60, 61.20, [P("F_BAL", 2)], 0.40),
        (33.50, 58.30, [P("R_C1B", 2)], 0.30),
        (33.50, 53.10, [P("R_C2B", 2)], 0.30),
        (48.00, 53.90, [P("R_C5B", 2)], 0.30),
        (48.10, 49.30, [P("R_C4B", 2), P("C_F4", 2)], 0.30),
        (54.80, 51.50, [P("R_C6B", 2), P("C_F6", 2)], 0.30),
        (40.85, 56.40, [P("C_F3", 2), P("R_C3B", 2)], 0.30),
        (46.20, 56.20, [P("C_ADS1", 2)], 0.30),
        (55.40, 57.60, [P("C_ADS2", 2)], 0.30),
    ]
    for vx, vy, pts, tw in bal_gnd_ties:
        routes.append(make_via(vx, vy, "/GND", size=0.55, drill=0.30))
        for pt in pts:
            routes += make_route([(vx, vy), pt], tw, "F.Cu", "/GND")

    # 5. /+5V_SERVO (J_PWR_SRV -> Servo Bar + J_WINCH_HUB Pad 2)
    routes += make_route([P("J_PWR_SRV", 2), (3.96, 83.50), (3.96, 74.00), (6.00, 71.96), (16.16, 71.96)], 0.60, "F.Cu", "/+5V_SERVO")
    routes += make_route([P("J_PWR_SRV", 2), (4.96, 81.50), (17.04, 81.50), P("J_WINCH_HUB", 2)], 0.60, "F.Cu", "/+5V_SERVO")

    # 6. /SERVO_WINCH & /SERVO_RACK tap from WIN/RCK Pad 3 down to J_WINCH_HUB Pads 5 & 6
    routes += make_route([
        (11.08, 69.42), (11.08, 66.30), (18.75, 66.30), (18.75, 76.80), (24.66, 76.80), P("J_WINCH_HUB", 5),
    ], 0.30, "F.Cu", "/SERVO_WINCH")
    routes += make_route([
        (13.62, 69.42), (13.62, 67.20), (17.90, 67.20), (17.90, 77.70), (23.20, 77.70),
    ], 0.30, "F.Cu", "/SERVO_RACK")
    routes.append(make_via(23.20, 77.70, "/SERVO_RACK", size=0.55, drill=0.30))
    routes += make_route([(23.20, 77.70), (27.20, 77.70)], 0.30, "B.Cu", "/SERVO_RACK")
    routes.append(make_via(27.20, 77.70, "/SERVO_RACK", size=0.55, drill=0.30))
    routes += make_route([(27.20, 77.70), P("J_WINCH_HUB", 6)], 0.30, "F.Cu", "/SERVO_RACK")

    # 7. Breakaway Sub-Board Routing (isolated /HALL_*, /SUB_* nets)
    routes += make_route([
        P("J_WINCH_SUB", 2), (18.54, 98.50), (31.70, 98.50), P("C_SRV", 1),
    ], 0.60, "F.Cu", "/SUB_5V")
    routes += make_route([(31.70, 98.50), (37.24, 98.50), P("SRV_WIN", 2)], 0.60, "F.Cu", "/SUB_5V")
    routes += make_route([(37.24, 98.50), (46.14, 98.50), P("SRV_RCK", 2)], 0.60, "F.Cu", "/SUB_5V")

    routes += make_route([P("J_WINCH_SUB", 5), (26.16, 94.30), (39.78, 94.30), P("SRV_WIN", 3)], 0.30, "B.Cu", "/SUB_WIN")
    routes += make_route([P("J_WINCH_SUB", 6), (28.70, 98.10), (48.68, 98.10), P("SRV_RCK", 3)], 0.30, "B.Cu", "/SUB_RCK")

    routes += make_route([P("J_WINCH_SUB", 3), (21.08, 99.00), (57.50, 99.00), (58.30, 98.20)], 0.35, "B.Cu", "/HALL_VCC")
    routes.append(make_via(58.30, 98.20, "/HALL_VCC", size=0.55, drill=0.30))
    routes += make_route([(58.30, 98.20), (72.50, 98.20), (72.50, 95.45), P("C_HALL", 2), P("U_HALL", 1)], 0.35, "F.Cu", "/HALL_VCC")

    routes += make_route([P("J_WINCH_SUB", 4), (23.62, 94.10), (58.50, 94.10)], 0.30, "F.Cu", "/HALL_OUT")
    routes.append(make_via(58.50, 94.10, "/HALL_OUT", size=0.55, drill=0.30))
    routes += make_route([(58.50, 94.10), (60.80, 97.35), (83.80, 97.35)], 0.30, "B.Cu", "/HALL_OUT")
    routes.append(make_via(83.80, 97.35, "/HALL_OUT", size=0.55, drill=0.30))
    routes += make_route([(83.80, 97.35), P("U_HALL", 2)], 0.30, "F.Cu", "/HALL_OUT")

    routes += make_route([P("C_HALL", 1), (75.00, 98.60)], 0.35, "F.Cu", "/HALL_GND")
    routes += make_route([P("U_HALL", 3), (88.65, 96.40)], 0.35, "F.Cu", "/HALL_GND")
    for gx, gy in [(15.00, 94.20), (51.20, 96.40), (75.00, 98.60), (88.65, 96.40)]:
        routes.append(make_via(gx, gy, "/HALL_GND", size=0.55, drill=0.30))

    # 8. USB-C + Solar + IP2326 Charger + 2S LiPo + MT2492 5V Buck Routing
    # USB CC1 & CC2 (extend straight out past X=91.35 so B7/A5 & B6/B5 clear pads with >=0.25mm)
    routes += make_route([P("J_USBC", "A5"), (91.40, 37.75), (90.75, 38.40), (P("R_CC1", 2)[0], 38.40), P("R_CC1", 2)], 0.20, "F.Cu", "/USB_CC1")
    routes += make_route([P("J_USBC", "B5"), (91.40, 34.75), (90.70, 34.05), (P("R_CC2", 2)[0], 34.05), P("R_CC2", 2)], 0.20, "F.Cu", "/USB_CC2")

    # USB DP & DN -> right down F.Cu corridor -> short B.Cu bridge past SCL -> U_C6:14 & U_C6:13
    routes += make_route([P("J_USBC", "B6"), (91.40, 35.75), (90.90, 35.20), (90.65, 35.20)], 0.20, "F.Cu", "/USB_DP")
    routes.append(make_via(90.65, 35.20, "/USB_DP", size=0.50, drill=0.30))
    routes += make_route([P("J_USBC", "A6"), (91.00, 36.75), (90.65, 36.60), (89.35, 36.60)], 0.20, "F.Cu", "/USB_DP")
    routes.append(make_via(89.35, 36.60, "/USB_DP", size=0.50, drill=0.30))
    routes += make_route([(90.65, 35.20), (88.75, 35.20), (88.75, 36.60), (89.35, 36.60)], 0.20, "B.Cu", "/USB_DP")
    routes += make_route([(88.75, 36.60), (88.75, 40.50), (89.30, 41.05)], 0.20, "B.Cu", "/USB_DP")
    routes.append(make_via(89.30, 41.05, "/USB_DP", size=0.50, drill=0.30))
    routes += make_route([
        (89.30, 41.05), (89.60, 41.35), (89.60, 44.60), (85.50, 44.60), (82.00, 48.10),
        (82.00, 63.50), (63.40, 63.50),
    ], 0.20, "F.Cu", "/USB_DP")
    routes.append(make_via(63.40, 63.50, "/USB_DP", size=0.50, drill=0.30))
    routes += make_route([(63.40, 63.50), (60.35, 63.50)], 0.20, "B.Cu", "/USB_DP")
    routes.append(make_via(60.35, 63.50, "/USB_DP", size=0.50, drill=0.30))
    routes += make_route([(60.35, 63.50), (60.35, 67.05), P("U_C6", 14)], 0.20, "F.Cu", "/USB_DP")

    routes += make_route([P("J_USBC", "A7"), (91.15, 36.25), (90.80, 35.90), (89.65, 35.90)], 0.20, "F.Cu", "/USB_DN")
    routes.append(make_via(89.65, 35.90, "/USB_DN", size=0.50, drill=0.30))
    routes += make_route([P("J_USBC", "B7"), (90.35, 37.25)], 0.20, "F.Cu", "/USB_DN")
    routes.append(make_via(90.35, 37.25, "/USB_DN", size=0.50, drill=0.30))
    routes += make_route([(89.65, 35.90), (90.35, 35.90), (90.35, 37.25)], 0.20, "B.Cu", "/USB_DN")
    routes += make_route([(90.35, 37.25), (90.35, 41.30), (90.20, 41.45)], 0.20, "B.Cu", "/USB_DN")
    routes.append(make_via(90.20, 41.45, "/USB_DN", size=0.50, drill=0.30))
    routes += make_route([
        (90.20, 41.45), (90.20, 45.20), (85.80, 45.20), (82.70, 48.30),
        (82.70, 64.10), (63.90, 64.10),
    ], 0.20, "F.Cu", "/USB_DN")
    routes.append(make_via(63.90, 64.10, "/USB_DN", size=0.50, drill=0.30))
    routes += make_route([(63.90, 64.10), (61.05, 64.10)], 0.20, "B.Cu", "/USB_DN")
    routes.append(make_via(61.05, 64.10, "/USB_DN", size=0.50, drill=0.30))
    routes += make_route([(61.05, 64.10), (61.05, 68.32), P("U_C6", 13)], 0.20, "F.Cu", "/USB_DN")

    # VBUS_USB (bridge A4<->B4 -> D_USB:2 AND B.Cu hop south to (91.50, 41.20) so R_CC1:1 /GND bridges to J_USBC:A1 -> X=83.40 -> D_LDO_USB:2)
    routes += make_route([P("J_USBC", "B4"), (91.50, 34.05)], 0.30, "F.Cu", "/VBUS_USB")
    routes.append(make_via(91.50, 34.05, "/VBUS_USB", size=0.50, drill=0.30))
    routes += make_route([P("J_USBC", "A4"), (91.50, 38.95)], 0.30, "F.Cu", "/VBUS_USB")
    routes.append(make_via(91.50, 38.95, "/VBUS_USB", size=0.50, drill=0.30))
    routes += make_route([(91.50, 38.95), (91.50, 30.80), (94.30, 30.80)], 0.40, "B.Cu", "/VBUS_USB")
    routes.append(make_via(94.30, 30.80, "/VBUS_USB", size=0.55, drill=0.30))
    routes += make_route([(94.30, 30.80), (P("D_USB", 2)[0], 30.80), P("D_USB", 2)], 0.50, "F.Cu", "/VBUS_USB")
    routes += make_route([(91.50, 38.95), (91.50, 41.20)], 0.30, "B.Cu", "/VBUS_USB")
    routes.append(make_via(91.50, 41.20, "/VBUS_USB", size=0.50, drill=0.30))
    routes += make_route([
        (91.50, 41.20), (90.95, 41.75), (90.95, 45.80), (86.10, 45.80), (83.40, 48.50),
        (83.40, 67.20), (77.60, 67.20),
    ], 0.30, "F.Cu", "/VBUS_USB")
    routes.append(make_via(77.60, 67.20, "/VBUS_USB", size=0.55, drill=0.30))
    routes += make_route([(77.60, 67.20), (75.60, 67.20)], 0.30, "B.Cu", "/VBUS_USB")
    routes.append(make_via(75.60, 67.20, "/VBUS_USB", size=0.55, drill=0.30))
    routes += make_route([(75.60, 67.20), (75.60, 66.20), P("D_LDO_USB", 2)], 0.30, "F.Cu", "/VBUS_USB")
    routes += make_route([P("R_CC1", 1), (91.00, 40.025), (91.50, 39.75), P("J_USBC", "A1")], 0.30, "F.Cu", "/GND")

    # VSOLAR_IN: J_SOLAR Pad 2 -> D_SOL Pad 2
    routes += make_route([P("J_SOLAR", 2), (95.60, 11.60), P("D_SOL", 2)], 0.60, "F.Cu", "/VSOLAR_IN")

    # /VIN_CHG: D_SOL:1, D_USB:1, L1:1 (RIGHT=97.35), C_CHGIN:1, R_VIN:1, R_CEN:1 (F.Cu only!)
    routes += make_route([P("D_SOL", 1), (97.35, 21.20), P("L1", 1)], 0.70, "F.Cu", "/VIN_CHG")
    routes += make_route([P("D_SOL", 1), (95.60, 20.50), (92.00, 20.50), (92.00, 23.45), P("C_CHGIN", 1)], 0.50, "F.Cu", "/VIN_CHG")
    routes += make_route([P("C_CHGIN", 1), (92.00, 23.45), (92.00, 15.99), P("R_VIN", 1)], 0.35, "F.Cu", "/VIN_CHG")
    routes += make_route([P("D_USB", 1), (91.85, 29.10), (91.43, 28.68), (91.43, 24.78), P("C_CHGIN", 1)], 0.50, "F.Cu", "/VIN_CHG")
    routes += make_route([P("L1", 1), (97.50, 24.40), P("R_CEN", 1)], 0.30, "F.Cu", "/VIN_CHG")

    # /CHG_LX: U_CHG:15-17 -> via (84.95, 33.80) -> B.Cu Y=24.20 -> via (94.20, 24.80) -> L1:2 & C_BST1:1
    lx_mid = P("U_CHG", 16)
    routes += make_route([P("U_CHG", 17), P("U_CHG", 16), P("U_CHG", 15)], 0.25, "F.Cu", "/CHG_LX")
    routes += make_route([lx_mid, (lx_mid[0], 33.80)], 0.45, "F.Cu", "/CHG_LX")
    routes.append(make_via(lx_mid[0], 33.80, "/CHG_LX", size=0.60, drill=0.32))
    routes += make_route([(lx_mid[0], 33.80), (lx_mid[0], 28.05), (88.80, 24.20), (93.60, 24.20), (94.20, 24.80)], 0.60, "B.Cu", "/CHG_LX")
    routes.append(make_via(94.20, 24.80, "/CHG_LX", size=0.65, drill=0.35))
    routes += make_route([(94.20, 24.80), (93.65, 24.25), P("L1", 2)], 0.60, "F.Cu", "/CHG_LX")
    routes += make_route([(94.20, 24.80), (94.45, 25.05), P("C_BST1", 1)], 0.45, "F.Cu", "/CHG_LX")

    # /CHG_BST: C_BST1:2 -> F.Cu (x, 25.50) -> via -> B.Cu -> via (85.95, 33.80) -> U_CHG:14
    ub = P("U_CHG", 14)
    cb2_x = P("C_BST1", 2)[0]
    routes += make_route([P("C_BST1", 2), (cb2_x, 25.50)], 0.25, "F.Cu", "/CHG_BST")
    routes.append(make_via(cb2_x, 25.50, "/CHG_BST", size=0.50, drill=0.30))
    routes += make_route([(cb2_x, 25.50), (89.30, 25.50), (ub[0], 28.85), (ub[0], 33.80)], 0.25, "B.Cu", "/CHG_BST")
    routes.append(make_via(ub[0], 33.80, "/CHG_BST", size=0.50, drill=0.30))
    routes += make_route([(ub[0], 33.80), ub], 0.22, "F.Cu", "/CHG_BST")

    # /CHG_VIN: R_VIN:2 -> C_VIN:1 -> U_CHG:13 (pure F.Cu!)
    uvin = P("U_CHG", 13)
    routes += make_route([P("R_VIN", 2), P("C_VIN", 1)], 0.30, "F.Cu", "/CHG_VIN")
    routes += make_route([P("C_VIN", 1), (87.40, 19.69), (87.40, 34.40), (uvin[0], 34.40), uvin], 0.22, "F.Cu", "/CHG_VIN")

    # /CHG_EN: R_CEN:2 -> F.Cu (97.30, 28.35) -> (92.45, 28.35) -> via -> B.Cu Y=28.60 -> via (87.45, 35.10) -> U_CHG:12
    uen = P("U_CHG", 12)
    routes += make_route([P("R_CEN", 2), (97.30, 28.35), (92.45, 28.35)], 0.22, "F.Cu", "/CHG_EN")
    routes.append(make_via(92.45, 28.35, "/CHG_EN", size=0.50, drill=0.30))
    routes += make_route([(92.45, 28.35), (92.20, 28.60), (87.45, 28.60), (87.45, 35.10)], 0.22, "B.Cu", "/CHG_EN")
    routes.append(make_via(87.45, 35.10, "/CHG_EN", size=0.50, drill=0.30))
    routes += make_route([(87.45, 35.10), (87.45, uen[1]), uen], 0.20, "F.Cu", "/CHG_EN")

    # /CHG_ISET: U_CHG:11 -> R_ISET:1 (runs at X=88.00, clear of DP/DN vias >=89.35 and CHG_EN)
    uiset = P("U_CHG", 11)
    routes += make_route([uiset, (88.00, uiset[1]), (88.00, 32.00), (89.50, 30.50), P("R_ISET", 1)], 0.20, "F.Cu", "/CHG_ISET")

    # /CHG_UV: U_CHG:8 -> straight right past Pin 7 to (88.4125, 38.25) -> down to R_UV:1 (pure F.Cu!)
    uuv = P("U_CHG", 8)
    ruv1 = P("R_UV", 1)
    routes += make_route([uuv, (ruv1[0], uuv[1]), ruv1], 0.20, "F.Cu", "/CHG_UV")

    # /CHG_NTC: U_CHG:4 -> R_NTC:1 (pure F.Cu!)
    untc = P("U_CHG", 4)
    routes += make_route([untc, (untc[0], 41.80), (P("R_NTC", 1)[0], 42.70), P("R_NTC", 1)], 0.20, "F.Cu", "/CHG_NTC")

    # /CHG_LED: U_CHG:6 -> via (86.45, 40.20) -> (86.25, 40.00) -> (82.80, 40.00) -> B.Cu -> via -> R_LED1:1 -> D_CHG:2
    uled = P("U_CHG", 6)
    routes += make_route([uled, (uled[0], 40.20)], 0.20, "F.Cu", "/CHG_LED")
    routes.append(make_via(uled[0], 40.20, "/CHG_LED", size=0.50, drill=0.30))
    routes += make_route([(uled[0], 40.20), (86.25, 40.00), (82.80, 40.00), (82.80, 26.50), (P("R_LED1", 1)[0], 26.50)], 0.22, "B.Cu", "/CHG_LED")
    routes.append(make_via(P("R_LED1", 1)[0], 26.50, "/CHG_LED", size=0.50, drill=0.30))
    routes += make_route([(P("R_LED1", 1)[0], 26.50), P("R_LED1", 1)], 0.22, "F.Cu", "/CHG_LED")
    routes += make_route([P("R_LED1", 2), (P("R_LED1", 2)[0], 25.40), P("D_CHG", 2)], 0.22, "F.Cu", "/CHG_LED_K")

    # /CHG_VSYS: U_CHG:19,20 -> C_SYS1:1 -> C_SYS2:1 (pure F.Cu!)
    routes += make_route([P("U_CHG", 19), P("U_CHG", 20)], 0.30, "F.Cu", "/CHG_VSYS")
    routes += make_route([P("U_CHG", 19), (83.2625, 33.71), (81.40, 31.85), P("C_SYS1", 1), P("C_SYS2", 1)], 0.50, "F.Cu", "/CHG_VSYS")

    # /VBAT_2S: F.Cu around (81.00..82.05) -> via (82.05, 44.20) -> B.Cu (82.05..88.40, 44.20) -> J_LIPO_2S:3 -> F1:1
    routes += make_route([P("U_CHG", 21), (83.2625, 37.45)], 0.25, "F.Cu", "/VBAT_2S")
    routes += make_route([P("U_CHG", 22), (83.2625, 37.45), (82.90, 37.25), (81.00, 37.25)], 0.25, "F.Cu", "/VBAT_2S")
    routes += make_route([(81.00, 37.25), (76.95, 37.25), (75.50, 35.80), (75.50, 31.85), P("C_BAT1", 1)], 0.45, "F.Cu", "/VBAT_2S")
    routes += make_route([(81.00, 37.25), (81.00, 40.50), (82.05, 41.55), (82.05, 44.20)], 0.30, "F.Cu", "/VBAT_2S")
    routes.append(make_via(82.05, 44.20, "/VBAT_2S", size=0.70, drill=0.35))
    routes += make_route([
        (82.05, 44.20), (88.40, 44.20), (88.40, 50.40), P("J_LIPO_2S", 3),
    ], 0.65, "B.Cu", "/VBAT_2S")
    routes += make_route([P("J_LIPO_2S", 3), (89.40, 54.40), P("F1", 1)], 0.65, "F.Cu", "/VBAT_2S")

    # /CHG_VBATM & /VBAT_MID: U_CHG:23 -> via (82.10, 38.60) -> B.Cu Y=42.30 -> via (94.40, 43.90) -> F.Cu R_BAL:1, R_BAL:2 -> J_LIPO_2S:2
    routes += make_route([P("U_CHG", 23), (82.45, 38.25), (82.10, 38.60)], 0.25, "F.Cu", "/CHG_VBATM")
    routes.append(make_via(82.10, 38.60, "/CHG_VBATM", size=0.50, drill=0.30))
    routes += make_route([(82.10, 38.60), (82.10, 42.30), (94.40, 42.30), (94.40, 43.90)], 0.25, "B.Cu", "/CHG_VBATM")
    routes.append(make_via(94.40, 43.90, "/CHG_VBATM", size=0.50, drill=0.30))
    routes += make_route([(94.40, 43.90), (97.60, 43.90), P("R_BAL", 1)], 0.25, "F.Cu", "/CHG_VBATM")
    routes += make_route([P("R_BAL", 2), (97.60, 47.90), P("J_LIPO_2S", 2)], 0.35, "F.Cu", "/VBAT_MID")

    # U_CHG thermal vias & GND stitching vias across all PCB zones (including C_BOUT1:2 island & C_BAT1/SYS!)
    for tx, ty in [
        (84.60, 36.90), (85.80, 36.90), (84.60, 38.10), (85.80, 38.10),
        (89.60, 31.80), (88.80, 22.00), (77.00, 35.00), (79.20, 35.00), (81.40, 35.00),
        (77.20, 24.00), (89.20, 74.00), (96.50, 76.10), (98.75, 79.75), (97.50, 84.50),
        (89.10, 64.50), (97.00, 69.20), (69.80, 91.00),
        (4.00, 62.00), (72.00, 11.00), (89.00, 11.00), (98.50, 96.00), (2.50, 96.00), (30.00, 89.50),
        (2.00, 2.00), (98.50, 2.00), (98.50, 98.50), (2.00, 98.50),
        # U_C6 EPAD 4x thermal vias & local U_C6/LDO/SW_C6_* GND stitching
        (45.50, 74.80), (47.50, 74.80), (45.50, 76.80), (47.50, 76.80),
        (56.60, 83.56), (36.20, 83.56),
        (35.00, 69.30), (35.00, 79.80), (33.20, 89.60),
        (65.20, 66.40), (66.20, 73.80), (75.35, 69.85),
    ]:
        routes.append(make_via(tx, ty, "/GND", size=0.55, drill=0.30))
    routes.append(make_via(89.55, 40.025, "/GND", size=0.50, drill=0.30))
    routes += make_route([(89.55, 40.025), P("R_CC1", 1)], 0.25, "F.Cu", "/GND")
    routes += make_route([(98.75, 79.75), P("C_BOUT1", 2)], 0.30, "F.Cu", "/GND")
    routes += make_route([P("U_C6", 1), (56.60, 83.56), P("C_C6_VDD", 2)], 0.30, "F.Cu", "/GND")
    routes += make_route([P("U_C6", 28), (36.20, 83.56)], 0.30, "F.Cu", "/GND")
    routes += make_route([P("U_LDO", 1), (63.35, 66.40), (65.20, 66.40)], 0.30, "F.Cu", "/GND")
    routes += make_route([P("C_LDO_IN", 2), (65.60, 74.00), (66.20, 73.80)], 0.30, "F.Cu", "/GND")
    routes += make_route([P("C_LDO_OUT", 2), (75.35, 69.85)], 0.30, "F.Cu", "/GND")
    routes += make_route([P("SW_C6_BOOT", 1), (35.00, 69.30)], 0.30, "F.Cu", "/GND")
    routes += make_route([P("C_C6_EN", 2), (35.00, 79.80)], 0.30, "F.Cu", "/GND")
    routes += make_route([P("SW_C6_RST", 1), (33.20, 89.60)], 0.30, "F.Cu", "/GND")

    # /VBAT_FUSED: F1:2 -> Q_PROT:3 (Drain)
    routes += make_route([P("F1", 2), (92.20, 56.66), P("Q_PROT", 3)], 0.65, "F.Cu", "/VBAT_FUSED")

    # /Q_GATE: Q_PROT:1 (Gate) -> R_QG:2
    routes += make_route([P("Q_PROT", 1), (90.00, 59.79), P("R_QG", 2)], 0.25, "F.Cu", "/Q_GATE")

    # /VBAT_PROT: Q_PROT:2 (Source) -> SW_PWR:3 (up X=93.45, clear of F1:2 & BUCK_EN_SW via), J_SW_EXT:1, U_BUCK:5, C_BIN:1
    routes += make_route([P("Q_PROT", 2), (93.45, 58.24), (93.45, 55.75), P("SW_PWR", 3)], 0.45, "F.Cu", "/VBAT_PROT")
    routes += make_route([P("Q_PROT", 2), (93.80, 61.19), P("J_SW_EXT", 1)], 0.50, "F.Cu", "/VBAT_PROT")
    routes += make_route([
        P("Q_PROT", 2), (91.60, 60.09), (91.60, 64.60), (90.20, 64.60), (90.20, 71.80), P("U_BUCK", 5),
    ], 0.45, "F.Cu", "/VBAT_PROT")
    routes += make_route([(90.20, 70.85), P("C_BIN", 1)], 0.45, "F.Cu", "/VBAT_PROT")

    # /BUCK_EN_SW: SW_PWR:2 -> via (94.55, 57.25) -> B.Cu (clears SW_PWR:1!) -> via -> J_SW_EXT:2 -> R_SW_PU:1 & B.Cu -> R_2S_TOP:1
    routes += make_route([P("SW_PWR", 2), (94.55, 57.25)], 0.30, "F.Cu", "/BUCK_EN_SW")
    routes.append(make_via(94.55, 57.25, "/BUCK_EN_SW", size=0.50, drill=0.30))
    routes += make_route([(94.55, 57.25), (94.55, 62.80), (P("J_SW_EXT", 2)[0], 64.59), P("J_SW_EXT", 2)], 0.30, "B.Cu", "/BUCK_EN_SW")
    routes += make_route([P("J_SW_EXT", 2), (96.34, 66.30), (91.49, 66.30), P("R_SW_PU", 1)], 0.25, "F.Cu", "/BUCK_EN_SW")
    routes.append(make_via(93.00, 66.30, "/BUCK_EN_SW", size=0.55, drill=0.30))
    routes += make_route([
        (93.00, 66.30), (93.00, 84.50), (91.70, 85.80), (71.10, 85.80), (69.4125, 86.45),
    ], 0.25, "B.Cu", "/BUCK_EN_SW")
    routes.append(make_via(69.4125, 86.45, "/BUCK_EN_SW", size=0.55, drill=0.30))
    routes += make_route([(69.4125, 86.45), P("R_2S_TOP", 1)], 0.25, "F.Cu", "/BUCK_EN_SW")

    # /BUCK_EN: R_SW_PU:2 -> R_SW_PD:2 & U_BUCK:4 (enters Pin 4 from top so clear of VIN)
    routes += make_route([P("R_SW_PU", 2), P("R_SW_PD", 2)], 0.25, "F.Cu", "/BUCK_EN")
    routes += make_route([P("R_SW_PU", 2), (P("R_SW_PU", 2)[0], 69.60), (P("U_BUCK", 4)[0], 69.60), P("U_BUCK", 4)], 0.22, "F.Cu", "/BUCK_EN")

    # /BUCK_LX: U_BUCK:6 -> C_BST2:2 & L2:1
    routes += make_route([P("U_BUCK", 6), (90.15, 74.46), P("L2", 1)], 0.65, "F.Cu", "/BUCK_LX")
    routes += make_route([(90.15, 75.00), P("C_BST2", 2)], 0.30, "F.Cu", "/BUCK_LX")

    # /BUCK_BST: U_BUCK:1 -> C_BST2:1
    routes += make_route([P("U_BUCK", 1), (P("C_BST2", 1)[0], 72.94), P("C_BST2", 1)], 0.25, "F.Cu", "/BUCK_BST")

    # /BUCK_FB: U_BUCK:3 -> R_FB1:2 & R_FB2:1
    routes += make_route([P("U_BUCK", 3), (95.90, 70.85), (97.20, 72.15), P("R_FB1", 2), P("R_FB2", 1)], 0.22, "F.Cu", "/BUCK_FB")

    # /+5V MAIN POWER BUS (0.70mm):
    routes += make_route([P("L2", 2), (95.40, 79.20), (96.75, 77.85), P("C_BOUT1", 1)], 0.60, "F.Cu", "/+5V")
    routes += make_route([P("C_BOUT1", 1), (99.10, 76.25), (99.10, 70.29), P("R_FB1", 1)], 0.30, "F.Cu", "/+5V")
    routes += make_route([(95.40, 79.20), (95.40, 81.65), P("C_BOUT2", 1)], 0.60, "F.Cu", "/+5V")
    routes += make_route([P("C_BOUT2", 1), (98.90, 83.05), (98.90, 85.94), P("J_PWR_ESP", 2)], 0.70, "F.Cu", "/+5V")
    routes += make_route([(95.40, 81.65), (90.9125, 81.65), P("R_LED2", 1)], 0.30, "F.Cu", "/+5V")
    routes += make_route([P("R_LED2", 2), (P("R_LED2", 2)[0], 85.10), (P("D_5V", 2)[0], 85.10), P("D_5V", 2)], 0.25, "F.Cu", "/LED_5V_A")

    # /+5V West distribution into D_LDO_5V (stopped well outside U_C6 antenna keepout)
    routes += make_route([
        (95.40, 81.65), (88.00, 81.65), (86.70, 82.95), (75.50, 82.95),
        (75.50, 76.75), P("D_LDO_5V", 2),
    ], 0.65, "F.Cu", "/+5V")

    # /+5V North highway along B.Cu (jogs right to X=98.35 past J_SW_EXT:2 then X=95.70 clear of J_USBC & SW_PWR NPTH)
    routes.append(make_via(95.40, 81.65, "/+5V", size=0.90, drill=0.50))
    routes += make_route([
        (95.40, 81.65), (98.35, 78.70), (98.35, 63.00), (95.70, 60.35),
        (95.70, 17.50), (94.70, 16.50), (88.00, 16.50),
    ], 0.65, "B.Cu", "/+5V")
    routes.append(make_via(88.00, 16.50, "/+5V", size=0.90, drill=0.50))
    routes += make_route([(88.00, 16.50), (71.32, 16.50), (68.32, 13.50)], 0.65, "F.Cu", "/+5V")

    # 9. /LDO_VIN, /C6_BOOT, /C6_IO8, /C6_EN & LoRa SX1278 SPI (U_C6:4..8 -> J3:4..8)
    routes += make_route([
        P("D_LDO_USB", 1), (71.60, 66.55), (71.60, 74.85), P("D_LDO_5V", 1),
        (70.00, 76.45), P("C_LDO_IN", 1), (62.40, 76.45), (62.40, 72.30), P("U_LDO", 3),
    ], 0.35, "F.Cu", "/LDO_VIN")

    routes += make_route([P("U_C6", 15), (36.80, 67.05), (34.85, 65.10), P("SW_C6_BOOT", 2)], 0.25, "F.Cu", "/C6_BOOT")
    routes += make_route([P("U_C6", 10), (56.80, 72.13), (57.75, 73.08), P("R_IO8_PU", 1)], 0.25, "F.Cu", "/C6_IO8")

    routes += make_route([
        P("R_C6_EN", 1), (56.60, 78.55), (56.60, 81.02), P("U_C6", 3), (53.60, 81.02),
    ], 0.25, "F.Cu", "/C6_EN")
    routes.append(make_via(53.60, 81.02, "/C6_EN", size=0.55, drill=0.30))
    routes += make_route([(53.60, 81.02), (53.40, 81.20), (34.80, 81.20)], 0.25, "B.Cu", "/C6_EN")
    routes.append(make_via(34.80, 81.20, "/C6_EN", size=0.55, drill=0.30))
    routes += make_route([(34.80, 81.20), (33.20, 82.15), P("C_C6_EN", 1), P("SW_C6_RST", 2)], 0.25, "F.Cu", "/C6_EN")

    # LoRa SPI + DIO0 on B.Cu: MISO/SCK above Y=67.5, DIO0 through J3 row gap Y=73.23, MOSI Y=75.94, NSS->F.Cu Y=77.21
    routes += make_route([P("U_C6", 8), (53.80, 74.67)], 0.25, "F.Cu", "/LORA_DIO0")
    routes.append(make_via(53.80, 74.67, "/LORA_DIO0", size=0.50, drill=0.30))
    routes += make_route([
        (53.80, 74.67), (56.00, 73.23), (84.73, 73.23), P("J3", 8),
    ], 0.25, "B.Cu", "/LORA_DIO0")

    routes += make_route([P("U_C6", 7), (53.80, 75.94)], 0.25, "F.Cu", "/LORA_MOSI")
    routes.append(make_via(53.80, 75.94, "/LORA_MOSI", size=0.50, drill=0.30))
    routes += make_route([
        (53.80, 75.94), (82.02, 75.94), P("J3", 6),
    ], 0.25, "B.Cu", "/LORA_MOSI")

    routes += make_route([P("U_C6", 6), (53.80, 77.21)], 0.25, "F.Cu", "/LORA_NSS")
    routes.append(make_via(53.80, 77.21, "/LORA_NSS", size=0.50, drill=0.30))
    routes += make_route([(53.80, 77.21), (77.60, 77.21)], 0.25, "B.Cu", "/LORA_NSS")
    routes.append(make_via(77.60, 77.21, "/LORA_NSS", size=0.50, drill=0.30))
    routes += make_route([(77.60, 77.21), (78.21, 77.21), P("J3", 4)], 0.25, "F.Cu", "/LORA_NSS")

    routes += make_route([P("U_C6", 5), (53.80, 78.48)], 0.25, "F.Cu", "/LORA_SCK")
    routes.append(make_via(53.80, 78.48, "/LORA_SCK", size=0.50, drill=0.30))
    routes += make_route([
        (53.80, 78.48), (52.00, 78.48), (52.00, 69.00), (81.80, 69.00), (83.46, 70.66), P("J3", 5),
    ], 0.25, "B.Cu", "/LORA_SCK")

    routes += make_route([P("U_C6", 4), (53.80, 79.75)], 0.25, "F.Cu", "/LORA_MISO")
    routes.append(make_via(53.80, 79.75, "/LORA_MISO", size=0.50, drill=0.30))
    routes += make_route([
        (53.80, 79.75), (51.40, 79.75), (51.40, 68.40), (84.20, 68.40), (86.00, 70.20), P("J3", 7),
    ], 0.25, "B.Cu", "/LORA_MISO")

    # Optional 2S ADC Monitor Divider (/ADC_2S_DIV)
    routes += make_route([P("R_2S_TOP", 2), P("R_2S_BOT", 2)], 0.25, "F.Cu", "/ADC_2S_DIV")
    routes += make_route([P("R_2S_TOP", 2), P("JP_2S_ADC", 1)], 0.25, "F.Cu", "/ADC_2S_DIV")

    new_blocks.extend(routes)

    # ==========================================================================
    # F. DOUBLE-SIDED SILKSCREEN (F.SilkS + B.SilkS) & SOLID GND BARS
    # ==========================================================================
    silk = []

    # Titles (both sides)
    silk.append(make_silk_text("WINDDRAGONS DUAL ESP32-C6 CARRIER V2.0", 48.0, 2.5, 0, "F.SilkS", 1.10, 0.18, True))
    silk.append(make_silk_text("FRONT PCBA: C6-WROOM-1, JOY1, 2S USB-C/SOLAR & ACTUATORS", 48.0, 5.2, 0, "F.SilkS", 0.65, 0.12, False))
    silk.append(make_silk_text("WINDDRAGONS DUAL ESP32-C6 CARRIER V2.0", 48.0, 2.5, 0, "B.SilkS", 1.10, 0.18, True))
    silk.append(make_silk_text("BACK: LAFVIN LCD, LORA 433MHz, GPS & IMU", 48.0, 5.2, 0, "B.SilkS", 0.70, 0.13, False))

    # --- 1. Mirror core board outlines & labels onto BOTH faces ---
    # Lafvin outline + J1/J2 labels on BOTH F.SilkS & B.SilkS
    silk.append(make_silk_text("LAFVIN ESP32-C6 LCD 1.47", 63.0, 22.5, 0, "F.SilkS", 0.75, 0.13, True))
    silk.append(make_silk_text("LAFVIN ESP32-C6 (MASTER)", 63.0, 22.5, 0, "B.SilkS", 0.75, 0.13, True))
    silk.append(make_silk_rect(43.0, 12.0, 89.0, 33.0, "B.SilkS", 0.18, False))
    silk.append(make_silk_text("5V  GND 3V3 IO0 IO1 IO2 IO3 IO4 IO5", 58.16, 11.1, 0, "F.SilkS", 0.55, 0.10, False))
    silk.append(make_silk_text("5V  GND 3V3 IO0 IO1 IO2 IO3 IO4 IO5", 58.16, 11.1, 0, "B.SilkS", 0.55, 0.10, False))
    silk.append(make_silk_text("BOOT 18 19  20  JSW 12  13  RX  TX", 58.16, 33.8, 0, "B.SilkS", 0.55, 0.10, False))
    silk.extend(make_gnd_bar_both(65.78, 15.1))  # J1 Pad 8 GND

    # Onboard ESP32-C6-WROOM-1-N8 + 2x ADS1115 + LDO + Boot/Reset labels
    silk.append(make_silk_text("2x ADS1115 CELL MON", 47.0, 46.2, 0, "F.SilkS", 0.52, 0.10, True))
    silk.append(make_silk_text("ONBOARD ESP32-C6-WROOM-1", 46.5, 76.0, 0, "B.SilkS", 0.62, 0.11, True))
    silk.append(make_silk_rect(37.5, 65.55, 55.5, 91.05, "B.SilkS", 0.15, False))
    silk.append(make_silk_rect(37.5, 84.85, 55.5, 91.05, "B.SilkS", 0.15, False))
    silk.append(make_silk_text("ANTENNA KEEPOUT", 46.5, 88.0, 0, "B.SilkS", 0.55, 0.10, True))
    silk.append(make_silk_text("ANTENNA", 46.5, 88.0, 0, "F.SilkS", 0.55, 0.10, True))
    silk.append(make_silk_text("C6 BOOT", 30.6, 67.2, 90, "F.SilkS", 0.48, 0.09, True))
    silk.append(make_silk_text("C6 RST", 30.6, 86.2, 90, "F.SilkS", 0.48, 0.09, True))
    silk.append(make_silk_text("3V3 LDO", 66.5, 70.0, 0, "B.SilkS", 0.52, 0.10, True))

    # 3S-6S Motor LiPo 90-deg Right-Angle Balance Header J_BAL_MOT on Left Edge (F.SilkS + B.SilkS)
    for lyr in ("F.SilkS", "B.SilkS"):
        silk.append(make_silk_rect(1.20, 41.60, 7.15, 60.55, lyr, 0.15, False))
        silk.append(make_silk_text("LIPO BAL 3S-6S", 4.50, 40.60, 0, lyr, 0.50, 0.10, True))
        for lbl, py, is_gnd in [
            ("GND", 58.62, True), ("1S", 56.08, False), ("2S", 53.54, False),
            ("3S", 51.00, False), ("4S", 48.46, False), ("5S", 45.92, False), ("6S", 43.38, False),
        ]:
            silk.append(make_silk_text(lbl, 6.25, py, 0, lyr, 0.46, 0.10, is_gnd))
    silk.extend(make_gnd_bar_both(4.40, 60.05))

    # Quectel LC29H GPS mirrored onto F.SilkS + GND bar
    silk.append(make_silk_rect(5.0, 6.5, 32.0, 37.5, "F.SilkS", 0.18, False))
    silk.append(make_silk_text("QUECTEL LC29H GPS (BACK)", 18.5, 8.2, 0, "F.SilkS", 0.75, 0.13, True))
    silk.append(make_silk_text("P  T2 R2 R1 T1  G  V", 19.5, 10.6, 0, "F.SilkS", 0.65, 0.11, False))
    silk.append(make_silk_text("QUECTEL LC29H (BACK)", 18.5, 7.8, 0, "B.SilkS", 0.80, 0.14, True))
    silk.extend(make_gnd_bar_both(24.58, 14.3))   # J6 Pad 6 GND

    # SX1278 LoRa mirrored onto F.SilkS + GND bar
    silk.append(make_silk_rect(73.5, 66.96, 91.5, 86.5, "F.SilkS", 0.15, False))
    silk.append(make_silk_text("LORA SX1278 (BACK)", 81.0, 85.2, 0, "F.SilkS", 0.65, 0.12, True))
    silk.append(make_silk_text("SX1278 433MHz", 81.0, 85.2, 0, "B.SilkS", 0.75, 0.13, True))
    silk.extend(make_gnd_bar_both(78.38, 70.3))   # J3 Pad 1 GND

    # Joystick & Buttons mirrored onto B.SilkS
    silk.append(make_silk_text("PS5 JOYSTICK", 72.75, 38.6, 0, "F.SilkS", 0.75, 0.13, True))
    silk.append(make_silk_text("PS5 JOYSTICK (FRONT)", 72.75, 40.5, 0, "B.SilkS", 0.75, 0.13, True))
    silk.append(make_silk_text("SW1:IO18 SW2:IO19 SW3:IO20 SW4:IO9", 16.4, 61.5, 0, "B.SilkS", 0.60, 0.11, False))

    # Dedicated GY-BNO055 9-DOF IMU JST-XH 4P connector (F.SilkS + B.SilkS + GND bar)
    silk.append(make_silk_rect(60.64, 78.04, 73.26, 84.01, "B.SilkS", 0.15, False))
    for lyr in ("F.SilkS", "B.SilkS"):
        silk.append(make_silk_text("IMU BNO055", 66.95, 75.85, 0, lyr, 0.58, 0.11, True))
        for lbl, px in [("GND", 63.20), ("3V3", 65.70), ("SDA", 68.20), ("SCL", 70.70)]:
            silk.append(make_silk_text(lbl, px, 77.15, 0, lyr, 0.50, 0.10, lbl == "GND"))
    silk.extend(make_gnd_bar_both(63.20, 82.25))

    # --- 2. FOOLPROOF SEPARATED ACTUATORS vs SENSORS BOXES (Both Sides) ---
    # Box 1: RC SERVOS (5V BEC): X=2.8..17.5, Y=64.0..76.8
    for lyr in ("F.SilkS", "B.SilkS"):
        silk.append(make_silk_rect(2.8, 64.0, 17.5, 76.8, lyr, 0.15, False))
        silk.append(make_silk_text("RC SERVOS (5V BEC)", 10.15, 65.2, 0, lyr, 0.60, 0.11, True))
        silk.append(make_silk_text("SIG", 4.1, 69.42, 0, lyr, 0.52, 0.10, False))
        silk.append(make_silk_text("+5V", 4.1, 71.96, 0, lyr, 0.52, 0.10, False))
        silk.append(make_silk_text("GND", 4.1, 74.50, 0, lyr, 0.52, 0.11, True))
        for nm, sx in [("ESC", 6.00), ("RUD", 8.54), ("WIN", 11.08), ("RCK", 13.62), ("STRB", 16.16)]:
            silk.append(make_silk_text(nm, sx, 67.3, 90, lyr, 0.55, 0.11, True))
    # Solid white GND bar under all 5 servo GND pads
    for lyr in ("F.SilkS", "B.SilkS"):
        silk.append(make_silk_rect(5.2, 75.65, 16.9, 76.10, lyr, 0.05, True))

    # Box 2: SENSORS (IO): X=18.0..30.0, Y=64.0..76.8
    for lyr in ("F.SilkS", "B.SilkS"):
        silk.append(make_silk_rect(18.0, 64.0, 30.0, 76.8, lyr, 0.18, False))
        silk.append(make_silk_text("SENSORS (IO)", 24.0, 65.2, 0, lyr, 0.56, 0.11, True))
        silk.append(make_silk_text("SIG", 19.2, 71.96, 0, lyr, 0.48, 0.10, False))
        silk.append(make_silk_text("GND", 19.2, 74.50, 0, lyr, 0.48, 0.10, True))
        silk.append(make_silk_text("WTR", 21.50, 67.5, 0, lyr, 0.52, 0.11, True))
        silk.append(make_silk_text("LIM", 26.80, 67.5, 0, lyr, 0.52, 0.11, True))
        silk.append(make_silk_rect(20.4, 75.65, 27.9, 76.10, lyr, 0.05, True))

    # Box 3: BEC 5V IN & WINCH HUB 6P (Y=81.5..88.8)
    for lyr in ("F.SilkS", "B.SilkS"):
        silk.append(make_silk_rect(3.2, 81.5, 10.0, 88.8, lyr, 0.15, False))
        silk.append(make_silk_text("BEC IN", 6.6, 82.6, 0, lyr, 0.58, 0.11, True))
        silk.append(make_silk_text("+5V", 4.96, 87.2, 0, lyr, 0.55, 0.11, False))
        silk.append(make_silk_text("GND", 7.50, 87.2, 0, lyr, 0.55, 0.11, True))
        silk.append(make_silk_rect(12.2, 81.5, 29.5, 88.8, lyr, 0.18, False))
        silk.append(make_silk_text("WINCH + HALL 6P HUB", 20.85, 82.6, 0, lyr, 0.62, 0.12, True))
        for lbl, px in [("GND", 14.50), ("5V", 17.04), ("3V3", 19.58), ("HAL", 22.12), ("WIN", 24.66), ("RCK", 27.20)]:
            silk.append(make_silk_text(lbl, px, 87.2, 0, lyr, 0.50, 0.10, lbl == "GND"))
    silk.extend(make_gnd_bar_both(7.50, 85.9))
    silk.extend(make_gnd_bar_both(14.50, 85.9))

    # --- 3. EXTENDED BREAKAWAY WINCH+HALL SUB-BOARD SILKSCREEN (Both Sides) ---
    for lyr in ("F.SilkS", "B.SilkS"):
        silk.append(make_silk_text("WINCH+HALL SUB", 22.4, 93.5, 0, lyr, 0.52, 0.10, True))
        for lbl, px in [("G", 16.00), ("5V", 18.54), ("3V", 21.08), ("H", 23.62), ("W", 26.16), ("R", 28.70)]:
            silk.append(make_silk_text(lbl, px, 98.3, 0, lyr, 0.50, 0.10, lbl == "G"))
        silk.append(make_silk_text("SRV_WIN", 37.24, 93.6, 0, lyr, 0.52, 0.10, True))
        for lbl, px in [("G", 34.70), ("5V", 37.24), ("W", 39.78)]:
            silk.append(make_silk_text(lbl, px, 98.3, 0, lyr, 0.50, 0.10, lbl == "G"))
        silk.append(make_silk_text("SRV_RCK", 46.14, 93.6, 0, lyr, 0.52, 0.10, True))
        for lbl, px in [("G", 43.60), ("5V", 46.14), ("R", 48.68)]:
            silk.append(make_silk_text(lbl, px, 98.3, 0, lyr, 0.50, 0.10, lbl == "G"))
        silk.append(make_silk_text("M3", 54.0, 93.6, 0, lyr, 0.52, 0.10, True))
        silk.append(make_silk_text("ANCHOR HALL", 65.0, 95.2, 0, lyr, 0.58, 0.11, True))
        silk.append(make_silk_text("1:3V3 2:OUT 3:GND", 65.0, 97.4, 0, lyr, 0.48, 0.10, False))
        silk.append(make_silk_text("3V3", 82.6, 95.1, 0, lyr, 0.48, 0.10, False))
        silk.append(make_silk_text("OUT", 82.6, 97.8, 0, lyr, 0.48, 0.10, False))
        silk.append(make_silk_text("GND", 87.8, 94.3, 0, lyr, 0.50, 0.10, True))
    silk.extend(make_gnd_bar_both(16.00, 94.9))
    silk.extend(make_gnd_bar_both(34.70, 94.9))
    silk.extend(make_gnd_bar_both(43.60, 94.9))
    silk.extend(make_gnd_bar_both(87.60, 97.9))  # U_HALL Pin 3 GND polarity bar

    # --- 4. 2S CHARGER, SOLAR, LIPO & 5V BUCK SILKSCREEN (Both Sides) ---
    for lyr in ("F.SilkS", "B.SilkS"):
        silk.append(make_silk_text("SOLAR 5-6V", 93.75, 7.2, 0, lyr, 0.60, 0.11, True))
        silk.append(make_silk_text("-", 92.50, 14.3, 0, lyr, 0.65, 0.12, True))
        silk.append(make_silk_text("+", 95.00, 14.3, 0, lyr, 0.65, 0.12, True))
        silk.append(make_silk_text("USB-C 5V", 97.5, 42.1, 0, lyr, 0.48, 0.10, True))
        silk.append(make_silk_text("2S LIPO", 92.5, 42.1, 0, lyr, 0.54, 0.11, True))
        silk.append(make_silk_text("GND", 89.5, 45.4, 0, lyr, 0.52, 0.11, True))
        silk.append(make_silk_text("4.2", 89.5, 47.9, 0, lyr, 0.50, 0.10, False))
        silk.append(make_silk_text("8.4", 89.5, 50.4, 0, lyr, 0.52, 0.11, True))
        silk.append(make_silk_text("ON", 97.8, 53.0, 0, lyr, 0.58, 0.11, True))
        silk.append(make_silk_text("OFF", 97.8, 62.6, 0, lyr, 0.58, 0.11, True))
        silk.append(make_silk_text("EXT SW", 95.1, 66.2, 0, lyr, 0.50, 0.10, False))
        silk.append(make_silk_text("5V AUX", 96.3, 85.8, 0, lyr, 0.54, 0.10, True))
        silk.append(make_silk_text("G  +5V", 95.8, 89.6, 0, lyr, 0.55, 0.11, True))
        silk.append(make_silk_text("2S ADC", 64.2, 89.6, 0, lyr, 0.52, 0.10, True))
    silk.append(make_silk_text("CHG", 79.0, 23.8, 0, "F.SilkS", 0.55, 0.11, True))
    silk.append(make_silk_text("5V OK", 91.8, 85.2, 0, "F.SilkS", 0.50, 0.10, True))
    silk.extend(make_gnd_bar_both(92.50, 9.2))   # J_SOLAR Pad 1 GND
    silk.extend(make_gnd_bar_both(91.20, 45.4))  # J_LIPO_2S Pad 1 GND
    silk.extend(make_gnd_bar_both(94.50, 89.2))  # J_PWR_ESP Pad 1 GND

    new_blocks.extend(silk)

    # ==========================================================================
    # G. COPPER ZONES (Main Board /GND above slot + Isolated Sub-Board /HALL_GND)
    # ==========================================================================
    main_zone_pts = [
        (0.5, 0.5), (99.6, 0.5), (99.6, 99.6), (92.0, 99.6),
        (92.0, 91.0), (12.0, 91.0), (12.0, 99.6), (0.5, 99.6),
    ]
    sub_zone_pts = [
        (14.4, 93.2), (89.6, 93.2), (89.6, 99.6), (14.4, 99.6),
    ]
    for lyr in ("F.Cu", "B.Cu"):
        new_blocks.append(make_zone("/GND", lyr, main_zone_pts))
        new_blocks.append(make_zone("/HALL_GND", lyr, sub_zone_pts))
    # RF Keepout Zone directly under ESP32-C6-WROOM-1 PCB Antenna (F.Cu & B.Cu)
    new_blocks.append(make_keepout_zone([
        (37.20, 84.85), (55.80, 84.85), (55.80, 91.30), (37.20, 91.30),
    ]))

    pcb_content = header + "\n" + "\n".join(kept_blocks + new_blocks) + "\n)\n"
    with open(PCB_PATH, "w") as f:
        f.write(pcb_content)
    print(f"Wrote {PCB_PATH}")

    # Upgrade & run DRC with zone refill
    subprocess.run([KICAD_CLI, "pcb", "upgrade", "--force", PCB_PATH], capture_output=True)
    drc_path = os.path.join(BASE_DIR, "drc_report.txt")
    res = subprocess.run(
        [KICAD_CLI, "pcb", "drc", "--refill-zones", "--save-board", "-o", drc_path, PCB_PATH],
        capture_output=True, text=True,
    )
    print("DRC exit:", res.returncode)

    with open(drc_path, "r") as f:
        drc_txt = f.read()

    err_count = sum(1 for ln in drc_txt.splitlines() if ln.strip().endswith("; error"))
    warn_count = sum(1 for ln in drc_txt.splitlines() if ln.strip().endswith("; warning"))
    print(f"DRC summary: {err_count} errors, {warn_count} warnings")
    print(drc_txt[:4000])
    if len(drc_txt) > 4000:
        print("... (see full drc_report.txt)")

    if err_count == 0:
        export_manufacturing_and_renders()


def export_manufacturing_and_renders():
    """Export JLCPCB Gerbers, Drill, single-sided Top-SMT BOM/CPL, and 3D renders."""
    os.makedirs(GERBER_DIR, exist_ok=True)
    os.makedirs(PCBA_DIR, exist_ok=True)

    # Clear old gerber files
    for fn in os.listdir(GERBER_DIR):
        os.remove(os.path.join(GERBER_DIR, fn))

    subprocess.run([
        KICAD_CLI, "pcb", "export", "gerbers",
        "--board-plot-params", "--subtract-soldermask",
        "--output", GERBER_DIR + "/", PCB_PATH,
    ], check=True)
    subprocess.run([
        KICAD_CLI, "pcb", "export", "drill",
        "--excellon-separate-th", "--generate-map", "--map-format", "gerberx2",
        "--output", GERBER_DIR + "/", PCB_PATH,
    ], check=True)

    zip_path = os.path.join(PCBA_DIR, f"{PROJECT_NAME}_Gerber.zip")
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        for fn in sorted(os.listdir(GERBER_DIR)):
            zf.write(os.path.join(GERBER_DIR, fn), arcname=fn)
    print(f"Exported Gerber zip: {zip_path}")

    # Export Top-side SMD POS and append JLCPCB THT JOY1 (Alps RKJXV122400R) into CPL.csv
    raw_pos = os.path.join(PCBA_DIR, "_raw_pos.csv")
    subprocess.run([
        KICAD_CLI, "pcb", "export", "pos",
        "--side", "front", "--smd-only", "--format", "csv", "--units", "mm",
        "--output", raw_pos, PCB_PATH,
    ], check=True)
    raw_pos_all = os.path.join(PCBA_DIR, "_raw_pos_all.csv")
    subprocess.run([
        KICAD_CLI, "pcb", "export", "pos",
        "--side", "front", "--format", "csv", "--units", "mm",
        "--output", raw_pos_all, PCB_PATH,
    ], check=True)

    cpl_rows = []
    cpl_refs = set()
    for pos_file, allowed_refs in [(raw_pos, None), (raw_pos_all, {"JOY1"})]:
        with open(pos_file, "r", newline="") as f:
            reader = csv.DictReader(f)
            for row in reader:
                ref = row["Ref"].strip()
                if allowed_refs is not None and ref not in allowed_refs:
                    continue
                if ref in {"JP_2S_ADC", "H_HALL", "STAMP"}:
                    continue
                cpl_refs.add(ref)
                cpl_rows.append({
                    "Designator": ref,
                    "Mid X": f"{float(row['PosX']):.4f}mm",
                    "Mid Y": f"{float(row['PosY']):.4f}mm",
                    "Layer": "Top",
                    "Rotation": f"{float(row['Rot']) % 360.0:.1f}",
                })
        os.remove(pos_file)

    cpl_path = os.path.join(PCBA_DIR, f"{PROJECT_NAME}_CPL.csv")
    with open(cpl_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
        writer.writeheader()
        writer.writerows(sorted(cpl_rows, key=lambda r: r["Designator"]))
    print(f"Exported CPL ({len(cpl_rows)} parts): {cpl_path}")

    # Build grouped JLCPCB BOM.csv including V1 SW1..SW4 + THT JOY1 + all BOM_ITEMS
    all_bom = list(BOM_ITEMS) + [
        (f"SW{i}", "SKRKAEE020", "SKRKAEE020", "C115357") for i in range(1, 5)
    ] + [
        ("JOY1", "RKJXV122400R", "Alps_RKJXV1224005", "C918854"),
    ]
    bom_refs = {item[0] for item in all_bom}
    assert bom_refs == cpl_refs, f"BOM/CPL mismatch! BOM-only={bom_refs - cpl_refs}, CPL-only={cpl_refs - bom_refs}"

    grouped = {}
    for ref, val, fp_name, lcsc in all_bom:
        key = (val, fp_name, lcsc)
        grouped.setdefault(key, []).append(ref)

    bom_path = os.path.join(PCBA_DIR, f"{PROJECT_NAME}_BOM.csv")
    with open(bom_path, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["Comment", "Designator", "Footprint", "LCSC Part #"])
        for (val, fp_name, lcsc), refs in sorted(grouped.items(), key=lambda x: sorted(x[1])[0]):
            writer.writerow([val, ",".join(sorted(refs)), fp_name, lcsc])
    print(f"Exported BOM ({len(bom_refs)} parts in {len(grouped)} lines): {bom_path}")

    # 3D Top and Bottom Renders (writable KICAD_CONFIG_HOME + explicit 3DMODEL_DIR so all STEP models render)
    render_cfg = "/tmp/kicad_render_cfg"
    os.makedirs(render_cfg, exist_ok=True)
    render_env = dict(os.environ, KICAD_CONFIG_HOME=render_cfg)
    for side in ("top", "bottom"):
        out_png = os.path.join(BASE_DIR, f"{PROJECT_NAME}_3d_{side}.png")
        subprocess.run([
            KICAD_CLI, "pcb", "render", PCB_PATH,
            "-o", out_png, "--side", side,
            "--quality", "high", "--preset", "follow_pcb_editor",
            "-D", "KICAD10_3DMODEL_DIR=/Applications/KiCad/KiCad.app/Contents/SharedSupport/3dmodels",
            "--width", "2000", "--height", "2000", "--zoom", "0.95",
        ], check=True, env=render_env)
        print(f"Rendered 3D {side}: {out_png}")


if __name__ == "__main__":
    main()
