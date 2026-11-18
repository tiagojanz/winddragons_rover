#!/usr/bin/env python3
"""
WindDragons Dual ESP32-C6 Carrier V2 - Native KiCad 10 Schematic Generator & Parity Verifier

Generates:
  - Dual_ESP_Carrier_V2.kicad_sym  (project local symbol library, KiCad 10)
  - sym-lib-table                  (registers Dual_ESP_Carrier_V2 library cleanly)
  - Dual_ESP_Carrier_V2.kicad_sch  (complete A1 schematic with all 112 parts & 90 nets)
  - Dual_ESP_Carrier_V2_schematic.pdf / .svg / .png
  - erc_report.txt                 (kicad-cli sch erc --severity-all)

Strict guarantees:
  1. Every pin, wire endpoint, junction, no-connect, and net label sits on an exact
     2.54 mm grid multiple (no floating-point drift).
  2. All wire crossings/T-joins are automatically split and fitted with KiCad junctions.
  3. Built-in assertion cross-checks every component Ref, Footprint, and (Ref, Pin)->Net
     against Dual_ESP_Carrier_V2.kicad_pcb before running KiCad CLI upgrade & ERC.
"""

import math
import os
import re
import shutil
import subprocess
import uuid
from collections import Counter, defaultdict

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
KICAD_CLI = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
GS_BIN = "/opt/homebrew/bin/gs"
PROJECT_NAME = "Dual_ESP_Carrier_V2"
ROOT_UUID = "a0000000-0000-0000-0000-000000000002"
GRID = 2.54


def new_uuid():
    return str(uuid.uuid4())


def g(units):
    """Return exact coordinate in mm for integer or half-integer 2.54 mm grid units."""
    return round(units * GRID, 4)


def fmt(val):
    """Format coordinate without trailing zeros or floating point noise."""
    v = round(float(val), 4)
    if abs(v) < 1e-6:
        return "0"
    s = f"{v:.4f}".rstrip("0").rstrip(".")
    return "0" if s == "-0" else s


def pt_key(x, y):
    return (round(float(x), 3), round(float(y), 3))


# ==============================================================================
# 1. PCB PARSER (Source of Truth for 100% Schematic <-> PCB Parity)
# ==============================================================================
def parse_pcb_components(pcb_path):
    """Extract {ref: {'footprint': fp, 'value': val, 'in_bom': bool, 'in_pos': bool, 'pads': {pin: net}}}."""
    with open(pcb_path, "r") as f:
        txt = f.read()

    blocks = []
    cur = []
    depth = 0
    for ln in txt.splitlines():
        if depth == 0 and ln.startswith("\t(footprint "):
            cur = [ln]
            depth = ln.count("(") - ln.count(")")
        elif cur:
            cur.append(ln)
            depth += ln.count("(") - ln.count(")")
            if depth == 0:
                blocks.append("\n".join(cur))
                cur = []

    comps = {}
    for blk in blocks:
        fp = re.match(r'\t\(footprint\s+"([^"]+)"', blk).group(1)
        ref_m = re.search(r'\(property\s+"Reference"\s+"([^"]*)"', blk)
        val_m = re.search(r'\(property\s+"Value"\s+"([^"]*)"', blk)
        attr_m = re.search(r'\(attr\s+([^)]+)\)', blk)
        ref = ref_m.group(1) if ref_m else ""
        if not ref or ref.startswith("STAMP"):
            continue
        val = val_m.group(1) if val_m else ""
        attr_str = attr_m.group(1) if attr_m else ""
        pads = {}
        for pm in re.finditer(r'\(pad\s+"([^"]*)"\s+(\S+)\s+\S+(.*?)\n\t\t\)', blk, re.S):
            pnum, ptype, pbody = pm.group(1), pm.group(2), pm.group(3)
            if pnum == "" and ptype == "np_thru_hole":
                continue
            if not pnum:
                continue
            nm = re.search(r'\(net\s+(?:\d+\s+)?"([^"]*)"\)', pbody)
            net = nm.group(1) if nm else ""
            if net.startswith("unconnected-"):
                net = ""
            if pnum in pads and pads[pnum] != net:
                raise AssertionError(f"Duplicate pad {ref}:{pnum} has conflicting nets {pads[pnum]} vs {net}")
            pads[pnum] = net
        comps[ref] = {
            "footprint": fp,
            "value": val,
            "in_bom": "exclude_from_bom" not in attr_str,
            "in_pos": "exclude_from_pos_files" not in attr_str,
            "pads": pads,
        }
    return comps


# ==============================================================================
# 2. COMPACT SYMBOL SPECIFICATION & KI-CAD 10 S-EXPRESSION BUILDER
# ==============================================================================
class SymDef:
    """Definition of a KiCad 10 schematic symbol (lib coords: Y-UP, rot=0 on sheet)."""

    def __init__(
        self,
        name,
        ref_prefix="U",
        ref_at=(0, 3.81, 0),
        val_at=(0, -3.81, 0),
        pin_names_hide=False,
        pin_numbers_hide=False,
        pin_names_offset=0.508,
        graphics=None,
        pins=None,
    ):
        self.name = name
        self.ref_prefix = ref_prefix
        self.ref_at = ref_at
        self.val_at = val_at
        self.pin_names_hide = pin_names_hide
        self.pin_numbers_hide = pin_numbers_hide
        self.pin_names_offset = pin_names_offset
        self.graphics = graphics or []  # list of S-expr strings inside <name>_1_1
        # pins: list of (num, name, px, py, angle, length)
        self.pins = pins or []
        self.pin_map = {str(p[0]): (p[2], p[3], p[4]) for p in self.pins}

    def render(self, full_name, indent="\t"):
        i1 = indent
        i2 = indent + "\t"
        i3 = indent + "\t\t"
        i4 = indent + "\t\t\t"
        pn_hide = f"\n{i2}(hide yes)" if self.pin_numbers_hide else ""
        nm_hide = f"\n{i2}(hide yes)" if self.pin_names_hide else ""
        lines = [
            f'{indent}(symbol "{full_name}"',
            f'{i1}(pin_numbers{pn_hide}\n{i1})' if pn_hide else None,
            f'{i1}(pin_names\n{i2}(offset {fmt(self.pin_names_offset)}){nm_hide}\n{i1})',
            f'{i1}(exclude_from_sim no)',
            f'{i1}(in_bom yes)',
            f'{i1}(on_board yes)',
            f'{i1}(in_pos_files yes)',
            f'{i1}(duplicate_pin_numbers_are_jumpers no)',
            f'{i1}(property "Reference" "{self.ref_prefix}"\n{i2}(at {fmt(self.ref_at[0])} {fmt(self.ref_at[1])} {self.ref_at[2]})\n{i2}(effects (font (size 1.27 1.27)))\n{i1})',
            f'{i1}(property "Value" "{self.name}"\n{i2}(at {fmt(self.val_at[0])} {fmt(self.val_at[1])} {self.val_at[2]})\n{i2}(effects (font (size 1.27 1.27)))\n{i1})',
            f'{i1}(property "Footprint" ""\n{i2}(at 0 0 0)\n{i2}(hide yes)\n{i2}(effects (font (size 1.27 1.27)))\n{i1})',
            f'{i1}(property "Datasheet" ""\n{i2}(at 0 0 0)\n{i2}(hide yes)\n{i2}(effects (font (size 1.27 1.27)))\n{i1})',
            f'{i1}(property "Description" ""\n{i2}(at 0 0 0)\n{i2}(hide yes)\n{i2}(effects (font (size 1.27 1.27)))\n{i1})',
            f'{i1}(symbol "{self.name}_0_1"',
        ]
        for gr in self.graphics:
            lines.append(f'{i2}{gr}')
        lines.append(f'{i1})')
        lines.append(f'{i1}(symbol "{self.name}_1_1"')
        for pnum, pname, px, py, ang, plen in self.pins:
            lines.append(
                f'{i2}(pin passive line\n'
                f'{i3}(at {fmt(px)} {fmt(py)} {ang})\n'
                f'{i3}(length {fmt(plen)})\n'
                f'{i3}(name "{pname}"\n{i4}(effects (font (size 1.016 1.016)))\n{i3})\n'
                f'{i3}(number "{pnum}"\n{i4}(effects (font (size 1.016 1.016)))\n{i3})\n'
                f'{i2})'
            )
        lines.append(f'{i1})')
        lines.append(f'{i1}(embedded_fonts no)')
        lines.append(f'{indent})')
        return "\n".join(x for x in lines if x is not None)


def rect_gr(x1, y1, x2, y2, w=0.254, fill="background"):
    return f'(rectangle (start {fmt(x1)} {fmt(y1)}) (end {fmt(x2)} {fmt(y2)}) (stroke (width {fmt(w)}) (type default)) (fill (type {fill})))'


def poly_gr(pts, w=0.254, fill="none"):
    pstr = " ".join(f"(xy {fmt(x)} {fmt(y)})" for x, y in pts)
    return f'(polyline (pts {pstr}) (stroke (width {fmt(w)}) (type default)) (fill (type {fill})))'


def circ_gr(cx, cy, r, w=0.254, fill="none"):
    return f'(circle (center {fmt(cx)} {fmt(cy)}) (radius {fmt(r)}) (stroke (width {fmt(w)}) (type default)) (fill (type {fill})))'


def arc_gr(sx, sy, mx, my, ex, ey, w=0.254):
    return f'(arc (start {fmt(sx)} {fmt(sy)}) (mid {fmt(mx)} {fmt(my)}) (end {fmt(ex)} {fmt(ey)}) (stroke (width {fmt(w)}) (type default)) (fill (type none)))'


