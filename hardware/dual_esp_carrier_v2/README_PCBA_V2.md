# WindDragons Dual ESP32-C6 Carrier PCB V2.0

Placa de suporte de **100.0 × 100.0 mm** (2 camadas, KiCad 10) com **`ESP32-C6-WROOM-1-N8` (`U_C6`)** e joystick **`JOY1` (`RKJXV122400R`)** integrados para fabrico e montagem **Top-Side na JLCPCB PCBA** (compatível com o firmware via `-DCARRIER_PCB_V2` em [`include/config_common.h`](../../include/config_common.h)).

---

## 1. Novidades da V2 em relação à V1

1. **Carregamento 2S LiPo via USB-C + Painel Solar (5V–6V) + Buck 5.0V dedicado à eletrónica**
   - Entrada dual com díodos Schottky anti-retorno `B5819W` (`D_USB` + `D_SOL`).
   - Carregador boost **IP2326** (`U_CHG`) para bateria **LiPo 2S (7.4V / 8.4V)** com balanceamento ativo integrado ligado diretamente à ficha **JST-XH 3 pinos (`J_LIPO_2S`)** com resistência de balanceamento `R_BAL = 10R`.
   - Proteção na descarga com fusível rearmável **1.5A PTC (`F1`)**, proteção contra inversão de polaridade com **PMOS AO3401A (`Q_PROT`)**, interruptor deslizante **`SW_PWR`** em paralelo com header **`J_SW_EXT`** (para interruptor estanque externo no casco), e conversor buck síncrono **MT2492 (`U_BUCK`, `5.00V 2A`)** exclusivo para os ESP32 / LoRa / GPS / sensores.
   - Os servos e ESC continuam completamente isolados do buck da eletrónica, alimentados pelo BEC externo em **`J_PWR_SRV` (`BEC IN`)**.
2. **Sub-placa destacável estendida para Guincho + Anchor Hall com cabo único de 6 condutores**
   - Sub-placa de `76.0 × 7.2 mm` (`X=14.0..90.0 mm`, `Y=92.8..100.0 mm`) destacável por ranhuras e 3 abas (`4.0 mm`) com micro-furos de corte em `Y=92.2 mm`.
   - Mantém exatamente o furo **M3 (`H_HALL`) em `(54.0, 96.4)`** e o sensor Hall omnipolar **`DRV5032FADBZR` (`U_HALL`) em `(86.5, 96.4)`**.
   - Integra headers para os dois servos da assembly do guincho (**`SRV_WIN`** e **`SRV_RCK`**), condensador de desacoplamento `C_SRV` (22uF) e conector **`J_WINCH_SUB`** (1×6 pins, 2.54 mm) ligado ao **`J_WINCH_HUB`** da placa principal por **um único cabo de 6 fios**.
   - **Correção crítica de pinagem na V2**: o footprint `U_HALL` foi corrigido para o pinout oficial Texas Instruments SOT-23 (`DBZ`) do `DRV5032FADBZR` (`Pin 1 = 3V3`, `Pin 2 = OUT`, `Pin 3 = GND`).
3. **Ficha JST-XH dedicada ao IMU `GY-BNO055` (`J_IMU`), Ergonomia do Joystick & Botões 100% Independentes**
   - O antigo header 2×3 `J_EXP` foi substituído por uma **ficha JST-XH 4 pinos (`J_IMU`, `B4B-XH-A`, passo 2.50 mm)** dedicada ao módulo IMU 9-DOF **GY-BNO055** (`1: GND | 2: 3V3 | 3: SDA (IO1) | 4: SCL (IO11)`), usando a mesma família JST-XH das portas Solar e LiPo 2S para ligação polarizada impossível de inverter.
   - O joystick PS5 (**`JOY1`**) foi deslocado **`5.50 mm` para dentro** (`X = 72.7525 mm`), aumentando a folga até à zona de carregador USB-C / LiPo 2S / switch de potência e melhorando a ergonomia frontal.
   - **Separação física do clique do Joystick (`JOY_SW` no `IO23`)**: os pinos 9/10 do clique central do `JOY1` passaram de `IO9_BOOT` para o pino dedicado **`IO23` (`J2` Pino 5, `/JOY_SW`)**, desligando o sinal não usado `PPS` de `J6` Pino 1. Isto evita entrar acidentalmente em *Download Boot Mode* se o joystick for premido ao ligar a alimentação e deixa **5 entradas digitais independentes** no Lafvin Master (`JOY_SW=IO23`, `SW1=IO18`, `SW2=IO19`, `SW3=IO20`, `SW4=IO9`).
