# Hardware & KiCad Routing Rules

Consult `../.agents/rules/kicad_standards.md` and `../HARDWARE_PINOUT_MEMORY.md` before modifying any PCB, schematic, or hardware generator script in `hardware/`.

## Mandatory PCB Routing & Layout Guidelines

1. **Sem pistas a 90° (No 90° Trace Bends — Mandatory 45° Miters, Including Across Vias):**
   - Never route PCB trace corners at 90° (or acute angles `< 90°`).
   - All directional changes on the same copper layer must use **45° chamfered/mitered bends** (`135°` interior angle), both in manual KiCad edits and automated generator scripts (`make_route` must automatically chamfer orthogonal corners at 45°). Avoid mid-trace T-junctions without a pad or via.
   - **Across 2-way via transitions (`F.Cu` <-> `B.Cu`)**: every 2-branch via transition between horizontal `F.Cu` and vertical `B.Cu` must include a 45° chamfered segment so the 2D projected interior angle across the via is `>= 135°` (never `90°`), and 3-branch T-tap vias on straight buses must never form acute angles (`< 90°`).

2. **Sem circuitos de sinal fechados em loop (No Closed Signal Loops):**
   - Never create closed loops/rings in signal nets (no redundant parallel paths across `F.Cu`/`B.Cu` and no duplicate overlapping trace segments).
   - Every signal net must form a strict acyclic tree / daisy-chain (`edges = nodes - 1`) to avoid EMI loop antennas and inductive coupling.

3. **Pistas Top (`F.Cu`) Horizontais:**
   - Route main traces and buses on the **Top (`F.Cu`)** layer **horizontally (X-axis)** with 45° miters; allow only short local fanouts from SMD pads to their via or adjacent pad.

4. **Pistas Bottom (`B.Cu`) Verticais:**
   - Route main traces and buses on the **Bottom (`B.Cu`)** layer **vertically (Y-axis)** with 45° miters.
   - Use vias between `F.Cu` (horizontal) and `B.Cu` (vertical) for long X ↔ Y direction transitions so perpendicular buses do not block channels or split the `GND` pours.

## Mandatory KiCad 10 Upgrade Workflow

After generating or modifying any `.kicad_pcb` or `.kicad_sch` file, always run KiCad 10 CLI upgrade and DRC with zone refill:
```bash
/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli pcb upgrade --force <path_to_pcb>
/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli pcb drc --refill-zones --save-board -o drc_report.txt <path_to_pcb>
```
