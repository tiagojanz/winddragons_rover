# Memory & Hardware Pinout Validation Reference
**WindDragons Autonomous Rover & Dual ESP32-C6 Carrier**
*Documento de Memória Permanente — Validação em Bancada Real (15/10/2026)*

---

## 1. Visão Geral da Arquitetura Dual-ESP32-C6

O sistema opera com dois microcontroladores **ESP32-C6** distribuídos entre tarefas de interface/armazenamento e controlo/telemetria:

- **Master Board (Painel Frontal):** Módulo comercial **Lafvin ESP32-C6 LCD 1.47" IPS** com leitor de cartão MicroSD (TF Card) integrado.
  - Responsabilidade: Ecrã ST7789, Leitura/Escrita no Cartão SD, Leitura do Joystick PS5 e Botões frontais, Interface de utilizador.
- **Slave Board (Painel Traseiro):** Placa **ESP32-C6 DevKitC-1**.
  - Responsabilidade: Rádio SX1278 LoRa 433 MHz, GPS, PWM para 4x Servos/ESC (Leme, Guincho, Vela/Rack, Motor), Sensores analógicos (Bateria, Corrente, Sensor de Água).
- **Link Inter-ESP:** UART de alta velocidade a **921600 baud** (comunicação bidirecional contínua com verificação CRC / Heartbeat).

---

## 2. Master: Lafvin ESP32-C6 (LCD 1.47" + TF MicroSD)

### 2.1. Conexões Internas do Módulo (Tabela Oficial de Fábrica Lafvin)

| GPIO | LCD ST7789 | SD Card | Header Físico | Periférico Onboard / Função |
| :--- | :--- | :--- | :--- | :--- |
| **IO0** | — | — | **Serigrafia 0** | **Livre** $\rightarrow$ Validado: Joystick Eixo Y (`/JOY_Y`) |
| **IO1** | — | — | **Serigrafia 1** | **Livre** $\rightarrow$ Validado: **Master UART TX** (`/INTER_TX_M2S`) |
| **IO2** | — | — | **Serigrafia 2** | **Livre** $\rightarrow$ Validado: **Master UART RX** (`/INTER_RX_S2M`) |
| **IO3** | — | — | **Serigrafia 3** | **Livre** $\rightarrow$ Validado: Joystick Eixo X (`/JOY_X`) |
| **IO4** | — | **SD CS** | **Serigrafia 4** | **BLOQUEADO:** Chip Select MicroSD (Pull-up interno) |
| **IO5** | — | **SD MISO**| **Serigrafia 5** | **BLOQUEADO:** Leitura MISO MicroSD |
| **IO6** | **LCD DIN** | **SD MOSI**| — | Linha de Dados SPI partilhada |
| **IO7** | **LCD CLK** | **SD SCLK**| — | Relógio SPI partilhado |
| **IO8** | — | — | — | LED RGB (WS2812 onboard) |
| **IO9** | — | — | — | Botão BOOT onboard |
| **IO12**| — | — | — | USB_N (USB D-) |
| **IO13**| — | — | — | USB_P (USB D+) |
| **IO14**| **LCD CS** | — | — | Chip Select do LCD |
| **IO15**| **LCD DC** | — | — | Data/Command do LCD |
| **TXD** (IO16) | — | — | — | UART0 Hardware padrão |
| **RXD** (IO17) | — | — | — | UART0 Hardware padrão |
| **IO18**..**IO20** | — | — | — | Internos não expostos no cabeçalho |
| **IO21**| **LCD RST**| — | — | Reset do LCD |
| **IO22**| **LCD BL** | — | — | Backlight PWM do LCD |
| **IO23**| — | — | — | Interno |
| **TF MicroSD** | SCLK | **GPIO 7** | Partilhado com LCD |

> [!IMPORTANT]
> **Descoberta Crítica de Hardware:**
> O **GPIO 4** do Lafvin está diretamente ligado à linha **CS do leitor MicroSD**. Não pode ser utilizado para UART nem para outro periférico quando o SD está inserido, pois entra em conflito com o bus SPI.
> O **GPIO 5** é o MISO do cartão SD e também não deve ser partilhado com sinais concorrentes.

---