4. **Serigrafia dupla face (`F.SilkS` + `B.SilkS`) à prova de erros**
   - Todas as etiquetas dos módulos, headers, interruptores e pinouts estão impressas nas **duas faces** da PCB.
   - Todos os pinos **GND** têm uma **barra branca sólida** junto ao pad nas duas faces (incluindo `U_HALL` Pin 3 `GND`, `J_BAL_MOT`, `J_IMU`, `J_SOLAR`, `J_LIPO_2S`, `SRV_WIN`, `SRV_RCK`, `J_WINCH_HUB/SUB`, `RC SERVOS`, `SENSORS (IO)`, `BEC IN`, `SX1278` e `LC29H`).
   - Separação visual explícita entre **`RC SERVOS (5V BEC)`** (`ESC`, `RUD`, `WIN`, `RCK`, `STRB`) e **`SENSORS (IO)`** (`WTR`, `LIM` — sem +5V no pino central para nunca ligar um servo por engano).
5. **Monitorização LiPo do Motor 3S–6S por célula via `2× ADS1115` com Auto-Deteção (`J_BAL_MOT` lateral a 90°, dispensa Power Module)**
   - O antigo header `J_BATT` do Power Module foi removido; em seu lugar foi integrado o conector **`J_BAL_MOT`** (`PinHeader 1×7` a **90° lateral**, `2.54 mm`, aberto sem abas) na **borda esquerda junto aos botões `SW1..SW4`** para encaixar lateralmente através do chassis (`Pin 1: GND` em baixo com barra branca sólida e pad quadrado; `1S..6S` a subir), ligado a **dois ADCs I2C de 16 bits (`ADS1115IDGSR`, LCSC `C37593`)**.
   - Auto-deteta automaticamente baterias **2S, 3S, 4S, 5S ou 6S** (cada canal não ligado é puxado a `0.000V` pelos divisores), calcula tensão total e tensão individual por célula (`C1..C6`), e protege a massa de balanceamento localmente junto ao pino 1 com fusível rearmável **`F_BAL` (`1.5A PTC`)**.
6. **ESP32-C6 Onboard (`ESP32-C6-WROOM-1-N8`, `U_C6`) + `JOY1` (`RKJXV122400R`) montados de fábrica pela JLCPCB**
   - Os antigos headers `J_DK_L` / `J_DK_R` (DevKitC-1 externo) foram substituídos pelo módulo **`ESP32-C6-WROOM-1-N8` (`U_C6`, LCSC `C5366877`)** montado diretamente na face `Top` (`F.Cu`) com zona *keepout* limpa na antena PCB inferior.
   - Alimentação dedicada de `+3.3V` via **`AMS1117-3.3` (`U_LDO`, LCSC `C6186`)** com entrada dual-OR Schottky (**`D_LDO_USB`** a partir de `/VBUS_USB` + **`D_LDO_5V`** a partir de `/+5V` do buck), `2× 22uF` (`C_LDO_IN`, `C_LDO_OUT`), `100nF` (`C_C6_VDD`, `C_C6_EN`), pull-ups `10k` (`R_C6_EN`, `R_IO8_PU`) e botões **`SW_C6_BOOT` / `SW_C6_RST`** (`SKRKAEE020`, LCSC `C115357`). Permite gravar e depurar o `U_C6` apenas com cabo USB-C ligado, mesmo sem bateria 2S.
   - **USB-C único para carregamento e flashing**: os pinos `DM/DP` do `IP2326` ficaram flutuantes e `/USB_DP` + `/USB_DN` da `J_USBC` ligam diretamente à porta nativa *USB Serial/JTAG* do `U_C6` (`GPIO13` / `U_C6:14` e `GPIO12` / `U_C6:13`).
   - **Adaptação de firmware (`#ifdef CARRIER_PCB_V2` em `include/config_common.h`)**:
     - `PIN_INTER_ESP_C6_TX = 3` (`U_C6:26`, `GPIO3` em vez de `GPIO13` que passa a USB D+; `PIN_INTER_ESP_C6_RX` mantém `22`).
     - `PIN_C6_WATER_SENS = 10` (`U_C6:11`, alinhado com a V2 sem conflito com `SERVO_RUDDER` no `GPIO15`).
     - `PIN_JOYSTICK_SW = 23` (`GPIO23` dedicado ao clique central do joystick).
   - O joystick **`JOY1` (`RKJXV122400R`, LCSC `C918854`)** foi incluído diretamente na BOM e no CPL (face `Top`) para montagem de fábrica pela JLCPCB.

