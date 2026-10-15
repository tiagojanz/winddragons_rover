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
| **Guincho (Winch)**| **GPIO 23** | **WIN Pin 3** (J_DK_R Pin 5 / J3-5) | **100% Validado em Bancada**: Controlo Web bidirecional (1800µs enrolar / 1200µs descer / 1500µs neutro). Espelhado também em GPIO 19. |
| **Rack Vela / Trinco**| **GPIO 21** | **RCK Pin 3** (J_DK_R Pin 7 / J3-7) | Sinal PWM (LEDC) Trinco da Âncora |
| **Leme (Rudder)** | **GPIO 15** | **RUD Pin 3** (J_DK_R Pin 4 / J3-4) | Sinal PWM (LEDC) Leme de Direção |
| **ESC Motor** | **GPIO 17** | **ESC Pin 3** (J_DK_R Pin 3 / J3-3) | Sinal PWM (LEDC) Propulsão Principal |
| **Sensor de Água**| GPIO 15 | J_WATER Pin 2 | Entrada analógica ADC |
| **Bateria Tensão** | GPIO 2 | J_BATT Pin 3 | Divisor de tensão ADC (Rácio 10.803) |
| **Bateria Corrente**| GPIO 3 | J_BATT Pin 2 | Shunt / Sensor Hall ADC |
| **GPS UART TX/RX**| GPIO 17 / 16 | J6 Header | Barramento NMEA 9600 baud |

---

## 4. Código das Cores dos Cabos (Bancada de Testes)

Durante os ensaios práticos com o firmware `src/test_uart/main_test_uart.cpp`:
- 🟢 **Verde:** DevKitC Pin 6 (GPIO 22 / RX) $\longleftarrow$ Lafvin Pad 4 (GPIO 2 / TX)
- 🟡 **Amarelo:** DevKitC Pin 13 (GPIO 13 / TX) $\longrightarrow$ Lafvin Pad 5 (GPIO 1 / RX)
- ⚫ **Preto:** GND Comum entre placas

---

## 5. Resultados de Validação em Hardware (15/10/2026)

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