### 2.2. Conector J1 (Barramento Frontal de 9 Pinos)
Ordem dos pinos no conector da PCB e correspondência com a serigrafia da placa Lafvin:

| Pino J1 | Serigrafia Lafvin | GPIO ESP32-C6 | Net na PCB | Função Validada em Teste Real |
| :---: | :---: | :---: | :---: | :--- |
| **Pad 1** | `5` | GPIO 5 | `/IO5_FREE` | MicroSD MISO (Reservado para o bus do cartão SD) |
| **Pad 2** | `4` | GPIO 4 | `/IO4_FREE` | MicroSD CS (Reservado para seleção do cartão SD) |
| **Pad 3** | `3` | GPIO 3 | `/JOY_X` | Joystick Eixo X (Leitura ADC Analógica) |
| **Pad 4** | `2` | GPIO 2 | `/INTER_TX_M2S` | **Master UART TX** $\longrightarrow$ (Envia para o Slave DevKitC) |
| **Pad 5** | `1` | GPIO 1 | `/INTER_RX_S2M` | **Master UART RX** $\longleftarrow$ (Recebe do Slave DevKitC) |
| **Pad 6** | `0` | GPIO 0 | `/JOY_Y` | Joystick Eixo Y (Leitura ADC Analógica) |
| **Pad 7** | `3V3` | +3.3V | `+3V3` | Alimentação lógica 3.3V |
| **Pad 8** | `GND` | GND | `GND` | Terra comum de referência |
| **Pad 9** | `5V` | +5V | `+5V` | Entrada de alimentação 5V |

---

## 3. Slave: ESP32-C6 DevKitC-1

### 3.1. Inter-ESP UART
| Sinal | GPIO DevKitC | Pino Cabeçalho | Ligação Física | Destino Master |
| :--- | :--- | :--- | :--- | :--- |
| **TX (S2M)** | **GPIO 13** | J_DK_R Pin 13 / J3 Pad 13 | Cabo Amarelo | Lafvin J1 Pad 5 (GPIO 1 - RX) |
| **RX (M2S)** | **GPIO 22** | J_DK_R Pin 6 / J3 Pad 6 | Cabo Verde | Lafvin J1 Pad 4 (GPIO 2 - TX) |
| **GND** | GND | J_DK_R Pin 14 / J1 Pad 15 | Cabo Preto | Lafvin J1 Pad 8 (GND) |

### 3.2. Módulo LoRa SX1278 (Ra-02 433 MHz)
Comunicação SPI independente no barramento secundário do DevKitC:
| Linha SX1278 | GPIO DevKitC | Cabeçalho PCB | Notas |
| :--- | :--- | :--- | :--- |
| **NSS / CS** | **GPIO 6** | J_DK_L Pin 5 / J3 Pin 4 | SPI Chip Select LoRa |
| **SCK** | **GPIO 5** | J_DK_L Pin 4 / J3 Pin 5 | SPI Clock |
| **MOSI** | **GPIO 7** | J_DK_L Pin 6 / J3 Pin 6 | SPI Master Out |
| **MISO** | **GPIO 4** | J_DK_L Pin 3 / J3 Pin 7 | SPI Master In |
| **DIO0** | **GPIO 0** | J_DK_L Pin 7 / J3 Pin 8 | Interrupção Packet RX |
| **RST** | **GPIO 1** | J_DK_L Pin 2 / J3 Pin 3 | Reset do rádio (opcional / NC) |