---

## 2. Arquitetura de Energia (2S LiPo + USB-C + Solar)

```text
  [USB-C 5V (J_USBC)] ──┬──► [D_USB B5819W] ─┐
         (DM/DP -> U_C6) │                    ├──► /VIN_CHG ──► [IP2326 Boost Charger (U_CHG)]
                         │ [SOLAR (J_SOLAR)] ►┴─► [D_SOL B5819W]       │             │
                         │                              (BAT 8.4V) ◄───┘             └──► R_BAL (10R) ──► J_LIPO_2S:2 (4.2V)
                         │                                   │
                         │                         [J_LIPO_2S (B3B-XH-A)]
                         │                          1:GND | 2:4.2V | 3:8.4V
                         │                                   │
                         │                         [F1 1.5A] ──► [Q_PROT AO3401A] ──► SW_PWR / J_SW_EXT
                         │                                                                │
                         │                                                                ▼
                         │                                                       [MT2492 5.0V Buck] ──► /+5V ──► D_5V (Verde)
                         │                                                                                 │
                         └──► [D_LDO_USB B5819W] ──┐                                                       │
                                                   ├──► /LDO_VIN ──► [AMS1117-3.3 (U_LDO)] ◄── [D_LDO_5V] ─┘
                                                   └──────────────────► /+3V3_C6 (U_C6, ADS1115, IMU, LoRa, Hall)
```

### Notas de utilização e jumpers
- **Porta `J_LIPO_2S` (`B3B-XH-A`)**: ligue diretamente a ficha de balanceamento **3 pinos (2S)** da LiPo (`Pin 1: GND`, `Pin 2: 4.2V célula intermédia`, `Pin 3: 8.4V +BAT`). A carga e a alimentação da placa fazem-se por esta única ficha.
- **Solar (`J_SOLAR` 5V–6V)**: suporta painel solar nominal de 5V–6V (`Vin_OV / UV` configurado via `R_UV = 120k`; `D_SOL` protege contra descarga noturna; `R_NTC = 51k` para temperatura fixa segura).
- **LEDs indicadores (face Top)**:
  - **`CHG`** (`D_CHG` vermelho): estado de carregamento do `IP2326`.
  - **`5V OK`** (`D_5V` verde): confirma saída regulada de `+5.0V` após o interruptor de potência.
