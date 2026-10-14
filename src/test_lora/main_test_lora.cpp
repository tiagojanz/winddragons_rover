#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include <esp_mac.h>

// =========================================================================
// DEFINIÇÃO DE PINOS - ESP32-C6-DevKitC-1 V1.2 para SX1278 (Ra-02 433MHz)
// =========================================================================
// Header J1 (Lado Esquerdo do DevKitC-1 no Carrier PCB):
#define LORA_DIO0   0   // J1 Pin 7  (GPIO0 -> LoRa DIO0 - pino 8)
#define LORA_MOSI   7   // J1 Pin 6  (GPIO7 -> LoRa MOSI - pino 6)
#define LORA_NSS    6   // J1 Pin 5  (GPIO6 -> LoRa NSS  - pino 4)
#define LORA_MISO   4   // J1 Pin 3  (GPIO4 -> LoRa MISO - pino 7)
#define LORA_SCK    5   // J1 Pin 4  (GPIO5 -> LoRa SCK  - pino 5)
#define LORA_RST    RADIOLIB_NC // J1 Pin 2 (EN/RST de hardware do ESP32-C6)
// J1 Pin 1: 3V3         // J1 Pin 1  (3V3 do LoRa - pino 2)
// J1 Pin 15: GND        // J1 Pin 15 (GND do LoRa - pino 1)

#define RGB_LED_PIN 8   // LED RGB WS2812 onboard do ESP32-C6 DevKitC-1

// Barramento SPI de hardware FSPI
SPIClass spiBus(FSPI);

// Instância do SX1278 no RadioLib
SX1278 radio = new Module(LORA_NSS, LORA_DIO0, LORA_RST, RADIOLIB_NC, spiBus);

// Nome único da placa gerado pelo MAC
char boardName[16] = "ESP-C6";

// Flag de interrupção do DIO0
volatile bool packetReceived = false;

#if defined(ESP8266) || defined(ESP32)
  ICACHE_RAM_ATTR
#endif
void onDio0() {
    packetReceived = true;
}

// Função auxiliar para imprimir no USB nativo (Serial) e UART CP2102 (Serial0)
void printLog(const char* format, ...) {
    char buf[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    Serial.print(buf);
    Serial0.print(buf);
}

void setRgb(uint8_t r, uint8_t g, uint8_t b) {
#ifdef RGB_BUILTIN
    rgbLedWrite(RGB_LED_PIN, r, g, b);
#else
    neopixelWrite(RGB_LED_PIN, r, g, b);
#endif
}

// Leitura direta de registo SPI sem passar pelo RadioLib (para diagnóstico)
uint8_t readRawRegister(uint8_t reg) {
    digitalWrite(LORA_NSS, LOW);
    spiBus.transfer(reg & 0x7F); // MSB=0 para leitura
    uint8_t val = spiBus.transfer(0x00);
    digitalWrite(LORA_NSS, HIGH);
    return val;
}

// Escrita direta de registo SPI
void writeRawRegister(uint8_t reg, uint8_t val) {
    digitalWrite(LORA_NSS, LOW);
    spiBus.transfer(reg | 0x80); // MSB=1 para escrita
    spiBus.transfer(val);
    digitalWrite(LORA_NSS, HIGH);
}

bool loraReady = false;
uint32_t pingCounter = 0;
uint32_t lastPingSentTime = 0;
uint32_t lastActionTime = 0;
uint32_t nextAutoPingDelay = 5000;

void sendPing();

void setup() {
    Serial.begin(115200);
    Serial0.begin(115200);

    // Identificador único da placa baseado nos últimos 2 bytes do MAC
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(boardName, sizeof(boardName), "C6_%02X%02X", mac[4], mac[5]);

    // Amarelo suave: a inicializar
    setRgb(30, 20, 0);

    delay(1500); // Aguarda estabilização do monitor série USB

    printLog("\n\n=======================================================\n");
    printLog("  TESTE SX1278 (Ra-02 433MHz) - PING/PONG BIDIRECIONAL \n");
    printLog("  Identificador desta Placa: [%s]                      \n", boardName);
    printLog("=======================================================\n");
    printLog("PINOS: SCK=GPIO%d, MISO=GPIO%d, MOSI=GPIO%d, CS=GPIO%d, DIO0=GPIO%d\n",
             LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS, LORA_DIO0);
    printLog("-------------------------------------------------------\n");

    pinMode(LORA_NSS, OUTPUT);
    digitalWrite(LORA_NSS, HIGH);

    // Iniciar barramento SPI de hardware (FSPI)
    spiBus.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);

    // Teste SPI Bruto: Leitura do RegVersion (0x42)
    uint8_t regVersion = readRawRegister(0x42);
    printLog("[SPI] RegVersion: 0x%02X (Esperado: 0x12) -> %s\n", 
             regVersion, (regVersion == 0x12) ? "OK!" : "AVISO: FALHA DE COMUNICAÇÃO!");

    // Inicialização pelo RadioLib (433.0 MHz, BW 250kHz, SF7, CR 4/5, SyncWord 0x12, Potência 17dBm)
    printLog("[LORA] A inicializar rádio SX1278 a 433.0 MHz...\n");
    int state = radio.begin(433.0, 250.0, 7, 5, 0x12, 17, 8);

    if (state == RADIOLIB_ERR_NONE) {
        printLog("\n=======================================================\n");
        printLog("  *** LORA PRONTO! A ESCUTAR POR PACOTES (RX)...       \n");
        printLog("  -> Pressiona ENTER na consola para enviar PING manual\n");
        printLog("=======================================================\n");
        loraReady = true;

        // Configurar ação de interrupção no DIO0
        radio.setDio0Action(onDio0, RISING);

        // Colocar imediatamente em modo de escuta contínua (RX)
        radio.startReceive();

        // Verde: Pronto para comunicar!
        setRgb(0, 30, 0);
        lastActionTime = millis();
        // Jitter aleatório para a primeira transmissão não coincidir
        nextAutoPingDelay = 4000 + ((mac[5] * 37) % 3000);
    } else {
        printLog("[ERRO] Falha ao inicializar RadioLib! Código: %d\n", state);
        setRgb(40, 0, 0); // Vermelho
    }
}