### 3.3. Servos e Sensores no DevKitC (Validação em Bancada Real)
| Periférico | GPIO DevKitC | Pino PCB / Cabeçalho | Função / Estado de Validação |
| :--- | :--- | :--- | :--- |
| **Guincho (Winch)**| **GPIO 23** | **WIN Pin 3** (X=11.08mm, Y=74.50mm)<br>J_DK_R Pin 5 / J3-5 | **100% Validado em Bancada**: Controlo Web bidirecional (1800µs enrolar / 1200µs descer / 1500µs neutro). |
| **Rack Vela / Trinco**| **GPIO 21** | **RCK Pin 3** (X=13.62mm, Y=74.50mm)<br>J_DK_R Pin 7 / J3-7 | **100% Validado em Bancada**: Sinal PWM (LEDC) Trinco da Âncora (0° engatado / 90° solto). |
| **Leme (Rudder)** | **GPIO 15** | **RUD Pin 3** (X=8.54mm, Y=74.50mm)<br>J_DK_R Pin 4 / J3-4 | **100% Validado em Bancada**: Sinal PWM (LEDC) Leme de Direção com controlo analógico proporcional (-100% a +100%). |
| **ESC Motor** | **GPIO 17** | **ESC Pin 3** (X=6.00mm, Y=74.50mm)<br>J_DK_R Pin 3 / J3-3 | Sinal PWM (LEDC) Propulsão Principal |
| **Fim de Curso Âncora** | **GPIO 18** | **J_LIMIT** (Pad 1: GND, Pad 2: GPIO 18)<br>X=21.24mm, Y=74.50mm | **100% Validado em Bancada Real (16/10/2026)**: Entrada digital `INPUT_PULLUP` (Microswitch fecha a GND no topo mecânico). Corte imediato de emergência do guincho por hardware/software. Conector de 2 pinos na PCB Dual_ESP_Carrier. |
| **LEDs WS2812B Strobe**| **GPIO 19** | **STRB** (Pad 1: GND, Pad 2: +5V_SERVO, Pad 3: DIN)<br>X=16.16mm, Y=74.50mm | **Integrado na PCB Dual_ESP_Carrier e Firmware**: Barramento WS2812B (8 LEDs RGB). Alimentado pelo barramento de 5V dos servos com linha de dados DIN direta. Conector padrão RC de 3 pinos. |
| **Contador Âncora (Hall)**| **GPIO 20** | **J_DK_R Pin 8** / DevKitC J3 Pin 8 | **100% Validado em Bancada Real (16/10/2026)**: Sensor de Efeito Hall Digital DRV5032 (Texas Instruments) para contagem de voltas do tambor da âncora. Modo `INPUT_PULLUP` com interrupção `FALLING`. Medição em tempo real de voltas, pulsos e profundidade do cabo solto em metros/cm com calibração interativa na Web e auto-zeroing no fim de curso superior. |
| **Sensor de Água**| GPIO 15 | **J_WATER** Pin 2 (X=23.78mm, Y=74.50mm, F.SilkS) | Entrada analógica ADC (Frente / F.SilkS) |
| **Bateria Tensão** | GPIO 2 | **J_BATT** Pin 3 (X=18.70mm, Y=74.50mm, F.SilkS) | Divisor de tensão ADC (Rácio 10.803, Frente / F.SilkS) |
| **Bateria Corrente**| GPIO 3 | **J_BATT** Pin 2 (X=18.70mm, Y=74.50mm, F.SilkS) | Shunt / Sensor Hall ADC (Frente / F.SilkS) |
| **GPS UART TX/RX**| GPIO 17 (RX) / 16 (TX) | J6 Header / J2 | **100% Validado em Bancada Real (16/10/2026)**: Módulo Quectel LC29H GNSS a 115200 baud. 14+ satélites sintonizados, FIX 3D imediato. R1 (RX Quectel) ligado ao TX (GPIO 16), T1 (TX Quectel) ligado ao RX (GPIO 17), PPS no GPIO 23. |

---

## 4. Código das Cores dos Cabos (Bancada de Testes)

Durante os ensaios práticos com o firmware `src/test_uart/main_test_uart.cpp`:
- 🟢 **Verde:** DevKitC Pin 6 (GPIO 22 / RX) $\longleftarrow$ Lafvin Pad 4 (GPIO 2 / TX)
- 🟡 **Amarelo:** DevKitC Pin 13 (GPIO 13 / TX) $\longrightarrow$ Lafvin Pad 5 (GPIO 1 / RX)
- ⚫ **Preto:** GND Comum entre placas

### 4.1. Ligação do Joystick Analógico (Direto na Placa Lafvin)
- **VCC:** Lafvin Pad 7 (`3V3` - Alimentação lógica estável)
- **GND:** Lafvin Pad 8 (`GND` - Terra de referência analógica)
- **VRX (Eixo X - Leme):** Lafvin Pad 3 (`GPIO 3` / ADC1_CH3)
- **VRY (Eixo Y - Motor):** Lafvin Pad 6 (`GPIO 0` / ADC1_CH0)
- **SW (Botão de Clique):** Não conectado (Navegação feita via botão BOOT onboard `GPIO 9`)