- **Interruptor ON/OFF (`SW_PWR` + `J_SW_EXT`)**: o interruptor SMD onboard `SW_PWR` liga em paralelo com o header externo `J_SW_EXT`. Se usar um interruptor externo no casco, deixe `SW_PWR` na posição **OFF** para o interruptor externo controlar a alimentação; se deixar `SW_PWR` em **ON**, o sistema fica ligado independentemente de `J_SW_EXT`.
- **`JP_2S_ADC` (Monitorização opcional da bateria 2S da eletrónica)**: jumper de solda aberto de fábrica abaixo da ficha `J_IMU` (`2S ADC`). Como o antigo `J_BATT` foi substituído pelo monitor `2× ADS1115` na porta `J_BAL_MOT`, fechar `JP_2S_ADC` com uma gota de solda liga diretamente o divisor `R_2S_TOP (100k) / R_2S_BOT (33k)` (pós-interruptor, consumo zero em OFF) ao pino `/BATT_VOLT` do ESP32-C6 (máx. ~2.08V para 8.4V) sem conflitos.
- **`J_IMU` (`B4B-XH-A` — GY-BNO055 9-DOF)**: ligue diretamente o módulo GY-BNO055 com cabo JST-XH 4 pinos (`1: GND | 2: 3V3 | 3: SDA (GPIO 1) | 4: SCL (GPIO 11)`).
- **`J_PWR_ESP` (`5V AUX`)**: saída auxiliar de `+5V` regulado e `GND` com fusível/proteção para uso eventual de bancada.

---

## 3. Cabo Único de 6 Pinos para a Assembly do Guincho (`J_WINCH_HUB` ↔ `J_WINCH_SUB`)

Como o sensor Hall precisa de `3.3V` + sinal e os dois servos precisam de `5V` + sinais + `GND` comum, a ligação à sub-placa destacável faz-se num único cabo plano de **6 pinos (passo 2.54 mm)**:

| Pino | Etiqueta PCB | Ligação / Função | Destino na Sub-Placa |
| :---: | :---: | :--- | :--- |
| **1** | `GND` | Massa comum (barra branca) | `SRV_WIN:1`, `SRV_RCK:1`, `U_HALL:3` |
| **2** | `5V` | `+5V_SERVO` (vindo do `BEC IN`) | `SRV_WIN:2`, `SRV_RCK:2`, `C_SRV` (22uF) |
| **3** | `3V3` | `+3.3V` dos ESP32 (`/+3V3_C6`) | `U_HALL:1` (`DRV5032 VCC`), `C_HALL` (100nF) |
| **4** | `HAL` | Sinal Anchor Hall (`/ANCHOR_HALL`) | `U_HALL:2` (`DRV5032 OUT`) |
| **5** | `WIN` | Sinal PWM Winch (`/SERVO_WINCH`) | `SRV_WIN:3` (`WIN`) |
| **6** | `RCK` | Sinal PWM Rack (`/SERVO_RACK`) | `SRV_RCK:3` (`RCK`) |

*(Dica de montagem: na sub-placa destacada pode soldar pin headers retos ou a 90° em `J_WINCH_SUB`, `SRV_WIN` e `SRV_RCK` consoante o ângulo de saída dos cabos na estrutura do guincho).*

---

## 4. Monitorização Motor LiPo 3S–6S com Auto-Deteção (`2× ADS1115` via `J_BAL_MOT`)

Para dispensar o Power Module externo e monitorizar **célula a célula** baterias entre **3S e 6S** (ou 2S), a V2 integra dois ADCs **`ADS1115IDGSR`** (`16-bit`, partilhando o barramento I2C `IO1 = SDA` / `IO11 = SCL` do `GY-BNO055` com pull-ups dedicados `R_I2C_SDA = 10k` e `R_I2C_SCL = 10k` para funcionar mesmo sem IMU ligado):

- **Ficha universal `J_BAL_MOT` lateral na borda esquerda** (`PinHeader 1×7` em **ângulo reto 90°**, `2.54 mm`, aberta sem abas): posicionada verticalmente ao lado dos botões (`SW1..SW4`) para saída direta pelo chassis; aceita fichas JST-XH **3S (4 pinos), 4S (5 pinos), 5S (6 pinos) e 6S (7 pinos)** sem cortar plástico. **Alinhar sempre o fio preto (`GND`) com o pino inferior `Pin 1 (GND)`** (pad quadrado + barra branca sólida em baixo: de baixo para cima `1: GND | 2: 1S | 3: 2S | 4: 3S | 5: 4S | 6: 5S | 7: 6S`).
- **Proteção `F_BAL`**: colocada logo junto ao `Pin 1 (GND)` (`1.5A PTC`, `< 0.15 Ω`, queda de tensão desprezável) para impedir correntes destrutivas caso a massa principal do ESC falhe.

