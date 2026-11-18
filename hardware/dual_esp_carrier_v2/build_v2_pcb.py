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


def make_route(pts, width, layer, net, chamfer=0.55):
    """Emit segments for a polyline with automatic 45-deg corner chamfering."""
    if len(pts) < 2:
        return []

    # 1. Deduplicate consecutive coincident vertices
    dedup = [pts[0]]
    for pt in pts[1:]:
        if math.hypot(pt[0] - dedup[-1][0], pt[1] - dedup[-1][1]) >= 1e-4:
            dedup.append(pt)

    # 2. Collapse same-direction collinear points and reject 180-deg foldbacks
    clean = [dedup[0]]
    for pt in dedup[1:]:
        while len(clean) >= 2:
            ax, ay = clean[-2]
            bx, by = clean[-1]
            dx1, dy1 = bx - ax, by - ay
            dx2, dy2 = pt[0] - bx, pt[1] - by
            l1 = math.hypot(dx1, dy1)
            l2 = math.hypot(dx2, dy2)
            if l1 < 1e-4 or l2 < 1e-4:
                break
            cross = dx1 * dy2 - dy1 * dx2
            dot = dx1 * dx2 + dy1 * dy2
            if abs(cross) <= 1e-5 * l1 * l2:
                if dot > 0:
                    clean.pop()
                    continue
                raise ValueError(f"180-deg foldback detected on net {net} ({layer}) near ({bx:.3f}, {by:.3f})")
            break
        clean.append(pt)

    if len(clean) < 2:
        return []

    # 3. Chamfer corners at 45 deg whenever turn angle exceeds ~49 deg (cos_turn < 0.65)
    out_pts = [clean[0]]
    n = len(clean)
    for i in range(1, n - 1):
        ax, ay = clean[i - 1]
        bx, by = clean[i]
        cx, cy = clean[i + 1]
        dx1, dy1 = bx - ax, by - ay
        dx2, dy2 = cx - bx, cy - by
        l1 = math.hypot(dx1, dy1)
        l2 = math.hypot(dx2, dy2)
        u1x, u1y = dx1 / l1, dy1 / l1
        u2x, u2y = dx2 / l2, dy2 / l2
        cos_turn = u1x * u2x + u1y * u2y
        if cos_turn < -0.05:
            raise ValueError(
                f"Acute bend detected on net {net} ({layer}) near ({bx:.3f}, {by:.3f}): cos_turn={cos_turn:.3f}"
            )
        if cos_turn < 0.65:
            lim1 = (0.90 if i == 1 else 0.45) * l1
            lim2 = (0.90 if i == n - 2 else 0.45) * l2
            c = min(chamfer, lim1, lim2)
            if c > 1e-3:
                out_pts.append((bx - c * u1x, by - c * u1y))
                out_pts.append((bx + c * u2x, by + c * u2y))
                continue
        out_pts.append((bx, by))
    out_pts.append(clean[-1])

    final_pts = [out_pts[0]]
    for pt in out_pts[1:]:
        if math.hypot(pt[0] - final_pts[-1][0], pt[1] - final_pts[-1][1]) >= 1e-4:
            final_pts.append(pt)

    segs = []
    for i in range(len(final_pts) - 1):
        segs.append(make_seg(final_pts[i][0], final_pts[i][1], final_pts[i + 1][0], final_pts[i + 1][1], width, layer, net))
    return segs


def record_footprint_pads(fp_blk):
    """Extract and register global (x, y) pad centers for a kept V1 footprint."""
    ref = get_fp_ref(fp_blk)
    if not ref:
        return
    am = re.search(r'\n\t\t\(at\s+([0-9.-]+)\s+([0-9.-]+)(?:\s+([0-9.-]+))?\)', fp_blk)
    if not am:
        return
    fx, fy = float(am.group(1)), float(am.group(2))
    frot = float(am.group(3)) if am.group(3) else 0.0
    rad = math.radians(frot)
    cs, sn = math.cos(rad), math.sin(rad)
    idx = 0
    while True:
        pp = fp_blk.find('\t\t(pad "', idx)
        if pp == -1:
            break
        pend = fp_blk.find('\n\t\t)', pp)
        if pend == -1:
            break
        pblk = fp_blk[pp:pend + 4]
        idx = pend + 4
        pm = re.match(r'\t\t\(pad\s+"([^"]*)"', pblk)
        if not pm or not pm.group(1):
            continue
        pnum = pm.group(1)
        pam = re.search(r'\(at\s+([0-9.-]+)\s+([0-9.-]+)', pblk)
        if not pam:
            continue
        px, py = float(pam.group(1)), float(pam.group(2))
        gx = fx + px * cs + py * sn
        gy = fy - px * sn + py * cs
        PAD_POS[(ref, pnum)] = (gx, gy)


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
        {"filename": f"{PROJECT_NAME}.kicad_sch", "name": PROJECT_NAME, "uuid": "a0000000-0000-0000-0000-000000000002"}
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


def _pt_seg_dist(px, py, x1, y1, x2, y2):
    dx, dy = x2 - x1, y2 - y1
    l2 = dx * dx + dy * dy
    if l2 < 1e-10:
        return math.hypot(px - x1, py - y1)
    t = max(0.0, min(1.0, ((px - x1) * dx + (py - y1) * dy) / l2))
    return math.hypot(px - (x1 + t * dx), py - (y1 + t * dy))


def _seg_seg_dist(ax1, ay1, ax2, ay2, bx1, by1, bx2, by2):
    def _ccw(x1, y1, x2, y2, x3, y3):
        return (x2 - x1) * (y3 - y1) - (y2 - y1) * (x3 - x1)

    d1, d2 = _ccw(ax1, ay1, ax2, ay2, bx1, by1), _ccw(ax1, ay1, ax2, ay2, bx2, by2)
    d3, d4 = _ccw(bx1, by1, bx2, by2, ax1, ay1), _ccw(bx1, by1, bx2, by2, ax2, ay2)
    if ((d1 > 0 and d2 < 0) or (d1 < 0 and d2 > 0)) and ((d3 > 0 and d4 < 0) or (d3 < 0 and d4 > 0)):
        return 0.0
    return min(
        _pt_seg_dist(ax1, ay1, bx1, by1, bx2, by2),
        _pt_seg_dist(ax2, ay2, bx1, by1, bx2, by2),
        _pt_seg_dist(bx1, by1, ax1, ay1, ax2, ay2),
        _pt_seg_dist(bx2, by2, ax1, ay1, ax2, ay2),
    )


def _pt_rect_dist(px, py, cx, cy, rx, ry):
    return math.hypot(max(abs(px - cx) - rx, 0.0), max(abs(py - cy) - ry, 0.0))


def _seg_rect_dist(x1, y1, x2, y2, cx, cy, rx, ry):
    if (cx - rx <= x1 <= cx + rx and cy - ry <= y1 <= cy + ry) or (
        cx - rx <= x2 <= cx + rx and cy - ry <= y2 <= cy + ry
    ):
        return 0.0
    corners = [(cx - rx, cy - ry), (cx + rx, cy - ry), (cx + rx, cy + ry), (cx - rx, cy + ry)]
    md = min(_pt_rect_dist(x1, y1, cx, cy, rx, ry), _pt_rect_dist(x2, y2, cx, cy, rx, ry))
    for k in range(4):
        c1, c2 = corners[k], corners[(k + 1) % 4]
        md = min(md, _seg_seg_dist(x1, y1, x2, y2, c1[0], c1[1], c2[0], c2[1]))
    return md


def _vec_angle_deg(a, b):
    la, lb = math.hypot(a[0], a[1]), math.hypot(b[0], b[1])
    if la < 1e-8 or lb < 1e-8:
        return 180.0
    dot = max(-1.0, min(1.0, (a[0] * b[0] + a[1] * b[1]) / (la * lb)))
    return math.degrees(math.acos(dot))


def _near_pt(a, b, tol=0.005):
    return abs(a[0] - b[0]) <= tol and abs(a[1] - b[1]) <= tol


def _extract_chamfer_geometry(pcb_text):
    """Extract pads, Edge.Cuts segments, copper segments, and vias from KiCad PCB text."""
    pads = []
    for fm in re.finditer(r'\t\(footprint\s+"[^"]+"[\s\S]*?\n\t\)', pcb_text):
        fblk = fm.group(0)
        am = re.search(r'\n\t\t\(at\s+([-\d\.]+)\s+([-\d\.]+)(?:\s+([-\d\.]+))?\)', fblk)
        if not am:
            continue
        fx, fy, frot = float(am.group(1)), float(am.group(2)), float(am.group(3) or 0.0)
        flm = re.search(r'\n\t\t\(layer\s+"([^"]+)"\)', fblk)
        flip = bool(flm and flm.group(1) == "B.Cu")
        rad = math.radians(frot)
        ca, sa = math.cos(rad), math.sin(rad)
        for pm in re.finditer(r'\t\t\(pad\s+"[^"]*"\s+(\S+)\s+\S+([\s\S]*?)\n\t\t\)', fblk):
            ptype, pbody = pm.group(1), pm.group(2)
            pam = re.search(r'\(at\s+([-\d\.]+)\s+([-\d\.]+)(?:\s+([-\d\.]+))?\)', pbody)
            psm = re.search(r'\(size\s+([-\d\.]+)\s+([-\d\.]+)\)', pbody)
            plm = re.search(r'\(layers\s+([^\)]+)\)', pbody)
            pnm = re.search(r'\(net\s+(?:\d+\s+)?"([^"]*)"\)', pbody)
            pdm = re.search(r'\(drill\s+(?:oval\s+)?([-\d\.]+)(?:\s+([-\d\.]+))?\)', pbody)
            if not (pam and psm and plm):
                continue
            lx, ly = float(pam.group(1)), float(pam.group(2))
            prot = float(pam.group(3) or 0.0)
            if flip:
                lx = -lx
            gx, gy = fx + lx * ca + ly * sa, fy - lx * sa + ly * ca
            sx, sy = float(psm.group(1)), float(psm.group(2))
            rx, ry = (sy / 2.0, sx / 2.0) if int(round(prot)) % 180 == 90 else (sx / 2.0, sy / 2.0)
            lstr = plm.group(1)
            lyrs = {"F.Cu", "B.Cu"} if '"*.Cu"' in lstr else {l for l in ("F.Cu", "B.Cu") if f'"{l}"' in lstr}
            if flip and lyrs == {"F.Cu"}:
                lyrs = {"B.Cu"}
            pnet = pnm.group(1) if pnm else ""
            dr = max(float(pdm.group(1)), float(pdm.group(2) or pdm.group(1))) / 2.0 if pdm else 0.0
            pads.append((gx, gy, rx, ry, lyrs, pnet, dr, ptype))

    edges = []
    for em in re.finditer(r'\t\(gr_line[\s\S]*?\n\t\)', pcb_text):
        eb = em.group(0)
        if '"Edge.Cuts"' in eb:
            sm = re.search(r'\(start\s+([-\d\.]+)\s+([-\d\.]+)\)', eb)
            dm = re.search(r'\(end\s+([-\d\.]+)\s+([-\d\.]+)\)', eb)
            if sm and dm:
                edges.append((float(sm.group(1)), float(sm.group(2)), float(dm.group(1)), float(dm.group(2))))

    segs = []
    for m in re.finditer(r'\t\(segment\b[\s\S]*?\n\t\)', pcb_text):
        b = m.group(0)
        sm = re.search(r'\(start\s+([-\d\.]+)\s+([-\d\.]+)\)', b)
        em = re.search(r'\(end\s+([-\d\.]+)\s+([-\d\.]+)\)', b)
        wm = re.search(r'\(width\s+([-\d\.]+)\)', b)
        lm = re.search(r'\(layer\s+"([^"]+)"\)', b)
        nm = re.search(r'\(net\s+(?:\d+\s+)?"([^"]*)"\)', b)
        segs.append({
            "span": m.span(),
            "p1": (round(float(sm.group(1)), 4), round(float(sm.group(2)), 4)),
            "p2": (round(float(em.group(1)), 4), round(float(em.group(2)), 4)),
            "w": float(wm.group(1)),
            "lyr": lm.group(1),
            "net": nm.group(1) if nm else "",
            "active": True,
        })

    vias = []
    for m in re.finditer(r'\t\(via\b[\s\S]*?\n\t\)', pcb_text):
        b = m.group(0)
        am = re.search(r'\(at\s+([-\d\.]+)\s+([-\d\.]+)\)', b)
        sm = re.search(r'\(size\s+([-\d\.]+)\)', b)
        dm = re.search(r'\(drill\s+([-\d\.]+)\)', b)
        nm = re.search(r'\(net\s+(?:\d+\s+)?"([^"]*)"\)', b)
        vias.append({
            "span": m.span(),
            "pt": (round(float(am.group(1)), 4), round(float(am.group(2)), 4)),
            "size": float(sm.group(1)),
            "drill": float(dm.group(1)),
            "net": nm.group(1) if nm else "",
        })
    return pads, edges, segs, vias


def _candidate_via_ok(vi, v_pt, v_size, v_drill, net, diag_segs, skip_sids, pads, edges, segs, vias):
    """Verify DRC clearance for moved via and new diagonal chamfer segment(s)."""
    vr, dr = v_size / 2.0, v_drill / 2.0
    clr, edge_clr, hole_clr = 0.153, 0.31, 0.21
    for ex1, ey1, ex2, ey2 in edges:
        if _pt_seg_dist(v_pt[0], v_pt[1], ex1, ey1, ex2, ey2) < vr + edge_clr:
            return False
        for p1, p2, w, _ in diag_segs:
            if _seg_seg_dist(p1[0], p1[1], p2[0], p2[1], ex1, ey1, ex2, ey2) < w / 2.0 + edge_clr:
                return False

    for cx, cy, rx, ry, lyrs, pnet, pdr, ptype in pads:
        if pnet != net:
            if lyrs & {"F.Cu", "B.Cu"} and _pt_rect_dist(v_pt[0], v_pt[1], cx, cy, rx, ry) < vr + clr:
                return False
            if pdr > 0 and math.hypot(v_pt[0] - cx, v_pt[1] - cy) < dr + pdr + 0.22:
                return False
            for p1, p2, w, lyr in diag_segs:
                if lyr in lyrs and _seg_rect_dist(p1[0], p1[1], p2[0], p2[1], cx, cy, rx, ry) < w / 2.0 + clr:
                    return False
                if pdr > 0 and _pt_seg_dist(cx, cy, p1[0], p1[1], p2[0], p2[1]) < w / 2.0 + pdr + 0.20:
                    return False
        else:
            if ptype == "smd" and _pt_rect_dist(v_pt[0], v_pt[1], cx, cy, rx, ry) < vr - 0.05:
                return False
            if pdr > 0 and math.hypot(v_pt[0] - cx, v_pt[1] - cy) < dr + pdr + 0.20:
                return False

    for ovi, ov in enumerate(vias):
        if ovi == vi:
            continue
        ox, oy = ov["pt"]
        if abs(ox - v_pt[0]) > 4.0 or abs(oy - v_pt[1]) > 4.0:
            continue
        d = math.hypot(v_pt[0] - ox, v_pt[1] - oy)
        if d < dr + ov["drill"] / 2.0 + hole_clr:
            return False
        if ov["net"] != net:
            if d < vr + ov["size"] / 2.0 + clr:
                return False
            for p1, p2, w, _ in diag_segs:
                if _pt_seg_dist(ox, oy, p1[0], p1[1], p2[0], p2[1]) < w / 2.0 + ov["size"] / 2.0 + clr:
                    return False

    for osi, osg in enumerate(segs):
        if not osg["active"] or osi in skip_sids or osg["net"] == net:
            continue
        op1, op2 = osg["p1"], osg["p2"]
        if _pt_seg_dist(v_pt[0], v_pt[1], op1[0], op1[1], op2[0], op2[1]) < vr + osg["w"] / 2.0 + clr:
            return False
        for p1, p2, w, lyr in diag_segs:
            if lyr == osg["lyr"] and _seg_seg_dist(p1[0], p1[1], p2[0], p2[1], op1[0], op1[1], op2[0], op2[1]) < (w + osg["w"]) / 2.0 + clr:
                return False
    return True


def chamfer_via_transitions(pcb_text):
    """Chamfer 2-way orthogonal F.Cu<->B.Cu via transitions at 45 deg without DRC regressions."""
    pads, edges, segs, vias = _extract_chamfer_geometry(pcb_text)

    # 1. Split any segment passing strictly through a same-net via so incidence is explicit
    vias_by_net = {}
    for v in vias:
        vias_by_net.setdefault(v["net"], []).append(v["pt"])
    for si in range(len(segs)):
        s = segs[si]
        p1, p2 = s["p1"], s["p2"]
        dx, dy = p2[0] - p1[0], p2[1] - p1[1]
        l2 = dx * dx + dy * dy
        if l2 < 1e-8:
            continue
        splits = []
        for vp in vias_by_net.get(s["net"], []):
            if _near_pt(vp, p1) or _near_pt(vp, p2):
                continue
            t = ((vp[0] - p1[0]) * dx + (vp[1] - p1[1]) * dy) / l2
            if 0.01 < t < 0.99 and math.hypot(vp[0] - (p1[0] + t * dx), vp[1] - (p1[1] + t * dy)) < 0.015:
                splits.append((t, vp))
        if splits:
            splits.sort()
            s["active"] = False
            pts = [p1] + [sp[1] for sp in splits] + [p2]
            for k in range(len(pts) - 1):
                if math.hypot(pts[k + 1][0] - pts[k][0], pts[k + 1][1] - pts[k][1]) > 1e-4:
                    segs.append({
                        "span": None, "p1": pts[k], "p2": pts[k + 1],
                        "w": s["w"], "lyr": s["lyr"], "net": s["net"], "active": True,
                    })

    # 2. Chamfer every non-GND 2-way F.Cu<->B.Cu via with 2D angle < 125 deg
    fixed = 0
    for vi, v in enumerate(vias):
        net = v["net"]
        if net in ("/GND", "/HALL_GND"):
            continue
        vp = v["pt"]
        inc = []
        for si, s in enumerate(segs):
            if not s["active"] or s["net"] != net:
                continue
            if _near_pt(s["p1"], vp):
                inc.append((si, s["p2"]))
            elif _near_pt(s["p2"], vp):
                inc.append((si, s["p1"]))
        if len(inc) != 2 or segs[inc[0][0]]["lyr"] == segs[inc[1][0]]["lyr"]:
            continue
        si1, p_far1 = inc[0]
        si2, p_far2 = inc[1]
        if segs[si1]["lyr"] == "B.Cu":
            si1, p_far1, si2, p_far2 = si2, p_far2, si1, p_far1
        s1, s2 = segs[si1], segs[si2]
        vf = (p_far1[0] - vp[0], p_far1[1] - vp[1])
        vb = (p_far2[0] - vp[0], p_far2[1] - vp[1])
        lf, lb = math.hypot(*vf), math.hypot(*vb)
        if lf < 1e-4 or lb < 1e-4 or _vec_angle_deg(vf, vb) >= 125.0:
            continue
        uf, ub = (vf[0] / lf, vf[1] / lf), (vb[0] / lb, vb[1] / lb)

        # Incident vectors at p_far1 / p_far2 for validating full-diagonal endpoints
        def _other_vecs(pt, exclude_si):
            out = []
            for osi, osg in enumerate(segs):
                if osi == exclude_si or not osg["active"] or osg["net"] != net:
                    continue
                if _near_pt(osg["p1"], pt):
                    out.append((osg["lyr"], (osg["p2"][0] - pt[0], osg["p2"][1] - pt[1])))
                elif _near_pt(osg["p2"], pt):
                    out.append((osg["lyr"], (osg["p1"][0] - pt[0], osg["p1"][1] - pt[1])))
            return out

        cands = []
        for d in (0.55, 0.45, 0.35, 0.28, 0.20, 0.16):
            if lf >= d + 0.18 and lb >= d + 0.15:
                vn = (round(vp[0] + d * uf[0], 4), round(vp[1] + d * uf[1], 4))
                kb = (round(vp[0] + d * ub[0], 4), round(vp[1] + d * ub[1], 4))
                cands.append((vn, [(p_far1, vn, s1["w"], "F.Cu"), (vn, kb, s2["w"], "B.Cu"), (kb, p_far2, s2["w"], "B.Cu")], None))
            if lb >= d + 0.18 and lf >= d + 0.15:
                vn = (round(vp[0] + d * ub[0], 4), round(vp[1] + d * ub[1], 4))
                kf = (round(vp[0] + d * uf[0], 4), round(vp[1] + d * uf[1], 4))
                cands.append((vn, [(p_far1, kf, s1["w"], "F.Cu"), (kf, vn, s1["w"], "F.Cu"), (vn, p_far2, s2["w"], "B.Cu")], None))
            if lf >= d + 0.12 and lb >= d + 0.12:
                kf = (round(vp[0] + d * uf[0], 4), round(vp[1] + d * uf[1], 4))
                kb = (round(vp[0] + d * ub[0], 4), round(vp[1] + d * ub[1], 4))
                vn = (round(0.5 * (kf[0] + kb[0]), 4), round(0.5 * (kf[1] + kb[1]), 4))
                cands.append((vn, [(p_far1, kf, s1["w"], "F.Cu"), (kf, vn, s1["w"], "F.Cu"), (vn, kb, s2["w"], "B.Cu"), (kb, p_far2, s2["w"], "B.Cu")], None))

        if abs(uf[0] * ub[0] + uf[1] * ub[1]) < 0.15:
            if 0.25 <= lb <= 0.90 and lf >= lb + 0.18:
                vn = (round(vp[0] + lb * uf[0], 4), round(vp[1] + lb * uf[1], 4))
                cands.append((vn, [(p_far1, vn, s1["w"], "F.Cu"), (vn, p_far2, s2["w"], "B.Cu")], ("B.Cu", p_far2, vn, si2)))
            if 0.25 <= lf <= 0.90 and lb >= lf + 0.18:
                vn = (round(vp[0] + lf * ub[0], 4), round(vp[1] + lf * ub[1], 4))
                cands.append((vn, [(p_far1, vn, s1["w"], "F.Cu"), (vn, p_far2, s2["w"], "B.Cu")], ("F.Cu", p_far1, vn, si1)))

        for vn, ns, full_chk in cands:
            # Angle guard: via transitions >= 125 deg and full-diagonal destination >= threshold
            diag_segs = [sg for sg in ns if not ((abs(sg[0][0] - sg[1][0]) < 1e-4 and abs(vp[0] - sg[0][0]) < 1e-4) or (abs(sg[0][1] - sg[1][1]) < 1e-4 and abs(vp[1] - sg[0][1]) < 1e-4))]
            if not diag_segs:
                diag_segs = [sg for sg in ns if abs(sg[0][0] - sg[1][0]) > 1e-4 and abs(sg[0][1] - sg[1][1]) > 1e-4]
            at_vn = [(sg[0][0] - vn[0], sg[0][1] - vn[1]) if _near_pt(sg[1], vn) else (sg[1][0] - vn[0], sg[1][1] - vn[1]) for sg in ns if _near_pt(sg[0], vn) or _near_pt(sg[1], vn)]
            if len(at_vn) != 2 or _vec_angle_deg(at_vn[0], at_vn[1]) < 125.0:
                continue
            if full_chk is not None:
                f_lyr, f_pt, f_src, f_si = full_chk
                inc_others = _other_vecs(f_pt, f_si)
                new_vec = (f_src[0] - f_pt[0], f_src[1] - f_pt[1])
                req_ang = 88.0 if len(inc_others) >= 2 else 125.0
                if any(_vec_angle_deg(ov, new_vec) < (125.0 if lyr == f_lyr else req_ang) for lyr, ov in inc_others):
                    continue
            if _candidate_via_ok(vi, vn, v["size"], v["drill"], net, diag_segs, {si1, si2}, pads, edges, segs, vias):
                v["pt"] = vn
                s1["active"] = False
                s2["active"] = False
                for p1, p2, w, lyr in ns:
                    if math.hypot(p2[0] - p1[0], p2[1] - p1[1]) > 1e-4:
                        segs.append({"span": None, "p1": p1, "p2": p2, "w": w, "lyr": lyr, "net": net, "active": True})
                fixed += 1
                break

    print(f"Chamfered {fixed} 2-way via transitions (F.Cu<->B.Cu) at 45 deg")

    repls = []
    for s in segs:
        if s["span"] is not None and not s["active"]:
            repls.append((s["span"][0], s["span"][1], ""))
    for v in vias:
        sp = v["span"]
        orig = pcb_text[sp[0]:sp[1]]
        new_v = re.sub(r'\(at\s+[-\d\.]+\s+[-\d\.]+\)', f'(at {v["pt"][0]:.4f} {v["pt"][1]:.4f})', orig, count=1)
        repls.append((sp[0], sp[1], new_v))
    repls.sort(key=lambda x: x[0])

    pos = 0
    buf = []
    for a, b, rep in repls:
        if a > pos:
            buf.append(pcb_text[pos:a])
        buf.append(rep)
        pos = max(pos, b)
    extra = [
        make_seg(s["p1"][0], s["p1"][1], s["p2"][0], s["p2"][1], s["w"], s["lyr"], s["net"]) + "\n"
        for s in segs if s["span"] is None and s["active"]
    ]
    tail = pcb_text[pos:]
    end_idx = tail.rfind("\n)")
    if end_idx != -1:
        buf.append(tail[:end_idx] + "\n" + "".join(extra) + tail[end_idx:])
    else:
        buf.append(tail)
    return "".join(buf)