### 4.2. Ligação do Sensor de Efeito Hall DRV5032 (Contador do Tambor da Âncora)
- **Pino 1 (VCC):** DevKitC Pino 1 do conector J1 ou J3 (`3V3`)
- **Pino 2 (OUT):** DevKitC Pino 8 do conector J3 / `J_DK_R Pin 8` (`GPIO 20` - Entrada digital `INPUT_PULLUP` com ISR `FALLING`)
- **Pino 3 (GND):** DevKitC Pino 15 do conector J1 ou J3 (`GND`)

### 4.3. Ligação do Módulo GNSS / GPS Quectel LC29H
- **VCC:** `+3.3V` (Obrigatório 3.3V; não alimentar a 5V)
- **GND:** `GND` Comum de referência
- **R1 (RX1 Quectel):** Ligado ao **TX** do ESP32 (`GPIO 16` / J2 Pad 8)
- **T1 (TX1 Quectel):** Ligado ao **RX** do ESP32 (`GPIO 17` / J2 Pad 9)
- **PPS (Pulse Per Second):** Ligado ao `GPIO 23` / J2 Pad 5 (LED verde pisca a 1 Hz com FIX 3D)
- **Baudrate:** **115200 baud** (Protocolo NMEA 0183 com constelações GPS, GLONASS, Galileo, BeiDou)
- **Correção da PCB `Dual_ESP_Carrier.kicad_pcb` (16/10/2026):**
  - O layout original conectava o R1 do Quectel ao RX (GPIO 17) e o T1 ao TX (GPIO 16) de forma direta (não cruzada).
  - Pistas B.Cu corrigidas com roteamento paralelo sem cruzamento de planos:
    - `J6 Pad 4 (R1)` $\longleftrightarrow$ `J2 Pad 8 (GPIO 16 / TX)` (Net `/GPS_RX`)
    - `J6 Pad 5 (T1)` $\longleftrightarrow$ `J2 Pad 9 (GPIO 17 / RX)` (Net `/GPS_TX`)
    - `J6 Pad 1 (PPS)` $\longleftrightarrow$ `J2 Pad 5 (GPIO 23)` (Net `/GPS_PPS`)
  - Verificação DRC: **0 itens desconectados**, **0 erros de isolamento/cruzamento**.
  - Gerbers e renders 3D atualizados e re-empacotados em `Dual_ESP_Carrier_Gerber.zip`.

### 4.4. Separação dos Domínios de Alimentação 3.3V (Lafvin vs DevKitC C6)
Para evitar o paralelismo indesejado de reguladores LDO internos das duas placas (o que provocava sobreaquecimento e correntes reversas entre os reguladores ao partilhar o mesmo barramento 3.3V), a alimentação 3.3V foi segregada em dois domínios independentes na carrier board `Dual_ESP_Carrier.kicad_pcb`:
- **Domínio Lafvin (`/+3V3`):**
  - Alimentado exclusivamente pelo regulador LDO da placa Lafvin (`J1 Pad 7`).
  - Alimenta:
    - Módulo GNSS Quectel LC29H (`J6 Pad 7` / VCC)
    - Potenciómetros analógicos do Joystick PS5 (`JOY1 Pad 6` para Eixo X e `JOY1 Pad 3` para Eixo Y)
- **Domínio DevKitC C6 (`/+3V3_C6`):**
  - Alimentado exclusivamente pelo regulador LDO do ESP32-C6 DevKitC-1 (`J_DK_L Pad 1`).
  - Alimenta:
    - Módulo de Rádio LoRa SX1278 433 MHz (`J3 Pad 2` / VCC)
    - Pista dedicada em `F.Cu` com isolamento superior a 3.0 mm de qualquer outro sinal.
- **Terra Comum (`GND`):**
  - O plano de massa continua 100% partilhado entre ambas as placas, módulos de RF, GPS e sensores para garantir a integridade de sinal na UART (921600 baud) e nas leituras analógicas.