### Mapeamento I2C, Divisores (1% / 0805) e Filtros Anti-Aliasing (`100nF`)

| Tap | Chip & Endereço I2C | Canal ADC | `R_top` | `R_bot` | Filtro | Rácio Firmware `(R_top + R_bot) / R_bot` | `V_ADC @ 4.20V/cél` |
| :---: | :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| **1S** | `U_ADS1` (`0x48`, `ADDR→GND`) | `AIN3` | `51k` (`R_C1T`) | `100k` (`R_C1B`) | `100nF` (`C_F1`) | **`1.5100`** | `2.781 V` (`4.2 V`) |
| **2S** | `U_ADS1` (`0x48`, `ADDR→GND`) | `AIN2` | `100k` (`R_C2T`) | `51k` (`R_C2B`) | `100nF` (`C_F2`) | **`2.9608`** | `2.837 V` (`8.4 V`) |
| **3S** | `U_ADS1` (`0x48`, `ADDR→GND`) | `AIN1` | `120k` (`R_C3T`) | `33k` (`R_C3B`) | `100nF` (`C_F3`) | **`4.6364`** | `2.718 V` (`12.6 V`) |
| **4S** | `U_ADS1` (`0x48`, `ADDR→GND`) | `AIN0` | `100k` (`R_C4T`) | `15k` (`R_C4B`) | `100nF` (`C_F4`) | **`7.6667`** | `2.191 V` (`16.8 V`) |
| **5S** | `U_ADS2` (`0x49`, `ADDR→3V3`) | `AIN1` | `110k` (`R_C5T`) | `15k` (`R_C5B`) | `100nF` (`C_F5`) | **`8.3333`** | `2.520 V` (`21.0 V`) |
| **6S** | `U_ADS2` (`0x49`, `ADDR→3V3`) | `AIN0` | `100k` (`R_C6T`) | `10k` (`R_C6B`) | `100nF` (`C_F6`) | **`11.0000`** | `2.291 V` (`25.2 V`) |

*(Nota: em `U_ADS2`, os canais não usados `AIN2` e `AIN3` estão ligados a `GND`. Todos os divisores garantem `V_ADC < 3.0 V` mesmo em células LiHV `4.35 V`, dentro do intervalo seguro de `GAIN_ONE` = `±4.096 V` a `0.125 mV/LSB`).*

### Como funciona a Auto-Deteção (2S–6S) no Firmware
1. Como todos os taps têm uma resistência inferior para massa (`R_C1B..R_C6B`), qualquer pino livre da ficha `J_BAL_MOT` lê limpo **`0.000 V`** sem ruído flutuante.
2. Ao inicializar (ou periodicamente), o firmware multiplica cada leitura `V_adc[k]` pelo rácio da tabela acima para obter `V_tap[k]` e percorre **`k = 6 → 1`**: o primeiro tap com `V_tap[k] > 2.0 V` define automaticamente o número exato de células `N` (`3S..6S`, ou `2S`).
3. A tensão total do pack é **`V_tap[N]`**, e cada célula individual calcula-se por **`V_cell[1] = V_tap[1]`** e **`V_cell[k] = V_tap[k] - V_tap[k-1]`** para `k = 2..N`.

---

## 5. Verificação & Ficheiros Gerados

- **DRC KiCad (`kicad-cli pcb drc --refill-zones`)**:
  - **`0 errors`**
  - **`0 unconnected pads`**
  - **`0 Footprint errors`**
  - Sem trilhos soltos (`0 track_dangling`), sem vias soltas (`0 via_dangling`), sem sobreposição de courtyards (`0 courtyards_overlap`) e sem pontes de máscara de solda (`0 solder_mask_bridge`).