def audit_routing_guidelines(pcb_path):
    """Audit generated PCB against 45-deg corners (same-layer & cross-via), zero closed loops, and F.Cu=H / B.Cu=V rules."""
    from collections import defaultdict

    with open(pcb_path, "r") as f:
        txt = f.read()

    # 1. Extract thru_hole pad positions per net
    tht_pads_by_net = defaultdict(set)
    idx = 0
    while True:
        p = txt.find("\t(footprint ", idx)
        if p == -1:
            break
        end = txt.find("\n\t)\n", p)
        if end == -1:
            break
        fp = txt[p:end + 3]
        idx = end + 3
        am = re.search(r'\n\t\t\(at\s+([0-9.-]+)\s+([0-9.-]+)(?:\s+([0-9.-]+))?\)', fp)
        if not am:
            continue
        fx, fy = float(am.group(1)), float(am.group(2))
        frot = float(am.group(3) or 0.0)
        rad = math.radians(frot)
        cs, sn = math.cos(rad), math.sin(rad)
        pidx = 0
        while True:
            pp = fp.find('\t\t(pad "', pidx)
            if pp == -1:
                break
            pend = fp.find('\n\t\t)', pp)
            if pend == -1:
                break
            pblk = fp[pp:pend + 4]
            pidx = pend + 4
            pm = re.match(r'\t\t\(pad\s+"[^"]*"\s+(\S+)', pblk)
            if not pm or pm.group(1) != "thru_hole":
                continue
            pam = re.search(r'\(at\s+([0-9.-]+)\s+([0-9.-]+)', pblk)
            nm = re.search(r'\(net\s+(?:\d+\s+)?"([^"]*)"\)', pblk)
            if pam and nm and nm.group(1):
                px_r, py_r = float(pam.group(1)), float(pam.group(2))
                ax = round(fx + px_r * cs + py_r * sn, 3)
                ay = round(fy - px_r * sn + py_r * cs, 3)
                tht_pads_by_net[nm.group(1)].add((ax, ay))

    # 2. Extract segments & vias
    segs = []
    for m in re.finditer(
        r'\t\(segment\n\t\t\(start\s+([0-9.-]+)\s+([0-9.-]+)\)\n\t\t\(end\s+([0-9.-]+)\s+([0-9.-]+)\)\n\t\t\(width\s+([0-9.-]+)\)\n\t\t\(layer\s+"([^"]+)"\)\n\t\t\(net\s+(?:\d+\s+)?"([^"]+)"\)',
        txt,
    ):
        segs.append((
            float(m.group(1)), float(m.group(2)),
            float(m.group(3)), float(m.group(4)),
            float(m.group(5)), m.group(6), m.group(7),
        ))

    net_vias = defaultdict(list)
    for m in re.finditer(
        r'\t\(via\n\t\t\(at\s+([0-9.-]+)\s+([0-9.-]+)\).*?\(net\s+(?:\d+\s+)?"([^"]+)"\)',
        txt, re.S,
    ):
        net_vias[m.group(3)].append((round(float(m.group(1)), 3), round(float(m.group(2)), 3)))

    # 3. Angle check (>= 125 deg interior angle at every junction on every layer/net)
    by_ln = defaultdict(list)
    by_net = defaultdict(list)
    for x1, y1, x2, y2, w, lyr, net in segs:
        by_ln[(lyr, net)].append((x1, y1, x2, y2))
        by_net[net].append((x1, y1, x2, y2, lyr))

    angle_violations = []
    for (lyr, net), slist in sorted(by_ln.items()):
        pts = set()
        for x1, y1, x2, y2 in slist:
            pts.add((round(x1, 3), round(y1, 3)))
            pts.add((round(x2, 3), round(y2, 3)))
        for vx, vy in net_vias[net]:
            pts.add((vx, vy))
        for tx, ty in tht_pads_by_net[net]:
            pts.add((tx, ty))

        adj = defaultdict(list)
        for x1, y1, x2, y2 in slist:
            p1, p2 = (round(x1, 3), round(y1, 3)), (round(x2, 3), round(y2, 3))
            dx, dy = p2[0] - p1[0], p2[1] - p1[1]
            l2 = dx * dx + dy * dy
            if l2 < 1e-8:
                continue
            on_seg = [(0.0, p1), (1.0, p2)]
            for px, py in pts:
                if (px, py) == p1 or (px, py) == p2:
                    continue
                t = ((px - p1[0]) * dx + (py - p1[1]) * dy) / l2
                if 0.001 < t < 0.999:
                    if math.hypot(px - (p1[0] + t * dx), py - (p1[1] + t * dy)) < 0.015:
                        on_seg.append((t, (px, py)))
            on_seg.sort()
            for k in range(len(on_seg) - 1):
                u, v = on_seg[k][1], on_seg[k + 1][1]
                if u != v:
                    adj[u].append(v)
                    adj[v].append(u)

        for pt, nbrs in adj.items():
            if len(nbrs) < 2:
                continue
            vecs = []
            for nb in nbrs:
                dx, dy = nb[0] - pt[0], nb[1] - pt[1]
                ln = math.hypot(dx, dy)
                if ln > 1e-4:
                    vecs.append((dx / ln, dy / ln))
            for i in range(len(vecs)):
                for j in range(i + 1, len(vecs)):
                    dot = max(-1.0, min(1.0, vecs[i][0] * vecs[j][0] + vecs[i][1] * vecs[j][1]))
                    ang = math.degrees(math.acos(dot))
                    if ang < 125.0:
                        angle_violations.append((net, lyr, pt, round(ang, 1)))

    # 4. Loop & Connectivity check on non-GND nets
    loop_violations = []
    comp_violations = []
    via_90_violations = []
    via_acute_violations = []
    for net, slist in sorted(by_net.items()):
        if net in ("/GND", "/HALL_GND"):
            continue
        pts_by_lyr = defaultdict(set)
        for x1, y1, x2, y2, lyr in slist:
            pts_by_lyr[lyr].add((round(x1, 3), round(y1, 3)))
            pts_by_lyr[lyr].add((round(x2, 3), round(y2, 3)))
        for vx, vy in net_vias[net]:
            pts_by_lyr["F.Cu"].add((vx, vy))
            pts_by_lyr["B.Cu"].add((vx, vy))
        for tx, ty in tht_pads_by_net[net]:
            pts_by_lyr["F.Cu"].add((tx, ty))
            pts_by_lyr["B.Cu"].add((tx, ty))

        nodes = set()
        edge_list = []
        for x1, y1, x2, y2, lyr in slist:
            p1, p2 = (round(x1, 3), round(y1, 3)), (round(x2, 3), round(y2, 3))
            dx, dy = p2[0] - p1[0], p2[1] - p1[1]
            l2 = dx * dx + dy * dy
            if l2 < 1e-8:
                continue
            on_seg = [(0.0, p1), (1.0, p2)]
            for px, py in pts_by_lyr[lyr]:
                if (px, py) == p1 or (px, py) == p2:
                    continue
                t = ((px - p1[0]) * dx + (py - p1[1]) * dy) / l2
                if 0.001 < t < 0.999:
                    if math.hypot(px - (p1[0] + t * dx), py - (p1[1] + t * dy)) < 0.015:
                        on_seg.append((t, (px, py)))
            on_seg.sort()
            for k in range(len(on_seg) - 1):
                u, v = (lyr, on_seg[k][1]), (lyr, on_seg[k + 1][1])
                if u != v:
                    nodes.add(u)
                    nodes.add(v)
                    edge_list.append(tuple(sorted([u, v])))

        for vx, vy in net_vias[net]:
            u, v = ("F.Cu", (vx, vy)), ("B.Cu", (vx, vy))
            nodes.add(u)
            nodes.add(v)
            edge_list.append(tuple(sorted([u, v])))

        for p in tht_pads_by_net[net]:
            u, v = ("F.Cu", p), ("B.Cu", p)
            if u in nodes and v in nodes:
                edge_list.append(tuple(sorted([u, v])))

        g_adj = defaultdict(list)
        for u, v in edge_list:
            g_adj[u].append(v)
            g_adj[v].append(u)

        for vp in sorted(set(net_vias[net])):
            nbr_vecs = [
                (nlyr, npt[0] - vp[0], npt[1] - vp[1])
                for lyr in ("F.Cu", "B.Cu")
                for nlyr, npt in g_adj[(lyr, vp)]
                if npt != vp
            ]
            if len(nbr_vecs) == 2 and nbr_vecs[0][0] != nbr_vecs[1][0]:
                ang = _vec_angle_deg(nbr_vecs[0][1:], nbr_vecs[1][1:])
                if ang < 125.0:
                    via_90_violations.append((net, vp, round(ang, 1)))
            elif len(nbr_vecs) >= 3:
                for i in range(len(nbr_vecs)):
                    for j in range(i + 1, len(nbr_vecs)):
                        if nbr_vecs[i][0] != nbr_vecs[j][0]:
                            ang = _vec_angle_deg(nbr_vecs[i][1:], nbr_vecs[j][1:])
                            if ang < 88.0:
                                via_acute_violations.append((net, vp, round(ang, 1)))

        vis = set()
        comps = 0
        for n in nodes:
            if n not in vis:
                comps += 1
                q = [n]
                vis.add(n)
                while q:
                    cur = q.pop()
                    for nb in g_adj[cur]:
                        if nb not in vis:
                            vis.add(nb)
                            q.append(nb)
        cycles = len(edge_list) - len(nodes) + comps
        if cycles != 0:
            loop_violations.append((net, len(nodes), len(edge_list), comps, cycles))
        if comps > 1:
            comp_violations.append((net, comps))

    # 5. Manhattan Orientation metrics (F.Cu = Horizontal, B.Cu = Vertical) on non-GND nets
    ori = {lyr: {"H": 0.0, "V": 0.0, "D": 0.0} for lyr in ("F.Cu", "B.Cu")}
    for x1, y1, x2, y2, w, lyr, net in segs:
        if net in ("/GND", "/HALL_GND") or lyr not in ori:
            continue
        dx, dy = abs(x2 - x1), abs(y2 - y1)
        ln = math.hypot(dx, dy)
        if ln < 1e-4:
            continue
        if dy <= 0.05 * ln:
            ori[lyr]["H"] += ln
        elif dx <= 0.05 * ln:
            ori[lyr]["V"] += ln
        else:
            ori[lyr]["D"] += ln

    fh, fv, fd = ori["F.Cu"]["H"], ori["F.Cu"]["V"], ori["F.Cu"]["D"]
    bh, bv, bd = ori["B.Cu"]["H"], ori["B.Cu"]["V"], ori["B.Cu"]["D"]
    f_pct = 100.0 * fh / max(fh + fv, 1e-6)
    b_pct = 100.0 * bv / max(bh + bv, 1e-6)

    print("=== ROUTING AUDIT SUMMARY ===")
    print(f"90-deg/acute (<125 deg) same-layer bends: {len(angle_violations)}")
    for item in angle_violations[:20]:
        print("  ANGLE VIOLATION:", item)
    print(f"Cross-via 2-way (<125 deg) bends: {len(via_90_violations)} | 3-way acute (<88 deg) forks: {len(via_acute_violations)}")
    for item in via_90_violations[:20]:
        print("  VIA 90 VIOLATION:", item)
    for item in via_acute_violations[:20]:
        print("  VIA ACUTE VIOLATION:", item)
    print(f"Non-GND closed loops: {len(loop_violations)} | Disconnected subnets: {len(comp_violations)}")
    for item in loop_violations:
        print("  LOOP VIOLATION:", item)
    for item in comp_violations:
        print("  SUBNET VIOLATION:", item)
    print(f"F.Cu (Top):    H={fh:6.1f}mm, V={fv:6.1f}mm, Diag45={fd:5.1f}mm -> Horizontal={f_pct:.1f}%")
    print(f"B.Cu (Bottom): H={bh:6.1f}mm, V={bv:6.1f}mm, Diag45={bd:5.1f}mm -> Vertical={b_pct:.1f}%")

    assert not angle_violations, f"Found {len(angle_violations)} same-layer bend violations (<125 deg)"
    assert not via_90_violations, f"Found {len(via_90_violations)} cross-via 2-way bend violations (<125 deg)"
    assert not via_acute_violations, f"Found {len(via_acute_violations)} cross-via acute fork violations (<88 deg)"
    assert not loop_violations, f"Found {len(loop_violations)} closed signal/power loops"
    assert not comp_violations, f"Found {len(comp_violations)} split nets"
    assert f_pct >= 80.0, f"F.Cu horizontal ratio too low: {f_pct:.1f}% < 80%"
    assert b_pct >= 80.0, f"B.Cu vertical ratio too low: {b_pct:.1f}% < 80%"


