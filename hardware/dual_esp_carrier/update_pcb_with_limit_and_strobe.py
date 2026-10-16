#!/usr/bin/env python3
import os
import uuid

PCB_PATH = "/Users/tiagotorredovale/Documents/projectos/winddragon/rover/hardware/dual_esp_carrier/Dual_ESP_Carrier.kicad_pcb"

def new_uuid():
    return str(uuid.uuid4())

def main():
    print(f"Reading {PCB_PATH}...")
    with open(PCB_PATH, "r") as f:
        content = f.read()

    # 1. Footprint STRB (WS2812B Strobe) at X=7.0, Y=73.58, rot 180
    # Pad 1 (GND): (7.0, 73.58)
    # Pad 2 (+5V_SERVO): (7.0, 71.04)
    # Pad 3 (STROBE_DIN / IO19): (7.0, 68.50)
    footprint_strobe = f"""
	(footprint "Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical"
		(layer "F.Cu")
		(uuid "{new_uuid()}")
		(at 7 73.58 180)
		(property "Reference" "STRB"
			(at 0 -2.4 0)
			(layer "F.SilkS")
			(uuid "{new_uuid()}")
			(effects
				(font
					(size 0.7 0.7)
					(thickness 0.12)
					(bold yes)
				)
			)
		)
		(property "Value" "WS2812B"
			(at 0 7.58 90)
			(layer "F.SilkS")
			(uuid "{new_uuid()}")
			(effects
				(font
					(size 0.6 0.6)
					(thickness 0.1)
				)
			)
		)
		(property "Datasheet" ""
			(at 0 0 0)
			(layer "F.Fab")
			(hide yes)
			(uuid "{new_uuid()}")
			(effects
				(font
					(size 1.27 1.27)
				)
			)
		)
		(property "Description" ""
			(at 0 0 0)
			(layer "F.Fab")
			(hide yes)
			(uuid "{new_uuid()}")
			(effects
				(font
					(size 1.27 1.27)
				)
			)
		)
		(duplicate_pad_numbers_are_jumpers no)
		(pad "1" thru_hole rect
			(at 0 0 180)
			(size 1.7 1.7)
			(drill 1)
			(layers "*.Cu" "*.Mask" "F.SilkS")
			(remove_unused_layers no)
			(net "/GND")
			(uuid "{new_uuid()}")
		)
		(pad "2" thru_hole circle
			(at 0 2.54 180)
			(size 1.7 1.7)
			(drill 1)
			(layers "*.Cu" "*.Mask" "F.SilkS")
			(net "/+5V_SERVO")
			(uuid "{new_uuid()}")
		)
		(pad "3" thru_hole circle
			(at 0 5.08 180)
			(size 1.7 1.7)
			(drill 1)
			(layers "*.Cu" "*.Mask" "F.SilkS")
			(net "/STROBE_DIN")
			(uuid "{new_uuid()}")
		)
		(embedded_fonts no)
	)"""

    # 2. Footprint J_LIMIT (Anchor Limit Switch) at X=4.0, Y=73.58, rot 180
    # Pad 1 (GND): (4.0, 73.58)
    # Pad 2 (ANCHOR_LIMIT / IO18): (4.0, 71.04)
    footprint_limit = f"""
	(footprint "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical"
		(layer "F.Cu")
		(uuid "{new_uuid()}")
		(at 4 73.58 180)
		(property "Reference" "J_LIMIT"
			(at 0 -2.4 0)
			(layer "F.SilkS")
			(uuid "{new_uuid()}")
			(effects
				(font
					(size 0.65 0.65)
					(thickness 0.1)
					(bold yes)
				)
			)
		)
		(property "Value" "LIMIT/ANC"
			(at 0 7.58 90)
			(layer "F.SilkS")
			(uuid "{new_uuid()}")
			(effects
				(font
					(size 0.6 0.6)
					(thickness 0.1)
				)
			)
		)
		(property "Datasheet" ""
			(at 0 0 0)
			(layer "F.Fab")
			(hide yes)
			(uuid "{new_uuid()}")
			(effects
				(font
					(size 1.27 1.27)
				)
			)
		)
		(property "Description" ""
			(at 0 0 0)
			(layer "F.Fab")
			(hide yes)
			(uuid "{new_uuid()}")
			(effects
				(font
					(size 1.27 1.27)
				)
			)
		)
		(duplicate_pad_numbers_are_jumpers no)
		(pad "1" thru_hole rect
			(at 0 0 180)
			(size 1.7 1.7)
			(drill 1)
			(layers "*.Cu" "*.Mask" "F.SilkS")
			(remove_unused_layers no)
			(net "/GND")
			(uuid "{new_uuid()}")
		)
		(pad "2" thru_hole circle
			(at 0 2.54 180)
			(size 1.7 1.7)
			(drill 1)
			(layers "*.Cu" "*.Mask" "F.SilkS")
			(net "/ANCHOR_LIMIT")
			(uuid "{new_uuid()}")
		)
		(embedded_fonts no)
	)"""

    # 3. Update J_DK_R pads 9 and 10
    old_pad9 = """(pad "9" thru_hole circle
			(at 0 20.32)
			(size 1.7 1.7)
			(drill 1)
			(layers "*.Cu" "*.Mask" "B.SilkS")
			(remove_unused_layers no)
			(uuid "475bcbb2-ece1-4923-84af-497e93c5beb8")
		)"""
    new_pad9 = """(pad "9" thru_hole circle
			(at 0 20.32)
			(size 1.7 1.7)
			(drill 1)
			(layers "*.Cu" "*.Mask" "B.SilkS")
			(remove_unused_layers no)
			(net "/STROBE_DIN")
			(uuid "475bcbb2-ece1-4923-84af-497e93c5beb8")
		)"""

    old_pad10 = """(pad "10" thru_hole circle
			(at 0 22.86)
			(size 1.7 1.7)
			(drill 1)
			(layers "*.Cu" "*.Mask" "B.SilkS")
			(remove_unused_layers no)
			(uuid "e95501f3-92c6-4628-a6d8-44b1c50651db")
		)"""
    new_pad10 = """(pad "10" thru_hole circle
			(at 0 22.86)
			(size 1.7 1.7)
			(drill 1)
			(layers "*.Cu" "*.Mask" "B.SilkS")
			(remove_unused_layers no)
			(net "/ANCHOR_LIMIT")
			(uuid "e95501f3-92c6-4628-a6d8-44b1c50651db")
		)"""

    assert old_pad9 in content, "old_pad9 not found in PCB content"
    assert old_pad10 in content, "old_pad10 not found in PCB content"
    content = content.replace(old_pad9, new_pad9)
    content = content.replace(old_pad10, new_pad10)
    print("Updated J_DK_R Pad 9 (GPIO 19 -> /STROBE_DIN) and Pad 10 (GPIO 18 -> /ANCHOR_LIMIT).")

    # Insert footprints right before the first (segment
    first_seg_idx = content.find("\t(segment")
    content = content[:first_seg_idx] + footprint_limit + "\n" + footprint_strobe + "\n" + content[first_seg_idx:]
    print("Inserted J_LIMIT and STRB footprints.")

    # 4. Routing segments & via:
    # A. +5V_SERVO to STRB Pad 2 (at 7.0, 71.04) from ESC Pad 2 (10.0, 71.04):
    # (7.0, 71.04) -> (10.0, 71.04) on F.Cu, width 0.6mm
    #
    # B. STROBE_DIN:
    # 100% on F.Cu: (7.0, 68.50) -> (7.0, 63.50) -> (33.00, 63.50) -> (33.00, 72.57) -> (35.50, 72.57) into Pin 9
    #
    # C. ANCHOR_LIMIT:
    # Starts on B.Cu from Pad 2 (4.0, 71.04) -> (4.0, 65.50) -> (8.50, 65.50)
    # Via at (8.50, 65.50) to F.Cu
    # On F.Cu: (8.50, 65.50) -> (31.50, 65.50) -> (31.50, 75.11) -> (35.50, 75.11) into Pin 10
    new_elements = f"""
	(segment
		(start 7.0 71.04)
		(end 10.0 71.04)
		(width 0.6)
		(layer "F.Cu")
		(net "/+5V_SERVO")
		(uuid "{new_uuid()}")
	)
	(segment
		(start 7.0 68.50)
		(end 7.0 63.50)
		(width 0.35)
		(layer "F.Cu")
		(net "/STROBE_DIN")
		(uuid "{new_uuid()}")
	)
	(segment
		(start 7.0 63.50)
		(end 33.00 63.50)
		(width 0.35)
		(layer "F.Cu")
		(net "/STROBE_DIN")
		(uuid "{new_uuid()}")
	)
	(segment
		(start 33.00 63.50)
		(end 33.00 72.57)
		(width 0.35)
		(layer "F.Cu")
		(net "/STROBE_DIN")
		(uuid "{new_uuid()}")
	)
	(segment
		(start 33.00 72.57)
		(end 35.50 72.57)
		(width 0.35)
		(layer "F.Cu")
		(net "/STROBE_DIN")
		(uuid "{new_uuid()}")
	)
	(segment
		(start 4.0 71.04)
		(end 4.0 65.50)
		(width 0.35)
		(layer "B.Cu")
		(net "/ANCHOR_LIMIT")
		(uuid "{new_uuid()}")
	)
	(segment
		(start 4.0 65.50)
		(end 8.50 65.50)
		(width 0.35)
		(layer "B.Cu")
		(net "/ANCHOR_LIMIT")
		(uuid "{new_uuid()}")
	)
	(via
		(at 8.5 65.5)
		(size 0.8)
		(drill 0.4)
		(layers "F.Cu" "B.Cu")
		(free yes)
		(net "/ANCHOR_LIMIT")
		(uuid "{new_uuid()}")
	)
	(segment
		(start 8.50 65.50)
		(end 31.50 65.50)
		(width 0.35)
		(layer "F.Cu")
		(net "/ANCHOR_LIMIT")
		(uuid "{new_uuid()}")
	)
	(segment
		(start 31.50 65.50)
		(end 31.50 75.11)
		(width 0.35)
		(layer "F.Cu")
		(net "/ANCHOR_LIMIT")
		(uuid "{new_uuid()}")
	)
	(segment
		(start 31.50 75.11)
		(end 35.50 75.11)
		(width 0.35)
		(layer "F.Cu")
		(net "/ANCHOR_LIMIT")
		(uuid "{new_uuid()}")
	)"""

    # Insert before the first zone
    first_zone_idx = content.find("\t(zone")
    content = content[:first_zone_idx] + new_elements + "\n" + content[first_zone_idx:]
    print("Appended new routing segments.")

    with open(PCB_PATH, "w") as f:
        f.write(content)
    print(f"Successfully saved {PCB_PATH}")

    # Also update template
    template_path = os.path.join(os.path.dirname(PCB_PATH), "Dual_ESP_Carrier_template.kicad_pcb")
    with open(template_path, "w") as f:
        f.write(content)
    print(f"Successfully saved {template_path}")

if __name__ == "__main__":
    main()