- **Renders 3D**:
  - [Dual_ESP_Carrier_V2_3d_top.png](./Dual_ESP_Carrier_V2_3d_top.png)
  - [Dual_ESP_Carrier_V2_3d_bottom.png](./Dual_ESP_Carrier_V2_3d_bottom.png)
- **PCB KiCad 10**:
  - [Dual_ESP_Carrier_V2.kicad_pcb](./Dual_ESP_Carrier_V2.kicad_pcb)
  - Script gerador: [build_v2_pcb.py](./build_v2_pcb.py)

---

## 6. Checklist de Encomenda JLCPCB (PCB + PCBA Top-Side Only)

Todos os ficheiros prontos para upload encontram-se em [`jlcpcb_pcba/`](./jlcpcb_pcba/):

1. **Upload Gerber**: carregar [`Dual_ESP_Carrier_V2_Gerber.zip`](./jlcpcb_pcba/Dual_ESP_Carrier_V2_Gerber.zip) na JLCPCB (2 camadas, `100.0 × 100.0 mm`, `1.6 mm` FR4, cor à escolha — verde ou preto mate ficam excelentes com a serigrafia branca dupla face).
2. **Ativar "PCB Assembly"**:
   - **Assembly Side**: **Top Side** *(todos os **84 componentes** estão exclusivamente na layer `Top` / `F.Cu`: 82 SMD + soldadura THT de `J_USBC` e `JOY1`)*.
   - **PCBA Type**: **Economic / Standard PCBA** conforme indicado na cotação JLCPCB para o módulo `ESP32-C6-WROOM-1-N8` e soldadura THT mista de `JOY1` e das patilhas de `J_USBC`.
   - **Já incluídos de fábrica na montagem JLCPCB (`BOM.csv` + `CPL.csv`)**:
     - **`U_C6`** (`ESP32-C6-WROOM-1-N8`, **LCSC `C5366877`**): MCU escravo onboard montado na face Top (com zona *keepout* livre sob a antena PCB).
     - **`U_LDO`** (`AMS1117-3.3`, SOT-223, **LCSC `C6186`**) + **`D_LDO_USB` / `D_LDO_5V`** (`B5819W`, **LCSC `C8598`**), `2× 22uF`, `100nF`, `R_C6_EN` / `R_IO8_PU` (`10k`).
     - **`JOY1`** (`RKJXV122400R`, Alps THT na face Top, **LCSC `C918854`**): joystick PS5 completo montado de fábrica.
     - **`U_ADS1` / `U_ADS2`** (`ADS1115IDGSR`, MSOP-10, **LCSC `C37593`**): os dois ADCs I2C de 16 bits (`0x48` e `0x49`) + divisores/filtros/`F_BAL`/`10k` pull-ups.
     - **`J_USBC`** (`TYPE-C-31-M-12`, **LCSC `C165948`**): conector USB-C com 16 pinos SMD + 4 patilhas mecânicas THT de blindagem.
     - **`SW_PWR`** (`MSK12C02`, **LCSC `C431540`**): interruptor deslizante SMD `ON/OFF` na borda direita.
     - **`SW1..SW4`, `SW_C6_BOOT`, `SW_C6_RST`** (`6× SKRKAEE020`, **LCSC `C115357`**): os 4 botões frontais + 2 botões dedicados de `BOOT`/`RST` do `U_C6`.
     - **`U_HALL`** (`DRV5032FADBZR`, **LCSC `C140921`**) + todos os restantes passivos, buck e carregador.