- **Alimentação 5V (`+5V`):**
  - Conector `J_PWR_ESP` distribui 5V da bateria/BEC a ambos os pinos `5V` das placas para que cada regulador LDO opere de forma limpa e autónoma.

---

## 5. Resultados de Validação em Hardware (15/10/2026 - 16/10/2026)

1. **LCD ST7789:** Inicializado a 80 MHz com rotação 3 (Landscape 320x172). Live update a 10 Hz sem artefactos.
2. **MicroSD Card:** Cartão 32GB FAT32 montado em concorrência no SPI partilhado (`PIN_SD_CS = 4`). Leitura e escrita periódica de ficheiros `/test_uart_sd.txt` e `/telemetry_test.log` com sucesso absoluto.
3. **LoRa SX1278:** Inicializado com registo versão `0x12` a 433 MHz. Modo RX contínuo sem qualquer colisão com os pinos de servos ou UART.
4. **Inter-ESP UART Link:** Taxa de transmissão testada com 100+ pacotes contínuos. **0% packet loss**, RTT médio de **34 ms** a 921600 baud.
5. **Power Module (Bateria 6S LiPo):** Validado em tempo real no DevKitC-1 (`PIN_ADC_VOLT = GPIO 2`, `PIN_ADC_CURR = GPIO 3`, `GND = J1-15`). Leitura estável de **23.50 V** (**3.92 V/célula**, 68% de carga LiPo 6S) e cancelamento de offset a vazio (**0.00 A** em repouso). Rácio de calibração: `10.803`.
6. **Cadeia Completa de 3 Nós (Lafvin Master $\leftrightarrow$ C6 #1 Local $\leftrightarrow$ C6 #2 Remote via LoRa 433 MHz):**
   - Transmissão bidirecional pelo ar (LoRa 433 MHz, BW 250 kHz, SF 7, CR 4/7) com **0% de perda de pacotes**.
   - Nível de sinal RF registado: **RSSI = -47 dBm a -49 dBm**, **SNR = +9.5 dB a +12.0 dB** (excelente margem de ruído).
   - Latência RTT completa da cadeia (Lafvin $\rightarrow$ UART $\rightarrow$ C6 #1 $\rightarrow$ RF $\rightarrow$ C6 #2 $\rightarrow$ RF $\rightarrow$ C6 #1 $\rightarrow$ UART $\rightarrow$ Lafvin): **36 ms a 46 ms**.
   - Telemetria integral (tensão 6S, corrente, percentagem, estado LoRa, RSSI, SNR, RTT) exibida graficamente no IPS ST7789 e gravada autonomamente no MicroSD em `/telemetry_test.log`.
7. **Joystick Analógico e Controlo Manual em Tempo Real (16/10/2026):**
   - **100% Validado em Bancada Real:** Resposta instantânea, suave e sem perdas de comando.
   - **Auto-calibração no Arranque:** Leitura dinâmica das primeiras 30 amostras em repouso eliminando qualquer offset de centro dos potenciómetros (repouso calibrado a exatamente 0% de Leme e 0% de Motor).
   - **Mapeamento Adaptativo de Pico:** O algoritmo ajusta dinamicamente a escala dos limites mecânicos (~1450 pontos a partir do centro). Atinge **-100%** a Bombordo, **0%** ao Centro e **+100%** a Estibordo aproveitando 100% do curso físico do manípulo.
   - **Watchdog / Failsafe Protegido:** O PING periódico da UART atua como heartbeat ativo entre as placas, mantendo o modo manual estável e evitando falsos disparos de *failsafe*.
   - **Interface Web Sincronizada:** O endpoint `/control` reflete a posição dos sliders e percentagens a 300ms com proteção contra conflitos de toque manual.
8. **Servo da Cremalheira / Trinco da Âncora (GPIO 21) (16/10/2026):**
   - **100% Validado em Bancada Real:** Atuação angular precisa (0° travado / 90° livre para queda livre por gravidade) através dos comandos do firmware e Web.
9. **Fim de Curso da Âncora (GPIO 18 / J3-10 no DevKitC):**
   - **100% Validado em Bancada Real (16/10/2026):** Corte imediato e bloqueio de tração de subida verificado em hardware com o microswitch físico.
   - **Configuração:** `PIN_ANCHOR_DETECTOR = 18`, modo `INPUT_PULLUP`.
   - **Comportamento Elétrico:** Microswitch mecânico NA (Normalmente Aberto) liga entre GPIO 18 e GND. Quando a âncora atinge o batente superior, o contacto fecha ao GND (`digitalRead == LOW`).
   - **Proteção Ativa:** Qualquer ordem de subida (`1800 µs` ou recolha automática) é imediatamente cortada e bloqueada enquanto o fim de curso estiver acionado, marcando o estado como `ANCHOR_RETRACTED` e profundidade 0 cm.
   - **Diagnóstico Serial:** Comando `LIMIT` ou `ANCHOR` na consola do C6 devolve em tempo real o estado lógico e elétrico do pino.
10. **LEDs WS2812B Strobe Náutico (GPIO 19 / J3-9 no DevKitC):**
    - **Configuração:** `PIN_STROBE_LEDS = 19`, `NUM_STROBE_LEDS = 8`, protocolo NeoPixel 800 kHz.
    - **Modo Alarme / Emergência (Web Button ou CMD:ALARM:10):** Estroboscópio náutico de alta intensidade (Branco puro `255, 255, 255`) em padrão duplo flash a cada 600 ms (40ms ON / 60ms OFF / 40ms ON / 460ms OFF).
    - **Modo Failsafe:** Aviso intermitente âmbar/laranja (`255, 60, 0`) a 2 Hz (250ms ON / 250ms OFF) quando ocorre perda de comunicação em modo manual.
    - **Diagnóstico Serial:** Comandos `ALARM 10` (liga por 10s) e `ALARM 0` (desliga) na consola do C6.
11. **Contador de Cabo da Âncora - Sensor Hall DRV5032 (GPIO 20 / J3-8 no DevKitC):**
    - **100% Validado em Bancada Real (16/10/2026):** Deteção precisa de rotações do tambor da âncora através da passagem de íman permanente pelo sensor digital Hall DRV5032 da Texas Instruments.
    - **Configuração:** `PIN_ANCHOR_HALL = 20`, modo `INPUT_PULLUP`, interrupção `FALLING` com filtro debounce de 10 ms por hardware/software.
    - **Cálculo Dinâmico de Cabo Solto (Profundidade):**
      $$\Delta_{\text{cabo}} = \frac{\pi \times \text{Diâmetro}_{\text{tambor}}}{\text{Pulsos/Volta}}$$
      Com o tambor padrão de 30.0 mm e 1 íman, resolução = **9.42 cm/pulso**.
    - **Deteção de Sentido e Auto-Zeroing:**
      - Quando o guincho está em recolha (`> 1500 µs`), os pulsos diminuem a profundidade até `0.0 m`.
      - Em queda livre ou rotação para descer (`<= 1500 µs`), os pulsos aumentam a profundidade.
      - Quando o fim de curso (`GPIO 18`) fecha no topo ou ao pressionar "Zerar Contador" na Web, a profundidade é reinicializada a zero.
    - **Interface Web Sincronizada:** Exibição contínua a 300 ms da profundidade em metros e centímetros, rotações, pulsos e formulário de calibração que grava na NVS e no `/config.json` do MicroSD.
12. **Módulo GNSS / GPS Quectel LC29H (16/10/2026):**
    - **100% Validado em Bancada Real:** Aquisição e descodificação de tramas NMEA em tempo real a **115200 baud** (`RX = GPIO 17`, `TX = GPIO 16`, `PPS = GPIO 23`).
    - **Desempenho Extraordinário:** **14 a 15 satélites sintonizados** em simultâneo (constelações GPS + GLONASS + Galileo + BeiDou) com **FIX 3D** ativo e contínuo.
    - **Cadeia de Propagação Integrada e Confirmada:**
      1. Descodificação direta via `TinyGPSPlus` no Lafvin Master (`gps.update()`).
      2. Transmissão imediata ao Slave C6 via pacote `PING` por UART a 921600 baud (`PING:...:lat7:lng7:spd:head:sats:fix`).
      3. Propagação pelo rádio LoRa 433 MHz na telemetria de voo/navegação.
      4. Apresentação em tempo real no Ecrã IPS ST7789, na página Web `/gps` com mapa dinâmico e na API `/api/status`.
