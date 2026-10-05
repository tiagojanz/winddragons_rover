# WindDragons - Sistema de Telemetria e Controlo Náutico (Base & Rover)

Projeto de automação e telemetria para boias robóticas náuticas (**Rovers**) e estação de controlo com ecrã (**Base**) baseado na placa **Lafvin ESP32-C6 com LCD 1.47" (ST7789)**.

---

## 🛰️ Arquitetura do Sistema

```
[ WindDragons Portal (Cloud) ]
        ▲
        │ HTTPS REST API (JSON)
        ▼
   [ BASE STATION (ESP32-C6) ]  <--- Joystick analógico (4 vias) + Ecrã LCD 1.47"
        ▲                      <--- Botão BOOT: Cicla entre ROVER-01 .. ROVER-08
        │ LoRa SX1278 (433MHz)
        ▼
 ┌──────────────────────┬──────────────────────┐
 │                      │                      │
 ▼                      ▼                      ▼
[ ROVER-01 (Boia) ]   [ ROVER-02 (Boia) ]    [ ROVER-03..08 ]
 - ESC Motor            - ESC Motor            - ...
 - Servo Leme           - Servo Leme
 - Guincho Âncora       - Guincho Âncora
 - GPS LC29H            - GPS LC29H
```

---

## 🔌 Esquema de Ligações (Pinout)

Ambas as placas usam a mesma pinagem para o módulo **LoRa Ra-02 (SX1278 a 433MHz)** no barramento SPI:

### Módulo LoRa SX1278 (Ra-02 433MHz) — Em ambas as placas
| Pino Ra-02 | Pino ESP32-C6 | Notas |
| :--- | :--- | :--- |
| **VCC** | `3.3V` | Nunca ligar a 5V |
| **GND** | `GND` | Terra comum |
| **SCK** | `IO7` | Barramento SPI (partilhado) |
| **MOSI**| `IO6` | Barramento SPI (partilhado) |
| **MISO**| `IO5` | Barramento SPI |
| **NSS / CS** | `IO4` | Chip Select LoRa |
| **RST** | `IO2` | Reset do rádio |
| **DIO0**| `IO20`| Interrupção de receção de pacotes |

---

### Build 1: ROVER (A Boia)
| Componente | Pino do Módulo / Atuador | Pino ESP32-C6 | Função |
| :--- | :--- | :--- | :--- |
| **Motor (ESC)** | Sinal PWM | `IO0` | 50Hz PWM (1000µs a 2000µs, centro 1500µs) |
| **Servo do Leme** | Sinal PWM | `IO1` | 50Hz PWM (Ângulo de direção) |
| **Guincho Âncora** | Sinal PWM | `IO23` | Servo de rotação contínua (1500µs parado, <1500 desce, >1500 sobe) |
| **GPS Quectel LC29H** | TX do GPS | `IO19` | RX UART1 (115200 bps) |
| | RX do GPS | `IO18` | TX UART1 |
| | VCC / GND | `3.3V / GND` | Alimentação |
| **LED RGB Onboard** | Integrado | `IO8` | Estado: Verde (GPS OK), Azul (LoRa), Vermelho (Failsafe) |

---

### Build 2: BASE (Comando Remoto com Ecrã)
| Componente | Pino do Módulo | Pino ESP32-C6 | Função |
| :--- | :--- | :--- | :--- |
| **Joystick (4 vias)** | VCC | `3.3V` | Alimentação do joystick |
| | GND | `GND` | Terra |
| | VRx (Avanço/Recuo) | `IO0` | Entrada Analógica ADC1_CH0 (Aceleração) |
| | VRy (Direção) | `IO1` | Entrada Analógica ADC1_CH1 (Leme) |
| **Botões de Âncora** | Subir Âncora | `IO18` | Entrada Digital (Pull-up interno) |
| | Descer Âncora | `IO19` | Entrada Digital (Pull-up interno) |
| **Botão BOOT** | Onboard | `IO9` | Cicla entre ROVER-01 a ROVER-08 |
| **Ecrã LCD 1.47"** | Integrado ST7789 | `IO14, 15, 21, 22` | Dashboard completo de telemetria e controlo |

---

## ⚙️ Configuração da Rede e Nuvem

Edite o ficheiro [`include/network_config.h`](file:///Users/tiagotorredovale/Documents/projectos/winddragon/rover/include/network_config.h):

```cpp
#define WIFI_SSID             "O_SEU_WIFI"
#define WIFI_PASSWORD         "A_SUA_PASSWORD"
#define BASE_STATION_ID       "BASE-ALCOCHETE-01"
#define WINDDRAGONS_API_URL   "https://winddragons.app/api/circuits/station/telemetry"
```

---

## 🚀 Como Compilar e Enviar o Firmware

Com o **PlatformIO** instalado:

### 1. Compilar e Enviar para o Rover:
Para alterar o identificador da boia (por ex. `ROVER-01`, `ROVER-02`), basta definir `-DROVER_ID=2` no build flag ou no topo do código.

```bash
# Compilar Rover
pio run -e rover

# Enviar via USB para o Rover
pio run -e rover -t upload

# Monitor Série (115200 baud)
pio device monitor -e rover
```

### 2. Compilar e Enviar para a Base:
```bash
# Compilar Base
pio run -e base

# Enviar via USB para a Base
pio run -e base -t upload

# Monitor Série (115200 baud)
pio device monitor -e base
```

---

## 📡 Integração com a API WindDragons

A cada 3 segundos, a Base agrupa os rovers ativos e envia um pedido `POST` para `https://winddragons.app/api/circuits/station/telemetry`:

```json
{
  "station_id": "BASE-ALCOCHETE-01",
  "rovers": [
    {
      "rover_code": "ROVER-01",
      "lat": 38.750508,
      "lng": -8.973995,
      "speed": 0.2,
      "heading": 185.0,
      "battery_pct": 92,
      "battery_voltage": 12.5,
      "motor_status": "holding_station",
      "anchor_status": "retracted",
      "anchor_depth_m": 0.0,
      "rssi": -65
    }
  ]
}
```

Caso o servidor responda com comandos pendentes (ex: `DROP_ANCHOR`, `RETRACT_ANCHOR`, `HOLD_STATION`), a Base coloca o comando na fila e transmite-o imediatamente via rádio LoRa para o rover visado!