void sendPing() {
    if (!loraReady) return;

    pingCounter++;
    char payload[64];
    snprintf(payload, sizeof(payload), "PING#%lu from %s", pingCounter, boardName);

    printLog("\n[TX PING] A transmitir: \"%s\"...", payload);
    setRgb(0, 0, 50); // Azul durante transmissão

    radio.standby();
    uint32_t t0 = millis();
    int state = radio.transmit(payload);
    uint32_t dt = millis() - t0;

    if (state == RADIOLIB_ERR_NONE) {
        printLog(" Enviado OK! (%lu ms)\n", dt);
        lastPingSentTime = millis();
    } else {
        printLog(" Falha no envio! Código: %d\n", state);
    }

    // Voltar a escutar (RX)
    packetReceived = false;
    radio.startReceive();
    setRgb(0, 25, 0); // Verde
    lastActionTime = millis();
    nextAutoPingDelay = 5000 + random(0, 2500);
}

void sendPong(const char* targetBoard, uint32_t origCounter) {
    if (!loraReady) return;

    char payload[64];
    snprintf(payload, sizeof(payload), "PONG#%lu from %s to %s", origCounter, boardName, targetBoard);

    printLog("[TX PONG] A responder: \"%s\"...", payload);
    setRgb(50, 25, 0); // Laranja durante envio de resposta

    radio.standby();
    uint32_t t0 = millis();
    int state = radio.transmit(payload);
    uint32_t dt = millis() - t0;

    if (state == RADIOLIB_ERR_NONE) {
        printLog(" Enviado OK! (%lu ms)\n", dt);
    } else {
        printLog(" Falha no envio! Código: %d\n", state);
    }

    // Voltar a escutar (RX)
    packetReceived = false;
    radio.startReceive();
    setRgb(0, 25, 0); // Verde
    lastActionTime = millis();
}

void loop() {
    if (!loraReady) {
        delay(2000);
        uint8_t v = readRawRegister(0x42);
        printLog("[DIAGNÓSTICO] RegVersion: 0x%02X (esperado 0x12)\n", v);
        return;
    }

    // 1. Comando manual via monitor série (tecla ENTER ou qualquer caractere envia PING)
    if (Serial.available() > 0 || Serial0.available() > 0) {
        while (Serial.available()) Serial.read();
        while (Serial0.available()) Serial.read();
        printLog("[TECLADO] Comando manual recebido!");
        sendPing();
        return;
    }

    // 2. Verificar se chegou um novo pacote LoRa (via interrupção DIO0 ou RegIrqFlags RxDone)
    bool hasPacket = packetReceived;
    if (!hasPacket) {
        // Fallback: verificar registo RegIrqFlags (0x12) bit 6 (RxDone = 0x40)
        uint8_t irq = readRawRegister(0x12);
        if (irq & 0x40) {
            hasPacket = true;
        }
    }

    if (hasPacket) {
        packetReceived = false;
        String str;
        int state = radio.readData(str);

        if (state == RADIOLIB_ERR_NONE && str.length() > 0) {
            float rssi = radio.getRSSI();
            float snr  = radio.getSNR();

            // Piscar Ciano em receção com sucesso
            setRgb(0, 50, 50);

            printLog("\n[RX] Recebido (%d bytes): \"%s\"\n", str.length(), str.c_str());
            printLog("     -> RSSI: %.1f dBm | SNR: %.2f dB\n", rssi, snr);

            // Verificar se é um PING
            if (str.startsWith("PING#")) {
                uint32_t num = 0;
                char sender[32] = "Desconhecido";
                sscanf(str.c_str(), "PING#%lu from %s", &num, sender);

                // Responder com PONG após pequena pausa (100ms)
                delay(100);
                sendPong(sender, num);
            }
            // Verificar se é um PONG
            else if (str.startsWith("PONG#")) {
                uint32_t rtt = (lastPingSentTime > 0) ? (millis() - lastPingSentTime) : 0;
                printLog("     >>> ★★★ PONG RECEBIDO COM SUCESSO! RTT: %lu ms ★★★ <<<\n", rtt);
                setRgb(0, 50, 0); // Verde
            }
        }

        // Garantir que o rádio volta ao modo de receção contínua (RX)
        packetReceived = false;
        radio.startReceive();
        lastActionTime = millis();
    }

    // 3. Auto-Ping periódico se a ligação estiver silenciosa há mais de nextAutoPingDelay
    if (millis() - lastActionTime >= nextAutoPingDelay) {
        sendPing();
    }

    delay(10);
}