3. **Carregar BOM & CPL**:
   - BOM: [`Dual_ESP_Carrier_V2_BOM.csv`](./jlcpcb_pcba/Dual_ESP_Carrier_V2_BOM.csv) (**31 linhas agrupadas / 84 peças** com códigos **LCSC Part #**).
   - CPL: [`Dual_ESP_Carrier_V2_CPL.csv`](./jlcpcb_pcba/Dual_ESP_Carrier_V2_CPL.csv) (**84 linhas**, correspondência 1-para-1 exata com a BOM, todas na layer `Top`).
4. **Conferir visualmente no JLCPCB Component Placement Viewer** antes de finalizar:
   - **`U_C6` (`C5366877`, ESP32-C6-WROOM-1-N8)**: confirmar a antena PCB voltada **para baixo (sul)** sobre a zona `ANTENNA KEEPOUT` e Pino 1 (`GND`) no canto superior direito (`180°` no CPL).
   - **`JOY1` (`C918854`, RKJXV122400R)**: confirmar orientação dos potenciómetros (azul à esquerda / laranja em cima, rotação `0°` no CPL) sobre os furos em `X = 72.7525 mm`.
   - **`U_LDO` (`C6186`, AMS1117-3.3)**: patilha larga `Tab` voltada para a direita (`0°` no CPL).
   - **`U_ADS1` / `U_ADS2` (ADS1115, MSOP-10)**: marca do Pino 1 voltada para cima/direita conforme orientação `270°` no CPL.
   - **`U_CHG` (IP2326, QFN-24)**: ponto do Pino 1 voltado para cima/esquerda conforme orientação `90°` no CPL.
   - **`U_BUCK` (MT2492, SOT-23-6)**: Pino 1 no canto direito/baixo conforme `180°`.
   - **`Q_PROT` (AO3401A, SOT-23)** e **`U_HALL` (DRV5032FADBZR, SOT-23)**: 3 terminais alinhados corretamente sobre os pads.
   - **`D_SOL` / `D_USB` / `D_LDO_USB` / `D_LDO_5V` (B5819W)** e **`D_CHG` / `D_5V` (LEDs)**: confirmar marca de cátodo com os pads nos renders JLCPCB.
   - **`J_USBC` (`C165948`)**: confirmar boca do conector alinhada exatamente para fora na borda direita (`X = 100.0 mm`).
   - **`SW_PWR` (`C431540`)**: confirmar patilha do interruptor alinhada para fora na borda direita (`X = 100.0 mm`); como componentes eletromecânicos em ângulo reto variam conforme o lote de fita JLCPCB, ajuste a rotação no preview online caso surja rodado 180°/90°.

### Componentes Through-Hole (THT) e Módulos a soldar manualmente após receção
- **`J_SOLAR`** (`JST B2B-XH-A` 2 pinos, 2.50 mm) — Entrada solar 5V–6V
- **`J_LIPO_2S`** (`JST B3B-XH-A` 3 pinos, 2.50 mm) — Ficha de balanceamento LiPo 2S
- **`J_IMU`** (`JST B4B-XH-A` 4 pinos, 2.50 mm) — Ficha dedicada ao IMU GY-BNO055 (`GND, 3V3, SDA, SCL`)
- **`J_BAL_MOT`** (`PinHeader 1×7` em ângulo reto **90° lateral**, 2.54 mm) — Balanceamento LiPo Motor 3S–6S na borda esquerda junto aos botões (`GND` em baixo, `1S..6S` a subir)
- **`J1` / `J2`** (1×9, 2.54 mm) — Lafvin ESP32-C6 LCD 1.47" (Master, Back side)
- **`J3`** (2×4, 2.54 mm) — SX1278 433 MHz LoRa (Back side)
- **`J6`** (1×7, 2.54 mm) — GPS Quectel LC29H (Back side)
- **Headers 2.54 mm** — `ESC`, `RUD`, `WIN`, `RCK`, `STRB` (1×3), `J_WATER` (1×2), `J_LIMIT` (1×2), `J_PWR_SRV` (1×2), `J_WINCH_HUB` (1×6), `J_SW_EXT` (1×2), `J_PWR_ESP` (1×2) e na sub-placa `J_WINCH_SUB` (1×6), `SRV_WIN` (1×3), `SRV_RCK` (1×3).
