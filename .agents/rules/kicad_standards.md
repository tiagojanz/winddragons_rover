# KiCad Standards & Versioning Rules

- **KiCad Version**: The environment is running **KiCad 10.0.6** located at `/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli`.
- **File Format Requirements**:
  - KiCad 10 uses internal layer structure and IDs that differ from KiCad 8/9 (`(0 "F.Cu" signal)`, `(2 "B.Cu" signal)`, `(5 "F.SilkS" user "F.Silkscreen")`).
  - Generating raw text files with legacy layer IDs causes KiCad 10 to display the yellow warning:
    *"Este ficheiro foi criado com uma versão anterior do KiCad. Será convertido para o novo formato quando for guardado."*
- **Mandatory Upgrade Workflow**:
  - Whenever generating, updating, or script-generating KiCad PCB (`.kicad_pcb`) or Schematic (`.kicad_sch`) files, ALWAYS run the native KiCad 10 upgrade tool immediately after:
    ```bash
    /Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli pcb upgrade --force <path_to_pcb>
    /Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli sch upgrade --force <path_to_sch>
    ```
  - This guarantees the file is converted cleanly into native KiCad 10 format with complete, modern layer maps and avoids the legacy version warning banner.
- **Projects**:
  - `hardware/universal_pcb/`: Original single ESP32-C6 LCD carrier (100 x 85 mm). Must remain untouched.
  - `hardware/dual_esp_carrier/`: Dual ESP32-C6 carrier (LAFVIN Master + DevKitC Slave, 100 x 100 mm). Native KiCad 10.