def main():
    generate_project_files()

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
        '(comment 2 "Back: LC29H GNSS | Front PCBA: C6-WROOM-1, Ra-02 LoRa, JOY1, 2xADS1115, IP2326, MT2492, AMS1117")',
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
        "J_DK_L", "J_DK_R", "J3", "ESC", "RUD", "WIN", "RCK", "STRB",
    }
    # Nets whose V1 segments/vias are replaced cleanly (all V1 signals now follow V2 45-deg & F.Cu=H / B.Cu=V rules)
    reroute_nets = {
        "/+5V", "/+5V_SERVO", "/+3V3", "/+3V3_C6",
        "/GPS_RX", "/GPS_TX", "/INTER_TX_M2S", "/INTER_RX_S2M",
        "/IO18_UP", "/IO19_MODE", "/IO20_DN", "/IO9_BOOT",
        "/WATER_SENS", "/BATT_CURR", "/BATT_VOLT", "/ANCHOR_HALL", "/ANCHOR_LIMIT",
        "/HALL_GND", "/HALL_VCC", "/HALL_OUT",
        "/JOY_X", "/JOY_Y", "/JOY_SW", "/GPS_PPS", "/EXP_IO1", "/EXP_IO11",
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
        if blk.startswith("\t(gr_rect") and any(s in blk for s in (
            "(start 60 75)", "(start 5 6.5)", "(start 73.5 66.96)", "(start 43 86)",
            "(start 43 12)", "(start 47 15.5)", "(start 85.5 19)",
        )):
            continue
        if blk.startswith("\t(footprint"):
            ref = get_fp_ref(blk)
            if ref in remove_refs or ref.startswith("STAMP"):
                continue
            if ref == "J1":
                blk = blk.replace('(property "Value" "ESP32_lafvin"', '(property "Value" "ESP32-C6-LCD-1.47"', 1)
            if ref == "J2":
                blk = blk.replace('(property "Value" "ESP32_B_LANDSCAPE"', '(property "Value" "ESP32-C6-LCD-1.47_B"', 1)
                blk = blk.replace('(net "/GPS_PPS")', '(net "/JOY_SW")', 1)
                PAD_POS[("J2", "5")] = (58.1600, 31.2800)
            if ref in {"J1", "J2"} and "(model " not in blk:
                hdr_models = (
                    '\t\t(model "${KICAD10_3DMODEL_DIR}/Connector_PinSocket_2.54mm.3dshapes/PinSocket_1x09_P2.54mm_Vertical.step"\n'
                    '\t\t\t(offset (xyz 0 0 0))\n'
                    '\t\t\t(scale (xyz 1 1 1))\n'
                    '\t\t\t(rotate (xyz 0 0 -90))\n'
                    '\t\t)\n'
                    '\t\t(model "${KICAD10_3DMODEL_DIR}/Connector_PinHeader_2.54mm.3dshapes/PinHeader_1x09_P2.54mm_Vertical.step"\n'
                    '\t\t\t(offset (xyz 0 0 9.90))\n'
                    '\t\t\t(scale (xyz 1 1 0.55))\n'
                    '\t\t\t(rotate (xyz 180 0 90))\n'
                    '\t\t)\n'
                )
                if ref == "J1":
                    hdr_models += (
                        '\t\t(model "${KIPRJMOD}/3dmodels/ESP32-C6-LCD-1.47.step"\n'
                        '\t\t\t(offset (xyz 12.45 -8.89 13.55))\n'
                        '\t\t\t(scale (xyz 1 1 1))\n'
                        '\t\t\t(rotate (xyz 0 0 -90))\n'
                        '\t\t)\n'
                    )
                blk = blk[:-2] + hdr_models + '\t)'
            if ref == "J6":
                blk = blk.replace('\n\t\t\t(net "/GPS_PPS")', '', 1)
            if ref == "JOY1":
                blk = blk.replace("(at 78.2525 51.715)", "(at 72.7525 51.715)", 1)
                blk = blk.replace("(at -8.73 -2.5)", "(at 8.73 -2.5)", 1)
                blk = blk.replace("(at -8.73 0)", "(at 8.73 0)", 1)
                blk = blk.replace("(at -8.73 2.5)", "(at 8.73 2.5)", 1)
                # Diagonal tactile switch wiring (Alps RKJXV122400R 6.5x4.5mm switch):
                # Pads 7 & 9 (Y=+5.75) are internally shorted; Pads 8 & 10 (Y=+10.25) are internally shorted.
                # Wire only diagonal Pad 7 (/GND) and Pad 10 (/JOY_SW); isolate Pads 8 & 9 (mechanical only).
                pad8_old = '\n\t\t\t(net "/GND")\n\t\t\t(uuid "07559e99-c163-406d-a279-5ad41016a359")'
                pad9_old = '\n\t\t\t(net "/IO9_BOOT")\n\t\t\t(uuid "a931a7f5-ac3f-4210-90e7-89ca11c591f8")'
                assert pad8_old in blk and pad9_old in blk, "Expected JOY1 switch pad 8/9 net blocks not found"
                blk = blk.replace(pad8_old, '\n\t\t\t(uuid "07559e99-c163-406d-a279-5ad41016a359")', 1)
                blk = blk.replace(pad9_old, '\n\t\t\t(uuid "a931a7f5-ac3f-4210-90e7-89ca11c591f8")', 1)
                blk = blk.replace('(net "/IO9_BOOT")', '(net "/JOY_SW")', 1)
                assert '(net "/IO9_BOOT")' not in blk, "Unexpected remaining /IO9_BOOT net in JOY1"
                PAD_POS[("JOY1", "1")] = (81.4825, 49.2150)
                PAD_POS[("JOY1", "2")] = (81.4825, 51.7150)
                PAD_POS[("JOY1", "3")] = (81.4825, 54.2150)
                PAD_POS[("JOY1", "5")] = (72.7525, 42.9850)
                PAD_POS[("JOY1", "6")] = (75.2525, 42.9850)
                PAD_POS[("JOY1", "7")] = (69.5025, 57.4650)
                PAD_POS[("JOY1", "10")] = (76.0025, 61.9650)
                joy_model = (
                    '\t\t(model "${KIPRJMOD}/3dmodels/RKJXV122400R.STEP"\n'
                    '\t\t\t(offset (xyz 0 0 0))\n'
                    '\t\t\t(scale (xyz 1 1 1))\n'
                    '\t\t\t(rotate (xyz 0 0 180))\n'
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
            if ref in {"H1", "H2", "H3", "H4"}:
                blk = blk.replace('(property "Value" ""', '(property "Value" "M3"', 1)
            # Hide overlapping V1 labels cleanly on multi-line KiCad 10 property blocks
            hide_both = {"J1", "J2", "H1", "H2", "H3", "H4"}
            hide_val_only = {"J6", "JOY1", "SW1", "SW2", "SW3", "SW4"}
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
            record_footprint_pads(blk)
            kept_blocks.append(blk)
            continue
        if blk.startswith("\t(via"):
            vnet = get_seg_net(blk)
            if vnet in reroute_nets:
                continue
            kept_blocks.append(blk)
            continue
        if blk.startswith("\t(segment"):
            snet = get_seg_net(blk)
            if snet in reroute_nets:
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
    for sidx, sx in enumerate(stamp_xs, start=1):
        new_blocks.append(f"""\t(footprint "MountingHole:MountingHole_0.5mm"
\t\t(layer "F.Cu")
\t\t(uuid "{new_uuid()}")
\t\t(at {sx:.2f} 92.20)
\t\t(property "Reference" "STAMP{sidx}" (at 0 0 0) (layer "F.SilkS") (hide yes) (uuid "{new_uuid()}") (effects (font (size 0.5 0.5))))
\t\t(property "Value" "" (at 0 0 0) (layer "F.Fab") (hide yes) (uuid "{new_uuid()}") (effects (font (size 0.5 0.5))))
\t\t(attr board_only exclude_from_pos_files exclude_from_bom)
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

    # J_PWR_SRV at (17.50, 84.50, rot=270): Pad 1 (rect) at (17.50, 84.50)=GND, Pad 2 at (14.96, 84.50)=+5V_SERVO
    new_blocks.append(load_std_footprint(
        "Connector_PinHeader_2.54mm.pretty/PinHeader_1x02_P2.54mm_Vertical.kicad_mod",
        "J_PWR_SRV", "5V_6V_BEC", 17.50, 84.50, rot=270,
        pad_nets={"1": "/GND", "2": "/+5V_SERVO"},
        ref_at=(0, -3.5, 0), ref_size=0.65, hide_ref=True, exclude_bom=True, exclude_pos=True,
    ))

    # J_WINCH_HUB (1x06 90-deg lateral on Left Edge @ 4.40, 91.20, rot=180, aligned with J_BAL_MOT):
    # Bottom-to-top: 1:GND (91.20), 2:+5V_SERVO (88.66), 3:+3V3_C6 (86.12), 4:ANCHOR_HALL (83.58), 5:SERVO_WINCH (81.04), 6:SERVO_RACK (78.50)
    j_winch_blk = load_std_footprint(
        "Connector_PinHeader_2.54mm.pretty/PinHeader_1x06_P2.54mm_Horizontal.kicad_mod",
        "J_WINCH_HUB", "WINCH_HUB_6P", 4.40, 91.20, rot=180,
        pad_nets={
            "1": "/GND",
            "2": "/+5V_SERVO",
            "3": "/+3V3_C6",
            "4": "/ANCHOR_HALL",
            "5": "/SERVO_WINCH",
            "6": "/SERVO_RACK",
        },
        ref_at=(0, -2.5, 0), ref_size=0.60, hide_ref=True, exclude_bom=True, exclude_pos=True,
    )
    j_winch_blk = re.sub(
        r'\t\t\(fp_(?:line|rect)\n\t\t\t\(start (?:4\.15|10\.15)[\s\S]*?\(layer "F\.SilkS"\)\n\t\t\)\n?',
        '', j_winch_blk,
    )
    new_blocks.append(j_winch_blk)

    # 3x5 Right-Angle (90-deg) 2.54mm RC Receiver Servo Header block on Left Edge (Y=64.92..75.08):
    # Col 1 (X=4.40, bottom pin tier Z=1.27): GND (-); Col 2 (X=6.94, middle tier Z=3.81): +5V_SERVO (+); Col 3 (X=9.48, top tier Z=6.35): SIG (S)
    def make_rc_servo_slice_footprint(ref, val, x, y, sig_net, include_3d=False):
        PAD_POS[(ref, "1")] = (x, y)
        PAD_POS[(ref, "2")] = (x + 2.54, y)
        PAD_POS[(ref, "3")] = (x + 5.08, y)
        model_str = ""
        if include_3d:
            model_str = """
\t\t(model "${KIPRJMOD}/3dmodels/PinHeader_3x05_RC_Horizontal.wrl"
\t\t\t(offset
\t\t\t\t(xyz 0 0 0)
\t\t\t)
\t\t\t(scale
\t\t\t\t(xyz 0.393701 0.393701 0.393701)
\t\t\t)
\t\t\t(rotate
\t\t\t\t(xyz 0 0 0)
\t\t\t)
\t\t)"""
        return f"""\t(footprint "Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Horizontal"
\t\t(layer "F.Cu")
\t\t(uuid "{new_uuid()}")
\t\t(at {x:.4f} {y:.4f})
\t\t(property "Reference" "{ref}"
\t\t\t(at 0 -1.8 0)
\t\t\t(layer "F.SilkS")
\t\t\t(hide yes)
\t\t\t(uuid "{new_uuid()}")
\t\t\t(effects
\t\t\t\t(font
\t\t\t\t\t(size 0.6 0.6)
\t\t\t\t\t(thickness 0.12)
\t\t\t\t\t(bold yes)
\t\t\t\t)
\t\t\t)
\t\t)
\t\t(property "Value" "{val}"
\t\t\t(at 0 1.8 0)
\t\t\t(layer "F.Fab")
\t\t\t(hide yes)
\t\t\t(uuid "{new_uuid()}")
\t\t\t(effects
\t\t\t\t(font
\t\t\t\t\t(size 0.6 0.6)
\t\t\t\t\t(thickness 0.12)
\t\t\t\t)
\t\t\t)
\t\t)
\t\t(property "Datasheet" ""
\t\t\t(at 0 0 0)
\t\t\t(layer "F.Fab")
\t\t\t(hide yes)
\t\t\t(uuid "{new_uuid()}")
\t\t\t(effects
\t\t\t\t(font
\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t\t(thickness 0.15)
\t\t\t\t)
\t\t\t)
\t\t)
\t\t(property "Description" ""
\t\t\t(at 0 0 0)
\t\t\t(layer "F.Fab")
\t\t\t(hide yes)
\t\t\t(uuid "{new_uuid()}")
\t\t\t(effects
\t\t\t\t(font
\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t\t(thickness 0.15)
\t\t\t\t)
\t\t\t)
\t\t)
\t\t(attr through_hole exclude_from_pos_files exclude_from_bom)
\t\t(fp_rect
\t\t\t(start -4.04 -1.24)
\t\t\t(end 6.1 1.24)
\t\t\t(stroke
\t\t\t\t(width 0.05)
\t\t\t\t(type solid)
\t\t\t)
\t\t\t(fill no)
\t\t\t(layer "F.CrtYd")
\t\t\t(uuid "{new_uuid()}")
\t\t)
\t\t(fp_rect
\t\t\t(start -4.04 -1.27)
\t\t\t(end -1.5 1.27)
\t\t\t(stroke
\t\t\t\t(width 0.1)
\t\t\t\t(type solid)
\t\t\t)
\t\t\t(fill no)
\t\t\t(layer "F.Fab")
\t\t\t(uuid "{new_uuid()}")
\t\t)
\t\t(pad "1" thru_hole rect
\t\t\t(at 0 0)
\t\t\t(size 1.7 1.7)
\t\t\t(drill 1)
\t\t\t(layers "*.Cu" "*.Mask")
\t\t\t(remove_unused_layers no)
\t\t\t(net "/GND")
\t\t\t(uuid "{new_uuid()}")
\t\t)
\t\t(pad "2" thru_hole circle
\t\t\t(at 2.54 0)
\t\t\t(size 1.7 1.7)
\t\t\t(drill 1)
\t\t\t(layers "*.Cu" "*.Mask")
\t\t\t(remove_unused_layers no)
\t\t\t(net "/+5V_SERVO")
\t\t\t(uuid "{new_uuid()}")
\t\t)
\t\t(pad "3" thru_hole circle
\t\t\t(at 5.08 0)
\t\t\t(size 1.7 1.7)
\t\t\t(drill 1)
\t\t\t(layers "*.Cu" "*.Mask")
\t\t\t(remove_unused_layers no)
\t\t\t(net "{sig_net}")
\t\t\t(uuid "{new_uuid()}")
\t\t)
\t\t(embedded_fonts no){model_str}
\t)"""

    for sref, sval, sy, snet, show_3d in [
        ("ESC", "ESC", 64.92, "/SERVO_ESC", False),
        ("RUD", "RUDDER", 67.46, "/SERVO_RUDDER", False),
        ("WIN", "WINCH", 70.00, "/SERVO_WINCH", True),
        ("RCK", "RACK", 72.54, "/SERVO_RACK", False),
        ("STRB", "WS2812B", 75.08, "/STROBE_DIN", False),
    ]:
        new_blocks.append(make_rc_servo_slice_footprint(sref, sval, 4.40, sy, snet, include_3d=show_3d))

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
    # 1. Solar 5V-6V Input (Right-Angle JST-XH outward on Right Edge)
    j_solar_blk = load_std_footprint(
        "Connector_JST.pretty/JST_XH_S2B-XH-A_1x02_P2.50mm_Horizontal.kicad_mod",
        "J_SOLAR", "SOLAR_5_6V", 95.60, 12.20, rot=90,
        pad_nets={"1": "/GND", "2": "/VSOLAR_IN"},
        ref_at=(1.25, -3.5, 0), ref_size=0.65, hide_ref=True, exclude_bom=True, exclude_pos=True,
    )
    j_solar_blk = re.sub(
        r'\t\t\(fp_(?:line|rect)\n[\s\S]*?\(layer "F\.SilkS"\)\n\t\t\)\n?',
        '', j_solar_blk,
    )
    new_blocks.append(j_solar_blk)
    # D_SOL rot=90: Pad 2 Anode TOP=/VSOLAR_IN, Pad 1 Cathode BOT=/VIN_CHG
    new_blocks.append(load_std_footprint(
        "Diode_SMD.pretty/D_SOD-123.kicad_mod",
        "D_SOL", "B5819W", 95.60, 17.80, rot=90,
        pad_nets={"1": "/VIN_CHG", "2": "/VSOLAR_IN"},
        ref_at=(0, -2.0, 90), ref_size=0.55, hide_ref=True, lcsc="C8598",
    ))

    # 2. USB-C Receptacle (max safe overhang: +1.40mm past X=100.00 -> mouth at X=101.40, front SH copper at X=99.60)
    j_usbc_blk = load_std_footprint(
        "Connector_USB.pretty/USB_C_Receptacle_HRO_TYPE-C-31-M-12.kicad_mod",
        "J_USBC", "TYPE-C-31-M-12", 97.75, 36.50, rot=90,
        pad_nets={
            "A1": "/GND", "B12": "/GND", "B1": "/GND", "A12": "/GND", "SH": "/GND",
            "A4": "/VBUS_USB", "B9": "/VBUS_USB", "B4": "/VBUS_USB", "A9": "/VBUS_USB",
            "A5": "/USB_CC1", "B5": "/USB_CC2",
            "A6": "/USB_DP", "B6": "/USB_DP",
            "A7": "/USB_DN", "B7": "/USB_DN",
        },
        ref_at=(-1.5, -5.5, 90), ref_size=0.60, hide_ref=True, lcsc="C165948",
        model_path="${KICAD10_3DMODEL_DIR}/Connector_USB.3dshapes/USB_C_Receptacle_GCT_USB4105-xx-A_16P_TopMnt_Horizontal.step",
    )
    j_usbc_blk = re.sub(
        r'\t\t\(fp_line\n(?:(?!\n\t\t\))[\s\S])*? 3\.9\)(?:(?!\n\t\t\))[\s\S])*?\(layer "F\.SilkS"\)\n\t\t\t\(uuid "[^"]+"\)\n\t\t\)\n?',
        '', j_usbc_blk,
    )
    new_blocks.append(j_usbc_blk)
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
    # R_BAL placed cleanly left of J_LIPO_2S (rot=90: Pad 1 BOT=/CHG_VBATM, Pad 2 TOP=/VBAT_MID)
    new_blocks.append(load_std_footprint(
        "Resistor_SMD.pretty/R_0805_2012Metric.kicad_mod",
        "R_BAL", "10R", 88.00, 49.40, rot=90,
        pad_nets={"1": "/CHG_VBATM", "2": "/VBAT_MID"},
        ref_at=(0, -1.5, 0), ref_size=0.50, hide_ref=True, lcsc="C17415",
    ))

    # 4. 2S LiPo Balance Port (Right-Angle JST-XH outward on Right Edge), PTC Fuse, Reverse-Polarity MOSFET & Power Switch
    j_lipo_blk = load_std_footprint(
        "Connector_JST.pretty/JST_XH_S3B-XH-A_1x03_P2.50mm_Horizontal.kicad_mod",
        "J_LIPO_2S", "2S_LIPO_BAL", 95.60, 50.20, rot=90,
        pad_nets={"1": "/GND", "2": "/VBAT_MID", "3": "/VBAT_2S"},
        ref_at=(2.5, -4.0, 0), ref_size=0.65, hide_ref=True, exclude_bom=True, exclude_pos=True,
    )
    j_lipo_blk = re.sub(
        r'\t\t\(fp_(?:line|rect)\n[\s\S]*?\(layer "F\.SilkS"\)\n\t\t\)\n?',
        '', j_lipo_blk,
    )
    new_blocks.append(j_lipo_blk)
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

    # Dedicated 4-pin JST-XH Right-Angle IMU Connector on Left Edge (GY-BNO055: 1=GND, 2=3V3, 3=SDA/IO1, 4=SCL/IO11)
    j_imu_blk = load_std_footprint(
        "Connector_JST.pretty/JST_XH_S4B-XH-A_1x04_P2.50mm_Horizontal.kicad_mod",
        "J_IMU", "GY_BNO055_I2C", 4.40, 17.50, rot=270,
        pad_nets={"1": "/GND", "2": "/+3V3_C6", "3": "/EXP_IO1", "4": "/EXP_IO11"},
        ref_at=(3.75, -3.5, 0), ref_size=0.60, hide_ref=True, exclude_bom=True, exclude_pos=True,
    )
    j_imu_blk = re.sub(
        r'\t\t\(fp_(?:line|rect)\n[\s\S]*?\(layer "F\.SilkS"\)\n\t\t\)\n?',
        '', j_imu_blk,
    )
    new_blocks.append(j_imu_blk)

    # Optional Base-Station 2S Voltage Monitor Divider + Solder Jumper
    new_blocks.append(load_std_footprint(
        "Resistor_SMD.pretty/R_0805_2012Metric.kicad_mod",
        "R_2S_TOP", "100k", 67.85, 87.50, rot=180,
        pad_nets={"1": "/BUCK_EN_SW", "2": "/ADC_2S_DIV"},
        ref_at=(0, -1.5, 0), ref_size=0.50, hide_ref=True, lcsc="C17407",
    ))
    new_blocks.append(load_std_footprint(
        "Resistor_SMD.pretty/R_0805_2012Metric.kicad_mod",
        "R_2S_BOT", "33k", 67.85, 89.80, rot=180,
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

    # --- D.7 Onboard ESP32-C6-WROOM-1-N8 + AMS1117-3.3 Dual-OR LDO + Boot/Reset + Onboard Ra-02 LoRa ---
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
        "U_LDO", "AMS1117-3.3", 65.40, 70.10, rot=90,
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
        "D_LDO_5V", "B5819W", 65.70, 84.35, rot=180,
        pad_nets={"1": "/LDO_VIN", "2": "/+5V"},
        ref_at=(0, -1.6, 0), ref_size=0.50, hide_ref=True, lcsc="C8598",
    ))
    new_blocks.append(load_std_footprint(
        "Capacitor_SMD.pretty/C_0805_2012Metric.kicad_mod",
        "C_LDO_IN", "22uF", 66.60, 75.55, rot=180,
        pad_nets={"1": "/LDO_VIN", "2": "/GND"},
        ref_at=(0, -1.6, 0), ref_size=0.50, hide_ref=True, lcsc="C45783",
    ))
    new_blocks.append(load_std_footprint(
        "Capacitor_SMD.pretty/C_0805_2012Metric.kicad_mod",
        "C_LDO_OUT", "22uF", 80.80, 66.20, rot=180,
        pad_nets={"1": "/+3V3_C6", "2": "/GND"},
        ref_at=(0, -1.6, 0), ref_size=0.50, hide_ref=True, lcsc="C45783",
    ))
    # Onboard Ai-Thinker Ra-02 (SX1278 433MHz LoRa with integrated U.FL/IPEX receptacle, LCSC C90039)
    u_lora_blk = load_std_footprint(
        "RF_Module.pretty/Ai-Thinker-Ra-01-LoRa.kicad_mod",
        "U_LORA", "Ra-02", 78.90, 77.60, rot=180,
        pad_nets={
            "1": "/GND", "2": "/GND", "3": "/+3V3_C6",
            "5": "/LORA_DIO0", "9": "/GND",
            "12": "/LORA_SCK", "13": "/LORA_MISO", "14": "/LORA_MOSI", "15": "/LORA_NSS",
            "16": "/GND",
        },
        ref_at=(0, -9.8, 0), ref_size=0.55, hide_ref=True, lcsc="C90039",
        bom_fp_name="Ai-Thinker-Ra-01-LoRa",
    )
    u_lora_models = (
        '\t\t(model "${KIPRJMOD}/3dmodels/Ai-Thinker-Ra-02.wrl"\n'
        '\t\t\t(offset\n\t\t\t\t(xyz 0 0 0)\n\t\t\t)\n'
        '\t\t\t(scale\n\t\t\t\t(xyz 0.393701 0.393701 0.393701)\n\t\t\t)\n'
        '\t\t\t(rotate\n\t\t\t\t(xyz 0 0 0)\n\t\t\t)\n'
        '\t\t)\n'
        '\t\t(model "${KICAD10_3DMODEL_DIR}/Connector_Coaxial.3dshapes/U.FL_Hirose_U.FL-R-SMT-1_Vertical.step"\n'
        '\t\t\t(offset\n\t\t\t\t(xyz -3.8 6.4 0.8)\n\t\t\t)\n'
        '\t\t\t(scale\n\t\t\t\t(xyz 1 1 1)\n\t\t\t)\n'
        '\t\t\t(rotate\n\t\t\t\t(xyz 0 0 0)\n\t\t\t)\n'
        '\t\t)'
    )
    u_lora_blk = re.sub(r'\t\t\(model\s+"[^"]+".*?\n\t\t\)', lambda _m: u_lora_models, u_lora_blk, count=1, flags=re.S)
    new_blocks.append(u_lora_blk)
    new_blocks.append(load_std_footprint(
        "Capacitor_SMD.pretty/C_0805_2012Metric.kicad_mod",
        "C_LORA", "100nF", 86.80, 88.80, rot=0,
        pad_nets={"1": "/+3V3_C6", "2": "/GND"},
        ref_at=(0, -1.6, 0), ref_size=0.50, hide_ref=True, lcsc="C49678",
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
    # E. COPPER ROUTING (45-deg Miters, Zero Loops, F.Cu=Horiz / B.Cu=Vert)
    # ==========================================================================
    routes = []

    # 1. Core Top V1 Signals (F.Cu = Horizontal, B.Cu = Vertical)
    # /+3V3: J6:7 -> B.Cu -> F.Cu Y=9.50 -> B.Cu J1:7 & (74.40, 41.40) -> JOY1:6 -> JOY1:3 (clears /VBAT_2S & R_NTC)
    routes += make_route([P("J6", 7), (27.12, 9.50)], 0.35, "B.Cu", "/+3V3")
    routes.append(make_via(27.12, 9.50, "/+3V3", size=0.60, drill=0.32))
    routes += make_route([(27.12, 9.50), (63.24, 9.50), (74.40, 9.50)], 0.35, "F.Cu", "/+3V3")
    routes.append(make_via(63.24, 9.50, "/+3V3", size=0.60, drill=0.32))
    routes += make_route([(63.24, 9.50), P("J1", 7)], 0.35, "B.Cu", "/+3V3")
    routes.append(make_via(74.40, 9.50, "/+3V3", size=0.60, drill=0.32))
    routes += make_route([(74.40, 9.50), (74.40, 40.6675), (73.6675, 41.40)], 0.35, "B.Cu", "/+3V3")
    routes.append(make_via(73.6675, 41.40, "/+3V3", size=0.60, drill=0.32))
    routes += make_route([(73.6675, 41.40), P("JOY1", 6), (78.60, 42.9850)], 0.32, "F.Cu", "/+3V3")
    routes.append(make_via(78.60, 42.9850, "/+3V3", size=0.55, drill=0.30))
    routes += make_route([(78.60, 42.9850), (78.60, 44.65)], 0.30, "B.Cu", "/+3V3")
    routes.append(make_via(78.60, 44.65, "/+3V3", size=0.55, drill=0.30))
    routes += make_route([(78.60, 44.65), (83.20, 44.65)], 0.30, "F.Cu", "/+3V3")
    routes.append(make_via(83.20, 44.65, "/+3V3", size=0.55, drill=0.30))
    routes += make_route([(83.20, 44.65), (83.20, 54.2150)], 0.30, "B.Cu", "/+3V3")
    routes.append(make_via(83.20, 54.2150, "/+3V3", size=0.55, drill=0.30))
    routes += make_route([(83.20, 54.2150), P("JOY1", 3)], 0.30, "F.Cu", "/+3V3")

    # /GPS_RX & /GPS_TX: J6:4,5 -> B.Cu vert -> F.Cu horiz Y=26.6/27.4 -> B.Cu vert -> J2:8,9
    routes += make_route([P("J6", 4), (19.50, 27.40)], 0.25, "B.Cu", "/GPS_RX")
    routes.append(make_via(19.50, 27.40, "/GPS_RX", size=0.55, drill=0.30))
    routes += make_route([(19.50, 27.40), (65.78, 27.40)], 0.25, "F.Cu", "/GPS_RX")
    routes.append(make_via(65.78, 27.40, "/GPS_RX", size=0.55, drill=0.30))
    routes += make_route([(65.78, 27.40), P("J2", 8)], 0.25, "B.Cu", "/GPS_RX")

    routes += make_route([P("J6", 5), (22.04, 26.60)], 0.25, "B.Cu", "/GPS_TX")
    routes.append(make_via(22.04, 26.60, "/GPS_TX", size=0.55, drill=0.30))
    routes += make_route([(22.04, 26.60), (68.32, 26.60)], 0.25, "F.Cu", "/GPS_TX")
    routes.append(make_via(68.32, 26.60, "/GPS_TX", size=0.55, drill=0.30))
    routes += make_route([(68.32, 26.60), P("J2", 9)], 0.25, "B.Cu", "/GPS_TX")

    # Front Pushbuttons SW1..SW4 -> B.Cu vert X=26.6..28.4 (clear of F.Cu /BAL bus) -> F.Cu horiz Y=33.2..35.0 -> B.Cu -> J2:1..4
    for sw_ref, j2_pin, y_sw, x_up, y_top, net in [
        ("SW4", 1, 43.60, 26.60, 33.20, "/IO9_BOOT"),
        ("SW1", 2, 49.40, 27.20, 33.80, "/IO18_UP"),
        ("SW2", 3, 53.00, 27.80, 34.40, "/IO19_MODE"),
        ("SW3", 4, 59.00, 28.40, 35.00, "/IO20_DN"),
    ]:
        px, _ = P(sw_ref, 1)
        jx, jy = P("J2", j2_pin)
        routes += make_route([P(sw_ref, 1), (px, y_sw), (x_up, y_sw)], 0.25, "F.Cu", net)
        routes.append(make_via(x_up, y_sw, net, size=0.50, drill=0.30))
        routes += make_route([(x_up, y_sw), (x_up, y_top)], 0.25, "B.Cu", net)
        routes.append(make_via(x_up, y_top, net, size=0.50, drill=0.30))
        routes += make_route([(x_up, y_top), (jx, y_top)], 0.25, "F.Cu", net)
        routes.append(make_via(jx, y_top, net, size=0.50, drill=0.30))
        routes += make_route([(jx, y_top), (jx, jy)], 0.25, "B.Cu", net)

    # Inter-ESP UART (/INTER_TX_M2S & /INTER_RX_S2M) -> F.Cu horiz Y=20.6/21.3 -> B.Cu vert X=35.8/35.2 -> F.Cu U_C6:20,26
    routes += make_route([P("J1", 4), (55.62, 20.60)], 0.25, "B.Cu", "/INTER_TX_M2S")
    routes.append(make_via(55.62, 20.60, "/INTER_TX_M2S", size=0.50, drill=0.30))
    routes += make_route([(55.62, 20.60), (35.80, 20.60)], 0.25, "F.Cu", "/INTER_TX_M2S")
    routes.append(make_via(35.80, 20.60, "/INTER_TX_M2S", size=0.50, drill=0.30))
    routes += make_route([(35.80, 20.60), (35.80, 73.40)], 0.25, "B.Cu", "/INTER_TX_M2S")
    routes.append(make_via(35.80, 73.40, "/INTER_TX_M2S", size=0.50, drill=0.30))
    routes += make_route([(35.80, 73.40), P("U_C6", 20)], 0.25, "F.Cu", "/INTER_TX_M2S")

    routes += make_route([P("J1", 5), (58.16, 21.30)], 0.25, "B.Cu", "/INTER_RX_S2M")
    routes.append(make_via(58.16, 21.30, "/INTER_RX_S2M", size=0.50, drill=0.30))
    routes += make_route([(58.16, 21.30), (35.20, 21.30)], 0.25, "F.Cu", "/INTER_RX_S2M")
    routes.append(make_via(35.20, 21.30, "/INTER_RX_S2M", size=0.50, drill=0.30))
    routes += make_route([(35.20, 21.30), (35.20, 81.02)], 0.25, "B.Cu", "/INTER_RX_S2M")
    routes.append(make_via(35.20, 81.02, "/INTER_RX_S2M", size=0.50, drill=0.30))
    routes += make_route([(35.20, 81.02), P("U_C6", 26)], 0.25, "F.Cu", "/INTER_RX_S2M")

    # Joystick Analog & Switch (/JOY_X, /JOY_Y, /JOY_SW)
    routes += make_route([P("J1", 3), (53.08, 18.20)], 0.30, "B.Cu", "/JOY_X")
    routes.append(make_via(53.08, 18.20, "/JOY_X", size=0.55, drill=0.30))
    routes += make_route([(53.08, 18.20), (72.7525, 18.20)], 0.30, "F.Cu", "/JOY_X")
    routes.append(make_via(72.7525, 18.20, "/JOY_X", size=0.55, drill=0.30))
    routes += make_route([(72.7525, 18.20), P("JOY1", 5)], 0.30, "B.Cu", "/JOY_X")

    routes += make_route([P("J1", 6), (60.70, 19.00)], 0.30, "B.Cu", "/JOY_Y")
    routes.append(make_via(60.70, 19.00, "/JOY_Y", size=0.55, drill=0.30))
    routes += make_route([(60.70, 19.00), (64.51, 19.00)], 0.30, "F.Cu", "/JOY_Y")
    routes.append(make_via(64.51, 19.00, "/JOY_Y", size=0.55, drill=0.30))
    routes += make_route([(64.51, 19.00), (64.51, 50.35)], 0.30, "B.Cu", "/JOY_Y")
    routes.append(make_via(64.51, 50.35, "/JOY_Y", size=0.55, drill=0.30))
    routes += make_route([(64.51, 50.35), (79.60, 50.35), (80.965, 51.715), P("JOY1", 2)], 0.30, "F.Cu", "/JOY_Y")

    routes += make_route([P("J2", 5), (58.16, 35.80)], 0.30, "B.Cu", "/JOY_SW")
    routes.append(make_via(58.16, 35.80, "/JOY_SW", size=0.55, drill=0.30))
    routes += make_route([(58.16, 35.80), (63.80, 35.80)], 0.30, "F.Cu", "/JOY_SW")
    routes.append(make_via(63.80, 35.80, "/JOY_SW", size=0.55, drill=0.30))
    routes += make_route([(63.80, 35.80), (63.80, 60.25)], 0.30, "B.Cu", "/JOY_SW")
    routes.append(make_via(63.80, 60.25, "/JOY_SW", size=0.55, drill=0.30))
    routes += make_route([(63.80, 60.25), (76.0025, 60.25), P("JOY1", 10)], 0.30, "F.Cu", "/JOY_SW")

    # 2. Sensor Bus (/WATER_SENS -> U_C6:11 & optional 2S /BATT_VOLT -> U_C6:27)
    routes += make_route([P("J_WATER", 2), (21.50, 61.85)], 0.25, "B.Cu", "/WATER_SENS")
    routes.append(make_via(21.50, 61.85, "/WATER_SENS", size=0.55, drill=0.30))
    routes += make_route([(21.50, 61.85), (32.80, 61.85)], 0.25, "F.Cu", "/WATER_SENS")
    routes.append(make_via(32.80, 61.85, "/WATER_SENS", size=0.55, drill=0.30))
    routes += make_route([(32.80, 61.85), (32.80, 66.30)], 0.25, "B.Cu", "/WATER_SENS")
    routes.append(make_via(32.80, 66.30, "/WATER_SENS", size=0.50, drill=0.30))
    routes += make_route([(32.80, 66.30), (56.50, 66.30)], 0.25, "F.Cu", "/WATER_SENS")
    routes.append(make_via(56.50, 66.30, "/WATER_SENS", size=0.50, drill=0.30))
    routes += make_route([(56.50, 66.30), (56.50, 70.86)], 0.25, "B.Cu", "/WATER_SENS")
    routes.append(make_via(56.50, 70.86, "/WATER_SENS", size=0.55, drill=0.30))
    routes += make_route([(56.50, 70.86), P("U_C6", 11)], 0.25, "F.Cu", "/WATER_SENS")

    routes += make_route([P("JP_2S_ADC", 2), (63.55, 90.85), (72.65, 90.85)], 0.22, "F.Cu", "/BATT_VOLT")
    routes.append(make_via(72.65, 90.85, "/BATT_VOLT", size=0.50, drill=0.30))
    routes += make_route([(72.65, 90.85), (72.65, 65.05)], 0.22, "B.Cu", "/BATT_VOLT")
    routes.append(make_via(72.65, 65.05, "/BATT_VOLT", size=0.50, drill=0.30))
    routes += make_route([(72.65, 65.05), (70.00, 65.05), (69.40, 65.65), (36.40, 65.65)], 0.22, "F.Cu", "/BATT_VOLT")
    routes.append(make_via(36.40, 65.65, "/BATT_VOLT", size=0.50, drill=0.30))
    routes += make_route([(36.40, 65.65), (36.40, 82.29)], 0.22, "B.Cu", "/BATT_VOLT")
    routes.append(make_via(36.40, 82.29, "/BATT_VOLT", size=0.55, drill=0.30))
    routes += make_route([(36.40, 82.29), P("U_C6", 27)], 0.22, "F.Cu", "/BATT_VOLT")

    # 3. Actuators & Sensors (Left Edge 3x5 RC Receiver Block -> U_C6:16..24, 15, 20, 26)
    # 3a. 0.60mm pitch on B.Cu X=14.20..16.60 & F.Cu Y=62.50..64.90 so tracks & vias have >=0.22mm clearance
    servo_left_map = [
        # (ref, u_pin, x_col, y_bus, x_via, y_pin, net)
        ("ESC", 24, 14.20, 62.50, 32.20, 78.48, "/SERVO_ESC"),
        ("RUD", 23, 14.80, 63.10, 31.60, 77.21, "/SERVO_RUDDER"),
        ("WIN", 21, 15.40, 63.70, 31.00, 74.67, "/SERVO_WINCH"),
        ("RCK", 19, 16.00, 64.30, 30.40, 72.13, "/SERVO_RACK"),
    ]
    for sref, upin, xc, yb, xv, yp, net in servo_left_map:
        sx, sy = P(sref, 3)
        routes += make_route([(sx, sy), (xc, sy)], 0.25, "F.Cu", net)
        routes.append(make_via(xc, sy, net, size=0.50, drill=0.30))
        if sref == "WIN":
            routes += make_route([(xc, 81.04), (xc, sy), (xc, yb)], 0.25, "B.Cu", net)
            routes.append(make_via(xc, 81.04, net, size=0.50, drill=0.30))
            routes += make_route([(xc, 81.04), P("J_WINCH_HUB", 5)], 0.25, "F.Cu", net)
        elif sref == "RCK":
            routes += make_route([(xc, 78.50), (xc, sy), (xc, yb)], 0.25, "B.Cu", net)
            routes.append(make_via(xc, 78.50, net, size=0.50, drill=0.30))
            routes += make_route([(xc, 78.50), P("J_WINCH_HUB", 6)], 0.25, "F.Cu", net)
        else:
            routes += make_route([(xc, sy), (xc, yb)], 0.25, "B.Cu", net)
        routes.append(make_via(xc, yb, net, size=0.50, drill=0.30))
        routes += make_route([(xc, yb), (xv, yb)], 0.25, "F.Cu", net)
        routes.append(make_via(xv, yb, net, size=0.50, drill=0.30))
        routes += make_route([(xv, yb), (xv, yp)], 0.25, "B.Cu", net)
        routes.append(make_via(xv, yp, net, size=0.50, drill=0.30))
        routes += make_route([(xv, yp), P("U_C6", upin)], 0.25, "F.Cu", net)

    # 3b. /ANCHOR_LIMIT, /STROBE_DIN (Y=70.25 clears SW_C6_BOOT:1), /ANCHOR_HALL (Y=71.20)
    routes += make_route([P("J_LIMIT", 2), (26.80, 67.70)], 0.25, "B.Cu", "/ANCHOR_LIMIT")
    routes.append(make_via(26.80, 67.70, "/ANCHOR_LIMIT", size=0.55, drill=0.30))
    routes += make_route([(26.80, 67.70), (35.90, 67.70), (36.52, 68.32), P("U_C6", 16)], 0.25, "F.Cu", "/ANCHOR_LIMIT")

    sx_strb, sy_strb = P("STRB", 3)
    routes += make_route([(sx_strb, sy_strb), (16.60, sy_strb)], 0.25, "F.Cu", "/STROBE_DIN")
    routes.append(make_via(16.60, sy_strb, "/STROBE_DIN", size=0.50, drill=0.30))
    routes += make_route([(16.60, sy_strb), (16.60, 64.90)], 0.25, "B.Cu", "/STROBE_DIN")
    routes.append(make_via(16.60, 64.90, "/STROBE_DIN", size=0.50, drill=0.30))
    routes += make_route([(16.60, 64.90), (29.80, 64.90)], 0.25, "F.Cu", "/STROBE_DIN")
    routes.append(make_via(29.80, 64.90, "/STROBE_DIN", size=0.50, drill=0.30))
    routes += make_route([(29.80, 64.90), (29.80, 70.25)], 0.25, "B.Cu", "/STROBE_DIN")
    routes.append(make_via(29.80, 70.25, "/STROBE_DIN", size=0.50, drill=0.30))
    routes += make_route([(29.80, 70.25), (35.95, 70.25), (36.61, 69.59), P("U_C6", 17)], 0.25, "F.Cu", "/STROBE_DIN")

    routes += make_route([P("J_WINCH_HUB", 4), (7.00, 83.58)], 0.25, "F.Cu", "/ANCHOR_HALL")
    routes.append(make_via(7.00, 83.58, "/ANCHOR_HALL", size=0.50, drill=0.30))
    routes += make_route([(7.00, 83.58), (7.00, 80.20)], 0.25, "B.Cu", "/ANCHOR_HALL")
    routes.append(make_via(7.00, 80.20, "/ANCHOR_HALL", size=0.50, drill=0.30))
    routes += make_route([(7.00, 80.20), (29.20, 80.20)], 0.25, "F.Cu", "/ANCHOR_HALL")
    routes.append(make_via(29.20, 80.20, "/ANCHOR_HALL", size=0.50, drill=0.30))
    routes += make_route([(29.20, 80.20), (29.20, 71.20)], 0.25, "B.Cu", "/ANCHOR_HALL")
    routes.append(make_via(29.20, 71.20, "/ANCHOR_HALL", size=0.50, drill=0.30))
    routes += make_route([(29.20, 71.20), (35.80, 71.20), (36.14, 70.86), P("U_C6", 18)], 0.25, "F.Cu", "/ANCHOR_HALL")

    # 4. /+3V3_C6, I2C (/EXP_IO1 SDA, /EXP_IO11 SCL), 2x ADS1115 Balance Monitor & U_LDO
    # /+3V3_C6 strict tree: U_LDO:2 -> B.Cu spine X=60.75 -> R_I2C_*, U_C6:2, J_IMU:2 & U_LDO:Tab -> C_LDO_OUT:1, U_LORA:3, C_LORA:1 & ADS1/2 -> J_WINCH_HUB:3
    routes += make_route([(60.75, 66.95), (65.40, 66.95), (68.00, 66.95)], 0.45, "F.Cu", "/+3V3_C6")
    routes.append(make_via(65.40, 66.95, "/+3V3_C6", size=0.50, drill=0.30))
    routes += make_route([(65.40, 66.95), (65.40, 73.25)], 0.35, "B.Cu", "/+3V3_C6")
    routes.append(make_via(65.40, 73.25, "/+3V3_C6", size=0.50, drill=0.30))
    routes.append(make_via(60.75, 66.95, "/+3V3_C6", size=0.55, drill=0.30))
    routes += make_route([(60.75, 71.22), P("R_IO8_PU", 2)], 0.35, "F.Cu", "/+3V3_C6")
    routes.append(make_via(60.75, 71.22, "/+3V3_C6", size=0.55, drill=0.30))
    routes += make_route([(60.75, 57.7125), (60.75, 62.4125), (60.75, 66.95), (60.75, 71.22), (60.75, 76.6875), (60.75, 82.29)], 0.40, "B.Cu", "/+3V3_C6")
    routes.append(make_via(60.75, 76.6875, "/+3V3_C6", size=0.55, drill=0.30))
    routes += make_route([(60.75, 76.6875), P("R_C6_EN", 2)], 0.30, "F.Cu", "/+3V3_C6")
    routes.append(make_via(60.75, 82.29, "/+3V3_C6", size=0.55, drill=0.30))
    routes += make_route([
        P("U_C6", 2), (56.20, 82.29), (57.14, 81.35), P("C_C6_VDD", 1),
        (59.10, 81.35), (60.04, 82.29), (60.75, 82.29),
    ], 0.35, "F.Cu", "/+3V3_C6")

    # U_LDO Tab -> (68.00, 66.95) -> B.Cu north (68.00, 63.35) -> F.Cu east Y=63.35 -> B.Cu drops to C_LDO_OUT:1, U_LORA:3 & C_LORA:1
    routes.append(make_via(68.00, 66.95, "/+3V3_C6", size=0.55, drill=0.30))
    routes += make_route([(68.00, 66.95), (68.00, 63.35)], 0.40, "B.Cu", "/+3V3_C6")
    routes.append(make_via(68.00, 63.35, "/+3V3_C6", size=0.55, drill=0.30))
    routes += make_route([(68.00, 63.35), (83.10, 63.35), (84.10, 63.35)], 0.35, "F.Cu", "/+3V3_C6")
    routes.append(make_via(83.10, 63.35, "/+3V3_C6", size=0.55, drill=0.30))
    routes += make_route([(83.10, 63.35), (83.10, 66.20)], 0.35, "B.Cu", "/+3V3_C6")
    routes.append(make_via(83.10, 66.20, "/+3V3_C6", size=0.55, drill=0.30))
    routes += make_route([(83.10, 66.20), P("C_LDO_OUT", 1)], 0.35, "F.Cu", "/+3V3_C6")
    routes.append(make_via(84.10, 63.35, "/+3V3_C6", size=0.55, drill=0.30))
    routes += make_route([(84.10, 63.35), (84.10, 80.60), (84.10, 88.80)], 0.35, "B.Cu", "/+3V3_C6")
    routes.append(make_via(84.10, 80.60, "/+3V3_C6", size=0.55, drill=0.30))
    routes += make_route([(84.10, 80.60), P("U_LORA", 3)], 0.35, "F.Cu", "/+3V3_C6")
    routes.append(make_via(84.10, 88.80, "/+3V3_C6", size=0.55, drill=0.30))
    routes += make_route([(84.10, 88.80), P("C_LORA", 1)], 0.35, "F.Cu", "/+3V3_C6")

    # Upper & middle /+3V3_C6 rails: R_I2C_SCL:1, R_I2C_SDA:1 -> C_ADS2:1 -> Y=59.95/60.85 -> ADS1/2 -> J_WINCH_HUB:3
    routes.append(make_via(60.75, 62.4125, "/+3V3_C6", size=0.55, drill=0.30))
    routes += make_route([(60.75, 62.4125), P("R_I2C_SCL", 1)], 0.30, "F.Cu", "/+3V3_C6")
    routes.append(make_via(60.75, 57.7125, "/+3V3_C6", size=0.55, drill=0.30))
    routes += make_route([
        (60.75, 57.7125), P("R_I2C_SDA", 1), (56.60, 57.7125), (56.60, 60.45), P("C_ADS2", 1),
        (53.80, 60.45), (53.20, 60.45), (52.70, 59.95), (42.60, 59.95), (41.70, 60.85), (30.00, 60.85), (28.10, 60.85),
    ], 0.25, "F.Cu", "/+3V3_C6")
    routes.append(make_via(30.00, 60.85, "/+3V3_C6", size=0.55, drill=0.30))
    routes += make_route([(30.00, 60.85), (30.00, 20.00)], 0.35, "B.Cu", "/+3V3_C6")
    routes.append(make_via(30.00, 20.00, "/+3V3_C6", size=0.55, drill=0.30))
    routes += make_route([(30.00, 20.00), P("J_IMU", 2)], 0.35, "F.Cu", "/+3V3_C6")
    # Tap U_ADS2:1 via (53.80, 60.45) -> B.Cu -> (53.80, 58.40) -> F.Cu U_ADS2:1 (no F.Cu T-junction!)
    routes.append(make_via(53.80, 60.45, "/+3V3_C6", size=0.50, drill=0.30))
    routes += make_route([(53.80, 60.45), (53.80, 58.40)], 0.22, "B.Cu", "/+3V3_C6")
    routes.append(make_via(53.80, 58.40, "/+3V3_C6", size=0.50, drill=0.30))
    routes += make_route([(53.80, 58.40), P("U_ADS2", 1)], 0.22, "F.Cu", "/+3V3_C6")

    # Tap U_ADS2:8, C_ADS1:1, U_ADS1:8 vertically via B.Cu stubs into F.Cu Y=59.95 (no 90-deg T-junctions!)
    routes.append(make_via(51.80, 59.95, "/+3V3_C6", size=0.50, drill=0.30))
    routes += make_route([(51.80, 59.95), (51.80, 61.40)], 0.22, "B.Cu", "/+3V3_C6")
    routes.append(make_via(51.80, 61.40, "/+3V3_C6", size=0.50, drill=0.30))
    routes += make_route([(51.80, 61.40), P("U_ADS2", 8)], 0.22, "F.Cu", "/+3V3_C6")

    routes.append(make_via(48.90, 59.95, "/+3V3_C6", size=0.50, drill=0.30))
    routes += make_route([(48.90, 59.95), (48.90, 59.35)], 0.22, "B.Cu", "/+3V3_C6")
    routes.append(make_via(48.90, 59.35, "/+3V3_C6", size=0.50, drill=0.30))
    routes += make_route([(48.90, 59.35), P("C_ADS1", 1)], 0.22, "F.Cu", "/+3V3_C6")

    routes.append(make_via(44.20, 59.95, "/+3V3_C6", size=0.50, drill=0.30))
    routes += make_route([(44.20, 59.95), (44.20, 60.80)], 0.22, "B.Cu", "/+3V3_C6")
    routes.append(make_via(44.20, 60.80, "/+3V3_C6", size=0.50, drill=0.30))
    routes += make_route([(44.20, 60.80), P("U_ADS1", 8)], 0.22, "F.Cu", "/+3V3_C6")

    routes.append(make_via(28.10, 60.85, "/+3V3_C6", size=0.60, drill=0.32))
    routes += make_route([(28.10, 60.85), (28.10, 86.12)], 0.35, "B.Cu", "/+3V3_C6")
    routes.append(make_via(28.10, 86.12, "/+3V3_C6", size=0.60, drill=0.32))
    routes += make_route([(28.10, 86.12), P("J_WINCH_HUB", 3)], 0.40, "F.Cu", "/+3V3_C6")

    # I2C SDA (/EXP_IO1): F.Cu horiz Y=65.05 (44.70..60.00) + B.Cu spine X=60.00 (37.20..73.85) -> upper-left -> J_IMU:3
    routes += make_route([P("U_ADS1", 9), (44.70, 65.05), (60.00, 65.05)], 0.22, "F.Cu", "/EXP_IO1")
    routes += make_route([P("U_ADS2", 9), (52.30, 64.25)], 0.20, "F.Cu", "/EXP_IO1")
    routes.append(make_via(52.30, 64.25, "/EXP_IO1", size=0.50, drill=0.30))
    routes += make_route([(52.30, 64.25), (52.30, 65.05)], 0.20, "B.Cu", "/EXP_IO1")
    routes.append(make_via(52.30, 65.05, "/EXP_IO1", size=0.50, drill=0.30))
    routes.append(make_via(60.00, 65.05, "/EXP_IO1", size=0.50, drill=0.30))
    routes += make_route([(59.75, 36.95), (60.00, 37.20), (60.00, 55.8875), (60.00, 65.05), (60.00, 73.85)], 0.25, "B.Cu", "/EXP_IO1")
    routes.append(make_via(59.75, 36.95, "/EXP_IO1", size=0.50, drill=0.30))
    routes += make_route([(59.75, 36.95), (32.65, 36.95)], 0.20, "F.Cu", "/EXP_IO1")
    routes.append(make_via(32.65, 36.95, "/EXP_IO1", size=0.50, drill=0.30))
    routes += make_route([(32.65, 36.95), (32.40, 36.70), (32.40, 22.75)], 0.22, "B.Cu", "/EXP_IO1")
    routes.append(make_via(32.40, 22.75, "/EXP_IO1", size=0.50, drill=0.30))
    routes += make_route([(32.40, 22.75), (32.15, 22.50), P("J_IMU", 3)], 0.22, "F.Cu", "/EXP_IO1")
    routes.append(make_via(60.00, 55.8875, "/EXP_IO1", size=0.50, drill=0.30))
    routes += make_route([(60.00, 55.8875), P("R_I2C_SDA", 2)], 0.25, "F.Cu", "/EXP_IO1")
    routes.append(make_via(60.00, 73.85, "/EXP_IO1", size=0.50, drill=0.30))
    routes += make_route([(60.00, 73.85), (56.60, 73.85), (56.15, 73.40), P("U_C6", 9)], 0.22, "F.Cu", "/EXP_IO1")

    # I2C SCL (/EXP_IO11): U_ADS1:10 -> B.Cu -> F.Cu Y=60.70/61.45 -> U_ADS2:10 -> R_I2C_SCL:2 -> B.Cu spine X=61.55 (36.63..69.00) -> U_C6:12 & J_IMU:4
    routes += make_route([P("U_ADS1", 10), (46.30, 62.60)], 0.20, "F.Cu", "/EXP_IO11")
    routes.append(make_via(46.30, 62.60, "/EXP_IO11", size=0.50, drill=0.30))
    routes += make_route([(46.30, 62.60), (46.30, 60.70)], 0.22, "B.Cu", "/EXP_IO11")
    routes.append(make_via(46.30, 60.70, "/EXP_IO11", size=0.50, drill=0.30))
    routes += make_route([
        (46.30, 60.70), (52.55, 60.70), (53.30, 61.45), (53.80, 61.45), (57.30, 61.45),
        (58.1625, 60.5875), P("R_I2C_SCL", 2), (61.55, 60.5875),
    ], 0.20, "F.Cu", "/EXP_IO11")
    routes.append(make_via(53.80, 61.45, "/EXP_IO11", size=0.50, drill=0.30))
    routes += make_route([(53.80, 61.45), (53.80, 62.60)], 0.20, "B.Cu", "/EXP_IO11")
    routes.append(make_via(53.80, 62.60, "/EXP_IO11", size=0.50, drill=0.30))
    routes += make_route([(53.80, 62.60), P("U_ADS2", 10)], 0.20, "F.Cu", "/EXP_IO11")
    routes.append(make_via(61.55, 60.5875, "/EXP_IO11", size=0.50, drill=0.30))
    routes += make_route([(61.30, 36.38), (61.55, 36.63), (61.55, 60.5875), (61.55, 69.00)], 0.25, "B.Cu", "/EXP_IO11")
    routes.append(make_via(61.30, 36.38, "/EXP_IO11", size=0.50, drill=0.30))
    routes += make_route([(61.30, 36.38), (31.45, 36.38)], 0.20, "F.Cu", "/EXP_IO11")
    routes.append(make_via(31.45, 36.38, "/EXP_IO11", size=0.50, drill=0.30))
    routes += make_route([(31.45, 36.38), (31.20, 36.13), (31.20, 25.25)], 0.22, "B.Cu", "/EXP_IO11")
    routes.append(make_via(31.20, 25.25, "/EXP_IO11", size=0.50, drill=0.30))
    routes += make_route([(31.20, 25.25), (30.95, 25.00), P("J_IMU", 4)], 0.22, "F.Cu", "/EXP_IO11")
    routes.append(make_via(61.55, 69.00, "/EXP_IO11", size=0.50, drill=0.30))
    routes += make_route([(61.55, 69.00), (57.09, 69.00), (56.50, 69.59), P("U_C6", 12)], 0.25, "F.Cu", "/EXP_IO11")

    # J_BAL_MOT (Left Edge 90-deg lateral) & 6-channel analog front-end
    routes += make_route([
        P("J_BAL_MOT", 1), (4.40, 60.40), (5.20, 61.20), P("F_BAL", 1),
    ], 0.50, "F.Cu", "/BAL_GND")

    # /BAL_C1..C6: B.Cu vertical up X=5.8..8.8 -> via -> F.Cu horizontal across Y=37.5..40.5 -> R_C*T:1
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
        routes += make_route([(px, py), (xv, py - (xv - px)), (xv, yh)], 0.25, "B.Cu", net)
        routes.append(make_via(xv, yh, net, size=0.55, drill=0.30))
        routes += make_route([(xv, yh), (x_dst, yh), P(f"R_C{k}T", 1)], 0.28, "F.Cu", net)

    # /ADC_C1..C6: vertical spans on B.Cu; horizontal/short local ties on F.Cu
    routes += make_route([P("R_C1T", 2), (38.45, 44.113)], 0.20, "F.Cu", "/ADC_C1")
    routes.append(make_via(38.45, 44.113, "/ADC_C1", size=0.50, drill=0.30))
    routes += make_route([(38.45, 44.113), (38.45, 57.20)], 0.20, "B.Cu", "/ADC_C1")
    routes.append(make_via(38.45, 57.20, "/ADC_C1", size=0.50, drill=0.30))
    routes += make_route([(38.45, 57.20), P("R_C1B", 1), P("C_F1", 1), (38.80, 59.40)], 0.20, "F.Cu", "/ADC_C1")
    routes.append(make_via(38.80, 59.40, "/ADC_C1", size=0.50, drill=0.30))
    routes += make_route([(38.80, 59.40), (38.80, 64.50)], 0.20, "B.Cu", "/ADC_C1")
    routes.append(make_via(38.80, 64.50, "/ADC_C1", size=0.50, drill=0.30))
    routes += make_route([(38.80, 64.50), (43.70, 64.50), P("U_ADS1", 7)], 0.20, "F.Cu", "/ADC_C1", chamfer=0.25)

    routes += make_route([P("R_C2T", 2), (41.88, 45.60)], 0.20, "F.Cu", "/ADC_C2")
    routes.append(make_via(41.88, 45.60, "/ADC_C2", size=0.50, drill=0.30))
    routes += make_route([(41.88, 45.60), (41.88, 48.50)], 0.20, "B.Cu", "/ADC_C2")
    routes.append(make_via(41.88, 48.50, "/ADC_C2", size=0.50, drill=0.30))
    routes += make_route([(41.88, 48.50), (37.00, 48.50)], 0.20, "F.Cu", "/ADC_C2")
    routes.append(make_via(37.00, 48.50, "/ADC_C2", size=0.50, drill=0.30))
    routes += make_route([(37.00, 48.50), (37.00, 52.00)], 0.20, "B.Cu", "/ADC_C2")
    routes.append(make_via(37.00, 52.00, "/ADC_C2", size=0.50, drill=0.30))
    routes += make_route([(37.00, 52.00), P("R_C2B", 1), P("C_F2", 1), (37.35, 54.20)], 0.20, "F.Cu", "/ADC_C2")
    routes.append(make_via(37.35, 54.20, "/ADC_C2", size=0.50, drill=0.30))
    routes += make_route([(37.35, 54.20), (37.35, 63.95)], 0.20, "B.Cu", "/ADC_C2")
    routes.append(make_via(37.35, 63.95, "/ADC_C2", size=0.50, drill=0.30))
    routes += make_route([(37.35, 63.95), (43.20, 63.95), P("U_ADS1", 6)], 0.20, "F.Cu", "/ADC_C2", chamfer=0.25)

    routes += make_route([P("R_C3T", 2), (44.42, 45.20), (42.75, 45.20)], 0.20, "F.Cu", "/ADC_C3")
    routes.append(make_via(42.75, 45.20, "/ADC_C3", size=0.50, drill=0.30))
    routes += make_route([(42.75, 45.20), (42.75, 51.60)], 0.20, "B.Cu", "/ADC_C3")
    routes.append(make_via(42.75, 51.60, "/ADC_C3", size=0.50, drill=0.30))
    routes += make_route([(42.75, 51.60), P("R_C3B", 1), P("C_F3", 1), (43.20, 55.45), P("U_ADS1", 5)], 0.20, "F.Cu", "/ADC_C3")

    routes += make_route([P("R_C4T", 2), (46.96, 45.30), (45.0875, 45.30)], 0.20, "F.Cu", "/ADC_C4")
    routes.append(make_via(45.0875, 45.30, "/ADC_C4", size=0.50, drill=0.30))
    routes += make_route([(45.0875, 45.30), (45.0875, 47.00)], 0.20, "B.Cu", "/ADC_C4")
    routes.append(make_via(45.0875, 47.00, "/ADC_C4", size=0.50, drill=0.30))
    routes += make_route([(45.0875, 47.00), P("R_C4B", 1), P("C_F4", 1), (43.70, 51.75)], 0.20, "F.Cu", "/ADC_C4")
    routes.append(make_via(43.70, 51.75, "/ADC_C4", size=0.50, drill=0.30))
    routes += make_route([(43.70, 51.75), (43.70, 56.60)], 0.20, "B.Cu", "/ADC_C4")
    routes.append(make_via(43.70, 56.60, "/ADC_C4", size=0.50, drill=0.30))
    routes += make_route([(43.70, 56.60), P("U_ADS1", 4)], 0.20, "F.Cu", "/ADC_C4")

    routes += make_route([P("R_C5T", 2), (49.50, 45.30), (50.7125, 45.30)], 0.20, "F.Cu", "/ADC_C5")
    routes.append(make_via(50.7125, 45.30, "/ADC_C5", size=0.50, drill=0.30))
    routes += make_route([(50.7125, 45.30), (50.7125, 51.55)], 0.20, "B.Cu", "/ADC_C5")
    routes.append(make_via(50.7125, 51.55, "/ADC_C5", size=0.50, drill=0.30))
    routes += make_route([(50.7125, 51.55), P("R_C5B", 1), P("C_F5", 1), (50.80, 55.05), P("U_ADS2", 5)], 0.20, "F.Cu", "/ADC_C5")

    routes += make_route([P("R_C6T", 2), (52.4875, 44.55), (52.4875, 45.30)], 0.20, "F.Cu", "/ADC_C6")
    routes.append(make_via(52.4875, 45.30, "/ADC_C6", size=0.50, drill=0.30))
    routes += make_route([(52.4875, 45.30), (52.4875, 47.00)], 0.20, "B.Cu", "/ADC_C6")
    routes.append(make_via(52.4875, 47.00, "/ADC_C6", size=0.50, drill=0.30))
    routes += make_route([(52.4875, 47.00), P("R_C6B", 1), P("C_F6", 1), (52.4875, 51.80)], 0.20, "F.Cu", "/ADC_C6")
    routes.append(make_via(52.4875, 51.80, "/ADC_C6", size=0.50, drill=0.30))
    routes += make_route([(52.4875, 51.80), (52.4875, 55.80)], 0.20, "B.Cu", "/ADC_C6")
    routes.append(make_via(52.4875, 55.80, "/ADC_C6", size=0.50, drill=0.30))
    routes += make_route([(52.4875, 55.80), (51.30, 55.80), P("U_ADS2", 4)], 0.20, "F.Cu", "/ADC_C6")

    # Local /GND ties & stitching vias (45-deg chamfered, loop-free)
    routes += make_route([P("U_ADS1", 3), (44.20, 56.90), (45.20, 56.90), P("U_ADS1", 1)], 0.20, "F.Cu", "/GND")
    routes.append(make_via(45.20, 57.25, "/GND", size=0.50, drill=0.30))

    routes += make_route([P("U_ADS2", 3), (51.80, 56.90)], 0.20, "F.Cu", "/GND")
    routes.append(make_via(51.80, 56.90, "/GND", size=0.50, drill=0.30))

    routes += make_route([P("U_ADS2", 7), P("U_ADS2", 6), (49.50, 62.60)], 0.20, "F.Cu", "/GND")
    routes.append(make_via(49.50, 62.60, "/GND", size=0.50, drill=0.30))

    bal_gnd_pairs = [
        ((34.25, 58.30), P("R_C1B", 2), P("C_F1", 2), 0.30),
        ((34.25, 53.10), P("R_C2B", 2), P("C_F2", 2), 0.30),
        ((40.85, 53.90), P("R_C3B", 2), P("C_F3", 2), 0.30),
        ((46.93, 49.30), P("R_C4B", 2), P("C_F4", 2), 0.30),
        ((48.87, 53.90), P("R_C5B", 2), P("C_F5", 2), 0.30),
        ((54.33, 49.30), P("R_C6B", 2), P("C_F6", 2), 0.30),
    ]
    for (vx, vy), p1, p2, tw in bal_gnd_pairs:
        routes.append(make_via(vx, vy, "/GND", size=0.50, drill=0.30))
        routes += make_route([p1, (vx, vy), p2], tw, "F.Cu", "/GND")

    for (vx, vy), pt, tw in [
        ((11.60, 61.20), P("F_BAL", 2), 0.40),
        ((46.60, 56.45), P("C_ADS1", 2), 0.30),
        ((55.40, 57.60), P("C_ADS2", 2), 0.30),
    ]:
        routes.append(make_via(vx, vy, "/GND", size=0.55, drill=0.30))
        routes += make_route([(vx, vy), pt], tw, "F.Cu", "/GND")

    # 5. /+5V_SERVO (Star from THT J_PWR_SRV:2 -> B.Cu south to J_WINCH_HUB:2 & vertical B.Cu bus along Col 2 X=6.94)
    routes += make_route([P("J_PWR_SRV", 2), (14.96, 88.66)], 0.60, "B.Cu", "/+5V_SERVO")
    routes.append(make_via(14.96, 88.66, "/+5V_SERVO", size=0.70, drill=0.35))
    routes += make_route([(14.96, 88.66), P("J_WINCH_HUB", 2)], 0.60, "F.Cu", "/+5V_SERVO")

    routes += make_route([P("ESC", 2), P("RUD", 2), P("WIN", 2), P("RCK", 2), P("STRB", 2), (6.94, 76.80)], 0.70, "B.Cu", "/+5V_SERVO")
    routes.append(make_via(6.94, 76.80, "/+5V_SERVO", size=0.70, drill=0.35))
    routes += make_route([(6.94, 76.80), (12.80, 76.80)], 0.60, "F.Cu", "/+5V_SERVO")
    routes.append(make_via(12.80, 76.80, "/+5V_SERVO", size=0.70, drill=0.35))
    routes += make_route([(12.80, 76.80), (12.80, 84.50)], 0.60, "B.Cu", "/+5V_SERVO")
    routes.append(make_via(12.80, 84.50, "/+5V_SERVO", size=0.70, drill=0.35))
    routes += make_route([(12.80, 84.50), P("J_PWR_SRV", 2)], 0.60, "F.Cu", "/+5V_SERVO")

    # 7. Breakaway Sub-Board Routing (isolated /HALL_*, /SUB_* nets; F.Cu=Horiz, B.Cu=Vert cross-unders)
    routes += make_route([P("J_WINCH_SUB", 2), (18.54, 98.25)], 0.55, "B.Cu", "/SUB_5V")
    routes.append(make_via(18.54, 98.25, "/SUB_5V", size=0.65, drill=0.35))
    routes += make_route([
        (18.54, 98.25), (30.50, 98.25), (31.40, 97.35), P("C_SRV", 1), (32.00, 97.35), (32.90, 98.25), (46.14, 98.25),
    ], 0.50, "F.Cu", "/SUB_5V")
    routes.append(make_via(37.24, 98.25, "/SUB_5V", size=0.60, drill=0.30))
    routes += make_route([(37.24, 98.25), P("SRV_WIN", 2)], 0.55, "B.Cu", "/SUB_5V")
    routes.append(make_via(46.14, 98.25, "/SUB_5V", size=0.60, drill=0.30))
    routes += make_route([(46.14, 98.25), P("SRV_RCK", 2)], 0.55, "B.Cu", "/SUB_5V")

    routes += make_route([P("J_WINCH_SUB", 5), (26.16, 94.60)], 0.25, "B.Cu", "/SUB_WIN")
    routes.append(make_via(26.16, 94.60, "/SUB_WIN", size=0.50, drill=0.30))
    routes += make_route([(26.16, 94.60), (39.78, 94.60)], 0.25, "F.Cu", "/SUB_WIN")
    routes.append(make_via(39.78, 94.60, "/SUB_WIN", size=0.50, drill=0.30))
    routes += make_route([(39.78, 94.60), P("SRV_WIN", 3)], 0.25, "B.Cu", "/SUB_WIN")

    routes += make_route([P("J_WINCH_SUB", 6), (28.70, 93.45)], 0.25, "B.Cu", "/SUB_RCK")
    routes.append(make_via(28.70, 93.45, "/SUB_RCK", size=0.50, drill=0.30))
    routes += make_route([(28.70, 93.45), (48.68, 93.45)], 0.25, "F.Cu", "/SUB_RCK")
    routes.append(make_via(48.68, 93.45, "/SUB_RCK", size=0.50, drill=0.30))
    routes += make_route([(48.68, 93.45), P("SRV_RCK", 3)], 0.25, "B.Cu", "/SUB_RCK")

    routes += make_route([P("J_WINCH_SUB", 3), (21.08, 99.15)], 0.35, "B.Cu", "/HALL_VCC")
    routes.append(make_via(21.08, 99.15, "/HALL_VCC", size=0.55, drill=0.30))
    routes += make_route([
        (21.08, 99.15), (72.50, 99.15), (73.50, 98.15), (73.50, 95.45), P("C_HALL", 2), P("U_HALL", 1),
    ], 0.35, "F.Cu", "/HALL_VCC")

    routes += make_route([P("J_WINCH_SUB", 4), (23.62, 94.03)], 0.25, "B.Cu", "/HALL_OUT")
    routes.append(make_via(23.62, 94.03, "/HALL_OUT", size=0.50, drill=0.30))
    routes += make_route([(23.62, 94.03), (83.80, 94.03)], 0.25, "F.Cu", "/HALL_OUT")
    routes.append(make_via(83.80, 94.03, "/HALL_OUT", size=0.50, drill=0.30))
    routes += make_route([(83.80, 94.03), (83.80, 97.35)], 0.25, "B.Cu", "/HALL_OUT")
    routes.append(make_via(83.80, 97.35, "/HALL_OUT", size=0.50, drill=0.30))
    routes += make_route([(83.80, 97.35), P("U_HALL", 2)], 0.25, "F.Cu", "/HALL_OUT")

    routes += make_route([P("C_HALL", 1), (75.00, 98.60)], 0.35, "F.Cu", "/HALL_GND")
    routes += make_route([P("U_HALL", 3), (88.65, 96.40)], 0.35, "F.Cu", "/HALL_GND")
    for gx, gy in [(15.00, 94.20), (51.20, 96.40), (75.00, 98.60), (88.65, 96.40)]:
        routes.append(make_via(gx, gy, "/HALL_GND", size=0.55, drill=0.30))

    # 8. USB-C + Solar + IP2326 Charger + 2S LiPo + MT2492 5V Buck Routing
    routes += make_route([P("J_USBC", "A5"), (92.80, 37.75), (92.15, 38.40), P("R_CC1", 2)], 0.20, "F.Cu", "/USB_CC1")
    routes += make_route([P("J_USBC", "B5"), (92.45, 34.75), (91.925, 34.225), P("R_CC2", 2)], 0.20, "F.Cu", "/USB_CC2")

    # USB DP & DN: local A/B pairing -> B.Cu vert X=84.70/87.00 -> F.Cu horiz Y=58.85/59.45 -> B.Cu vert X=57.60/58.30 -> U_C6:14,13
    routes += make_route([P("J_USBC", "B6"), (92.80, 35.75), (92.30, 35.20), (92.05, 35.20)], 0.20, "F.Cu", "/USB_DP")
    routes.append(make_via(92.05, 35.20, "/USB_DP", size=0.50, drill=0.30))
    routes += make_route([P("J_USBC", "A6"), (92.40, 36.75), (92.05, 36.60), (90.75, 36.60)], 0.20, "F.Cu", "/USB_DP")
    routes.append(make_via(90.75, 36.60, "/USB_DP", size=0.50, drill=0.30))
    routes += make_route([(92.05, 35.20), (90.75, 35.20), (90.75, 36.60), (90.75, 44.45)], 0.20, "B.Cu", "/USB_DP")
    routes.append(make_via(90.75, 44.45, "/USB_DP", size=0.50, drill=0.30))
    routes += make_route([(90.75, 44.45), (84.70, 44.45)], 0.20, "F.Cu", "/USB_DP")
    routes.append(make_via(84.70, 44.45, "/USB_DP", size=0.50, drill=0.30))
    routes += make_route([(84.70, 44.45), (84.70, 58.85)], 0.20, "B.Cu", "/USB_DP")
    routes.append(make_via(84.70, 58.85, "/USB_DP", size=0.50, drill=0.30))
    routes += make_route([(84.70, 58.85), (57.60, 58.85)], 0.20, "F.Cu", "/USB_DP")
    routes.append(make_via(57.60, 58.85, "/USB_DP", size=0.50, drill=0.30))
    routes += make_route([(57.60, 58.85), (57.60, 67.05)], 0.20, "B.Cu", "/USB_DP")
    routes.append(make_via(57.60, 67.05, "/USB_DP", size=0.50, drill=0.30))
    routes += make_route([(57.60, 67.05), P("U_C6", 14)], 0.20, "F.Cu", "/USB_DP")

    routes += make_route([P("J_USBC", "A7"), (92.55, 36.25), (92.20, 35.90), (91.75, 35.90)], 0.20, "F.Cu", "/USB_DN")
    routes.append(make_via(91.75, 35.90, "/USB_DN", size=0.50, drill=0.30))
    routes += make_route([P("J_USBC", "B7"), (91.75, 37.25)], 0.20, "F.Cu", "/USB_DN")
    routes.append(make_via(91.75, 37.25, "/USB_DN", size=0.50, drill=0.30))
    routes += make_route([(91.75, 35.90), (91.75, 37.25), (91.75, 45.05)], 0.20, "B.Cu", "/USB_DN")
    routes.append(make_via(91.75, 45.05, "/USB_DN", size=0.50, drill=0.30))
    routes += make_route([(91.75, 45.05), (87.00, 45.05)], 0.20, "F.Cu", "/USB_DN")
    routes.append(make_via(87.00, 45.05, "/USB_DN", size=0.50, drill=0.30))
    routes += make_route([(87.00, 45.05), (87.00, 59.45)], 0.20, "B.Cu", "/USB_DN")
    routes.append(make_via(87.00, 59.45, "/USB_DN", size=0.50, drill=0.30))
    routes += make_route([(87.00, 59.45), (58.30, 59.45)], 0.20, "F.Cu", "/USB_DN")
    routes.append(make_via(58.30, 59.45, "/USB_DN", size=0.50, drill=0.30))
    routes += make_route([(58.30, 59.45), (58.30, 68.32)], 0.20, "B.Cu", "/USB_DN")
    routes.append(make_via(58.30, 68.32, "/USB_DN", size=0.50, drill=0.30))
    routes += make_route([(58.30, 68.32), P("U_C6", 13)], 0.20, "F.Cu", "/USB_DN")

    # VBUS_USB (B4/A4 -> D_USB:2 & B.Cu vert X=87.80 -> F.Cu D_LDO_USB:2)
    routes += make_route([P("J_USBC", "B4"), (92.90, 34.05)], 0.30, "F.Cu", "/VBUS_USB")
    routes.append(make_via(92.90, 34.05, "/VBUS_USB", size=0.50, drill=0.30))
    routes += make_route([P("J_USBC", "A4"), (92.90, 38.95)], 0.30, "F.Cu", "/VBUS_USB")
    routes.append(make_via(92.90, 38.95, "/VBUS_USB", size=0.50, drill=0.30))
    routes += make_route([(92.90, 45.65), (92.90, 38.95), (92.90, 34.05), (92.90, 30.95)], 0.40, "B.Cu", "/VBUS_USB")
    routes.append(make_via(92.90, 30.95, "/VBUS_USB", size=0.50, drill=0.30))
    routes += make_route([(92.90, 30.95), (94.00, 30.95), P("D_USB", 2)], 0.40, "F.Cu", "/VBUS_USB")
    routes.append(make_via(92.90, 45.65, "/VBUS_USB", size=0.50, drill=0.30))
    routes += make_route([(92.90, 45.65), (87.80, 45.65)], 0.30, "F.Cu", "/VBUS_USB")
    routes.append(make_via(87.80, 45.65, "/VBUS_USB", size=0.50, drill=0.30))
    routes += make_route([(87.80, 45.65), (87.80, 67.80)], 0.30, "B.Cu", "/VBUS_USB")
    routes.append(make_via(87.80, 67.80, "/VBUS_USB", size=0.55, drill=0.30))
    routes += make_route([(87.80, 67.80), (76.85, 67.80), P("D_LDO_USB", 2)], 0.30, "F.Cu", "/VBUS_USB")
    routes += make_route([P("R_CC1", 1), (91.00, 40.025), (91.50, 39.75), P("J_USBC", "A1")], 0.30, "F.Cu", "/GND")
    routes += make_route([P("R_CC2", 1), (92.725, 32.575), (93.40, 33.25), P("J_USBC", "B1")], 0.30, "F.Cu", "/GND")

    # VSOLAR_IN: J_SOLAR:2 -> B.Cu vertical around J_SOLAR:1 -> via -> F.Cu D_SOL:2
    routes += make_route([P("J_SOLAR", 2), (97.80, 11.90), (97.80, 15.65), (97.30, 16.15)], 0.50, "B.Cu", "/VSOLAR_IN")
    routes.append(make_via(97.30, 16.15, "/VSOLAR_IN", size=0.65, drill=0.35))
    routes += make_route([(97.30, 16.15), P("D_SOL", 2)], 0.50, "F.Cu", "/VSOLAR_IN")

    # /VIN_CHG (loop-free strict tree with 180-deg straight pass through C_CHGIN:1 & R_VIN:1)
    routes += make_route([P("D_SOL", 1), (97.35, 21.20), P("L1", 1)], 0.70, "F.Cu", "/VIN_CHG")
    routes += make_route([P("L1", 1), (97.50, 24.40), P("R_CEN", 1)], 0.30, "F.Cu", "/VIN_CHG")
    routes += make_route([P("D_SOL", 1), (92.20, 19.45)], 0.50, "F.Cu", "/VIN_CHG")
    routes.append(make_via(92.20, 19.45, "/VIN_CHG", size=0.65, drill=0.35))
    routes += make_route([(92.20, 19.45), (92.20, 15.9875)], 0.45, "B.Cu", "/VIN_CHG")
    routes.append(make_via(92.20, 15.9875, "/VIN_CHG", size=0.55, drill=0.30))
    routes += make_route([(92.20, 15.9875), P("R_VIN", 1), (88.40, 15.9875)], 0.35, "F.Cu", "/VIN_CHG")
    routes.append(make_via(88.40, 15.9875, "/VIN_CHG", size=0.55, drill=0.30))
    routes += make_route([(88.40, 15.9875), (88.40, 22.80)], 0.45, "B.Cu", "/VIN_CHG")
    routes.append(make_via(88.40, 22.80, "/VIN_CHG", size=0.60, drill=0.32))
    routes += make_route([
        (88.40, 22.80), (89.45, 22.80), P("C_CHGIN", 1), (91.45, 24.80), (91.45, 29.40), P("D_USB", 1),
    ], 0.50, "F.Cu", "/VIN_CHG")

    # /CHG_LX: U_CHG:15..17 -> chamfered exit -> via (84.45, 33.80) -> B.Cu bridge -> via (93.65, 24.20) -> L1:2 & C_BST1:1
    routes += make_route([P("U_CHG", 15), P("U_CHG", 16), P("U_CHG", 17), (84.45, 33.80)], 0.35, "F.Cu", "/CHG_LX", chamfer=0.30)
    routes.append(make_via(84.45, 33.80, "/CHG_LX", size=0.60, drill=0.32))
    routes += make_route([(84.45, 33.80), (84.45, 28.55), (88.80, 24.20), (93.65, 24.20)], 0.60, "B.Cu", "/CHG_LX")
    routes.append(make_via(93.65, 24.20, "/CHG_LX", size=0.65, drill=0.35))
    routes += make_route([P("L1", 2), (93.65, 24.20), (94.45, 25.00), P("C_BST1", 1)], 0.50, "F.Cu", "/CHG_LX")

    # /CHG_BST: C_BST1:2 -> F.Cu -> via -> B.Cu -> via -> U_CHG:14
    ub = P("U_CHG", 14)
    cb2_x = P("C_BST1", 2)[0]
    routes += make_route([P("C_BST1", 2), (cb2_x, 25.50)], 0.25, "F.Cu", "/CHG_BST")
    routes.append(make_via(cb2_x, 25.50, "/CHG_BST", size=0.50, drill=0.30))
    routes += make_route([(cb2_x, 25.50), (89.30, 25.50), (ub[0], 28.85), (ub[0], 33.80)], 0.25, "B.Cu", "/CHG_BST")
    routes.append(make_via(ub[0], 33.80, "/CHG_BST", size=0.50, drill=0.30))
    routes += make_route([(ub[0], 33.80), ub], 0.22, "F.Cu", "/CHG_BST")

    # /CHG_VIN: R_VIN:2 -> C_VIN:1 -> via (80.50, 19.65) -> B.Cu -> (80.50, 30.65) -> F.Cu (86.65, 30.65) -> B.Cu (86.65, 34.65) -> F.Cu U_CHG:13
    uvin = P("U_CHG", 13)
    routes += make_route([P("R_VIN", 2), P("C_VIN", 1), (80.50, 19.65)], 0.25, "F.Cu", "/CHG_VIN")
    routes.append(make_via(80.50, 19.65, "/CHG_VIN", size=0.50, drill=0.30))
    routes += make_route([(80.50, 19.65), (80.50, 30.65)], 0.22, "B.Cu", "/CHG_VIN")
    routes.append(make_via(80.50, 30.65, "/CHG_VIN", size=0.50, drill=0.30))
    routes += make_route([(80.50, 30.65), (86.65, 30.65)], 0.22, "F.Cu", "/CHG_VIN")
    routes.append(make_via(86.65, 30.65, "/CHG_VIN", size=0.50, drill=0.30))
    routes += make_route([(86.65, 30.65), (86.65, 34.65)], 0.22, "B.Cu", "/CHG_VIN")
    routes.append(make_via(86.65, 34.65, "/CHG_VIN", size=0.50, drill=0.30))
    routes += make_route([(86.65, 34.65), (86.45, 34.85), uvin], 0.20, "F.Cu", "/CHG_VIN")

    # /CHG_EN, /CHG_ISET, /CHG_UV, /CHG_NTC, /CHG_LED, /CHG_LED_K, /CHG_VSYS
    uen = P("U_CHG", 12)
    routes += make_route([P("R_CEN", 2), (97.30, 28.35), (92.45, 28.35)], 0.22, "F.Cu", "/CHG_EN")
    routes.append(make_via(92.45, 28.35, "/CHG_EN", size=0.50, drill=0.30))
    routes += make_route([(92.45, 28.35), (87.80, 28.35), (87.45, 28.70), (87.45, 35.10)], 0.22, "B.Cu", "/CHG_EN")
    routes.append(make_via(87.45, 35.10, "/CHG_EN", size=0.50, drill=0.30))
    routes += make_route([(87.45, 35.10), (87.45, uen[1]), uen], 0.20, "F.Cu", "/CHG_EN")

    uiset = P("U_CHG", 11)
    routes += make_route([uiset, (88.00, uiset[1])], 0.20, "F.Cu", "/CHG_ISET")
    routes.append(make_via(88.00, uiset[1], "/CHG_ISET", size=0.50, drill=0.30))
    routes += make_route([(88.00, uiset[1]), (88.00, 30.80)], 0.20, "B.Cu", "/CHG_ISET")
    routes.append(make_via(88.00, 30.80, "/CHG_ISET", size=0.50, drill=0.30))
    routes += make_route([(88.00, 30.80), (89.50, 30.80), P("R_ISET", 1)], 0.20, "F.Cu", "/CHG_ISET")

    uuv = P("U_CHG", 8)
    ruv1 = P("R_UV", 1)
    routes += make_route([uuv, (ruv1[0], uuv[1]), ruv1], 0.20, "F.Cu", "/CHG_UV")

    untc = P("U_CHG", 4)
    routes += make_route([untc, (untc[0], 41.80), (P("R_NTC", 1)[0], 42.70), P("R_NTC", 1)], 0.20, "F.Cu", "/CHG_NTC")

    uled = P("U_CHG", 6)
    routes += make_route([uled, (uled[0], 39.95)], 0.20, "F.Cu", "/CHG_LED")
    routes.append(make_via(uled[0], 39.95, "/CHG_LED", size=0.50, drill=0.30))
    routes += make_route([(uled[0], 39.95), (86.10, 40.30), (82.80, 40.30), (82.80, 26.50)], 0.22, "B.Cu", "/CHG_LED")
    routes.append(make_via(82.80, 26.50, "/CHG_LED", size=0.50, drill=0.30))
    routes += make_route([(82.80, 26.50), (P("R_LED1", 1)[0], 26.50), P("R_LED1", 1)], 0.22, "F.Cu", "/CHG_LED")
    routes += make_route([P("R_LED1", 2), (P("R_LED1", 2)[0], 25.40), P("D_CHG", 2)], 0.22, "F.Cu", "/CHG_LED_K")

    routes += make_route([P("U_CHG", 20), P("U_CHG", 19), (83.2625, 33.71), (81.40, 31.85), P("C_SYS1", 1), P("C_SYS2", 1)], 0.40, "F.Cu", "/CHG_VSYS")

    # /VBAT_2S: U_CHG:22 -> U_CHG:21 -> C_BAT1:1 & (81.00, 46.50) -> F.Cu horiz -> B.Cu vert X=89.40 -> J_LIPO_2S:3 & F1:1
    routes += make_route([P("U_CHG", 22), P("U_CHG", 21), (81.00, 37.25), (77.30, 37.25), (75.85, 35.80)], 0.30, "F.Cu", "/VBAT_2S", chamfer=0.30)
    routes.append(make_via(75.85, 35.80, "/VBAT_2S", size=0.60, drill=0.32))
    routes += make_route([(75.85, 35.80), (75.85, 31.85)], 0.45, "B.Cu", "/VBAT_2S")
    routes.append(make_via(75.85, 31.85, "/VBAT_2S", size=0.60, drill=0.32))
    routes += make_route([(75.85, 31.85), P("C_BAT1", 1)], 0.45, "F.Cu", "/VBAT_2S")
    routes.append(make_via(81.00, 37.25, "/VBAT_2S", size=0.60, drill=0.32))
    routes += make_route([(81.00, 37.25), (81.00, 46.50)], 0.35, "B.Cu", "/VBAT_2S")
    routes.append(make_via(81.00, 46.50, "/VBAT_2S", size=0.65, drill=0.35))
    routes += make_route([(81.00, 46.50), (89.40, 46.50), (93.80, 46.50), (95.10, 45.20), P("J_LIPO_2S", 3)], 0.60, "F.Cu", "/VBAT_2S")
    routes.append(make_via(89.40, 46.50, "/VBAT_2S", size=0.70, drill=0.35))
    routes += make_route([(89.40, 46.50), (89.40, 54.40)], 0.65, "B.Cu", "/VBAT_2S")
    routes.append(make_via(89.40, 54.40, "/VBAT_2S", size=0.70, drill=0.35))
    routes += make_route([(89.40, 54.40), P("F1", 1)], 0.65, "F.Cu", "/VBAT_2S")

    # /CHG_VBATM & /VBAT_MID (F.Cu=Horiz, B.Cu=Vert via X=83.95 -> R_BAL -> J_LIPO_2S:2)
    routes += make_route([P("U_CHG", 23), (82.45, 38.25), (82.10, 38.60)], 0.25, "F.Cu", "/CHG_VBATM")
    routes.append(make_via(82.10, 38.60, "/CHG_VBATM", size=0.50, drill=0.30))
    routes += make_route([(82.10, 38.60), (82.10, 43.80)], 0.25, "B.Cu", "/CHG_VBATM")
    routes.append(make_via(82.10, 43.80, "/CHG_VBATM", size=0.50, drill=0.30))
    routes += make_route([(82.10, 43.80), (83.95, 43.80)], 0.25, "F.Cu", "/CHG_VBATM")
    routes.append(make_via(83.95, 43.80, "/CHG_VBATM", size=0.50, drill=0.30))
    routes += make_route([(83.95, 43.80), (83.95, 50.3125)], 0.25, "B.Cu", "/CHG_VBATM")
    routes.append(make_via(83.95, 50.3125, "/CHG_VBATM", size=0.50, drill=0.30))
    routes += make_route([(83.95, 50.3125), P("R_BAL", 1)], 0.25, "F.Cu", "/CHG_VBATM")
    routes += make_route([P("R_BAL", 2), (88.7875, 47.70), P("J_LIPO_2S", 2)], 0.35, "F.Cu", "/VBAT_MID")

    # U_CHG thermal vias & GND stitching vias across all PCB zones
    for tx, ty in [
        (84.60, 36.90), (85.80, 36.90), (84.60, 38.10), (85.80, 38.10),
        (87.55, 40.25),
        (89.60, 31.80), (86.50, 22.00), (75.50, 29.50), (79.20, 35.35), (81.40, 35.35),
        (77.20, 24.00), (89.20, 74.00), (96.50, 76.10), (98.75, 79.75), (97.50, 84.50),
        (89.10, 64.50), (97.00, 69.20), (61.50, 90.50),
        (4.00, 62.00), (72.00, 11.00), (89.00, 11.00), (98.50, 96.00), (2.50, 96.00), (30.00, 89.50),
        (2.00, 2.00), (98.50, 2.00), (98.50, 98.50), (2.00, 98.50),
        # U_C6 EPAD 4x thermal vias & local U_C6/LDO/SW_C6_* GND stitching
        (45.50, 74.80), (47.50, 74.80), (45.50, 76.80), (47.50, 76.80),
        (56.60, 83.56), (36.20, 83.56),
        (34.50, 69.00), (33.20, 79.55), (33.20, 89.60),
        (61.80, 73.25), (65.00, 75.55), (78.40, 66.20), (89.20, 88.80),
        (85.20, 84.60), (85.20, 82.60), (73.60, 70.60), (73.60, 84.60),
    ]:
        routes.append(make_via(tx, ty, "/GND", size=0.55, drill=0.30))
    routes += make_route([(98.75, 79.75), P("C_BOUT1", 2)], 0.30, "F.Cu", "/GND")
    routes += make_route([P("U_C6", 1), (56.60, 83.56), P("C_C6_VDD", 2)], 0.30, "F.Cu", "/GND")
    routes += make_route([P("U_C6", 28), (36.20, 83.56)], 0.30, "F.Cu", "/GND")
    routes += make_route([P("U_LDO", 1), (61.80, 73.25)], 0.35, "F.Cu", "/GND")
    routes += make_route([P("C_LDO_IN", 2), (65.00, 75.55)], 0.35, "F.Cu", "/GND")
    routes += make_route([P("C_LDO_OUT", 2), (78.40, 66.20)], 0.35, "F.Cu", "/GND")
    routes += make_route([P("C_LORA", 2), (89.20, 88.80)], 0.35, "F.Cu", "/GND")
    routes += make_route([P("U_LORA", 1), (85.20, 84.60)], 0.35, "F.Cu", "/GND")
    routes += make_route([P("U_LORA", 2), (85.20, 82.60)], 0.35, "F.Cu", "/GND")
    routes += make_route([P("U_LORA", 9), (73.60, 70.60)], 0.35, "F.Cu", "/GND")
    routes += make_route([P("U_LORA", 16), (73.60, 84.60)], 0.35, "F.Cu", "/GND")
    routes += make_route([P("SW_C6_BOOT", 1), (34.50, 69.00)], 0.30, "F.Cu", "/GND")
    routes += make_route([P("C_C6_EN", 2), (33.20, 79.55)], 0.30, "F.Cu", "/GND")
    routes += make_route([P("SW_C6_RST", 1), (33.20, 89.60)], 0.30, "F.Cu", "/GND")

    # /VBAT_FUSED, /Q_GATE, /VBAT_PROT
    routes += make_route([P("F1", 2), (92.20, 56.66), P("Q_PROT", 3)], 0.65, "F.Cu", "/VBAT_FUSED")
    routes += make_route([P("Q_PROT", 1), (90.00, 59.79), P("R_QG", 2)], 0.25, "F.Cu", "/Q_GATE")

    routes += make_route([P("SW_PWR", 3), (93.80, 55.75)], 0.45, "F.Cu", "/VBAT_PROT")
    routes.append(make_via(93.80, 55.75, "/VBAT_PROT", size=0.65, drill=0.35))
    routes += make_route([(93.80, 55.75), P("J_SW_EXT", 1)], 0.50, "B.Cu", "/VBAT_PROT")
    routes += make_route([P("J_SW_EXT", 1), (93.80, 61.85), (93.40, 61.85), (92.95, 61.85), (92.15, 61.05), P("Q_PROT", 2)], 0.45, "F.Cu", "/VBAT_PROT")
    routes.append(make_via(93.40, 61.85, "/VBAT_PROT", size=0.60, drill=0.32))
    routes += make_route([(93.40, 61.85), (93.40, 62.80), (90.20, 66.00), (90.20, 70.85), (90.20, 71.80)], 0.45, "B.Cu", "/VBAT_PROT")
    routes.append(make_via(90.20, 71.80, "/VBAT_PROT", size=0.65, drill=0.35))
    routes += make_route([(90.20, 71.80), P("U_BUCK", 5)], 0.45, "F.Cu", "/VBAT_PROT")
    routes.append(make_via(90.20, 70.85, "/VBAT_PROT", size=0.60, drill=0.32))
    routes += make_route([(90.20, 70.85), P("C_BIN", 1)], 0.45, "F.Cu", "/VBAT_PROT")

    # /BUCK_EN_SW: SW_PWR:2 -> via -> B.Cu -> J_SW_EXT:2 -> F.Cu R_SW_PU:1 & via -> B.Cu X=93.00 -> via -> F.Cu Y=87.50 -> R_2S_TOP:1
    routes += make_route([P("SW_PWR", 2), (94.55, 57.25)], 0.30, "F.Cu", "/BUCK_EN_SW")
    routes.append(make_via(94.55, 57.25, "/BUCK_EN_SW", size=0.50, drill=0.30))
    routes += make_route([(94.55, 57.25), (94.55, 62.80), (P("J_SW_EXT", 2)[0], 64.59), P("J_SW_EXT", 2)], 0.30, "B.Cu", "/BUCK_EN_SW")
    routes += make_route([P("J_SW_EXT", 2), (96.34, 66.30), (93.00, 66.30), (91.49, 66.30), P("R_SW_PU", 1)], 0.25, "F.Cu", "/BUCK_EN_SW")
    routes.append(make_via(93.00, 66.30, "/BUCK_EN_SW", size=0.55, drill=0.30))
    routes += make_route([(93.00, 66.30), (93.00, 86.75)], 0.25, "B.Cu", "/BUCK_EN_SW")
    routes.append(make_via(93.00, 86.75, "/BUCK_EN_SW", size=0.55, drill=0.30))
    routes += make_route([(93.00, 86.75), (69.5125, 86.75), (68.7625, 87.50), P("R_2S_TOP", 1)], 0.25, "F.Cu", "/BUCK_EN_SW")

    # /BUCK_EN, /BUCK_LX, /BUCK_BST, /BUCK_FB
    routes += make_route([P("R_SW_PD", 2), P("R_SW_PU", 2), (91.8625, 69.25), P("U_BUCK", 4)], 0.22, "F.Cu", "/BUCK_EN")
    routes += make_route([P("U_BUCK", 6), (90.15, 74.46)], 0.55, "F.Cu", "/BUCK_LX")
    routes.append(make_via(90.15, 74.46, "/BUCK_LX", size=0.65, drill=0.35))
    routes += make_route([(90.15, 74.46), (90.15, 75.00), (90.15, 77.20)], 0.60, "B.Cu", "/BUCK_LX")
    routes.append(make_via(90.15, 75.00, "/BUCK_LX", size=0.55, drill=0.30))
    routes += make_route([(90.15, 75.00), P("C_BST2", 2)], 0.30, "F.Cu", "/BUCK_LX")
    routes.append(make_via(90.15, 77.20, "/BUCK_LX", size=0.65, drill=0.35))
    routes += make_route([(90.15, 77.20), P("L2", 1)], 0.65, "F.Cu", "/BUCK_LX")
    routes += make_route([P("U_BUCK", 1), (P("C_BST2", 1)[0], 72.94), P("C_BST2", 1)], 0.25, "F.Cu", "/BUCK_BST")
    routes += make_route([P("U_BUCK", 3), (96.2375, 70.85), P("R_FB1", 2), P("R_FB2", 1)], 0.22, "F.Cu", "/BUCK_FB")

    # /+5V MAIN POWER BUS (loop-free strict tree, F.Cu=Horiz, B.Cu=Vert):
    routes += make_route([P("R_FB1", 1), (99.00, 70.2875)], 0.30, "F.Cu", "/+5V")
    routes.append(make_via(99.00, 70.2875, "/+5V", size=0.60, drill=0.32))
    routes += make_route([(99.00, 70.2875), (99.00, 77.85)], 0.45, "B.Cu", "/+5V")
    routes.append(make_via(99.00, 77.85, "/+5V", size=0.65, drill=0.35))
    routes += make_route([(99.00, 77.85), P("C_BOUT1", 1), (96.10, 77.85), (95.20, 77.85), (93.85, 79.20), P("L2", 2)], 0.60, "F.Cu", "/+5V")
    routes.append(make_via(96.10, 77.85, "/+5V", size=0.70, drill=0.35))
    routes += make_route([(96.10, 77.85), (96.10, 81.65)], 0.60, "B.Cu", "/+5V")
    routes.append(make_via(96.10, 81.65, "/+5V", size=0.75, drill=0.40))
    # Branch East from (96.10, 81.65) on F.Cu -> C_BOUT2:1 -> via (98.90, 81.65) -> B.Cu -> J_PWR_ESP:2
    routes += make_route([(96.10, 81.65), P("C_BOUT2", 1), (98.90, 81.65)], 0.65, "F.Cu", "/+5V")
    routes.append(make_via(98.90, 81.65, "/+5V", size=0.70, drill=0.35))
    routes += make_route([(98.90, 81.65), (98.90, 87.80)], 0.65, "B.Cu", "/+5V")
    routes.append(make_via(98.90, 87.80, "/+5V", size=0.70, drill=0.35))
    routes += make_route([(98.90, 87.80), P("J_PWR_ESP", 2)], 0.70, "F.Cu", "/+5V")
    # Branch West from (96.10, 81.65) on F.Cu -> Y=81.00 -> via (90.9125, 81.00) -> R_LED2:1 & via (94.10, 81.00) -> B.Cu -> Y=86.00 -> F.Cu (62.50, 86.00) -> B.Cu -> D_LDO_5V:2
    routes += make_route([(96.10, 81.65), (95.45, 81.65), (94.80, 81.00), (94.10, 81.00), (90.9125, 81.00)], 0.55, "F.Cu", "/+5V")
    routes.append(make_via(90.9125, 81.00, "/+5V", size=0.55, drill=0.30))
    routes += make_route([(90.9125, 81.00), (90.9125, 82.00)], 0.25, "B.Cu", "/+5V")
    routes.append(make_via(90.9125, 82.00, "/+5V", size=0.50, drill=0.30))
    routes += make_route([(90.9125, 82.00), P("R_LED2", 1)], 0.25, "F.Cu", "/+5V")
    routes.append(make_via(94.10, 81.00, "/+5V", size=0.65, drill=0.35))
    routes += make_route([(94.10, 81.00), (94.10, 86.00)], 0.55, "B.Cu", "/+5V")
    routes.append(make_via(94.10, 86.00, "/+5V", size=0.65, drill=0.35))
    routes += make_route([(94.10, 86.00), (62.50, 86.00)], 0.55, "F.Cu", "/+5V")
    routes.append(make_via(62.50, 86.00, "/+5V", size=0.65, drill=0.35))
    routes += make_route([(62.50, 86.00), (62.50, 84.35)], 0.55, "B.Cu", "/+5V")
    routes.append(make_via(62.50, 84.35, "/+5V", size=0.65, drill=0.35))
    routes += make_route([(62.50, 84.35), P("D_LDO_5V", 2)], 0.55, "F.Cu", "/+5V")
    routes += make_route([P("R_LED2", 2), (P("R_LED2", 2)[0], 85.10), (P("D_5V", 2)[0], 85.10), P("D_5V", 2)], 0.25, "F.Cu", "/LED_5V_A")
    # Branch North from (99.00, 70.2875) on B.Cu -> jog at X=98.20 around J_LIPO_2S -> X=96.85 through J_USBC SH window -> X=95.70 -> (93.80, 14.20) -> via -> F.Cu horiz -> J1:9
    routes += make_route([
        (99.00, 70.2875), (99.00, 63.65), (95.70, 60.35), (95.70, 53.50),
        (98.20, 51.00), (98.20, 44.40), (96.85, 43.05), (96.85, 30.50),
        (95.70, 29.35), (95.70, 17.50), (93.80, 15.60), (93.80, 14.20),
    ], 0.65, "B.Cu", "/+5V")
    routes.append(make_via(93.80, 14.20, "/+5V", size=0.80, drill=0.40))
    routes += make_route([(93.80, 14.20), (69.02, 14.20), P("J1", 9)], 0.65, "F.Cu", "/+5V")

    # 9. /LDO_VIN, /C6_BOOT, /C6_IO8, /C6_EN & Onboard Ra-02 LoRa SPI (F.Cu = Horizontal, B.Cu = Vertical)
    routes += make_route([P("D_LDO_USB", 1), (68.90, 66.20)], 0.35, "F.Cu", "/LDO_VIN")
    routes.append(make_via(68.90, 66.20, "/LDO_VIN", size=0.55, drill=0.30))
    routes += make_route([(68.90, 66.20), (68.90, 73.25), (68.90, 75.55), (68.90, 84.35)], 0.35, "B.Cu", "/LDO_VIN")
    routes.append(make_via(68.90, 73.25, "/LDO_VIN", size=0.50, drill=0.30))
    routes += make_route([(68.90, 73.25), P("U_LDO", 3)], 0.35, "F.Cu", "/LDO_VIN")
    routes.append(make_via(68.90, 75.55, "/LDO_VIN", size=0.50, drill=0.30))
    routes += make_route([(68.90, 75.55), P("C_LDO_IN", 1)], 0.35, "F.Cu", "/LDO_VIN")
    routes.append(make_via(68.90, 84.35, "/LDO_VIN", size=0.50, drill=0.30))
    routes += make_route([(68.90, 84.35), P("D_LDO_5V", 1)], 0.35, "F.Cu", "/LDO_VIN")

    routes += make_route([P("SW_C6_BOOT", 2), (34.40, 65.10)], 0.25, "F.Cu", "/C6_BOOT")
    routes.append(make_via(34.40, 65.10, "/C6_BOOT", size=0.50, drill=0.30))
    routes += make_route([(34.40, 65.10), (34.40, 67.05)], 0.25, "B.Cu", "/C6_BOOT")
    routes.append(make_via(34.40, 67.05, "/C6_BOOT", size=0.50, drill=0.30))
    routes += make_route([(34.40, 67.05), P("U_C6", 15)], 0.25, "F.Cu", "/C6_BOOT")
    routes += make_route([P("U_C6", 10), (56.85, 72.13), (57.7625, 73.0425), P("R_IO8_PU", 1)], 0.20, "F.Cu", "/C6_IO8")

    # /C6_EN: C_C6_EN:1 -> SW_C6_RST:2 -> F.Cu horiz Y=84.43 -> via (59.35, 84.43) -> B.Cu vert X=59.35 -> U_C6:3 & R_C6_EN:1
    routes += make_route([P("C_C6_EN", 1), P("SW_C6_RST", 2), (33.53, 84.43), (59.35, 84.43)], 0.22, "F.Cu", "/C6_EN")
    routes.append(make_via(59.35, 84.43, "/C6_EN", size=0.50, drill=0.30))
    routes += make_route([(59.35, 84.43), (59.35, 80.35), (59.35, 78.5125)], 0.25, "B.Cu", "/C6_EN")
    routes.append(make_via(59.35, 80.35, "/C6_EN", size=0.50, drill=0.30))
    routes += make_route([(59.35, 80.35), (56.75, 80.35), (56.08, 81.02), P("U_C6", 3)], 0.22, "F.Cu", "/C6_EN")
    routes.append(make_via(59.35, 78.5125, "/C6_EN", size=0.50, drill=0.30))
    routes += make_route([(59.35, 78.5125), P("R_C6_EN", 1)], 0.25, "F.Cu", "/C6_EN")

    # Onboard Ra-02 LoRa SPI + DIO0 (B.Cu Vertical, F.Cu Horizontal — zero B.Cu horizontal runs!)
    routes += make_route([P("U_C6", 8), (59.25, 74.67)], 0.25, "F.Cu", "/LORA_DIO0")
    routes.append(make_via(59.25, 74.67, "/LORA_DIO0", size=0.50, drill=0.30))
    routes += make_route([(59.25, 74.67), (59.25, 64.45)], 0.25, "B.Cu", "/LORA_DIO0")
    routes.append(make_via(59.25, 64.45, "/LORA_DIO0", size=0.50, drill=0.30))
    routes += make_route([(59.25, 64.45), (84.90, 64.45)], 0.25, "F.Cu", "/LORA_DIO0")
    routes.append(make_via(84.90, 64.45, "/LORA_DIO0", size=0.50, drill=0.30))
    routes += make_route([(84.90, 64.45), (84.90, 76.60)], 0.25, "B.Cu", "/LORA_DIO0")
    routes.append(make_via(84.90, 76.60, "/LORA_DIO0", size=0.50, drill=0.30))
    routes += make_route([(84.90, 76.60), P("U_LORA", 5)], 0.25, "F.Cu", "/LORA_DIO0")

    routes += make_route([P("U_C6", 7), (56.20, 75.94), (56.59, 75.55), (60.95, 75.55), (62.50, 77.10)], 0.25, "F.Cu", "/LORA_MOSI")
    routes.append(make_via(62.50, 77.10, "/LORA_MOSI", size=0.50, drill=0.30))
    routes += make_route([(62.50, 77.10), (62.50, 80.60)], 0.25, "B.Cu", "/LORA_MOSI")
    routes.append(make_via(62.50, 80.60, "/LORA_MOSI", size=0.50, drill=0.30))
    routes += make_route([(62.50, 80.60), P("U_LORA", 14)], 0.25, "F.Cu", "/LORA_MOSI")

    routes += make_route([P("U_C6", 6), (56.65, 77.21)], 0.22, "F.Cu", "/LORA_NSS")
    routes.append(make_via(56.65, 77.21, "/LORA_NSS", size=0.50, drill=0.30))
    routes += make_route([(56.65, 77.21), (56.65, 79.55)], 0.22, "B.Cu", "/LORA_NSS")
    routes.append(make_via(56.65, 79.55, "/LORA_NSS", size=0.50, drill=0.30))
    routes += make_route([(56.65, 79.55), (61.50, 79.55)], 0.22, "F.Cu", "/LORA_NSS")
    routes.append(make_via(61.50, 79.55, "/LORA_NSS", size=0.50, drill=0.30))
    routes += make_route([(61.50, 79.55), (61.50, 82.60)], 0.22, "B.Cu", "/LORA_NSS")
    routes.append(make_via(61.50, 82.60, "/LORA_NSS", size=0.50, drill=0.30))
    routes += make_route([(61.50, 82.60), P("U_LORA", 15)], 0.25, "F.Cu", "/LORA_NSS")

    routes += make_route([P("U_C6", 5), (53.80, 78.48)], 0.25, "F.Cu", "/LORA_SCK")
    routes.append(make_via(53.80, 78.48, "/LORA_SCK", size=0.50, drill=0.30))
    routes += make_route([(53.80, 78.48), (53.80, 63.90)], 0.25, "B.Cu", "/LORA_SCK")
    routes.append(make_via(53.80, 63.90, "/LORA_SCK", size=0.50, drill=0.30))
    routes += make_route([(53.80, 63.90), (64.30, 63.90)], 0.25, "F.Cu", "/LORA_SCK")
    routes.append(make_via(64.30, 63.90, "/LORA_SCK", size=0.50, drill=0.30))
    routes += make_route([(64.30, 63.90), (64.30, 76.60)], 0.25, "B.Cu", "/LORA_SCK")
    routes.append(make_via(64.30, 76.60, "/LORA_SCK", size=0.50, drill=0.30))
    routes += make_route([(64.30, 76.60), P("U_LORA", 12)], 0.25, "F.Cu", "/LORA_SCK")

    routes += make_route([P("U_C6", 4), (53.05, 79.75)], 0.25, "F.Cu", "/LORA_MISO")
    routes.append(make_via(53.05, 79.75, "/LORA_MISO", size=0.50, drill=0.30))
    routes += make_route([(53.05, 79.75), (53.05, 63.35)], 0.25, "B.Cu", "/LORA_MISO")
    routes.append(make_via(53.05, 63.35, "/LORA_MISO", size=0.50, drill=0.30))
    routes += make_route([(53.05, 63.35), (63.40, 63.35)], 0.25, "F.Cu", "/LORA_MISO")
    routes.append(make_via(63.40, 63.35, "/LORA_MISO", size=0.50, drill=0.30))
    routes += make_route([(63.40, 63.35), (63.40, 78.60)], 0.25, "B.Cu", "/LORA_MISO")
    routes.append(make_via(63.40, 78.60, "/LORA_MISO", size=0.50, drill=0.30))
    routes += make_route([(63.40, 78.60), P("U_LORA", 13)], 0.25, "F.Cu", "/LORA_MISO")

    # Optional 2S ADC Monitor Divider (/ADC_2S_DIV) - strict tree via B.Cu vertical (65.85, 87.50..89.80)
    routes += make_route([P("JP_2S_ADC", 1), (65.85, 88.00)], 0.25, "F.Cu", "/ADC_2S_DIV")
    routes.append(make_via(65.85, 88.00, "/ADC_2S_DIV", size=0.50, drill=0.30))
    routes += make_route([(65.85, 87.50), (65.85, 88.00), (65.85, 89.80)], 0.25, "B.Cu", "/ADC_2S_DIV")
    routes.append(make_via(65.85, 87.50, "/ADC_2S_DIV", size=0.50, drill=0.30))
    routes += make_route([(65.85, 87.50), P("R_2S_TOP", 2)], 0.25, "F.Cu", "/ADC_2S_DIV")
    routes.append(make_via(65.85, 89.80, "/ADC_2S_DIV", size=0.50, drill=0.30))
    routes += make_route([(65.85, 89.80), P("R_2S_BOT", 2)], 0.25, "F.Cu", "/ADC_2S_DIV")

    new_blocks.extend(routes)

    # ==========================================================================
    # F. DOUBLE-SIDED SILKSCREEN (F.SilkS + B.SilkS) & SOLID GND BARS
    # ==========================================================================
    silk = []

    # Titles (both sides)
    silk.append(make_silk_text("WINDDRAGONS DUAL ESP32-C6 CARRIER V2.0", 48.0, 2.5, 0, "F.SilkS", 1.10, 0.18, True))
    silk.append(make_silk_text("FRONT PCBA: WAVESHARE LCD, C6-WROOM-1, RA-02 LORA, JOY1 & 2S", 48.0, 5.2, 0, "F.SilkS", 0.65, 0.12, False))
    silk.append(make_silk_text("WINDDRAGONS DUAL ESP32-C6 CARRIER V2.0", 48.0, 2.5, 0, "B.SilkS", 1.10, 0.18, True))
    silk.append(make_silk_text("BACK: QUECTEL LC29H GNSS (ZERO BACK MODULES ON BASE)", 48.0, 5.2, 0, "B.SilkS", 0.70, 0.13, False))

    # --- 1. Mirror core board outlines & labels onto BOTH faces ---
    # Waveshare ESP32-C6-LCD-1.47 outline (36.37 x 20.32 mm: X=43.27..79.64, Y=12.23..32.55) + J1/J2 labels
    silk.append(make_silk_text("WAVESHARE ESP32-C6-LCD-1.47", 61.45, 22.40, 0, "F.SilkS", 0.68, 0.12, True))
    silk.append(make_silk_text("WAVESHARE ESP32-C6-LCD-1.47", 58.00, 21.75, 0, "B.SilkS", 0.62, 0.11, True))
    silk.append(make_silk_text("(MASTER - FRONT)", 58.00, 23.15, 0, "B.SilkS", 0.62, 0.11, True))
    silk.append(make_silk_rect(43.27, 12.23, 79.64, 32.55, "B.SilkS", 0.18, False))
    # Segmented F.SilkS outline avoiding Front-only SMD pads/silk (D_CHG, C_BAT1, C_SYS2)
    silk.append(make_silk_line(43.27, 12.23, 79.64, 12.23, "F.SilkS", 0.15))
    silk.append(make_silk_line(43.27, 12.23, 43.27, 32.55, "F.SilkS", 0.15))
    silk.append(make_silk_line(43.27, 32.55, 75.50, 32.55, "F.SilkS", 0.15))
    silk.append(make_silk_line(79.64, 12.23, 79.64, 23.00, "F.SilkS", 0.15))
    silk.append(make_silk_line(79.64, 26.50, 79.64, 30.80, "F.SilkS", 0.15))
    for lyr in ("F.SilkS", "B.SilkS"):
        silk.append(make_silk_text("J1", 45.30, 13.50, 0, lyr, 0.62, 0.12, True))
        silk.append(make_silk_text("J2", 45.30, 31.28, 0, lyr, 0.62, 0.12, True))
        for lbl, px in [
            ("IO5", 48.00), ("IO4", 50.54), ("IO3", 53.08), ("IO2", 55.62), ("IO1", 58.16),
            ("IO0", 60.70), ("3V3", 63.24), ("GND", 65.78), ("5V", 68.32),
        ]:
            silk.append(make_silk_text(lbl, px, 11.1, 0, lyr, 0.46, 0.09, lbl == "GND"))
        for lbl, px in [
            ("BT", 48.00), ("18", 50.54), ("19", 53.08), ("20", 55.62), ("JSW", 58.16),
            ("12", 60.70), ("13", 63.24), ("RX", 65.78), ("TX", 68.32),
        ]:
            silk.append(make_silk_text(lbl, px, 33.8, 0, lyr, 0.46, 0.09, False))
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
    silk.append(make_silk_text("3V3 LDO", 65.4, 70.10, 0, "B.SilkS", 0.60, 0.11, True))

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

    # Quectel LC29H GPS mirrored onto F.SilkS (no big body box over Front Dragon logo; exact 2.54mm pad-centered labels)
    silk.append(make_silk_text("QUECTEL LC29H GPS (BACK)", 19.50, 8.0, 0, "F.SilkS", 0.70, 0.13, True))
    silk.append(make_silk_text("J6", 9.90, 12.50, 0, "F.SilkS", 0.62, 0.12, True))
    for lbl, px in [
        ("P", 11.88), ("T2", 14.42), ("R2", 16.96), ("R1", 19.50),
        ("T1", 22.04), ("G", 24.58), ("V", 27.12),
    ]:
        silk.append(make_silk_text(lbl, px, 10.2, 0, "F.SilkS", 0.60, 0.11, lbl == "G"))
    silk.append(make_silk_text("QUECTEL LC29H GPS", 19.50, 7.8, 0, "B.SilkS", 0.75, 0.13, True))
    silk.append(make_silk_rect(6.40, 6.50, 32.60, 37.50, "B.SilkS", 0.15, False))
    silk.extend(make_gnd_bar_both(24.58, 14.3))   # J6 Pad 6 GND

    # Onboard Ai-Thinker Ra-02 (SX1278 433MHz) labels on Front & Back
    silk.append(make_silk_text("RA-02 433MHz", 78.90, 67.85, 0, "F.SilkS", 0.60, 0.11, True))
    silk.append(make_silk_rect(70.90, 69.10, 86.90, 86.10, "B.SilkS", 0.15, False))
    silk.append(make_silk_text("ONBOARD RA-02 433MHz", 78.90, 77.60, 0, "B.SilkS", 0.62, 0.11, True))

    # Joystick & Buttons mirrored onto B.SilkS
    silk.append(make_silk_text("PS5 JOYSTICK", 72.75, 38.6, 0, "F.SilkS", 0.75, 0.13, True))
    silk.append(make_silk_text("PS5 JOYSTICK (FRONT)", 72.75, 40.5, 0, "B.SilkS", 0.75, 0.13, True))
    silk.append(make_silk_text("SW1:IO18 SW2:IO19 SW3:IO20 SW4:IO9", 16.4, 61.5, 0, "B.SilkS", 0.60, 0.11, False))

    # Dedicated GY-BNO055 9-DOF IMU Right-Angle JST-XH 4P connector on Left Edge (F.SilkS + B.SilkS + GND bar)
    silk.append(make_silk_rect(1.20, 14.55, 9.55, 27.95, "F.SilkS", 0.15, False))
    silk.append(make_silk_text("IMU BNO055", 5.35, 13.55, 0, "F.SilkS", 0.52, 0.10, True))
    silk.append(make_silk_rect(1.10, 14.55, 5.95, 27.95, "B.SilkS", 0.15, False))
    silk.append(make_silk_text("IMU BNO055", 3.50, 13.55, 0, "B.SilkS", 0.50, 0.10, True))
    for lbl, py, is_gnd in [("GND", 17.50, True), ("3V3", 20.00, False), ("SDA", 22.50, False), ("SCL", 25.00, False)]:
        silk.append(make_silk_text(lbl, 8.25, py, 0, "F.SilkS", 0.46, 0.10, is_gnd))
        silk.append(make_silk_text(lbl, 2.15, py, 0, "B.SilkS", 0.46, 0.10, is_gnd))
    silk.extend(make_gnd_bar_both(4.40, 16.05))

    # --- 2. FOOLPROOF SEPARATED ACTUATORS vs SENSORS BOXES (Both Sides) ---
    # Box 1: Left-Edge 3x5 Right-Angle RC SERVOS (5V BEC): Col 1 GND (-), Col 2 +5V (+), Col 3 SIG (S)
    for lyr in ("F.SilkS", "B.SilkS"):
        silk.append(make_silk_rect(1.20, 62.65, 10.75, 76.35, lyr, 0.15, False))
        for lbl, sx, is_bold in [("-", 4.40, True), ("+", 6.94, False), ("S", 9.48, True)]:
            silk.append(make_silk_text(lbl, sx, 63.35, 0, lyr, 0.50, 0.10, is_bold))
        for nm, sy in [("ESC", 64.92), ("RUD", 67.46), ("WIN", 70.00), ("RCK", 72.54), ("STRB", 75.08)]:
            silk.append(make_silk_text(nm, 11.95, sy, 0, lyr, 0.48, 0.10, True))
        silk.append(make_silk_text("RC SERVOS (BEC)", 13.65, 70.00, 90, lyr, 0.50, 0.10, True))
        # Solid white GND bar along outer GND column (Col 1, X=4.40)
        silk.append(make_silk_rect(2.95, 64.10, 3.35, 75.90, lyr, 0.05, True))

    # Box 2: SENSORS (IO): X=18.0..30.0, Y=64.0..76.8
    for lyr in ("F.SilkS", "B.SilkS"):
        silk.append(make_silk_rect(18.0, 64.0, 30.0, 76.8, lyr, 0.18, False))
        silk.append(make_silk_text("SENSORS (IO)", 24.0, 65.2, 0, lyr, 0.56, 0.11, True))
        silk.append(make_silk_text("SIG", 19.2, 71.96, 0, lyr, 0.48, 0.10, False))
        silk.append(make_silk_text("GND", 19.2, 74.50, 0, lyr, 0.48, 0.10, True))
        silk.append(make_silk_text("WTR", 21.50, 67.5, 0, lyr, 0.52, 0.11, True))
        silk.append(make_silk_text("LIM", 26.80, 67.5, 0, lyr, 0.52, 0.11, True))
        silk.append(make_silk_rect(20.4, 75.65, 27.9, 76.10, lyr, 0.05, True))

    # Box 3: Left-Edge 90-deg WINCH HUB 6P (X=4.40, Y=78.50..91.20) & BEC 5V IN (17.50, 84.50)
    for lyr in ("F.SilkS", "B.SilkS"):
        silk.append(make_silk_rect(1.20, 77.15, 7.65, 92.95, lyr, 0.15, False))
        silk.append(make_silk_text("WINCH 6P HUB", 8.65, 85.05, 90, lyr, 0.50, 0.10, True))
        for lbl, py, is_gnd in [
            ("GND", 91.20, True), ("5V", 88.66, False), ("3V3", 86.12, False),
            ("HAL", 83.58, False), ("WIN", 81.04, False), ("RCK", 78.50, False),
        ]:
            silk.append(make_silk_text(lbl, 6.35, py, 0, lyr, 0.44, 0.09, is_gnd))
        silk.append(make_silk_rect(13.20, 81.50, 20.00, 88.80, lyr, 0.15, False))
        silk.append(make_silk_text("BEC IN", 16.60, 82.60, 0, lyr, 0.58, 0.11, True))
        silk.append(make_silk_text("+5V", 14.96, 87.20, 0, lyr, 0.50, 0.10, False))
        silk.append(make_silk_text("GND", 17.50, 87.20, 0, lyr, 0.50, 0.11, True))
    silk.extend(make_gnd_bar_both(4.40, 92.55))
    silk.extend(make_gnd_bar_both(17.50, 85.90))

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
    silk.append(make_silk_rect(90.75, 7.25, 98.80, 14.55, "F.SilkS", 0.15, False))
    silk.append(make_silk_rect(94.25, 7.25, 98.90, 14.55, "B.SilkS", 0.15, False))
    silk.append(make_silk_rect(90.45, 44.20, 98.80, 52.75, "F.SilkS", 0.15, False))
    silk.append(make_silk_rect(94.05, 44.20, 98.90, 52.75, "B.SilkS", 0.15, False))
    for lyr in ("F.SilkS", "B.SilkS"):
        silk.append(make_silk_text("SOLAR 5-6V", 90.80, 6.40, 0, lyr, 0.58, 0.11, True))
        silk.append(make_silk_text("USB-C 5V", 97.80, 29.80, 0, lyr, 0.48, 0.10, True))
        silk.append(make_silk_text("2S LIPO", 95.60, 42.05, 0, lyr, 0.54, 0.11, True))
        silk.append(make_silk_text("ON", 94.80, 53.65, 0, lyr, 0.58, 0.11, True))
        silk.append(make_silk_text("OFF", 97.8, 62.6, 0, lyr, 0.58, 0.11, True))
        silk.append(make_silk_text("EXT SW", 95.1, 66.2, 0, lyr, 0.50, 0.10, False))
        silk.append(make_silk_text("5V AUX", 96.3, 85.8, 0, lyr, 0.54, 0.10, True))
        silk.append(make_silk_text("GND", 94.50, 90.2, 0, lyr, 0.46, 0.09, True))
        silk.append(make_silk_text("+5V", 97.04, 90.2, 0, lyr, 0.46, 0.09, False))
        silk.append(make_silk_text("2S ADC", 64.2, 89.6, 0, lyr, 0.52, 0.10, True))
    for lbl, py in [("+", 9.70), ("-", 12.20)]:
        silk.append(make_silk_text(lbl, 91.85, py, 0, "F.SilkS", 0.62, 0.12, True))
        silk.append(make_silk_text(lbl, 97.85, py, 0, "B.SilkS", 0.62, 0.12, True))
    for lbl, py, is_gnd in [("8.4", 45.20, True), ("4.2", 47.70, False), ("GND", 50.20, True)]:
        silk.append(make_silk_text(lbl, 91.85, py, 0, "F.SilkS", 0.48, 0.10, is_gnd))
        silk.append(make_silk_text(lbl, 97.85, py, 0, "B.SilkS", 0.48, 0.10, is_gnd))
    silk.append(make_silk_text("CHG", 79.0, 23.8, 0, "F.SilkS", 0.55, 0.11, True))
    silk.append(make_silk_text("5V OK", 91.8, 85.2, 0, "F.SilkS", 0.50, 0.10, True))
    silk.extend(make_gnd_bar_both(95.60, 13.65))  # J_SOLAR Pad 1 GND
    silk.extend(make_gnd_bar_both(95.60, 51.65))  # J_LIPO_2S Pad 1 GND
    silk.extend(make_gnd_bar_both(94.50, 89.2))   # J_PWR_ESP Pad 1 GND

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

    nc_pad_nets = {
        ("J6", "1"): "unconnected-(J6-P1-Pad1)",
        ("J6", "2"): "unconnected-(J6-P2-Pad2)",
        ("J6", "3"): "unconnected-(J6-P3-Pad3)",
        ("JOY1", "8"): "unconnected-(JOY1-SW_B1-Pad8)",
        ("JOY1", "9"): "unconnected-(JOY1-SW_A2-Pad9)",
        ("J_USBC", "A8"): "unconnected-(J_USBC-SBU1-PadA8)",
        ("J_USBC", "B8"): "unconnected-(J_USBC-SBU2-PadB8)",
        ("SW_PWR", "1"): "unconnected-(SW_PWR-OFF-Pad1)",
        ("U_ADS1", "2"): "unconnected-(U_ADS1-ALERT-Pad2)",
        ("U_ADS2", "2"): "unconnected-(U_ADS2-ALERT-Pad2)",
        ("U_C6", "22"): "unconnected-(U_C6-IO14-Pad22)",
        ("U_C6", "25"): "unconnected-(U_C6-TXD0-Pad25)",
        ("U_CHG", "1"): "unconnected-(U_CHG-NC1-Pad1)",
        ("U_CHG", "2"): "unconnected-(U_CHG-NC2-Pad2)",
        ("U_CHG", "3"): "unconnected-(U_CHG-NC3-Pad3)",
        ("U_CHG", "5"): "unconnected-(U_CHG-NC5-Pad5)",
        ("U_CHG", "7"): "unconnected-(U_CHG-NC7-Pad7)",
        ("U_CHG", "9"): "unconnected-(U_CHG-DP_NC-Pad9)",
        ("U_CHG", "10"): "unconnected-(U_CHG-DM_NC-Pad10)",
        ("U_LORA", "4"): "unconnected-(U_LORA-~{RESET}-Pad4)",
        ("U_LORA", "6"): "unconnected-(U_LORA-DIO1-Pad6)",
        ("U_LORA", "7"): "unconnected-(U_LORA-DIO2-Pad7)",
        ("U_LORA", "8"): "unconnected-(U_LORA-DIO3-Pad8)",
        ("U_LORA", "10"): "unconnected-(U_LORA-DIO4-Pad10)",
        ("U_LORA", "11"): "unconnected-(U_LORA-DIO5-Pad11)",
    }

    def apply_nc_nets(blks):
        out_blks = []
        for b in blks:
            if b.startswith("\t(footprint"):
                r = get_fp_ref(b)
                for (nr, npad), nname in nc_pad_nets.items():
                    if nr == r:
                        b = re.sub(
                            rf'(\t\t\(pad "{re.escape(npad)}" [^\n]+\n(?:(?!\n\t\t\))[^\n]+\n)*?)(\t\t\t\(uuid )',
                            lambda m: m.group(0) if "(net " in m.group(1) else f'{m.group(1)}\t\t\t(net "{nname}")\n{m.group(2)}',
                            b,
                            count=1,
                        )
            out_blks.append(b)
        return out_blks

    all_blocks = apply_nc_nets(kept_blocks + new_blocks)
    pcb_content = header + "\n" + "\n".join(all_blocks) + "\n)\n"
    with open(PCB_PATH, "w") as f:
        f.write(pcb_content)
    print(f"Wrote {PCB_PATH}")

    # Upgrade PCB, build matching KiCad 10 Schematic (+ ERC + PDF/SVG/PNG), and run DRC with --schematic-parity
    kicad_cfg = "/tmp/kicad_render_cfg"
    os.makedirs(kicad_cfg, exist_ok=True)
    kicad_env = dict(os.environ, KICAD_CONFIG_HOME=kicad_cfg)
    subprocess.run([KICAD_CLI, "pcb", "upgrade", "--force", PCB_PATH], env=kicad_env, capture_output=True)

    with open(PCB_PATH, "r") as f:
        upgraded_txt = f.read()
    chamfered_txt = chamfer_via_transitions(upgraded_txt)
    with open(PCB_PATH, "w") as f:
        f.write(chamfered_txt)

    audit_routing_guidelines(PCB_PATH)

    import generate_v2_schematic
    generate_v2_schematic.build_schematic_and_verify(PCB_PATH, SCH_PATH)

    drc_path = os.path.join(BASE_DIR, "drc_report.txt")
    res = subprocess.run(
        [KICAD_CLI, "pcb", "drc", "--schematic-parity", "--refill-zones", "--save-board", "-o", drc_path, PCB_PATH],
        env=kicad_env, capture_output=True, text=True,
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
    render_cfg = "/tmp/kicad_render_cfg"
    os.makedirs(render_cfg, exist_ok=True)
    render_env = dict(os.environ, KICAD_CONFIG_HOME=render_cfg)

    # Clear old gerber files
    for fn in os.listdir(GERBER_DIR):
        os.remove(os.path.join(GERBER_DIR, fn))

    subprocess.run([
        KICAD_CLI, "pcb", "export", "gerbers",
        "--board-plot-params", "--subtract-soldermask",
        "--output", GERBER_DIR + "/", PCB_PATH,
    ], check=True, env=render_env)
    subprocess.run([
        KICAD_CLI, "pcb", "export", "drill",
        "--excellon-separate-th", "--generate-map", "--map-format", "gerberx2",
        "--output", GERBER_DIR + "/", PCB_PATH,
    ], check=True, env=render_env)

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
    ], check=True, env=render_env)
    raw_pos_all = os.path.join(PCBA_DIR, "_raw_pos_all.csv")
    subprocess.run([
        KICAD_CLI, "pcb", "export", "pos",
        "--side", "front", "--format", "csv", "--units", "mm",
        "--output", raw_pos_all, PCB_PATH,
    ], check=True, env=render_env)

    cpl_rows = []
    cpl_refs = set()
    for pos_file, allowed_refs in [(raw_pos, None), (raw_pos_all, {"JOY1"})]:
        with open(pos_file, "r", newline="") as f:
            reader = csv.DictReader(f)
            for row in reader:
                ref = row["Ref"].strip()
                if allowed_refs is not None and ref not in allowed_refs:
                    continue
                if ref in {"JP_2S_ADC", "H_HALL"} or ref.startswith("STAMP"):
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

    iso_png = os.path.join(BASE_DIR, f"{PROJECT_NAME}_3d_iso.png")
    subprocess.run([
        KICAD_CLI, "pcb", "render", PCB_PATH,
        "-o", iso_png, "--perspective", "--rotate", "-50,0,25",
        "--quality", "high", "--preset", "follow_pcb_editor",
        "-D", "KICAD10_3DMODEL_DIR=/Applications/KiCad/KiCad.app/Contents/SharedSupport/3dmodels",
        "--width", "2000", "--height", "1600", "--zoom", "0.82",
    ], check=True, env=render_env)
    print(f"Rendered 3D iso: {iso_png}")


if __name__ == "__main__":
    main()