def build_symbol_catalog():
    """Create all KiCad 10 schematic symbols needed for V2 with exact 2.54 mm pin grids."""
    syms = {}

    def add(s):
        syms[s.name] = s

    # --- Resistors (5.08 mm from center -> total 10.16 mm span, pins on 2.54 mm grid) ---
    add(SymDef(
        "R_Vert", "R", ref_at=(-2.54, 1.27, 0), val_at=(-2.54, -1.27, 0),
        pin_names_hide=True, pin_numbers_hide=False,
        graphics=[rect_gr(-1.016, 2.54, 1.016, -2.54, w=0.254, fill="none")],
        pins=[("1", "1", 0, 5.08, 270, 2.54), ("2", "2", 0, -5.08, 90, 2.54)],
    ))
    add(SymDef(
        "R_Vert_Flip", "R", ref_at=(-2.54, 1.27, 0), val_at=(-2.54, -1.27, 0),
        pin_names_hide=True, pin_numbers_hide=False,
        graphics=[rect_gr(-1.016, 2.54, 1.016, -2.54, w=0.254, fill="none")],
        pins=[("2", "2", 0, 5.08, 270, 2.54), ("1", "1", 0, -5.08, 90, 2.54)],
    ))
    add(SymDef(
        "R_Horiz", "R", ref_at=(0, 2.54, 0), val_at=(0, -2.54, 0),
        pin_names_hide=True, pin_numbers_hide=False,
        graphics=[rect_gr(-2.54, 1.016, 2.54, -1.016, w=0.254, fill="none")],
        pins=[("1", "1", -5.08, 0, 0, 2.54), ("2", "2", 5.08, 0, 180, 2.54)],
    ))
    add(SymDef(
        "R_Horiz_Flip", "R", ref_at=(0, 2.54, 0), val_at=(0, -2.54, 0),
        pin_names_hide=True, pin_numbers_hide=False,
        graphics=[rect_gr(-2.54, 1.016, 2.54, -1.016, w=0.254, fill="none")],
        pins=[("2", "2", -5.08, 0, 0, 2.54), ("1", "1", 5.08, 0, 180, 2.54)],
    ))

    # --- Capacitors (pin 1/2 = +/-5.08) ---
    add(SymDef(
        "C_Vert", "C", ref_at=(-2.54, 1.27, 0), val_at=(-2.54, -1.27, 0),
        pin_names_hide=True, pin_numbers_hide=False,
        graphics=[
            poly_gr([(-1.778, 0.762), (1.778, 0.762)], w=0.35),
            poly_gr([(-1.778, -0.762), (1.778, -0.762)], w=0.35),
            poly_gr([(0, 2.54), (0, 0.762)], w=0.254),
            poly_gr([(0, -2.54), (0, -0.762)], w=0.254),
        ],
        pins=[("1", "1", 0, 5.08, 270, 2.54), ("2", "2", 0, -5.08, 90, 2.54)],
    ))
    add(SymDef(
        "C_Vert_Flip", "C", ref_at=(-2.54, 1.27, 0), val_at=(-2.54, -1.27, 0),
        pin_names_hide=True, pin_numbers_hide=False,
        graphics=[
            poly_gr([(-1.778, 0.762), (1.778, 0.762)], w=0.35),
            poly_gr([(-1.778, -0.762), (1.778, -0.762)], w=0.35),
            poly_gr([(0, 2.54), (0, 0.762)], w=0.254),
            poly_gr([(0, -2.54), (0, -0.762)], w=0.254),
        ],
        pins=[("2", "2", 0, 5.08, 270, 2.54), ("1", "1", 0, -5.08, 90, 2.54)],
    ))
    add(SymDef(
        "C_Horiz", "C", ref_at=(0, 2.54, 0), val_at=(0, -2.54, 0),
        pin_names_hide=True, pin_numbers_hide=False,
        graphics=[
            poly_gr([(-0.762, 1.778), (-0.762, -1.778)], w=0.35),
            poly_gr([(0.762, 1.778), (0.762, -1.778)], w=0.35),
            poly_gr([(-2.54, 0), (-0.762, 0)], w=0.254),
            poly_gr([(2.54, 0), (0.762, 0)], w=0.254),
        ],
        pins=[("1", "1", -5.08, 0, 0, 2.54), ("2", "2", 5.08, 0, 180, 2.54)],
    ))
    add(SymDef(
        "C_Horiz_Flip", "C", ref_at=(0, 2.54, 0), val_at=(0, -2.54, 0),
        pin_names_hide=True, pin_numbers_hide=False,
        graphics=[
            poly_gr([(-0.762, 1.778), (-0.762, -1.778)], w=0.35),
            poly_gr([(0.762, 1.778), (0.762, -1.778)], w=0.35),
            poly_gr([(-2.54, 0), (-0.762, 0)], w=0.254),
            poly_gr([(2.54, 0), (0.762, 0)], w=0.254),
        ],
        pins=[("2", "2", -5.08, 0, 0, 2.54), ("1", "1", 5.08, 0, 180, 2.54)],
    ))

    # --- Schottky Diode (SOD-123 PCB convention: Pin 2 = Anode (LEFT), Pin 1 = Cathode (RIGHT)) ---
    add(SymDef(
        "D_Schottky_Horiz", "D", ref_at=(0, 2.54, 0), val_at=(0, -2.54, 0),
        pin_names_hide=True, pin_numbers_hide=False,
        graphics=[
            poly_gr([(-1.524, 1.27), (-1.524, -1.27), (1.016, 0), (-1.524, 1.27)], w=0.254, fill="outline"),
            poly_gr([(1.016, 1.27), (1.016, -1.27)], w=0.30),
            poly_gr([(1.016, 1.27), (1.524, 1.27), (1.524, 0.762)], w=0.254),
            poly_gr([(1.016, -1.27), (0.508, -1.27), (0.508, -0.762)], w=0.254),
            poly_gr([(-2.54, 0), (-1.524, 0)], w=0.254),
            poly_gr([(1.016, 0), (2.54, 0)], w=0.254),
        ],
        pins=[("2", "A", -5.08, 0, 0, 2.54), ("1", "K", 5.08, 0, 180, 2.54)],
    ))

    # --- LED (LED_0805 PCB convention: Pin 2 = Anode (TOP), Pin 1 = Cathode (BOT)) ---
    add(SymDef(
        "LED_Vert", "D", ref_at=(-2.54, 1.27, 0), val_at=(-2.54, -1.27, 0),
        pin_names_hide=True, pin_numbers_hide=False,
        graphics=[
            poly_gr([(-1.27, 1.27), (1.27, 1.27), (0, -1.016), (-1.27, 1.27)], w=0.254, fill="outline"),
            poly_gr([(-1.27, -1.016), (1.27, -1.016)], w=0.30),
            poly_gr([(0, 2.54), (0, 1.27)], w=0.254),
            poly_gr([(0, -1.016), (0, -2.54)], w=0.254),
            poly_gr([(1.27, 0.254), (2.286, -0.762)], w=0.2),
            poly_gr([(1.27, -0.508), (2.286, -1.524)], w=0.2),
        ],
        pins=[("2", "A", 0, 5.08, 270, 2.54), ("1", "K", 0, -5.08, 90, 2.54)],
    ))

    # --- Inductor (Horizontal) ---
    add(SymDef(
        "L_Horiz", "L", ref_at=(0, 2.54, 0), val_at=(0, -2.54, 0),
        pin_names_hide=True, pin_numbers_hide=False,
        graphics=[
            arc_gr(-2.54, 0, -1.905, 0.9, -1.27, 0),
            arc_gr(-1.27, 0, -0.635, 0.9, 0, 0),
            arc_gr(0, 0, 0.635, 0.9, 1.27, 0),
            arc_gr(1.27, 0, 1.905, 0.9, 2.54, 0),
        ],
        pins=[("1", "1", -5.08, 0, 0, 2.54), ("2", "2", 5.08, 0, 180, 2.54)],
    ))
    add(SymDef(
        "L_Horiz_Flip", "L", ref_at=(0, 2.54, 0), val_at=(0, -2.54, 0),
        pin_names_hide=True, pin_numbers_hide=False,
        graphics=[
            arc_gr(-2.54, 0, -1.905, 0.9, -1.27, 0),
            arc_gr(-1.27, 0, -0.635, 0.9, 0, 0),
            arc_gr(0, 0, 0.635, 0.9, 1.27, 0),
            arc_gr(1.27, 0, 1.905, 0.9, 2.54, 0),
        ],
        pins=[("2", "2", -5.08, 0, 0, 2.54), ("1", "1", 5.08, 0, 180, 2.54)],
    ))

    # --- Resettable PTC Fuse (Horizontal) ---
    add(SymDef(
        "Fuse_Horiz", "F", ref_at=(0, 2.54, 0), val_at=(0, -2.54, 0),
        pin_names_hide=True, pin_numbers_hide=False,
        graphics=[
            rect_gr(-2.54, 1.016, 2.54, -1.016, w=0.254, fill="none"),
            poly_gr([(-2.54, 0), (2.54, 0)], w=0.2),
        ],
        pins=[("1", "1", -5.08, 0, 0, 2.54), ("2", "2", 5.08, 0, 180, 2.54)],
    ))

    # --- Tactile Switch SPST (Horizontal & Vertical) ---
    add(SymDef(
        "SW_Push_H", "SW", ref_at=(0, 3.048, 0), val_at=(0, -2.54, 0),
        pin_names_hide=True, pin_numbers_hide=False,
        graphics=[
            circ_gr(-2.032, 0, 0.508),
            circ_gr(2.032, 0, 0.508),
            poly_gr([(-1.778, 1.27), (1.778, 1.27)], w=0.254),
            poly_gr([(0, 1.27), (0, 2.286)], w=0.254),
        ],
        pins=[("1", "1", -5.08, 0, 0, 2.54), ("2", "2", 5.08, 0, 180, 2.54)],
    ))
    add(SymDef(
        "SW_Push_V", "SW", ref_at=(-3.81, 1.27, 0), val_at=(-3.81, -1.27, 0),
        pin_names_hide=True, pin_numbers_hide=False,
        graphics=[
            circ_gr(0, 2.032, 0.508),
            circ_gr(0, -2.032, 0.508),
            poly_gr([(1.27, 1.778), (1.27, -1.778)], w=0.254),
            poly_gr([(1.27, 0), (2.286, 0)], w=0.254),
        ],
        pins=[("2", "2", 0, 5.08, 270, 2.54), ("1", "1", 0, -5.08, 90, 2.54)],
    ))

    # --- Slide Power Switch MSK12C02 (1=NC, 2=COM, 3=ON, SH=SHIELD) ---
    add(SymDef(
        "SW_SPDT_MSK12C02", "SW", ref_at=(0, 4.572, 0), val_at=(0, -7.62, 0),
        pin_names_hide=False, pin_numbers_hide=False, pin_names_offset=0.508,
        graphics=[rect_gr(-2.54, 3.81, 2.54, -3.81, w=0.254, fill="background")],
        pins=[
            ("3", "ON", -5.08, 2.54, 0, 2.54),
            ("1", "OFF", -5.08, -2.54, 0, 2.54),
            ("2", "COM", 5.08, 0, 180, 2.54),
            ("SH", "SH", 0, -5.08, 90, 1.27),
        ],
    ))

    # --- Solder Jumper 2-pin ---
    add(SymDef(
        "SolderJumper_2", "JP", ref_at=(0, 2.54, 0), val_at=(0, -2.54, 0),
        pin_names_hide=True, pin_numbers_hide=False,
        graphics=[
            arc_gr(-1.524, 0, -0.762, 1.016, -0.254, 0),
            arc_gr(0.254, 0, 0.762, 1.016, 1.524, 0),
            poly_gr([(-2.54, 0), (-1.524, 0)], w=0.254),
            poly_gr([(1.524, 0), (2.54, 0)], w=0.254),
        ],
        pins=[("1", "1", -5.08, 0, 0, 2.54), ("2", "2", 5.08, 0, 180, 2.54)],
    ))

    # --- AO3401A P-MOSFET (SOT-23: 1=G, 2=D, 3=S) ---
    add(SymDef(
        "Q_PMOS_AO3401A", "Q", ref_at=(0, 3.81, 0), val_at=(3.81, -3.81, 0),
        pin_names_hide=False, pin_numbers_hide=False, pin_names_offset=0.508,
        graphics=[
            circ_gr(0, 0, 2.54, w=0.254, fill="background"),
            poly_gr([(-2.54, 0), (2.54, 0)], w=0.254),
            poly_gr([(-0.762, 0.762), (0.762, 0), (-0.762, -0.762)], w=0.254),
            poly_gr([(-1.27, -1.27), (1.27, -1.27)], w=0.254),
            poly_gr([(0, -2.54), (0, -1.27)], w=0.254),
        ],
        pins=[
            ("3", "S", -5.08, 0, 0, 2.54),
            ("2", "D", 5.08, 0, 180, 2.54),
            ("1", "G", 0, -5.08, 90, 2.54),
        ],
    ))

    # --- Mounting Hole (0-pin) ---
    add(SymDef(
        "MountingHole", "H", ref_at=(0, 2.54, 0), val_at=(0, -2.54, 0),
        pin_names_hide=True, pin_numbers_hide=True,
        graphics=[circ_gr(0, 0, 1.27, w=0.254, fill="none")],
        pins=[],
    ))

    # --- AMS1117-3.3 LDO (SOT-223: 3=VIN, 2=VOUT, 1=GND) ---
    add(SymDef(
        "AMS1117_3V3", "U", ref_at=(0, 5.08, 0), val_at=(0, -7.62, 0),
        pin_names_hide=False, pin_numbers_hide=False, pin_names_offset=0.508,
        graphics=[rect_gr(-5.08, 3.81, 5.08, -2.54, w=0.254, fill="background")],
        pins=[
            ("3", "VI", -7.62, 0, 0, 2.54),
            ("2", "VO", 7.62, 0, 180, 2.54),
            ("1", "GND", 0, -5.08, 90, 2.54),
        ],
    ))

    # --- DRV5032FADBZR Hall Sensor (SOT-23: 1=VCC, 2=OUT, 3=GND) ---
    add(SymDef(
        "DRV5032", "U", ref_at=(0, 5.08, 0), val_at=(0, -5.08, 0),
        pin_names_hide=False, pin_numbers_hide=False, pin_names_offset=0.508,
        graphics=[rect_gr(-5.08, 3.81, 5.08, -3.81, w=0.254, fill="background")],
        pins=[
            ("1", "VCC", -7.62, 2.54, 0, 2.54),
            ("3", "GND", -7.62, -2.54, 0, 2.54),
            ("2", "OUT", 7.62, 0, 180, 2.54),
        ],
    ))

    # --- MT2492 5.0V 2A Buck (SOT-23-6: 1=BST, 2=GND, 3=FB, 4=EN, 5=VIN, 6=LX) ---
    add(SymDef(
        "MT2492", "U", ref_at=(0, 8.89, 0), val_at=(0, -8.89, 0),
        pin_names_hide=False, pin_numbers_hide=False, pin_names_offset=0.508,
        graphics=[rect_gr(-7.62, 7.62, 7.62, -7.62, w=0.254, fill="background")],
        pins=[
            ("5", "VIN", -10.16, 5.08, 0, 2.54),
            ("4", "EN", -10.16, 0, 0, 2.54),
            ("2", "GND", -10.16, -5.08, 0, 2.54),
            ("1", "BST", 10.16, 5.08, 180, 2.54),
            ("6", "LX", 10.16, 0, 180, 2.54),
            ("3", "FB", 10.16, -5.08, 180, 2.54),
        ],
    ))

    # --- ADS1115 16-bit 4ch I2C ADC (MSOP-10) ---
    add(SymDef(
        "ADS1115", "U", ref_at=(0, 8.89, 0), val_at=(0, -8.89, 0),
        pin_names_hide=False, pin_numbers_hide=False, pin_names_offset=0.508,
        graphics=[rect_gr(-7.62, 7.62, 7.62, -7.62, w=0.254, fill="background")],
        pins=[
            ("4", "AIN0", -10.16, 5.08, 0, 2.54),
            ("5", "AIN1", -10.16, 2.54, 0, 2.54),
            ("6", "AIN2", -10.16, 0, 0, 2.54),
            ("7", "AIN3", -10.16, -2.54, 0, 2.54),
            ("1", "ADDR", -10.16, -5.08, 0, 2.54),
            ("8", "VDD", 10.16, 5.08, 180, 2.54),
            ("10", "SCL", 10.16, 2.54, 180, 2.54),
            ("9", "SDA", 10.16, 0, 180, 2.54),
            ("2", "ALERT", 10.16, -2.54, 180, 2.54),
            ("3", "GND", 10.16, -5.08, 180, 2.54),
        ],
    ))

    # --- IP2326 2S Balance Charger (QFN-24-1EP, 25 pins) ---
    add(SymDef(
        "IP2326", "U", ref_at=(0, 19.05, 0), val_at=(0, -19.05, 0),
        pin_names_hide=False, pin_numbers_hide=False, pin_names_offset=0.508,
        graphics=[rect_gr(-10.16, 17.78, 10.16, -17.78, w=0.254, fill="background")],
        pins=[
            ("13", "VIN", -12.70, 15.24, 0, 2.54),
            ("12", "EN", -12.70, 12.70, 0, 2.54),
            ("8", "VSET", -12.70, 10.16, 0, 2.54),
            ("11", "ISET", -12.70, 7.62, 0, 2.54),
            ("4", "NTC", -12.70, 5.08, 0, 2.54),
            ("6", "LED", -12.70, 2.54, 0, 2.54),
            ("1", "NC1", -12.70, 0, 0, 2.54),
            ("2", "NC2", -12.70, -2.54, 0, 2.54),
            ("3", "NC3", -12.70, -5.08, 0, 2.54),
            ("5", "NC5", -12.70, -7.62, 0, 2.54),
            ("7", "NC7", -12.70, -10.16, 0, 2.54),
            ("9", "DP_NC", -12.70, -12.70, 0, 2.54),
            ("10", "DM_NC", -12.70, -15.24, 0, 2.54),
            ("14", "BST", 12.70, 15.24, 180, 2.54),
            ("15", "LX1", 12.70, 12.70, 180, 2.54),
            ("16", "LX2", 12.70, 10.16, 180, 2.54),
            ("17", "LX3", 12.70, 7.62, 180, 2.54),
            ("19", "VSYS1", 12.70, 5.08, 180, 2.54),
            ("20", "VSYS2", 12.70, 2.54, 180, 2.54),
            ("21", "BAT1", 12.70, 0, 180, 2.54),
            ("22", "BAT2", 12.70, -2.54, 180, 2.54),
            ("23", "VBATM", 12.70, -5.08, 180, 2.54),
            ("18", "GND18", 12.70, -7.62, 180, 2.54),
            ("24", "GND24", 12.70, -10.16, 180, 2.54),
            ("25", "EPAD", 12.70, -12.70, 180, 2.54),
        ],
    ))

    # --- ESP32-C6-WROOM-1-N8 (28 castellated + EPAD 29) ---
    wroom_pins = [
        ("1", "GND", -12.70, 17.78, 0, 2.54),
        ("2", "3V3", -12.70, 15.24, 0, 2.54),
        ("3", "EN", -12.70, 12.70, 0, 2.54),
        ("4", "IO4", -12.70, 10.16, 0, 2.54),
        ("5", "IO5", -12.70, 7.62, 0, 2.54),
        ("6", "IO6", -12.70, 5.08, 0, 2.54),
        ("7", "IO7", -12.70, 2.54, 0, 2.54),
        ("8", "IO0", -12.70, 0, 0, 2.54),
        ("9", "IO1", -12.70, -2.54, 0, 2.54),
        ("10", "IO8", -12.70, -5.08, 0, 2.54),
        ("11", "IO10", -12.70, -7.62, 0, 2.54),
        ("12", "IO11", -12.70, -10.16, 0, 2.54),
        ("13", "IO12/DN", -12.70, -12.70, 0, 2.54),
        ("14", "IO13/DP", -12.70, -15.24, 0, 2.54),
        ("29", "EPAD", -12.70, -17.78, 0, 2.54),
        ("28", "GND28", 12.70, 17.78, 180, 2.54),
        ("27", "IO2", 12.70, 15.24, 180, 2.54),
        ("26", "IO3", 12.70, 12.70, 180, 2.54),
        ("25", "TXD0", 12.70, 10.16, 180, 2.54),
        ("24", "RXD0", 12.70, 7.62, 180, 2.54),
        ("23", "IO15", 12.70, 5.08, 180, 2.54),
        ("22", "IO14", 12.70, 2.54, 180, 2.54),
        ("21", "IO23", 12.70, 0, 180, 2.54),
        ("20", "IO22", 12.70, -2.54, 180, 2.54),
        ("19", "IO21", 12.70, -5.08, 180, 2.54),
        ("18", "IO20", 12.70, -7.62, 180, 2.54),
        ("17", "IO19", 12.70, -10.16, 180, 2.54),
        ("16", "IO18", 12.70, -12.70, 180, 2.54),
        ("15", "IO9/BOOT", 12.70, -15.24, 180, 2.54),
    ]
    add(SymDef(
        "ESP32_C6_WROOM_1", "U", ref_at=(0, 21.59, 0), val_at=(0, -21.59, 0),
        pin_names_hide=False, pin_numbers_hide=False, pin_names_offset=0.508,
        graphics=[rect_gr(-10.16, 20.32, 10.16, -20.32, w=0.254, fill="background")],
        pins=wroom_pins,
    ))

    # --- Alps RKJXV122400R PS5 3D Joystick (14 pins) ---
    add(SymDef(
        "Alps_RKJXV122400R", "JOY", ref_at=(0, 16.51, 0), val_at=(0, -16.51, 0),
        pin_names_hide=False, pin_numbers_hide=False, pin_names_offset=0.508,
        graphics=[rect_gr(-10.16, 15.24, 10.16, -12.70, w=0.254, fill="background")],
        pins=[
            ("1", "Y_GND", -12.70, 12.70, 0, 2.54),
            ("2", "Y_WIPER", -12.70, 10.16, 0, 2.54),
            ("3", "Y_3V3", -12.70, 7.62, 0, 2.54),
            ("4", "X_GND", -12.70, 0, 0, 2.54),
            ("5", "X_WIPER", -12.70, -2.54, 0, 2.54),
            ("6", "X_3V3", -12.70, -5.08, 0, 2.54),
            ("7", "SW_A1", 12.70, 12.70, 180, 2.54),
            ("9", "SW_A2", 12.70, 10.16, 180, 2.54),
            ("8", "SW_B1", 12.70, 7.62, 180, 2.54),
            ("10", "SW_B2", 12.70, 5.08, 180, 2.54),
            ("11", "SH1", 12.70, -2.54, 180, 2.54),
            ("12", "SH2", 12.70, -5.08, 180, 2.54),
            ("13", "SH3", 12.70, -7.62, 180, 2.54),
            ("14", "SH4", 12.70, -10.16, 180, 2.54),
        ],
    ))

    # --- HRO TYPE-C-31-M-12 16P USB-C Receptacle (all 17 pin groups on RIGHT) ---
    usbc_list = [
        ("A4", "VBUS_A4"), ("A9", "VBUS_A9"), ("B4", "VBUS_B4"), ("B9", "VBUS_B9"),
        ("A5", "CC1"), ("B5", "CC2"),
        ("A6", "DP1"), ("B6", "DP2"),
        ("A7", "DN1"), ("B7", "DN2"),
        ("A8", "SBU1"), ("B8", "SBU2"),
        ("A1", "GND_A1"), ("B1", "GND_B1"), ("A12", "GND_A12"), ("B12", "GND_B12"), ("SH", "SHIELD"),
    ]
    usbc_pins = [(num, nm, 7.62, 20.32 - idx * 2.54, 180, 2.54) for idx, (num, nm) in enumerate(usbc_list)]
    add(SymDef(
        "USB_C_HRO_16P", "J", ref_at=(-1.27, 24.13, 0), val_at=(-1.27, -24.13, 0),
        pin_names_hide=False, pin_numbers_hide=False, pin_names_offset=0.508,
        graphics=[rect_gr(-7.62, 22.86, 5.08, -22.86, w=0.254, fill="background")],
        pins=usbc_pins,
    ))

    # --- Generic Connectors 1x02 .. 1x09 (both Left-facing and Right-facing pins) ---
    for n in (2, 3, 4, 6, 7, 9):
        top_y = ((n - 1) // 2) * 2.54
        bot_y = top_y - (n - 1) * 2.54
        box_top = top_y + 2.54
        box_bot = bot_y - 2.54
        # Right-facing pins (_R): body -2.54..2.54, pins at +5.08 pointing 180
        add(SymDef(
            f"Conn_01x{n:02d}_R", "J", ref_at=(0, box_top + 1.778, 0), val_at=(0, box_bot - 1.778, 0),
            pin_names_hide=True, pin_numbers_hide=False,
            graphics=[rect_gr(-2.54, box_top, 2.54, box_bot, w=0.254, fill="background")],
            pins=[(str(i + 1), f"P{i+1}", 5.08, top_y - i * 2.54, 180, 2.54) for i in range(n)],
        ))
        # Left-facing pins (_L): body -2.54..2.54, pins at -5.08 pointing 0
        add(SymDef(
            f"Conn_01x{n:02d}_L", "J", ref_at=(0, box_top + 1.778, 0), val_at=(0, box_bot - 1.778, 0),
            pin_names_hide=True, pin_numbers_hide=False,
            graphics=[rect_gr(-2.54, box_top, 2.54, box_bot, w=0.254, fill="background")],
            pins=[(str(i + 1), f"P{i+1}", -5.08, top_y - i * 2.54, 0, 2.54) for i in range(n)],
        ))

    # --- Ai-Thinker Ra-02 (SX1278 433MHz LoRa SMD-16P, IPEX/U.FL) ---
    add(SymDef(
        "Ra02_LoRa", "U", ref_at=(0, 12.70, 0), val_at=(0, -12.70, 0),
        pin_names_hide=False, pin_numbers_hide=False, pin_names_offset=0.508,
        graphics=[rect_gr(-7.62, 11.43, 7.62, -11.43, w=0.254, fill="background")],
        pins=[
            ("1", "GND", -10.16, 8.89, 0, 2.54),
            ("2", "GND2", -10.16, 6.35, 0, 2.54),
            ("3", "VDD_3V3", -10.16, 3.81, 0, 2.54),
            ("4", "~{RESET}", -10.16, 1.27, 0, 2.54),
            ("5", "DIO0", -10.16, -1.27, 0, 2.54),
            ("6", "DIO1", -10.16, -3.81, 0, 2.54),
            ("7", "DIO2", -10.16, -6.35, 0, 2.54),
            ("8", "DIO3", -10.16, -8.89, 0, 2.54),
            ("16", "GND16", 10.16, 8.89, 180, 2.54),
            ("15", "~{NSS}", 10.16, 6.35, 180, 2.54),
            ("14", "MOSI", 10.16, 3.81, 180, 2.54),
            ("13", "MISO", 10.16, 1.27, 180, 2.54),
            ("12", "SCK", 10.16, -1.27, 180, 2.54),
            ("11", "DIO5", 10.16, -3.81, 180, 2.54),
            ("10", "DIO4", 10.16, -6.35, 180, 2.54),
            ("9", "GND9", 10.16, -8.89, 180, 2.54),
        ],
    ))

    return syms


# ==============================================================================
# 3. SCHEMATIC BUILDER (Strict Grid + Wire Splitter + Junction Inference + Parity)
# ==============================================================================
class SchematicBuilder:
    def __init__(self, sym_catalog, pcb_comps):
        self.syms = sym_catalog
        self.pcb_comps = pcb_comps
        self.instances = []     # (sym_name, ref, val, fp, gx, gy, options)
        self.pin_coords = {}    # (ref, pnum) -> (sx_mm, sy_mm, dir_hint)
        self.raw_wires = []     # ((x1, y1), (x2, y2))
        self.labels = []        # (text, x, y, angle, justify)
        self.no_connects = []   # (x, y)
        self.boxes = []         # (x1, y1, x2, y2, dash, w)
        self.texts = []         # (txt, x, y, size, bold, justify)
        self.used_syms = set()
        self.explicit_nets = {} # (ref, pnum) -> expected net (checked alongside wires)

    @staticmethod
    def _clean_net(pcb_net):
        if not pcb_net:
            return ""
        return pcb_net[1:] if pcb_net.startswith("/") else pcb_net

    def place(
        self, sym_name, ref, gx, gy,
        val=None, fp=None, in_bom=None, in_pos=None,
        ref_offset=None, val_offset=None,
        ref_justify=None, val_justify=None,
        hide_ref=False, hide_val=False,
    ):
        assert sym_name in self.syms, f"Unknown symbol {sym_name}"
        assert ref in self.pcb_comps, f"Ref {ref} not found in PCB!"
        assert ref not in {inst[1] for inst in self.instances}, f"Duplicate placed ref {ref}"
        self.used_syms.add(sym_name)
        meta = self.pcb_comps[ref]
        sx, sy = g(gx), g(gy)
        sym = self.syms[sym_name]
        # Verify 1-to-1 pin number parity with PCB
        pcb_pnums = set(meta["pads"].keys())
        sym_pnums = set(sym.pin_map.keys())
        assert pcb_pnums == sym_pnums, f"Pin mismatch on {ref} ({sym_name}): PCB={sorted(pcb_pnums)} vs Sym={sorted(sym_pnums)}"

        if ref_offset is None:
            ref_offset = (sym.ref_at[0], -sym.ref_at[1])
        if val_offset is None:
            val_offset = (sym.val_at[0], -sym.val_at[1])
        if ref_justify is None:
            ref_justify = "right" if ref_offset[0] <= -1.9 else ("left" if ref_offset[0] >= 1.9 else "center")
        if val_justify is None:
            val_justify = "right" if val_offset[0] <= -1.9 else ("left" if val_offset[0] >= 1.9 else "center")

        use_val = meta["value"] if val is None else val
        use_fp = meta["footprint"] if fp is None else fp
        use_bom = meta["in_bom"] if in_bom is None else in_bom
        use_pos = meta["in_pos"] if in_pos is None else in_pos

        self.instances.append((
            sym_name, ref, use_val, use_fp, sx, sy, {
                "in_bom": use_bom,
                "in_pos": use_pos,
                "ref_at": (round(sx + ref_offset[0], 4), round(sy + ref_offset[1], 4), 0),
                "val_at": (round(sx + val_offset[0], 4), round(sy + val_offset[1], 4), 0),
                "ref_justify": ref_justify,
                "val_justify": val_justify,
                "hide_ref": hide_ref,
                "hide_val": hide_val,
                "uuid": new_uuid(),
            },
        ))
        for pnum, (px, py, pang) in sym.pin_map.items():
            # Library is Y-UP, schematic sheet is Y-DOWN -> sheet_y = sy - py
            shx = round(sx + px, 4)
            shy = round(sy - py, 4)
            self.pin_coords[(ref, pnum)] = (shx, shy, pang)
        return self

    def P(self, ref, pnum):
        return self.pin_coords[(ref, str(pnum))][:2]

    def wire_pts(self, pts):
        for a, b in zip(pts[:-1], pts[1:]):
            p1 = (round(a[0], 4), round(a[1], 4))
            p2 = (round(b[0], 4), round(b[1], 4))
            if pt_key(*p1) == pt_key(*p2):
                continue
            dx = abs(p1[0] - p2[0])
            dy = abs(p1[1] - p2[1])
            assert dx < 1e-3 or dy < 1e-3, f"Non-orthogonal schematic wire {p1} -> {p2}"
            self.raw_wires.append((p1, p2))

    def wg(self, gpts):
        self.wire_pts([(g(x), g(y)) for x, y in gpts])

    def label(self, net_name, x, y, angle=0, justify="left"):
        lbl = self._clean_net(net_name)
        assert lbl, f"Empty label at {(x, y)}"
        self.labels.append((lbl, round(x, 4), round(y, 4), angle, justify))

    def nc_pin(self, ref, pnum):
        px, py = self.P(ref, pnum)
        self.no_connects.append((px, py))
        pcb_net = self.pcb_comps[ref]["pads"][str(pnum)]
        assert pcb_net == "", f"Cannot mark {ref}:{pnum} as no_connect when PCB connects it to {pcb_net}"

    def stub(self, ref, pnum, length_g=2, side=None):
        """Attach wire stub + exact PCB net label (or no_connect if <NC>) to a symbol pin."""
        pnum = str(pnum)
        px, py, pang = self.pin_coords[(ref, pnum)]
        pcb_net = self.pcb_comps[ref]["pads"][pnum]
        if not pcb_net:
            self.nc_pin(ref, pnum)
            return (px, py)
        if side is None:
            # angle 0 -> pin points right into body -> stub extends LEFT (-x)
            # angle 180 -> pin points left into body -> stub extends RIGHT (+x)
            # angle 270 -> pin points down into body -> stub extends UP (-y)
            # angle 90 -> pin points up into body -> stub extends DOWN (+y)
            side = {0: "L", 180: "R", 270: "U", 90: "D"}[pang]
        d = g(length_g)
        if side == "L":
            ex, ey = round(px - d, 4), py
            self.wire_pts([(px, py), (ex, ey)])
            self.label(pcb_net, ex, ey, angle=180, justify="right")
        elif side == "R":
            ex, ey = round(px + d, 4), py
            self.wire_pts([(px, py), (ex, ey)])
            self.label(pcb_net, ex, ey, angle=0, justify="left")
        elif side == "U":
            ex, ey = px, round(py - d, 4)
            self.wire_pts([(px, py), (ex, ey)])
            self.label(pcb_net, ex, ey, angle=0, justify="left")
        elif side == "D":
            ex, ey = px, round(py + d, 4)
            self.wire_pts([(px, py), (ex, ey)])
            self.label(pcb_net, ex, ey, angle=0, justify="left")
        return (ex, ey)

    def auto_pin(self, ref, exclude=(), length_g=2, sides=None):
        sides = sides or {}
        for pnum in self.syms[[i[0] for i in self.instances if i[1] == ref][0]].pin_map:
            if pnum in exclude:
                continue
            self.stub(ref, pnum, length_g=length_g, side=sides.get(pnum))

    def add_section_box(self, title, subtitle, gx1, gy1, gx2, gy2, solid=False):
        x1, y1, x2, y2 = g(gx1), g(gy1), g(gx2), g(gy2)
        self.boxes.append((x1, y1, x2, y2, not solid, 0.30 if not solid else 0.25))
        if title:
            self.texts.append((title, g(gx1 + 2), g(gy1 + 3), 2.54, True, "left"))
        if subtitle:
            self.texts.append((subtitle, g(gx1 + 2), g(gy1 + 6), 1.524, False, "left"))

    def add_text(self, txt, gx, gy, size=1.27, bold=False, justify="left"):
        self.texts.append((txt, g(gx), g(gy), size, bold, justify))

    def _split_and_finalize_wires(self):
        """Split H/V wire segments at all interior pin/wire/label endpoints, dedup, and compute junctions."""
        all_pts = set()
        for p1, p2 in self.raw_wires:
            all_pts.add(pt_key(*p1))
            all_pts.add(pt_key(*p2))
        for ref, pnum in self.pin_coords:
            all_pts.add(pt_key(*self.pin_coords[(ref, pnum)][:2]))
        for _, lx, ly, _, _ in self.labels:
            all_pts.add(pt_key(lx, ly))

        split_segs = set()
        for p1, p2 in self.raw_wires:
            x1, y1 = pt_key(*p1)
            x2, y2 = pt_key(*p2)
            if abs(y1 - y2) < 1e-3:  # horizontal
                xmin, xmax = min(x1, x2), max(x1, x2)
                xs = sorted({x for (x, y) in all_pts if abs(y - y1) < 1e-3 and xmin - 1e-3 <= x <= xmax + 1e-3})
                for xa, xb in zip(xs[:-1], xs[1:]):
                    if xb - xa > 1e-3:
                        split_segs.add(((xa, y1), (xb, y1)))
            else:  # vertical
                ymin, ymax = min(y1, y2), max(y1, y2)
                ys = sorted({y for (x, y) in all_pts if abs(x - x1) < 1e-3 and ymin - 1e-3 <= y <= ymax + 1e-3})
                for ya, yb in zip(ys[:-1], ys[1:]):
                    if yb - ya > 1e-3:
                        split_segs.add(((x1, ya), (x1, yb)))

        final_wires = sorted(split_segs)
        # Count wire endpoints + pin endpoints at each point -> >= 3 needs a junction!
        deg = Counter()
        for a, b in final_wires:
            deg[a] += 1
            deg[b] += 1
        for ref, pnum in self.pin_coords:
            deg[pt_key(*self.pin_coords[(ref, pnum)][:2])] += 1
        junctions = sorted(pt for pt, c in deg.items() if c >= 3)
        return final_wires, junctions

    def verify_net_connectivity(self, final_wires):
        """Assert that the schematic connectivity matches Dual_ESP_Carrier_V2.kicad_pcb 100%."""
        # 1. All non-STAMP PCB components must be placed
        missing = set(self.pcb_comps.keys()) - {i[1] for i in self.instances}
        assert not missing, f"Components missing from schematic: {sorted(missing)}"

        # 2. Build Union-Find across wires + labels + pins
        parent = {}

        def find(u):
            parent.setdefault(u, u)
            while parent[u] != u:
                parent[u] = parent[parent[u]]
                u = parent[u]
            return u

        def union(u, v):
            ru, rv = find(u), find(v)
            if ru != rv:
                parent[rv] = ru

        for a, b in final_wires:
            union(("pt", a), ("pt", b))
        wire_pts_set = {a for a, _ in final_wires} | {b for _, b in final_wires}
        pin_pts_set = {pt_key(px, py) for (px, py, _) in self.pin_coords.values()}
        for lbl, lx, ly, _, _ in self.labels:
            lpt = pt_key(lx, ly)
            assert lpt in wire_pts_set or lpt in pin_pts_set, f"Floating label {lbl} at ({lx/GRID:.1f}, {ly/GRID:.1f})!"
            union(("lbl", lbl), ("pt", lpt))
        for (ref, pnum), (px, py, _) in self.pin_coords.items():
            union(("pin", ref, pnum), ("pt", pt_key(px, py)))

        # Map each connected component to its net label(s)
        comp_labels = defaultdict(set)
        for lbl, lx, ly, _, _ in self.labels:
            comp_labels[find(("lbl", lbl))].add(lbl)
        for comp, lbls in comp_labels.items():
            assert len(lbls) == 1, f"Conflicting labels shorted together in schematic: {sorted(lbls)}"

        # Check every pin resolves to exact PCB net!
        nc_set = {pt_key(x, y) for x, y in self.no_connects}
        for (ref, pnum), pcb_net in sorted(
            (k, self.pcb_comps[k[0]]["pads"][k[1]]) for k in self.pin_coords
        ):
            expected = self._clean_net(pcb_net)
            root = find(("pin", ref, pnum))
            got_lbls = comp_labels.get(root, set())
            got = next(iter(got_lbls)) if got_lbls else ""
            if expected == "":
                px, py = self.P(ref, pnum)
                assert got == "", f"PCB <NC> pin {ref}:{pnum} accidentally wired to {got}"
                assert pt_key(px, py) in nc_set, f"Pin {ref}:{pnum} is <NC> without no_connect X"
            else:
                assert got == expected, f"Parity mismatch on {ref}:{pnum}: expected={expected!r}, schematic={got!r}"

        print(f"Verified 100% Python schematic<->PCB parity: {len(self.instances)} components, {len(self.pin_coords)} pins, {len(comp_labels)} nets.")

    def write_sym_lib(self, lib_path, table_path):
        lines = [
            "(kicad_symbol_lib",
            "\t(version 20251024)",
            '\t(generator "kicad_symbol_editor")',
            '\t(generator_version "10.0")',
        ]
        for sname in sorted(self.used_syms):
            lines.append(self.syms[sname].render(sname, indent="\t"))
        lines.append(")")
        with open(lib_path, "w") as f:
            f.write("\n".join(lines) + "\n")

        tbl = (
            "(sym_lib_table\n"
            "  (version 7)\n"
            f'  (lib (name "{PROJECT_NAME}")(type "KiCad")(uri "${{KIPRJMOD}}/{PROJECT_NAME}.kicad_sym")(options "")(descr "Project local symbols"))\n'
            ")\n"
        )
        with open(table_path, "w") as f:
            f.write(tbl)

    def write_sch(self, sch_path):
        final_wires, junctions = self._split_and_finalize_wires()
        self.verify_net_connectivity(final_wires)

        out = [
            "(kicad_sch",
            "\t(version 20260306)",
            '\t(generator "eeschema")',
            '\t(generator_version "10.0")',
            f'\t(uuid "{ROOT_UUID}")',
            '\t(paper "A1")',
            "\t(title_block",
            '\t\t(title "WindDragons Dual ESP32-C6 Carrier V2")',
            '\t\t(date "2026-11-18")',
            '\t\t(rev "V2.0 - 2S Solar/USB-C + Onboard C6 + JOY1 PCBA")',
            '\t\t(company "WindDragons Autonomous Rover")',
            '\t\t(comment 1 "Master: Waveshare ESP32-C6-LCD-1.47 | Slave: Onboard ESP32-C6-WROOM-1-N8 | LoRa: Ai-Thinker Ra-02 433MHz | Joy: Alps RKJXV122400R")',
            '\t\t(comment 2 "2S USB-C/Solar IP2326 + MT2492 5V Buck + AMS1117 3V3 + 2x ADS1115 (3S-6S) + Breakaway Winch/Hall")',
            "\t)",
            "\t(lib_symbols",
        ]
        for sname in sorted(self.used_syms):
            out.append(self.syms[sname].render(f"{PROJECT_NAME}:{sname}", indent="\t\t"))
        out.append("\t)")

        # Section boxes
        for x1, y1, x2, y2, dash, w in self.boxes:
            stype = "dash" if dash else "solid"
            pts = f"(xy {fmt(x1)} {fmt(y1)}) (xy {fmt(x2)} {fmt(y1)}) (xy {fmt(x2)} {fmt(y2)}) (xy {fmt(x1)} {fmt(y2)}) (xy {fmt(x1)} {fmt(y1)})"
            out.append(
                f'\t(polyline\n\t\t(pts {pts})\n\t\t(stroke (width {fmt(w)}) (type {stype}))\n\t\t(fill (type none))\n\t\t(uuid "{new_uuid()}")\n\t)'
            )

        # Free texts
        for txt, tx, ty, sz, bold, just in self.texts:
            safe_txt = txt.replace("\\", "\\\\").replace('"', '\\"')
            b_str = "\n\t\t\t\t(bold yes)" if bold else ""
            j_str = "" if just == "center" else f" (justify {just})"
            out.append(
                f'\t(text "{safe_txt}"\n'
                f'\t\t(exclude_from_sim no)\n'
                f'\t\t(at {fmt(tx)} {fmt(ty)} 0)\n'
                f'\t\t(effects (font (size {fmt(sz)} {fmt(sz)}){b_str}){j_str})\n'
                f'\t\t(uuid "{new_uuid()}")\n\t)'
            )

        # Wires
        for (x1, y1), (x2, y2) in final_wires:
            out.append(
                f'\t(wire\n\t\t(pts (xy {fmt(x1)} {fmt(y1)}) (xy {fmt(x2)} {fmt(y2)}))\n'
                f'\t\t(stroke (width 0) (type default))\n\t\t(uuid "{new_uuid()}")\n\t)'
            )

        # Junctions
        for jx, jy in junctions:
            out.append(
                f'\t(junction (at {fmt(jx)} {fmt(jy)}) (diameter 0) (color 0 0 0 0) (uuid "{new_uuid()}"))'
            )

        # Labels
        for lbl, lx, ly, ang, just in self.labels:
            out.append(
                f'\t(label "{lbl}"\n'
                f'\t\t(at {fmt(lx)} {fmt(ly)} {ang})\n'
                f'\t\t(fields_autoplaced yes)\n'
                f'\t\t(effects (font (size 1.143 1.143)) (justify {just}))\n'
                f'\t\t(uuid "{new_uuid()}")\n\t)'
            )

        # No-connects
        for nx, ny in self.no_connects:
            out.append(f'\t(no_connect (at {fmt(nx)} {fmt(ny)}) (uuid "{new_uuid()}"))')

        # Symbol instances
        for sname, ref, val, fp, sx, sy, opts in self.instances:
            rx, ry, ra = opts["ref_at"]
            vx, vy, va = opts["val_at"]
            href = "\n\t\t\t(hide yes)" if opts["hide_ref"] else ""
            hval = "\n\t\t\t(hide yes)" if opts["hide_val"] else ""
            bom_s = "yes" if opts["in_bom"] else "no"
            pos_s = "yes" if opts["in_pos"] else "no"
            sym_obj = self.syms[sname]
            pin_lines = "\n".join(f'\t\t(pin "{p[0]}" (uuid "{new_uuid()}"))' for p in sym_obj.pins)
            if pin_lines:
                pin_lines = "\n" + pin_lines
            rj = "" if opts["ref_justify"] == "center" else f' (justify {opts["ref_justify"]})'
            vj = "" if opts["val_justify"] == "center" else f' (justify {opts["val_justify"]})'
            out.append(
                f'\t(symbol\n'
                f'\t\t(lib_id "{PROJECT_NAME}:{sname}")\n'
                f'\t\t(at {fmt(sx)} {fmt(sy)} 0)\n'
                f'\t\t(unit 1)\n'
                f'\t\t(exclude_from_sim no)\n'
                f'\t\t(in_bom {bom_s})\n'
                f'\t\t(on_board yes)\n'
                f'\t\t(in_pos_files {pos_s})\n'
                f'\t\t(dnp no)\n'
                f'\t\t(fields_autoplaced yes)\n'
                f'\t\t(uuid "{opts["uuid"]}")\n'
                f'\t\t(property "Reference" "{ref}" (at {fmt(rx)} {fmt(ry)} {ra}){href} (effects (font (size 1.27 1.27)){rj}))\n'
                f'\t\t(property "Value" "{val}" (at {fmt(vx)} {fmt(vy)} {va}){hval} (effects (font (size 1.143 1.143)){vj}))\n'
                f'\t\t(property "Footprint" "{fp}" (at {fmt(sx)} {fmt(sy)} 0) (hide yes) (effects (font (size 1.27 1.27))))\n'
                f'\t\t(property "Datasheet" "" (at {fmt(sx)} {fmt(sy)} 0) (hide yes) (effects (font (size 1.27 1.27))))\n'
                f'\t\t(property "Description" "" (at {fmt(sx)} {fmt(sy)} 0) (hide yes) (effects (font (size 1.27 1.27)))){pin_lines}\n'
                f'\t\t(instances\n\t\t\t(project "{PROJECT_NAME}"\n'
                f'\t\t\t\t(path "/{ROOT_UUID}" (reference "{ref}") (unit 1))\n\t\t\t)\n\t\t)\n\t)'
            )

        out.extend([
            "\t(sheet_instances",
            '\t\t(path "/"',
            '\t\t\t(page "1")',
            "\t\t)",
            "\t)",
            "\t(embedded_fonts no)",
            ")",
        ])
        with open(sch_path, "w") as f:
            f.write("\n".join(out) + "\n")


# ==============================================================================
# 4. SCHEMATIC LAYOUT (9 Clean Functional Sections on A1 841 x 594 mm Sheet)
# ==============================================================================
def populate_schematic(sb):
    # ==========================================================================
    # BLOCK 1: WAVESHARE ESP32-C6-LCD-1.47 MASTER & UI BUTTONS (Col 1 Top)
    # ==========================================================================
    sb.add_section_box(
        "1. WAVESHARE ESP32-C6-LCD-1.47 MASTER & UI BUTTONS",
        "Dual 1x09 Master Headers (J1 Left / J2 Right) & 4x SKRKAEE020 Navigation/Boot Pushbuttons",
        6, 6, 108, 60,
    )
    sb.place("Conn_01x09_L", "J1", 31, 31, ref_offset=(0, -13.97), val_offset=(0, 13.97))
    sb.auto_pin("J1", length_g=3)
    sb.add_section_box("", "", 34, 18, 56, 44, solid=True)
    sb.add_text("WAVESHARE ESP32-C6-LCD-1.47", 45, 28, size=1.60, bold=True, justify="center")
    sb.add_text("ST7789 IPS + TF MicroSD (SPI)", 45, 32, size=1.27, justify="center")
    sb.add_text("Inter-ESP UART: TX=IO2, RX=IO1", 45, 35, size=1.143, justify="center")
    sb.place("Conn_01x09_R", "J2", 59, 31, ref_offset=(0, -13.97), val_offset=(0, 13.97))
    sb.auto_pin("J2", length_g=3)

    sw_list = [("SW1", 21), ("SW2", 30), ("SW3", 39), ("SW4", 48)]
    for sref, sy in sw_list:
        sb.place("SW_Push_H", sref, 92, sy, ref_offset=(0, -3.048), val_offset=(0, 3.048))
        sb.stub(sref, "1", length_g=2, side="L")
        sb.wire_pts([sb.P(sref, "2"), (g(101), g(sy))])
    sb.wg([(101, 21), (101, 52), (103, 52)])
    sb.label("GND", g(103), g(52), angle=0, justify="left")

    # ==========================================================================
    # BLOCK 2: ONBOARD ALPS RKJXV122400R 3D JOYSTICK (Col 1 Mid)
    # ==========================================================================
    sb.add_section_box(
        "2. ONBOARD ALPS RKJXV122400R 3D JOYSTICK (JOY1)",
        "Dual 10k-B Potentiometers (GPIO3/GPIO0) & Diagonal Center-Push Tactile Switch (GPIO23)",
        6, 63, 108, 114,
    )
    sb.place("Alps_RKJXV122400R", "JOY1", 52, 88, ref_offset=(0, -17.78), val_offset=(0, 16.51))
    # Pot Y (1,2,3) & Pot X (4,5,6)
    for pnum in ("1", "2", "3", "4", "5", "6"):
        sb.stub("JOY1", pnum, length_g=4, side="L")

    # Tactile switch diagonal wiring (7=GND, 10=JOY_SW, 8 & 9=NC mechanical) + Shield tabs 11..14
    sb.stub("JOY1", "7", length_g=3, side="R")
    sb.nc_pin("JOY1", "8")
    sb.nc_pin("JOY1", "9")
    sb.stub("JOY1", "10", length_g=3, side="R")
    sb.add_text("Diagonal Tact Switch: 7-9 & 8-10 shorted internally", 67, 81, size=1.143)
    for sh in ("11", "12", "13", "14"):
        px, py = sb.P("JOY1", sh)
        sb.wire_pts([(px, py), (g(63), py)])
    sb.wg([(63, 89), (63, 94), (66, 94)])
    sb.label("GND", g(66), g(94), angle=0, justify="left")

    # ==========================================================================
    # BLOCK 3: ONBOARD AI-THINKER RA-02 LORA 433MHz, LC29H GNSS & GY-BNO055 IMU
    # ==========================================================================
    sb.add_section_box(
        "3. ONBOARD AI-THINKER RA-02 LORA 433MHz, LC29H GNSS & GY-BNO055 IMU",
        "Factory-Mounted SX1278 Ra-02 (IPEX/U.FL, SPI), Back GNSS UART Port (J6) & Left-Edge I2C IMU Port (J_IMU)",
        6, 117, 108, 224,
    )
    sb.place("Ra02_LoRa", "U_LORA", 34, 148, ref_offset=(0, -13.97), val_offset=(0, 13.97))
    sb.auto_pin("U_LORA", length_g=4)
    sb.place("C_Vert", "C_LORA", 62, 148, ref_offset=(2.54, -1.27), val_offset=(2.54, 1.27))
    sb.stub("C_LORA", "1", length_g=2, side="U")
    sb.stub("C_LORA", "2", length_g=2, side="D")

    sb.place("Conn_01x04_L", "J_IMU", 90, 146)
    sb.auto_pin("J_IMU", length_g=4)

    sb.place("Conn_01x07_L", "J6", 58, 192)
    sb.auto_pin("J6", length_g=4)

    # ==========================================================================
    # BLOCK 4: ONBOARD ESP32-C6-WROOM-1-N8 SLAVE & DUAL-OR 3.3V LDO (Col 2 Top)
    # ==========================================================================
    sb.add_section_box(
        "4. ONBOARD ESP32-C6-WROOM-1-N8 SLAVE MCU & DUAL-OR 3.3V LDO",
        "USB-C + 5V Buck Schottky OR -> AMS1117-3.3 -> Onboard C6 (RF Keepout, Native USB JTAG, BOOT/RST)",
        113, 6, 217, 94,
    )
    # Subcircuit A: Dual Schottky OR -> LDO_VIN -> U_LDO -> +3V3_C6
    sb.place("D_Schottky_Horiz", "D_LDO_USB", 125, 21, ref_offset=(0, -2.54), val_offset=(0, 2.54))
    sb.place("D_Schottky_Horiz", "D_LDO_5V", 125, 29, ref_offset=(0, -2.54), val_offset=(0, 2.54))
    sb.stub("D_LDO_USB", "2", length_g=2, side="L")
    sb.stub("D_LDO_5V", "2", length_g=2, side="L")

    sb.place("AMS1117_3V3", "U_LDO", 147, 25, ref_offset=(0, -6.35), val_offset=(0, 8.89))
    sb.place("C_Vert", "C_LDO_IN", 136, 33, ref_offset=(-3.81, -1.27), val_offset=(-3.81, 1.27))
    sb.place("C_Vert", "C_LDO_OUT", 156, 33, ref_offset=(2.54, -1.27), val_offset=(2.54, 1.27))

    # LDO_VIN junction bus
    sb.wire_pts([sb.P("D_LDO_USB", "1"), (g(131), g(21)), (g(131), g(29)), sb.P("D_LDO_5V", "1")])
    sb.wire_pts([(g(131), g(25)), sb.P("U_LDO", "3")])
    sb.wire_pts([sb.P("C_LDO_IN", "1"), (g(136), g(25))])
    sb.label("LDO_VIN", g(132), g(25), angle=0, justify="left")

    # +3V3_C6 output bus
    sb.wire_pts([sb.P("U_LDO", "2"), (g(161), g(25))])
    sb.wire_pts([sb.P("C_LDO_OUT", "1"), (g(156), g(25))])
    sb.label("+3V3_C6", g(161), g(25), angle=0, justify="left")

    # Common GND rail under Subcircuit A
    sb.wire_pts([sb.P("C_LDO_IN", "2"), (g(136), g(39)), (g(156), g(39)), sb.P("C_LDO_OUT", "2")])
    sb.wire_pts([sb.P("U_LDO", "1"), (g(147), g(39))])
    sb.wg([(147, 39), (147, 41)])
    sb.label("GND", g(147), g(41), angle=0, justify="left")

    # Subcircuit B: EN RC Reset + SW_C6_RST + SW_C6_BOOT + R_IO8_PU + C_C6_VDD
    sb.place("R_Vert_Flip", "R_C6_EN", 125, 53, ref_offset=(-2.54, -1.27), val_offset=(-2.54, 1.27))
    sb.place("C_Vert", "C_C6_EN", 133, 62, ref_offset=(-2.54, -1.27), val_offset=(-2.54, 1.27))
    sb.place("SW_Push_V", "SW_C6_RST", 142, 62, ref_offset=(-2.54, -1.27), val_offset=(-2.54, 1.27))
    sb.stub("R_C6_EN", "2", length_g=1, side="U")
    sb.wire_pts([sb.P("R_C6_EN", "1"), (g(125), g(57)), (g(145), g(57))])
    sb.wire_pts([sb.P("C_C6_EN", "1"), (g(133), g(57))])
    sb.wire_pts([sb.P("SW_C6_RST", "2"), (g(142), g(57))])
    sb.label("C6_EN", g(145), g(57), angle=0, justify="left")
    sb.wire_pts([sb.P("C_C6_EN", "2"), (g(133), g(67)), (g(142), g(67)), sb.P("SW_C6_RST", "1")])
    sb.wg([(137, 67), (137, 69)])
    sb.label("GND", g(137), g(69), angle=0, justify="left")

    sb.place("SW_Push_V", "SW_C6_BOOT", 125, 81, ref_offset=(-2.54, -1.27), val_offset=(-2.54, 1.27))
    sb.stub("SW_C6_BOOT", "2", length_g=2, side="U")
    sb.stub("SW_C6_BOOT", "1", length_g=1, side="D")

    sb.place("R_Vert_Flip", "R_IO8_PU", 136, 81, ref_offset=(-2.54, -1.27), val_offset=(-2.54, 1.27))
    sb.stub("R_IO8_PU", "2", length_g=1, side="U")
    sb.stub("R_IO8_PU", "1", length_g=1, side="D")

    sb.place("C_Vert", "C_C6_VDD", 148, 81, ref_offset=(-2.54, -1.27), val_offset=(-2.54, 1.27))
    sb.stub("C_C6_VDD", "1", length_g=1, side="U")
    sb.stub("C_C6_VDD", "2", length_g=1, side="D")

    # Subcircuit C: U_C6 ESP32-C6-WROOM-1-N8
    sb.place("ESP32_C6_WROOM_1", "U_C6", 187, 50, ref_offset=(0, -22.86), val_offset=(0, 21.59))
    # Tie GND pins 1 & 29 together cleanly on Left; tie GND pin 28 on Right
    sb.wire_pts([sb.P("U_C6", "1"), (g(178), g(43))])
    sb.label("GND", g(178), g(43), angle=180, justify="right")
    sb.wire_pts([sb.P("U_C6", "29"), (g(178), g(57))])
    sb.label("GND", g(178), g(57), angle=180, justify="right")
    sb.auto_pin("U_C6", exclude=("1", "29"), length_g=4)

    # ==========================================================================
    # BLOCK 5: MOTOR LIPO 3S-6S CELL MONITOR (2x ADS1115 I2C) (Col 2 Bot)
    # ==========================================================================
    sb.add_section_box(
        "5. MOTOR LIPO 3S-6S CELL MONITOR (2x ADS1115 16-BIT I2C)",
        "J_BAL_MOT 90-deg Input + F_BAL 1.5A PTC + 6x RC Dividers (GAIN_ONE) + ADS1115 (0x48 & 0x49) + I2C Pull-Ups",
        113, 97, 217, 224,
    )
    sb.place("Conn_01x07_R", "J_BAL_MOT", 120, 121)
    sb.place("Fuse_Horiz", "F_BAL", 131, 118, ref_offset=(0, -2.54), val_offset=(0, 2.54))
    sb.wire_pts([sb.P("J_BAL_MOT", "1"), sb.P("F_BAL", "1")])
    sb.label("BAL_GND", g(125), g(118), angle=0, justify="left")
    sb.stub("F_BAL", "2", length_g=2, side="R")
    for k in range(1, 7):
        sb.stub("J_BAL_MOT", str(k + 1), length_g=3, side="R")

    # 6 clean divider + RC filter channels stacked at gy = 136, 150, 164, 178, 192, 206
    for k in range(1, 7):
        yc = 136 + (k - 1) * 14
        rt, rb, cf = f"R_C{k}T", f"R_C{k}B", f"C_F{k}"
        sb.place("R_Horiz", rt, 127, yc, ref_offset=(0, -2.54), val_offset=(0, 2.54))
        sb.place("R_Vert", rb, 137, yc + 4, ref_offset=(-2.54, -1.27), val_offset=(-2.54, 1.27))
        sb.place("C_Vert", cf, 146, yc + 4, ref_offset=(2.54, -1.27), val_offset=(2.54, 1.27))
        sb.stub(rt, "1", length_g=3, side="L")
        sb.wire_pts([sb.P(rt, "2"), (g(151), g(yc))])
        sb.wire_pts([sb.P(rb, "1"), (g(137), g(yc))])
        sb.wire_pts([sb.P(cf, "1"), (g(146), g(yc))])
        sb.label(f"ADC_C{k}", g(151), g(yc), angle=0, justify="left")
        sb.wire_pts([sb.P(rb, "2"), (g(137), g(yc + 7)), (g(146), g(yc + 7)), sb.P(cf, "2")])
        sb.wg([(141, yc + 7), (141, yc + 8)])
        sb.label("GND", g(141), g(yc + 8), angle=0, justify="left")

    # U_ADS1 (0x48) & decoupling
    sb.place("ADS1115", "U_ADS1", 184, 122, ref_offset=(0, -10.16), val_offset=(0, 10.16))
    sb.auto_pin("U_ADS1", length_g=4)
    sb.place("C_Vert", "C_ADS1", 210, 122, ref_offset=(-2.54, -1.27), val_offset=(-2.54, 1.27))
    sb.stub("C_ADS1", "1", length_g=2, side="U")
    sb.stub("C_ADS1", "2", length_g=2, side="D")

    # U_ADS2 (0x49) & decoupling
    sb.place("ADS1115", "U_ADS2", 184, 156, ref_offset=(0, -10.16), val_offset=(0, 10.16))
    sb.auto_pin("U_ADS2", length_g=4)
    sb.place("C_Vert", "C_ADS2", 210, 156, ref_offset=(-2.54, -1.27), val_offset=(-2.54, 1.27))
    sb.stub("C_ADS2", "1", length_g=2, side="U")
    sb.stub("C_ADS2", "2", length_g=2, side="D")

    # I2C Pull-ups (10k to +3V3_C6)
    sb.place("R_Vert", "R_I2C_SDA", 180, 194, ref_offset=(-2.54, -1.27), val_offset=(-2.54, 1.27))
    sb.stub("R_I2C_SDA", "1", length_g=2, side="U")
    sb.stub("R_I2C_SDA", "2", length_g=2, side="D")
    sb.place("R_Vert", "R_I2C_SCL", 196, 194, ref_offset=(-2.54, -1.27), val_offset=(-2.54, 1.27))
    sb.stub("R_I2C_SCL", "1", length_g=2, side="U")
    sb.stub("R_I2C_SCL", "2", length_g=2, side="D")

    # ==========================================================================
    # BLOCK 6: USB-C, SOLAR INPUT & IP2326 2S BALANCE CHARGER (Col 3 Top)
    # ==========================================================================
    sb.add_section_box(
        "6. USB-C (JTAG + 5V), SOLAR INPUT & IP2326 2S BALANCE CHARGER",
        "USB-C Native JTAG + 5.1k CC + Dual-OR Schottky + IP2326 Boost-Balance Charger -> JST-XH 2S",
        222, 6, 324, 92,
    )
    sb.place("USB_C_HRO_16P", "J_USBC", 232, 34, ref_offset=(-1.27, -25.4), val_offset=(-1.27, 25.4))
    # Merge J_USBC duplicate pins cleanly onto bus wires (A4/A9/B4/B9 = VBUS_USB)
    vbus_xs = [sb.P("J_USBC", p)[0] for p in ("A4", "A9", "B4", "B9")]
    vbus_ys = [sb.P("J_USBC", p)[1] for p in ("A4", "A9", "B4", "B9")]
    sb.wire_pts([(vbus_xs[0], min(vbus_ys)), (g(239), min(vbus_ys)), (g(239), max(vbus_ys)), (vbus_xs[-1], max(vbus_ys))])
    for p in ("A9", "B4"):
        px, py = sb.P("J_USBC", p)
        sb.wire_pts([(px, py), (g(239), py)])
    sb.place("D_Schottky_Horiz", "D_USB", 248, 27, ref_offset=(0, -2.54), val_offset=(0, 2.54))
    sb.wire_pts([(g(239), g(27)), sb.P("D_USB", "2")])
    sb.label("VBUS_USB", g(241), g(27), angle=0, justify="left")

    # CC1 / CC2 pull-downs placed cleanly below USB-C (avoid crossing A6..B7)
    sb.stub("J_USBC", "A5", length_g=4, side="R")
    sb.stub("J_USBC", "B5", length_g=4, side="R")
    sb.place("R_Horiz_Flip", "R_CC1", 232, 56, ref_offset=(0, -2.54), val_offset=(0, 2.54))
    sb.stub("R_CC1", "2", length_g=2, side="L")
    sb.stub("R_CC1", "1", length_g=2, side="R")
    sb.place("R_Horiz_Flip", "R_CC2", 232, 63, ref_offset=(0, -2.54), val_offset=(0, 2.54))
    sb.stub("R_CC2", "2", length_g=2, side="L")
    sb.stub("R_CC2", "1", length_g=2, side="R")

    # USB_DP (A6, B6) & USB_DN (A7, B7) & SBU NC (A8, B8)
    sb.wire_pts([sb.P("J_USBC", "A6"), (g(238), sb.P("J_USBC", "A6")[1]), (g(238), sb.P("J_USBC", "B6")[1]), sb.P("J_USBC", "B6")])
    sb.wire_pts([(g(238), sb.P("J_USBC", "B6")[1]), (g(242), sb.P("J_USBC", "B6")[1])])
    sb.label("USB_DP", g(242), sb.P("J_USBC", "B6")[1], angle=0, justify="left")

    sb.wire_pts([sb.P("J_USBC", "A7"), (g(237), sb.P("J_USBC", "A7")[1]), (g(237), sb.P("J_USBC", "B7")[1]), sb.P("J_USBC", "B7")])
    sb.wire_pts([(g(237), sb.P("J_USBC", "B7")[1]), (g(242), sb.P("J_USBC", "B7")[1])])
    sb.label("USB_DN", g(242), sb.P("J_USBC", "B7")[1], angle=0, justify="left")

    sb.nc_pin("J_USBC", "A8")
    sb.nc_pin("J_USBC", "B8")

    # USB GND/SH bus
    ugnd_ys = [sb.P("J_USBC", p)[1] for p in ("A1", "B1", "A12", "B12", "SH")]
    sb.wire_pts([(sb.P("J_USBC", "A1")[0], min(ugnd_ys)), (g(238), min(ugnd_ys)), (g(238), max(ugnd_ys)), (sb.P("J_USBC", "SH")[0], max(ugnd_ys))])
    for p in ("B1", "A12", "B12"):
        px, py = sb.P("J_USBC", p)
        sb.wire_pts([(px, py), (g(238), py)])
    sb.wg([(238, 42), (242, 42)])
    sb.label("GND", g(242), g(42), angle=0, justify="left")

    # Solar input + Schottky OR
    sb.place("Conn_01x02_R", "J_SOLAR", 229, 75)
    sb.stub("J_SOLAR", "1", length_g=2, side="R")
    sb.place("D_Schottky_Horiz", "D_SOL", 242, 76, ref_offset=(0, -2.54), val_offset=(0, 2.54))
    sb.wire_pts([sb.P("J_SOLAR", "2"), sb.P("D_SOL", "2")])
    sb.label("VSOLAR_IN", g(234), g(76), angle=0, justify="left")

    # Merge D_USB:1 + D_SOL:1 -> VIN_CHG spine
    sb.wire_pts([sb.P("D_USB", "1"), (g(255), g(27)), (g(255), g(76)), sb.P("D_SOL", "1")])
    sb.label("VIN_CHG", g(255), g(25), angle=0, justify="left")

    # U_CHG IP2326 (centered at 284, 52: Left pins gX=279, Right pins gX=289)
    sb.place("IP2326", "U_CHG", 284, 52, ref_offset=(0, -19.05), val_offset=(0, 19.05))
    sb.place("R_Horiz", "R_VIN", 262, 23, ref_offset=(0, -2.54), val_offset=(0, 2.54))
    sb.place("C_Vert", "C_VIN", 271, 31, ref_offset=(-2.54, -1.27), val_offset=(-2.54, 1.27))
    sb.wire_pts([(g(255), g(27)), (g(255), g(23)), sb.P("R_VIN", "1")])
    sb.wire_pts([sb.P("R_VIN", "2"), (g(275), g(23)), (g(275), sb.P("U_CHG", "13")[1]), sb.P("U_CHG", "13")])
    sb.wire_pts([sb.P("C_VIN", "1"), (g(271), g(23))])
    sb.stub("C_VIN", "2", length_g=1, side="D")
    sb.label("CHG_VIN", g(268), g(23), angle=0, justify="left")

    sb.place("C_Vert", "C_CHGIN", 259, 39, ref_offset=(-2.54, -1.27), val_offset=(-2.54, 1.27))
    sb.wire_pts([sb.P("C_CHGIN", "1"), (g(259), g(23))])
    sb.stub("C_CHGIN", "2", length_g=1, side="D")

    sb.place("R_Horiz", "R_CEN", 265, 47, ref_offset=(0, -2.54), val_offset=(0, 2.54))
    sb.stub("R_CEN", "1", length_g=2, side="L")
    sb.wire_pts([sb.P("R_CEN", "2"), sb.P("U_CHG", "12")])
    sb.label("CHG_EN", g(270), g(47), angle=0, justify="left")

    # Left config/LED resistors: R_Horiz_Flip (Pin 2 Left=GND/LED_K, Pin 1 Right=U_CHG net)
    sb.place("R_Horiz_Flip", "R_UV", 265, 62, ref_offset=(0, -2.54), val_offset=(0, 2.54))
    sb.wire_pts([sb.P("R_UV", "1"), (g(272), g(62)), (g(272), sb.P("U_CHG", "8")[1]), sb.P("U_CHG", "8")])
    sb.label("CHG_UV", g(272), g(62), angle=0, justify="left")
    sb.stub("R_UV", "2", length_g=1, side="L")

    sb.place("R_Horiz_Flip", "R_ISET", 265, 68, ref_offset=(0, -2.54), val_offset=(0, 2.54))
    sb.wire_pts([sb.P("R_ISET", "1"), (g(274), g(68)), (g(274), sb.P("U_CHG", "11")[1]), sb.P("U_CHG", "11")])
    sb.label("CHG_ISET", g(274), g(68), angle=0, justify="left")
    sb.stub("R_ISET", "2", length_g=1, side="L")

    sb.place("R_Horiz_Flip", "R_NTC", 265, 74, ref_offset=(0, -2.54), val_offset=(0, 2.54))
    sb.wire_pts([sb.P("R_NTC", "1"), (g(276), g(74)), (g(276), sb.P("U_CHG", "4")[1]), sb.P("U_CHG", "4")])
    sb.label("CHG_NTC", g(276), g(74), angle=0, justify="left")
    sb.stub("R_NTC", "2", length_g=1, side="L")

    sb.place("R_Horiz_Flip", "R_LED1", 265, 82, ref_offset=(0, -2.54), val_offset=(0, 2.54))
    sb.place("LED_Vert", "D_CHG", 256, 86, ref_offset=(-2.54, -1.27), val_offset=(-2.54, 1.27))
    sb.wire_pts([sb.P("R_LED1", "1"), (g(277), g(82)), (g(277), sb.P("U_CHG", "6")[1]), sb.P("U_CHG", "6")])
    sb.label("CHG_LED", g(270), g(82), angle=0, justify="left")
    sb.wire_pts([sb.P("R_LED1", "2"), (g(256), g(82)), sb.P("D_CHG", "2")])
    sb.label("CHG_LED_K", g(256), g(82), angle=0, justify="left")
    sb.stub("D_CHG", "1", length_g=1, side="D")

    for ncp in ("1", "2", "3", "5", "7", "9", "10"):
        sb.nc_pin("U_CHG", ncp)

    # Right side of U_CHG (gX=289): clean staircase fanout avoiding crossings!
    # 14(BST, gY=46) -> C_BST1 (C_Horiz_Flip: 2=BST Left, 1=LX Right)
    sb.place("C_Horiz_Flip", "C_BST1", 299, 43, ref_offset=(0, -2.54), val_offset=(0, 2.54))
    sb.wire_pts([sb.P("U_CHG", "14"), (g(293), sb.P("U_CHG", "14")[1]), (g(293), g(43)), sb.P("C_BST1", "2")])
    sb.label("CHG_BST", g(293), g(43), angle=0, justify="left")

    # 15..17(LX, gY=47..49) -> L1 (L_Horiz_Flip: 2=LX Left, 1=VIN_CHG Right)
    lx_y1, lx_y2, lx_y3 = sb.P("U_CHG", "15")[1], sb.P("U_CHG", "16")[1], sb.P("U_CHG", "17")[1]
    sb.wire_pts([sb.P("U_CHG", "15"), (g(296), lx_y1), (g(296), lx_y3), sb.P("U_CHG", "17")])
    sb.wire_pts([sb.P("U_CHG", "16"), (g(296), lx_y2)])
    sb.place("L_Horiz_Flip", "L1", 310, 48, ref_offset=(0, -2.54), val_offset=(0, 2.54))
    sb.wire_pts([(g(296), g(48)), sb.P("L1", "2")])
    sb.wire_pts([sb.P("C_BST1", "1"), (g(303), g(43)), (g(303), g(48))])
    sb.label("CHG_LX", g(298), g(48), angle=0, justify="left")
    sb.stub("L1", "1", length_g=2, side="R")

    # 19..20(VSYS, gY=50..51) -> drop at gX=306 (below L1 at 48) -> C_SYS1 & C_SYS2
    vsys_y1, vsys_y2 = sb.P("U_CHG", "19")[1], sb.P("U_CHG", "20")[1]
    sb.wire_pts([sb.P("U_CHG", "19"), (g(298), vsys_y1), (g(298), vsys_y2), sb.P("U_CHG", "20")])
    sb.wire_pts([(g(298), g(51)), (g(306), g(51)), (g(306), g(54)), (g(316), g(54))])
    sb.label("CHG_VSYS", g(307), g(54), angle=0, justify="left")
    sb.place("C_Vert", "C_SYS1", 308, 60, ref_offset=(-2.54, -1.27), val_offset=(-2.54, 1.27))
    sb.place("C_Vert", "C_SYS2", 316, 60, ref_offset=(2.54, -1.27), val_offset=(2.54, 1.27))
    sb.wire_pts([sb.P("C_SYS1", "1"), (g(308), g(54))])
    sb.wire_pts([sb.P("C_SYS2", "1"), (g(316), g(54))])
    sb.wire_pts([sb.P("C_SYS1", "2"), (g(308), g(64)), (g(316), g(64)), sb.P("C_SYS2", "2")])
    sb.wg([(312, 64), (312, 65)])
    sb.label("GND", g(312), g(65), angle=0, justify="left")

    # 21..22(VBAT_2S, gY=52..53) -> drop at gX=296 down to gY=77 -> J_LIPO_2S:3 & C_BAT1
    vb_y1, vb_y2 = sb.P("U_CHG", "21")[1], sb.P("U_CHG", "22")[1]
    sb.wire_pts([sb.P("U_CHG", "21"), (g(296), vb_y1), (g(296), vb_y2), sb.P("U_CHG", "22")])
    sb.place("Conn_01x03_L", "J_LIPO_2S", 318, 72)
    sb.wire_pts([(g(296), g(53)), (g(296), g(77)), (g(312), g(77)), (g(312), sb.P("J_LIPO_2S", "3")[1]), sb.P("J_LIPO_2S", "3")])
    sb.label("VBAT_2S", g(298), g(77), angle=0, justify="left")

    sb.place("C_Vert", "C_BAT1", 304, 83, ref_offset=(-2.54, -1.27), val_offset=(-2.54, 1.27))
    sb.wire_pts([sb.P("C_BAT1", "1"), (g(304), g(77))])
    sb.stub("C_BAT1", "2", length_g=1, side="D")

    # 23(VBATM, gY=54) -> drop at gX=294 down to gY=72 -> R_BAL -> J_LIPO_2S:2
    sb.place("R_Horiz", "R_BAL", 306, 72, ref_offset=(0, -2.54), val_offset=(0, 2.54))
    sb.wire_pts([sb.P("U_CHG", "23"), (g(294), sb.P("U_CHG", "23")[1]), (g(294), g(72)), sb.P("R_BAL", "1")])
    sb.label("CHG_VBATM", g(297), g(72), angle=0, justify="left")
    sb.wire_pts([sb.P("R_BAL", "2"), sb.P("J_LIPO_2S", "2")])
    sb.label("VBAT_MID", g(312), g(72), angle=0, justify="left")
    sb.stub("J_LIPO_2S", "1", length_g=2, side="L")

    # 18, 24, 25 (GND, gY=55..57) -> drop at gX=292 to gY=60 GND
    sb.wire_pts([sb.P("U_CHG", "18"), (g(292), sb.P("U_CHG", "18")[1]), (g(292), sb.P("U_CHG", "25")[1]), sb.P("U_CHG", "25")])
    sb.wire_pts([sb.P("U_CHG", "24"), (g(292), sb.P("U_CHG", "24")[1])])
    sb.wg([(292, 57), (292, 60)])
    sb.label("GND", g(292), g(60), angle=0, justify="left")

    # ==========================================================================
    # BLOCK 7: BATTERY PROTECTION, SWITCHES & MT2492 5.0V 2A BUCK (Col 3 Mid)
    # ==========================================================================
    sb.add_section_box(
        "7. 2S BATTERY PROTECTION, POWER SWITCH & MT2492 5.0V 2A BUCK",
        "1.5A PTC + AO3401A Reverse-Polarity PMOS + Parallel Power Switches + MT2492 Buck + Optional 2S ADC",
        222, 95, 324, 156,
    )
    sb.place("Fuse_Horiz", "F1", 232, 107, ref_offset=(0, -2.54), val_offset=(0, 2.54))
    sb.stub("F1", "1", length_g=2, side="L")
    sb.place("Q_PMOS_AO3401A", "Q_PROT", 244, 107, ref_offset=(0, -4.572), val_offset=(2.54, 3.048))
    sb.wire_pts([sb.P("F1", "2"), sb.P("Q_PROT", "3")])
    sb.label("VBAT_FUSED", g(236), g(107), angle=0, justify="left")

    sb.place("R_Vert_Flip", "R_QG", 244, 117, ref_offset=(-2.54, -1.27), val_offset=(-2.54, 1.27))
    sb.wire_pts([sb.P("Q_PROT", "1"), sb.P("R_QG", "2")])
    sb.label("Q_GATE", g(244), g(111), angle=0, justify="left")
    sb.stub("R_QG", "1", length_g=1, side="D")

    sb.place("C_Vert", "C_BIN", 254, 114, ref_offset=(-2.54, -1.27), val_offset=(-2.54, 1.27))
    sb.place("SW_SPDT_MSK12C02", "SW_PWR", 268, 108, ref_offset=(0, -5.588), val_offset=(3.556, 3.81))
    sb.place("Conn_01x02_L", "J_SW_EXT", 268, 121)

    sb.wire_pts([sb.P("Q_PROT", "2"), sb.P("SW_PWR", "3")])
    sb.wire_pts([sb.P("C_BIN", "1"), (g(254), g(107))])
    sb.wire_pts([(g(258), g(107)), (g(258), sb.P("J_SW_EXT", "1")[1]), sb.P("J_SW_EXT", "1")])
    sb.label("VBAT_PROT", g(250), g(107), angle=0, justify="left")
    sb.stub("C_BIN", "2", length_g=1, side="D")
    sb.nc_pin("SW_PWR", "1")
    sb.stub("SW_PWR", "SH", length_g=1, side="D")

    sb.place("R_Horiz", "R_SW_PU", 282, 108, ref_offset=(0, -2.54), val_offset=(0, 2.54))
    sb.place("R_Vert_Flip", "R_SW_PD", 290, 114, ref_offset=(-2.54, -1.27), val_offset=(-2.54, 1.27))
    sb.wire_pts([sb.P("SW_PWR", "2"), sb.P("R_SW_PU", "1")])
    sb.wire_pts([sb.P("J_SW_EXT", "2"), (g(276), sb.P("J_SW_EXT", "2")[1]), (g(276), g(108))])
    sb.label("BUCK_EN_SW", g(275), g(108), angle=0, justify="left")

    sb.wire_pts([sb.P("R_SW_PU", "2"), (g(293), g(108))])
    sb.wire_pts([sb.P("R_SW_PD", "2"), (g(290), g(108))])
    sb.label("BUCK_EN", g(293), g(108), angle=0, justify="left")
    sb.stub("R_SW_PD", "1", length_g=1, side="D")

    # Optional 2S ADC Monitor (R_2S_TOP, R_2S_BOT, JP_2S_ADC)
    sb.place("R_Vert", "R_2S_TOP", 304, 110, ref_offset=(-2.54, -1.27), val_offset=(-2.54, 1.27))
    sb.place("R_Vert_Flip", "R_2S_BOT", 304, 122, ref_offset=(-2.54, -1.27), val_offset=(-2.54, 1.27))
    sb.place("SolderJumper_2", "JP_2S_ADC", 312, 116, ref_offset=(0, -2.54), val_offset=(0, 2.54))
    sb.stub("R_2S_TOP", "1", length_g=1, side="U")
    sb.wire_pts([sb.P("R_2S_TOP", "2"), sb.P("R_2S_BOT", "2")])
    sb.wire_pts([(g(304), g(116)), sb.P("JP_2S_ADC", "1")])
    sb.label("ADC_2S_DIV", g(304), g(115), angle=0, justify="left")
    sb.stub("R_2S_BOT", "1", length_g=1, side="D")
    sb.stub("JP_2S_ADC", "2", length_g=2, side="R")

    # U_BUCK MT2492 + C_BST2 + L2 + R_FB1/2 + C_BOUT1/2 + D_5V + J_PWR_ESP
    sb.place("MT2492", "U_BUCK", 242, 138, ref_offset=(0, -10.16), val_offset=(0, 10.16))
    sb.stub("U_BUCK", "5", length_g=3, side="L")
    sb.stub("U_BUCK", "4", length_g=3, side="L")
    sb.stub("U_BUCK", "2", length_g=3, side="L")

    sb.place("C_Horiz", "C_BST2", 254, 132, ref_offset=(0, -2.54), val_offset=(0, 2.54))
    sb.place("L_Horiz", "L2", 266, 138, ref_offset=(0, -2.54), val_offset=(0, 2.54))
    sb.wire_pts([sb.P("U_BUCK", "1"), (g(248), sb.P("U_BUCK", "1")[1]), (g(248), g(132)), sb.P("C_BST2", "1")])
    sb.label("BUCK_BST", g(248), g(132), angle=0, justify="left")
    sb.wire_pts([sb.P("U_BUCK", "6"), sb.P("L2", "1")])
    sb.wire_pts([sb.P("C_BST2", "2"), (g(259), g(132)), (g(259), g(138))])
    sb.label("BUCK_LX", g(251), g(138), angle=0, justify="left")

    # +5V Output rail + Feedback + Output Caps + LED + J_PWR_ESP
    sb.wire_pts([sb.P("L2", "2"), (g(312), g(138))])
    sb.label("+5V", g(312), g(138), angle=0, justify="left")

    sb.place("R_Vert", "R_FB1", 278, 144, ref_offset=(-2.54, -1.27), val_offset=(-2.54, 1.27))
    sb.place("R_Vert", "R_FB2", 278, 151, ref_offset=(-2.54, -1.27), val_offset=(-2.54, 1.27))
    sb.wire_pts([sb.P("R_FB1", "1"), (g(278), g(138))])
    sb.wire_pts([sb.P("R_FB1", "2"), sb.P("R_FB2", "1")])
    sb.wire_pts([sb.P("U_BUCK", "3"), (g(254), sb.P("U_BUCK", "3")[1]), (g(254), g(148)), (g(278), g(148))])
    sb.label("BUCK_FB", g(263), g(148), angle=0, justify="left")
    sb.stub("R_FB2", "2", length_g=1, side="D")

    sb.place("C_Vert", "C_BOUT1", 287, 144, ref_offset=(-2.54, -1.27), val_offset=(-2.54, 1.27))
    sb.place("C_Vert", "C_BOUT2", 294, 144, ref_offset=(2.54, -1.27), val_offset=(2.54, 1.27))
    sb.wire_pts([sb.P("C_BOUT1", "1"), (g(287), g(138))])
    sb.wire_pts([sb.P("C_BOUT2", "1"), (g(294), g(138))])
    sb.wire_pts([sb.P("C_BOUT1", "2"), (g(287), g(150)), (g(294), g(150)), sb.P("C_BOUT2", "2")])
    sb.wg([(290, 150), (290, 152)])
    sb.label("GND", g(290), g(152), angle=0, justify="left")

    sb.place("R_Vert", "R_LED2", 302, 143, ref_offset=(-2.54, -1.27), val_offset=(-2.54, 1.27))
    sb.place("LED_Vert", "D_5V", 302, 150, ref_offset=(2.54, -1.27), val_offset=(2.54, 1.27))
    sb.wire_pts([sb.P("R_LED2", "1"), (g(302), g(138))])
    sb.wire_pts([sb.P("R_LED2", "2"), sb.P("D_5V", "2")])
    sb.label("LED_5V_A", g(302), g(147), angle=0, justify="left")
    sb.stub("D_5V", "1", length_g=1, side="D")

    sb.place("Conn_01x02_L", "J_PWR_ESP", 316, 148)
    sb.auto_pin("J_PWR_ESP", length_g=2)

    # ==========================================================================
    # BLOCK 8: RC SERVOS (5V BEC), SENSORS & BREAKAWAY SUB-BOARD (Col 3 Bot)
    # ==========================================================================
    sb.add_section_box(
        "8. RC SERVOS (5V BEC), SENSORS & BREAKAWAY WINCH/HALL SUB-BOARD",
        "Isolated Servo BEC Rail + Actuators + Water/Limit + 6P Winch Hub <-> Breakaway Hall + 2 Servos",
        222, 159, 324, 208,
    )
    # Main Board Row 1 & Row 2 (3x headers per row, 18G = 45.7 mm pitch -> zero label overlap)
    for href, hx in (("J_PWR_SRV", 227), ("ESC", 245), ("RUD", 263)):
        sname = "Conn_01x02_R" if href == "J_PWR_SRV" else "Conn_01x03_R"
        sb.place(sname, href, hx, 173)
        sb.auto_pin(href, length_g=2)
    for href, hx in (("WIN", 227), ("RCK", 245), ("STRB", 263)):
        sb.place("Conn_01x03_R", href, hx, 186)
        sb.auto_pin(href, length_g=2)

    # Main Board Row 3: Water, Limit, 6-Pin Hub
    sb.place("Conn_01x02_R", "J_WATER", 227, 199)
    sb.auto_pin("J_WATER", length_g=2)
    sb.place("Conn_01x02_R", "J_LIMIT", 243, 199)
    sb.auto_pin("J_LIMIT", length_g=2)
    sb.place("Conn_01x06_R", "J_WINCH_HUB", 261, 198)
    sb.auto_pin("J_WINCH_HUB", length_g=2)

    # Breakaway Sub-Board Sub-Box (right side of Block 8)
    sb.add_section_box("", "", 281, 166, 322, 206, solid=True)
    sb.add_text("BREAKAWAY WINCH + HALL SUB-BOARD", 283, 169, size=1.397, bold=True)
    sb.place("Conn_01x06_R", "J_WINCH_SUB", 286, 180)
    sb.auto_pin("J_WINCH_SUB", length_g=2)
    sb.place("Conn_01x03_R", "SRV_WIN", 303, 175)
    sb.auto_pin("SRV_WIN", length_g=2)
    sb.place("Conn_01x03_R", "SRV_RCK", 303, 188)
    sb.auto_pin("SRV_RCK", length_g=2)
    sb.place("C_Vert", "C_SRV", 317, 176, ref_offset=(2.54, -1.27), val_offset=(2.54, 1.27))
    sb.stub("C_SRV", "1", length_g=1, side="U")
    sb.stub("C_SRV", "2", length_g=1, side="D")
    sb.place("DRV5032", "U_HALL", 293, 200, ref_offset=(0, -5.08), val_offset=(0, 6.35))
    sb.auto_pin("U_HALL", length_g=2)
    sb.place("C_Vert_Flip", "C_HALL", 317, 198, ref_offset=(2.54, -1.27), val_offset=(2.54, 1.27))
    sb.stub("C_HALL", "2", length_g=1, side="U")
    sb.stub("C_HALL", "1", length_g=1, side="D")

    # ==========================================================================
    # BLOCK 9: M3 MOUNTING HOLES (Col 3 Footer-Left, clear of A1 Title Block)
    # ==========================================================================
    sb.add_section_box(
        "9. M3 MOUNTING HOLES",
        "4x Corner M3 (H1-H4) + Breakaway Hall Probe M3 (H_HALL)",
        222, 211, 278, 228,
    )
    for idx, href in enumerate(("H1", "H2", "H3", "H4", "H_HALL")):
        sb.place("MountingHole", href, 228 + idx * 10, 222, ref_offset=(0, -2.54), val_offset=(0, 2.54))


def build_schematic_and_verify(pcb_path, sch_path):
    """Build Dual_ESP_Carrier_V2.kicad_sch + lib + exports and verify 0 ERC errors."""
    base_dir = os.path.dirname(os.path.abspath(sch_path))
    sym_path = os.path.join(base_dir, f"{PROJECT_NAME}.kicad_sym")
    tbl_path = os.path.join(base_dir, "sym-lib-table")
    erc_path = os.path.join(base_dir, "erc_report.txt")
    pdf_path = os.path.join(base_dir, f"{PROJECT_NAME}_schematic.pdf")
    svg_dir = os.path.join(base_dir, "_sch_svg_tmp")
    svg_out = os.path.join(base_dir, f"{PROJECT_NAME}_schematic.svg")
    png_path = os.path.join(base_dir, f"{PROJECT_NAME}_schematic.png")

    pcb_comps = parse_pcb_components(pcb_path)
    catalog = build_symbol_catalog()
    sb = SchematicBuilder(catalog, pcb_comps)
    populate_schematic(sb)

    sb.write_sym_lib(sym_path, tbl_path)
    sb.write_sch(sch_path)
    print(f"Wrote {sch_path} & {sym_path} ({len(sb.instances)} components)")

    cfg_dir = "/tmp/kicad_render_cfg"
    os.makedirs(cfg_dir, exist_ok=True)
    kicad_env = dict(os.environ, KICAD_CONFIG_HOME=cfg_dir)

    subprocess.run([KICAD_CLI, "sch", "upgrade", "--force", sch_path], env=kicad_env, check=True)
    erc_res = subprocess.run(
        [KICAD_CLI, "sch", "erc", "--severity-all", "-o", erc_path, sch_path],
        env=kicad_env, capture_output=True, text=True,
    )
    with open(erc_path, "r") as f:
        erc_txt = f.read()
    erc_errs = sum(1 for ln in erc_txt.splitlines() if ln.strip().endswith("; error"))
    erc_warns = sum(1 for ln in erc_txt.splitlines() if ln.strip().endswith("; warning"))
    print(f"ERC exit={erc_res.returncode}: {erc_errs} errors, {erc_warns} warnings ({erc_path})")

    subprocess.run([KICAD_CLI, "sch", "export", "pdf", "-o", pdf_path, sch_path], env=kicad_env, check=True)
    if os.path.exists(svg_dir):
        shutil.rmtree(svg_dir)
    subprocess.run([KICAD_CLI, "sch", "export", "svg", "-o", svg_dir, sch_path], env=kicad_env, check=True)
    svgs = [fn for fn in os.listdir(svg_dir) if fn.endswith(".svg")]
    if svgs:
        shutil.move(os.path.join(svg_dir, svgs[0]), svg_out)
    shutil.rmtree(svg_dir, ignore_errors=True)

    if os.path.exists(GS_BIN):
        subprocess.run(
            [GS_BIN, "-dNOPAUSE", "-dBATCH", "-sDEVICE=png16m", "-r150", f"-sOutputFile={png_path}", pdf_path],
            check=True, capture_output=True,
        )
        print(f"Exported schematic renders: {pdf_path}, {svg_out}, {png_path}")

    return erc_errs, erc_warns


if __name__ == "__main__":
    build_schematic_and_verify(
        os.path.join(BASE_DIR, f"{PROJECT_NAME}.kicad_pcb"),
        os.path.join(BASE_DIR, f"{PROJECT_NAME}.kicad_sch"),
    )
