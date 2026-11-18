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
  - `hardware/dual_esp_carrier_v2/`: Dual ESP32-C6 Carrier V2 (Onboard ESP32-C6-WROOM-1, JOY1 RKJXV122400R, 2S LiPo IP2326 USB-C/Solar, MT2492 5V Buck, 2x ADS1115 3S–6S cell monitor, breakaway winch/Hall sub-board; 100 x 100 mm, native KiCad 10).

## Mandatory PCB Routing & Layout Guidelines

1. **Sem pistas a 90° (No 90° Trace Bends — Mandatory 45° Miters, Incluindo em Vias):**
   - É má prática usar curvas de pistas a 90° (ou ângulos agudos `< 90°`).
   - Todas as mudanças de direção numa mesma camada devem ser chanfradas a **45°** (`135°` interno), tanto no desenho manual como em scripts geradores (`make_route` deve chanfrar automaticamente vértices ortogonais a 45°).
   - Evitar junções em T a meio de pistas sem pad ou via.
   - **Transições em vias de 2 ramos (`F.Cu` <-> `B.Cu`)**: toda a transição em via entre uma pista horizontal em `F.Cu` e vertical em `B.Cu` deve incluir um segmento chanfrado a 45° de modo a que o ângulo 2D projetado na via seja `>= 135°` (nunca `90°`), e derivações em T de 3 ramos sobre barramentos retos nunca devem formar ângulos agudos (`< 90°`).

2. **Sem circuitos de sinal fechados em loop (No Closed Signal Loops):**
   - Nunca fechar anéis/loops em pistas de sinal (nem criar caminhos paralelos redundantes entre `F.Cu` e `B.Cu` ou segmentos duplicados sobrepostos).
   - Cada net de sinal deve seguir uma topologia estritamente acíclica (árvore / *daisy-chain*, `arestas = nós - 1`) para evitar antenas de loop EMI e acoplamento indutivo.

3. **Pistas Top (`F.Cu`) Horizontais:**
   - Na camada superior (`F.Cu`), encaminhar os barramentos e pistas principais na direção **horizontal (eixo X)** com inflexões a 45°, reservando apenas pequenos *fanouts* locais curtos para saída de pads SMD até à via.

4. **Pistas Bottom (`B.Cu`) Verticais:**
   - Na camada inferior (`B.Cu`), encaminhar os barramentos e pistas principais na direção **vertical (eixo Y)** com inflexões a 45°.
   - Mudanças de eixo longo (X ↔ Y) devem ser feitas através de vias entre `F.Cu` (horizontal) e `B.Cu` (vertical) para evitar bloqueio de canais e cortes extensos nos planos de massa (`GND`).
