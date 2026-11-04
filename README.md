# WindDragons — Sistema de Telemetria e Controlo Náutico (Rover & Base Station)

Projeto de automação, navegação autónoma e telemetria para boias robóticas de regata (**Rovers**) e estação de comando com ecrã (**Base Station**), baseado na **Placa Carrier Universal Dual ESP32-C6** (**Master Lafvin LCD 1.47" IPS** + **Coprocessador ESP32-C6 DevKitC-1**).

---

## 🛰️ 1. Arquitetura Global do Sistema

```mermaid
flowchart LR
  subgraph CLOUD["☁️ WindDragons Cloud"]
    API["HTTPS REST API<br/>/api/circuits/station/telemetry"]
  end

  subgraph BASE["🎮 BASE STATION - Dual ESP32-C6"]
    BM["Master: Lafvin ESP32-C6<br/>• LCD 1.47 pol. ST7789 320x172<br/>• MicroSD /www/base/<br/>• Web Server WiFi STA e AP<br/>• Joystick Analógico + Botões<br/>• Coordenador OTA Dual"]
    BC["Slave: ESP32-C6 DevKitC-1<br/>• Rádio LoRa SX1278 433 MHz<br/>• Polling e Escuta Contínua<br/>• Telemetria da Frota 1..8"]
    BM <-->|"UART Inter-ESP 115200 bps<br/>TX:IO2 / RX:IO1"| BC
  end

  subgraph ROVER["🚤 ROVER / BOIA AUTÓNOMA - Dual ESP32-C6"]
    RM["Master: Lafvin ESP32-C6<br/>• LCD 1.47 pol. ST7789 + MicroSD<br/>• GPS Quectel LC29H UART1<br/>• Dashboard Web e OTA Dual<br/>• Joystick Local Opcional"]
    RC["Slave: ESP32-C6 DevKitC-1<br/>• Rádio LoRa SX1278 433 MHz<br/>• IMU 9-DOF GY-BNO055 em J_EXP<br/>• PWM: ESC, Leme, Guincho, Trinco<br/>• Sensores: Bateria 6S, Hall, Fim-Curso<br/>• Fita LEDs WS2812B Strobe"]
    RM <-->|"UART Inter-ESP 115200 bps<br/>TX:IO2 / RX:IO1"| RC
  end

  BM <-->|"WiFi HTTPS JSON"| API
  BC <-->|"LoRa 433 MHz<br/>BW 250kHz • SF7"| RC
```

---

## 🧭 2. Esquema de Ligação — Bússola & IMU 9-DOF (`GY-BNO055` em `J_EXP`)

O módulo **GY-BNO055** liga-se diretamente à ilha de expansão **`J_EXP` (2x03 pinos, passo 2.54 mm)** da PCB Carrier (ligada ao **Coprocessador ESP32-C6 DevKitC-1**).

```mermaid
flowchart LR
  subgraph J_EXP["📌 Conector J_EXP - PCB Carrier C6"]
    direction TB
    P1["Pino 1: IO1 - GPIO 1<br/>Coluna Esq. Topo"]
    P3["Pino 3: IO11 - GPIO 11<br/>Coluna Esq. Meio"]
    P5["Pino 5: GND<br/>Coluna Esq. Base"]
    P2["Pino 2: 3V3 - +3.3V_C6<br/>Coluna Dir. Topo"]
    P4["Pino 4: 5V<br/>Não ligar ao BNO055"]
    P6["Pino 6: GND<br/>Coluna Dir. Base"]
  end

  subgraph BNO["🧭 Módulo GY-BNO055 - I2C 0x28"]
    direction TB
    VIN["VIN - Alimentação 3.3V"]
    GND["GND - Terra Comum"]
    SCL["SCL / Rx - I2C Clock"]
    SDA["SDA / Tx - I2C Data"]
    ADD["ADD - Aberto = 0x28"]
    INT["INT / BOOT / REST - NC"]
  end

  P2 -->|"🔴 Fio Vermelho +3.3V"| VIN
  P5 -->|"⚫ Fio Preto GND"| GND
  P3 -->|"🟡 Fio Amarelo I2C SCL"| SCL
  P1 <-->|"🔵 Fio Azul/Verde I2C SDA"| SDA
```

### Disposição Física da Serigrafia `J_EXP` na PCB vs `GY-BNO055`

```text
       PCB CARRIER — ILHA J_EXP                     MÓDULO GY-BNO055
    ┌────────────────────────────┐               ┌────────────────────┐
    │     Col. Esq.    Col. Dir. │               │  [o] VIN     <────────── Pino 2 (3V3)
    │                            │               │  [o] GND     <────────── Pino 5 ou 6 (GND)
    │     [■] IO1      [●] 3V3   │ ──(Pinos 1,2) │  [o] SCL\Rx  <────────── Pino 3 (IO11)
    │     [●] IO11     [●] 5V    │ ──(Pinos 3,4) │  [o] SDA\Tx  <────────── Pino 1 (IO1)
    │     [●] GND      [●] GND   │ ──(Pinos 5,6) │  [o] ADD     (NC = 0x28)
    │         EXPANSION          │               │  [o] INT     (NC)
    └────────────────────────────┘               │  [o] BOOT    (NC)
                                                 │  [o] REST    (NC)
                                                 └────────────────────┘
```

| Pino no `GY-BNO055` | Pino em `J_EXP` (PCB) | GPIO ESP32-C6 (Slave) | Função / Notas |
| :--- | :--- | :--- | :--- |
| **`VIN`** | **`3V3`** (Pino 2 — Topo Direito) | `+3.3V_C6` | **Usar `3V3`** para manter níveis lógicos I2C a 3.3V puros |
| **`GND`** | **`GND`** (Pino 5 ou 6 — Base) | `GND` | Terra comum de referência |
| **`SCL \ Rx`** | **`IO11`** (Pino 3 — Meio Esquerdo) | `GPIO 11` | **I2C SCL** (Clock a 100 kHz com *clock stretching* ativo) |
| **`SDA \ Tx`** | **`IO1`** (Pino 1 — Topo Esquerdo) | `GPIO 1` | **I2C SDA** (Dados bidirecionais) |
| **`ADD` / `INT` / `BOOT` / `REST`** | *Não ligados (NC)* | — | Endereço I2C padrão `0x28` (deteção automática `0x28` / `0x29`) |

> [!IMPORTANT]
> **Pontes `PS0` e `PS1` no módulo GY-BNO055:** Certifique-se de que as duas pontes de solda **`PS0`** e **`PS1`** na placa azul do `GY-BNO055` estão **abertas (`LOW` / `0`)** para operar em modo **Standard I2C**.

---

## 🔌 3. Esquema Completo de Ligações — ROVER (Boia Autónoma)

```mermaid
flowchart TB
  subgraph MASTER["🖥️ MASTER: Lafvin ESP32-C6 - LCD 1.47 pol. IPS + MicroSD"]
    LCD["Ecrã ST7789 172x320<br/>MOSI:IO6 • SCLK:IO7 • CS:IO14<br/>DC:IO15 • RST:IO21 • BL:IO22"]
    SD["Leitor MicroSD Onboard<br/>CS:IO4 • MISO:IO5<br/>MOSI:IO6 • SCLK:IO7"]
    M_UART["UART Inter-ESP J1<br/>TX: IO2 Pad 4<br/>RX: IO1 Pad 5"]
    M_GPS["UART1 GPS J2<br/>TX: IO16 Pad 8<br/>RX: IO17 Pad 9"]
    M_JOY["Joystick Local J1<br/>VRx: IO3 Pad 3<br/>VRy: IO0 Pad 6"]
  end

  subgraph SLAVE["⚙️ SLAVE / COPROCESSADOR: ESP32-C6 DevKitC-1"]
    S_UART["UART Inter-ESP J3<br/>RX: GPIO 22 Pin 6<br/>TX: GPIO 13 Pin 13"]
    S_LORA["SPI Hardware FSPI<br/>NSS:IO6 • SCK:IO5 • MOSI:IO7<br/>MISO:IO4 • DIO0:IO0"]
    S_I2C["Barramento I2C J_EXP<br/>SDA: GPIO 1 IO1<br/>SCL: GPIO 11 IO11"]
    S_PWM["Controladores PWM 50 Hz<br/>ESC: GPIO 17 • RUD: GPIO 15<br/>WIN: GPIO 23 • RCK: GPIO 21"]
    S_SENS["Entradas de Sensores<br/>Bateria V: GPIO 2 • I: GPIO 3<br/>Fim Curso: GPIO 18 • Hall: GPIO 20<br/>LEDs Strobe: GPIO 19"]
  end

  GPS_MOD["🛰️ GPS Quectel LC29H<br/>115200 bps • 3.3V"]
  JOY_MOD["🕹️ Joystick Analógico<br/>Opcional no Rover"]
  M_GPS <-->|"TX:IO16 para R1 / RX:IO17 de T1"| GPS_MOD
  JOY_MOD --> M_JOY

  M_UART <-->|"115200 bps<br/>Verde IO2-IO22 • Amarelo IO13-IO1"| S_UART

  LORA_MOD["📡 Rádio LoRa Ra-02<br/>SX1278 433 MHz"]
  BNO_MOD["🧭 Bússola e IMU 9-DOF<br/>GY-BNO055 0x28"]
  ACT_MOD["⚡ Atuadores PWM<br/>• ESC Motor IO17<br/>• Servo Leme IO15<br/>• Servo Guincho IO23<br/>• Servo Cremalheira IO21"]
  ANCH_MOD["⚓ Sensores e Strobe<br/>• Power Module 6S IO2 e IO3<br/>• Microswitch Topo IO18<br/>• Sensor Hall DRV5032 IO20<br/>• Fita WS2812B 8 LEDs IO19"]

  S_LORA <--> LORA_MOD
  S_I2C <--> BNO_MOD
  S_PWM --> ACT_MOD
  S_SENS <--> ANCH_MOD
```

### Mapa da Barra Inferior de Conectores da PCB Carrier (`Dual_ESP_Carrier`)

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│  BARRA INFERIOR DE ATUADORES E SENSORES (Passo 2.54 mm — Esquerda para a Direita)      │
├───────┬───────┬───────┬───────┬────────┬────────┬─────────┬─────────┬──────────────────┤
│  ESC  │  RUD  │  WIN  │  RCK  │  STRB  │ J_BATT │ J_LIMIT │ J_WATER │       HALL       │
│ (3p)  │ (3p)  │ (3p)  │ (3p)  │  (3p)  │  (3p)  │  (2p)   │  (2p)   │       (3p)       │
├───────┼───────┼───────┼───────┼────────┼────────┼─────────┼─────────┼──────────────────┤
│ 1:GND │ 1:GND │ 1:GND │ 1:GND │ 1:GND  │ 1:GND  │ 1:GND   │ 1:GND   │ 1:GND (Preto)    │
│ 2:+5V │ 2:+5V │ 2:+5V │ 2:+5V │ 2:+5V  │ 2:IO3  │ 2:IO18  │ 2:IO15  │ 2:+3V3_C6 (Verm) │
│ 3:IO17│ 3:IO15│ 3:IO23│ 3:IO21│ 3:IO19 │ 3:IO2  │ (Switch)│ (Água)  │ 3:IO20 (Amarelo) │
└───────┴───────┴───────┴───────┴────────┴────────┴─────────┴─────────┴──────────────────┘
```

### Tabela Completa de Pinos do Rover

#### A. Comunicação Entre Placas (Master Lafvin $\leftrightarrow$ Slave DevKitC-1)
| Sinal | Pino Master (Lafvin `J1`) | Pino Slave (DevKitC-1 `J3`) | Cor de Referência | Função |
| :--- | :--- | :--- | :--- | :--- |
| **Master TX $\rightarrow$ Slave RX** | `GPIO 2` (`J1` Pad 4 — Serigrafia `2`) | `GPIO 22` (`J3` Pin 6) | 🟢 Verde | Comandos, GPS, Config e OTA para o C6 |
| **Slave TX $\rightarrow$ Master RX** | `GPIO 1` (`J1` Pad 5 — Serigrafia `1`) | `GPIO 13` (`J3` Pin 13) | 🟡 Amarelo | Telemetria LoRa, Bateria, Âncora e Estados |
| **Terra Comum** | `GND` (`J1` Pad 8) | `GND` (`J3` Pin 14) | ⚫ Preto | Referência comum obrigatória |

#### B. Periféricos Ligados ao Coprocessador ESP32-C6 DevKitC-1 (Rover)
| Periférico / Conector PCB | Pino do Módulo | GPIO DevKitC-1 | Notas Técnicas |
| :--- | :--- | :--- | :--- |
| **LoRa SX1278 (`J3_LORA`)** | `NSS` / `SCK` / `MOSI` / `MISO` / `DIO0` | `IO6` / `IO5` / `IO7` / `IO4` / `IO0` | Alimentado por `+3V3_C6` dedicado (433.0 MHz) |
| **Bússola `GY-BNO055` (`J_EXP`)** | `SDA` (`Pino 1`) / `SCL` (`Pino 3`) | `GPIO 1` (`SDA`) / `GPIO 11` (`SCL`) | Alimentado em `J_EXP Pino 2 (3V3)` e `Pino 5/6 (GND)` |
| **ESC Motor (`ESC`)** | Sinal PWM (Pino 3) | `GPIO 17` | 50 Hz (`1000..2000 µs`, Neutro `1500 µs`) |
| **Servo do Leme (`RUD`)** | Sinal PWM (Pino 3) | `GPIO 15` | 50 Hz proporcional (`-100%` a `+100%`) |
| **Guincho da Âncora (`WIN`)** | Sinal PWM (Pino 3) | `GPIO 23` | Rotação contínua (`1800 µs` sobe, `1200 µs` desce, `1500 µs` para) |
| **Servo Cremalheira (`RCK`)** | Sinal PWM (Pino 3) | `GPIO 21` | Trinco mecânico (`0°` travado / `90°` queda livre) |
| **LEDs WS2812B Strobe (`STRB`)**| `DIN` (Pino 3) + `+5V_SERVO` + `GND` | `GPIO 19` | Fita/Anel NeoPixel 8 LEDs RGB (Strobe / Navegação / Bateria) |
| **Fim de Curso Âncora (`J_LIMIT`)**| Contacto NA (Pino 1 `GND`, Pino 2 `Sinal`) | `GPIO 18` | `INPUT_PULLUP` — fecha ao `GND` quando a âncora toca no topo |
| **Sensor Hall Tambor (`HALL`)** | `GND` (1), `+3V3_C6` (2), `OUT` (3) | `GPIO 20` | Sensor TI **DRV5032** (`INPUT_PULLUP`, interrupção `FALLING`) |
| **Power Module 6S (`J_BATT`)** | `GND` (1), `CURR` (2), `VOLT` (3) | `GPIO 2` (V) / `GPIO 3` (I) | Leitura ADC LiPo 6S (divisor `10.803`, `27.32 A/V`) |

#### C. Periféricos Ligados ao Master Lafvin ESP32-C6 (Rover)
| Periférico / Conector PCB | Pino do Módulo | GPIO Lafvin | Notas Técnicas |
| :--- | :--- | :--- | :--- |
| **GPS Quectel LC29H (`J6`)** | `VCC` (`3V3`), `GND`, `R1` (RX), `T1` (TX) | `GPIO 16` (TX) / `GPIO 17` (RX) | UART1 a **115200 bps** (`R1` liga a `IO16`, `T1` liga a `IO17`) |
| **Ecrã LCD 1.47" ST7789** | Integrado na placa Lafvin | `IO6, IO7, IO14, IO15, IO21, IO22` | Resolução `320x172` Landscape |
| **Leitor MicroSD (TF Card)** | Integrado na placa Lafvin | `CS=IO4`, `MISO=IO5`, `MOSI=IO6`, `SCK=IO7` | Armazena `/www/rover/`, `/config.json` e `/telemetry.csv` |
| **Joystick Físico (`JOY1`)** | `3V3`, `GND`, `VRx`, `VRy` | `GPIO 3` (X) / `GPIO 0` (Y) | Ativável/desativável na página `/config` |

---

## 🎮 4. Esquema Completo de Ligações — BASE STATION (Comando de Frota)

```mermaid
flowchart LR
  subgraph BASE_M["🖥️ BASE MASTER: Lafvin ESP32-C6"]
    B_LCD["Ecrã LCD 1.47 pol. ST7789<br/>Dashboard Frota, Regata e Rede"]
    B_SD["MicroSD Onboard CS:IO4<br/>/www/base/ + Registo de Missões"]
    B_JOY["Joystick Analógico PS5 J1<br/>Eixo X Leme: GPIO 3<br/>Eixo Y Motor: GPIO 0"]
    B_BTN["Botões Físicos<br/>BOOT Página: GPIO 9<br/>SW1/SW2/SW3: IO18 / IO19 / IO20"]
    B_UART["UART Inter-ESP<br/>TX: GPIO 2 • RX: GPIO 1"]
  end

  subgraph BASE_C["📡 BASE SLAVE: ESP32-C6 DevKitC-1"]
    BC_UART["UART Inter-ESP<br/>RX: GPIO 22 • TX: GPIO 13"]
    BC_LORA["Rádio LoRa SX1278 433 MHz<br/>NSS:IO6 • SCK:IO5 • MOSI:IO7<br/>MISO:IO4 • DIO0:IO0"]
  end

  B_UART <-->|"115200 bps<br/>Comandos e Telemetria Frota"| BC_UART
  BC_LORA <-->|"Antena 433 MHz"| AIR(("🌊 Rovers 1..8"))
```

---

## 🚀 5. Compilação e Pacotes de Atualização OTA Dual

Com o **PlatformIO** e **Python 3** instalados:

```bash
# 1. Gerar apenas os ficheiros Web comprimidos (.gz) para Rover e Base:
python3 scripts/build_web_assets.py

# 2. Compilar e empacotar o Firmware Completo do ROVER (C6 + Lafvin + Web):
python3 scripts/bundle_firmware.py
# -> Gera: dist/rover_firmware.zip

# 3. Compilar e empacotar o Firmware Completo da BASE STATION (C6 + Lafvin + Web):
python3 scripts/bundle_base_firmware.py
# -> Gera: dist/base_firmware.zip
```

> [!TIP]
> Para documentação detalhada de validação elétrica de bancada, separação dos domínios `+3V3` / `+3V3_C6` e especificações da régua destacável do sensor Hall DRV5032, consulte o ficheiro [`HARDWARE_PINOUT_MEMORY.md`](HARDWARE_PINOUT_MEMORY.md).
