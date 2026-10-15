#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include <esp_mac.h>
#include <FS.h>
#include <SD.h>
#include <Arduino_GFX_Library.h>
#include <canvas/Arduino_Canvas.h>

#include "config_common.h"
#include "Display_ST7789.h"
#include "sd_card_manager.h"

// =========================================================================
// TESTE COMPLETO EM CADEIA DE 3 NÓS (Ponta a Ponta):
//
// [Lafvin Master (LCD+SD)] <---UART---> [C6 #1 Local] <---RF 433MHz---> [C6 #2 Remoto]
//
// 1. Lafvin Master: Display ST7789, MicroSD 30GB, UART TX(GPIO 2), RX(GPIO 1)
// 2. C6 #1 Local  : UART TX(GPIO 13), RX(GPIO 22), LoRa SX1278 #1, Power Module 6S
// 3. C6 #2 Remoto : LoRa SX1278 #2 (Nó autónomo sem display)
// =========================================================================

#define RGB_LED_PIN 8

// --- Pinos LoRa SX1278 no ESP32-C6 (Padrão para C6 #1 e C6 #2) ---
#define LORA_DIO0   0
#define LORA_MOSI   7
#define LORA_NSS    6
#define LORA_MISO   4
#define LORA_SCK    5
#define LORA_RST    RADIOLIB_NC

// LoRa Hardware (FSPI)
SPIClass loraSpiBus(FSPI);
SX1278 radio = new Module(LORA_NSS, LORA_DIO0, LORA_RST, RADIOLIB_NC, loraSpiBus);
bool loraInitialized = false;
uint8_t loraVersion = 0x00;

// LCD & SD Hardware (Apenas Lafvin Master)
ST7789Display st7789;
Arduino_Canvas* canvas = nullptr;
SDCardManager sdCard;
bool sdTested = false;
bool sdOk = false;
char sdStatusStr[64] = "A aguardar...";

// UART Inter-ESP (Lafvin <-> C6 #1)
HardwareSerial interSerial(1);
int txPin = -1;
int rxPin = -1;

// --- Pinos Power Module no C6 #1 (Bateria 6S LiPo) ---
#define PIN_ADC_VOLT    2   // GPIO 2
#define PIN_ADC_CURR    3   // GPIO 3
const float VOLT_DIVIDER_RATIO = 10.803f;
const float AMPS_PER_VOLT      = 27.32f;
const float CURR_OFFSET_MV     = 6.5f;

float liveBatVolts = 0.0f;
float liveBatAmps  = 0.0f;
int   liveBatPct   = 0;

// Estado e Métricas de Rede
enum NodeRole {
    ROLE_MASTER_LAFVIN = 1,
    ROLE_SLAVE_C6_LOCAL = 2,
    ROLE_REMOTE_C6 = 3
};
NodeRole currentRole = ROLE_SLAVE_C6_LOCAL;
char boardRole[36] = "";

uint32_t txCount = 0;
uint32_t rxCount = 0;
uint32_t lastTxTime = 0;
uint32_t lastLcdUpdateTime = 0;
uint32_t lastSdAccessTime = 0;
uint32_t lastRtt = 0;

char lastLoraStatus[32] = "A aguardar...";
float lastRssi = 0.0f;
float lastSnr = 0.0f;
uint32_t rfPacketsTx = 0;
uint32_t rfPacketsRx = 0;

void setRgb(uint8_t r, uint8_t g, uint8_t b) {
#ifdef RGB_BUILTIN
    rgbLedWrite(RGB_BUILTIN, r, g, b);
#else
    neopixelWrite(RGB_LED_PIN, r, g, b);
#endif
}

void printLog(const char* format, ...) {
    char buf[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    Serial.print(buf);
    Serial0.print(buf);
}

uint8_t readRawRegister(uint8_t reg) {
    digitalWrite(LORA_NSS, LOW);
    loraSpiBus.transfer(reg & 0x7F);
    uint8_t val = loraSpiBus.transfer(0x00);
    digitalWrite(LORA_NSS, HIGH);
    return val;
}

void measureBattery() {
    uint32_t sumV = 0, sumI = 0;
    for (int i = 0; i < 15; i++) {
        sumV += analogReadMilliVolts(PIN_ADC_VOLT);
        sumI += analogReadMilliVolts(PIN_ADC_CURR);
        delayMicroseconds(100);
    }
    float mvV = (float)sumV / 15.0f;
    float mvI = (float)sumI / 15.0f;
    liveBatVolts = (mvV * VOLT_DIVIDER_RATIO) / 1000.0f;
    if (mvI > CURR_OFFSET_MV) {
        liveBatAmps = ((mvI - CURR_OFFSET_MV) / 1000.0f) * AMPS_PER_VOLT;
    } else {
        liveBatAmps = 0.0f;
    }
    if (liveBatVolts >= 25.2f) liveBatPct = 100;
    else if (liveBatVolts <= 19.8f) liveBatPct = 0;
    else liveBatPct = (int)(((liveBatVolts - 19.8f) / (25.2f - 19.8f)) * 100.0f);
}

void renderLafvinLcd() {
    if (!canvas) return;

    canvas->fillScreen(0x0000); // Preto

    // Barra de Título
    canvas->fillRect(0, 0, 320, 24, 0x0010); // Azul Escuro
    canvas->setTextColor(0xFFFF); // Branco
    canvas->setTextSize(1);
    canvas->setCursor(8, 7);
    canvas->print("WINDDRAGONS: TESTE INTEGRAL 3 NOS");

    canvas->drawFastHLine(0, 25, 320, 0x07E0); // Linha Verde

    // Secção 1: MicroSD
    canvas->setTextColor(0x07FF); // Cyan
    canvas->setCursor(8, 32);
    canvas->print("[SD CARD]");
    if (sdOk) {
        canvas->setTextColor(0x07E0); // Verde
        canvas->setCursor(75, 32);
        canvas->printf("%s", sdStatusStr);
    } else {
        canvas->setTextColor(0xF800); // Vermelho
        canvas->setCursor(75, 32);
        canvas->printf("%s", sdStatusStr);
    }

    // Secção 2: UART Inter-ESP (Lafvin <-> C6 #1)
    canvas->setTextColor(0x07FF); // Cyan
    canvas->setCursor(8, 48);
    canvas->print("[UART]");
    canvas->setTextColor(0xFFFF);
    canvas->setCursor(75, 48);
    canvas->printf("TX=IO%d, RX=IO%d | Sent:%u Recv:%u", txPin, rxPin, txCount, rxCount);

    canvas->setCursor(75, 60);
    if (lastRtt > 0) {
        canvas->setTextColor(0x07E0); // Verde
        canvas->printf("Latency RTT: %u ms (UART 100%% OK)", lastRtt);
    } else {
        canvas->setTextColor(0xFFE0); // Amarelo
        canvas->print("Latency: A aguardar PONG...");
    }

    // Secção 3: LoRa RF 433MHz (C6 #1 <---> C6 #2)
    canvas->setTextColor(0xFD20); // Laranja
    canvas->setCursor(8, 76);
    canvas->print("[LORA RF]");
    if (rfPacketsRx > 0 || strstr(lastLoraStatus, "OK") != nullptr) {
        canvas->setTextColor(0x07E0); // Verde
        canvas->setCursor(75, 76);
        canvas->printf("Status: %s (RF LINK OK)", lastLoraStatus);
        canvas->setTextColor(0xFFFF);
        canvas->setCursor(75, 88);
        canvas->printf("RSSI: %5.1f dBm  |  SNR: %4.1f dB", lastRssi, lastSnr);
    } else {
        canvas->setTextColor(0xFFE0); // Amarelo
        canvas->setCursor(75, 76);
        canvas->printf("Status: %s", lastLoraStatus);
        canvas->setTextColor(0xC618);
        canvas->setCursor(75, 88);
        canvas->print("A escutar transmissao do C6 #2...");
    }

    // Secção 4: Bateria 6S LiPo (Power Module)
    canvas->setTextColor(0xFFE0); // Amarelo
    canvas->setCursor(8, 106);
    canvas->print("[BATT 6S]");
    if (liveBatVolts > 5.0f) {
        canvas->setTextColor(0x07E0); // Verde
        canvas->setCursor(75, 106);
        canvas->printf("%5.2fV (%3d%%) | %4.2fA | %4.1fW", liveBatVolts, liveBatPct, liveBatAmps, (liveBatVolts * liveBatAmps));
        canvas->setTextColor(0xC618);
        canvas->setCursor(75, 118);
        canvas->printf("%4.2f V/celula (Curva LiPo calibrada)", liveBatVolts / 6.0f);
    } else {
        canvas->setTextColor(0xC618);
        canvas->setCursor(75, 106);
        canvas->print("A aguardar telemetria...");
    }

    // Barra de rodapé
    canvas->drawFastHLine(0, 144, 320, 0x4208); // Dark Grey
    canvas->setTextColor(0xC618);
    canvas->setCursor(8, 152);
    canvas->printf("Uptime: %us | SD=IO4 | TX=IO%d, RX=IO%d | 433MHz", millis() / 1000, txPin, rxPin);

    // Enviar buffer para o controlador ST7789
    st7789.drawPixelBuffer(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, canvas->getFramebuffer());
}

void testMicroSd() {
    printLog("\n[SD] =======================================================\n");
    printLog("[SD] A INICIALIZAR LEITOR MICROSD (TF CARD)...\n");
    printLog("[SD] Pinos: CS=GPIO%d, MISO=GPIO%d, MOSI=GPIO%d, SCLK=GPIO%d\n",
             PIN_SD_CS, PIN_SD_MISO, PIN_LCD_MOSI, PIN_LCD_SCLK);

    sdOk = sdCard.begin();
    if (sdOk) {
        uint64_t totalBytes = sdCard.getTotalBytes();
        uint64_t usedBytes  = sdCard.getUsedBytes();
        uint32_t totalMB    = (uint32_t)(totalBytes / (1024 * 1024));
        uint32_t usedMB     = (uint32_t)(usedBytes / (1024 * 1024));
        uint32_t freeMB     = totalMB > usedMB ? totalMB - usedMB : 0;

        snprintf(sdStatusStr, sizeof(sdStatusStr), "OK: %u MB (%u MB Livres)", totalMB, freeMB);
        printLog("[SD] ★★★ CARTAO MICROSD DETETADO COM SUCESSO! ★★★\n");

        String testPath = "/test_chain_sd.txt";
        String testMsg  = "WINDDRAGONS_CHAIN_TEST_OK_" + String(millis());
        if (sdCard.writeFile(testPath.c_str(), testMsg.c_str())) {
            String readBack = sdCard.readFile(testPath.c_str());
            readBack.trim();
            if (readBack == testMsg) {
                printLog("[SD] ★★★ LEITURA E ESCRITA NO SD 100%% VERIFICADAS! ★★★\n");
            }
        }
    } else {
        snprintf(sdStatusStr, sizeof(sdStatusStr), "FALHA: Nao Detetado");
        printLog("[SD] FALHA: Nenhum cartao detetado ou conflito no barramento.\n");
    }
    printLog("[SD] =======================================================\n\n");
}

void setup() {
    Serial.begin(115200);
    Serial0.begin(115200);
    delay(1000);

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    // Identificação de papel por endereço MAC:
    if (mac[0] == 0x98 || (mac[4] == 0xAA && mac[5] == 0xB0)) {
        currentRole = ROLE_MASTER_LAFVIN;
        snprintf(boardRole, sizeof(boardRole), "LAFVIN MASTER (LCD)");
        txPin = 2;  // Pino 2 da serigrafia -> TX (Pad 4)
        rxPin = 1;  // Pino 1 da serigrafia -> RX (Pad 5)
    } else if (mac[4] == 0x43 && mac[5] == 0x08) {
        currentRole = ROLE_SLAVE_C6_LOCAL;
        snprintf(boardRole, sizeof(boardRole), "C6 #1 LOCAL (LORA+BATT)");
        txPin = 13; // DevKitC TX -> Lafvin RX
        rxPin = 22; // DevKitC RX <- Lafvin TX
    } else {
        currentRole = ROLE_REMOTE_C6;
        snprintf(boardRole, sizeof(boardRole), "C6 #2 REMOTE (LORA)");
    }

    printLog("\n\n=======================================================\n");
    printLog("  TESTE INTEGRAL DE 3 NOS: LAFVIN <-> C6 #1 <-> C6 #2\n");
    printLog("  Papel Atual: [%s]\n", boardRole);
    printLog("  MAC:   %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    printLog("=======================================================\n");

    // -------------------------------------------------------------
    // INICIALIZAÇÃO ESPECÍFICA DO MASTER (LAFVIN)
    // -------------------------------------------------------------
    if (currentRole == ROLE_MASTER_LAFVIN) {
        printLog("\n--- INICIALIZACAO DE HARDWARE: LCD ST7789 ---\n");
        st7789.begin();
        st7789.setBacklight(85);

        canvas = new Arduino_Canvas(LCD_WIDTH, LCD_HEIGHT, nullptr, 0, 0, DISPLAY_ROTATION);
        if (canvas) {
            canvas->begin();
            canvas->fillScreen(0x0000);
            renderLafvinLcd();
            printLog("[LCD] Ecrã ST7789 1.47\" ativo com brilho a 85%%!\n");
        }

        testMicroSd();
        renderLafvinLcd();

        interSerial.setRxBufferSize(512);
        interSerial.begin(115200, SERIAL_8N1, rxPin, txPin);
        printLog("[UART] Canal serie iniciado nos pinos TX=%d, RX=%d\n", txPin, rxPin);
    }

    // -------------------------------------------------------------
    // INICIALIZAÇÃO DO C6 #1 LOCAL
    // -------------------------------------------------------------
    if (currentRole == ROLE_SLAVE_C6_LOCAL) {
        interSerial.setRxBufferSize(512);
        interSerial.begin(115200, SERIAL_8N1, rxPin, txPin);
        printLog("[UART] Canal serie iniciado nos pinos TX=%d, RX=%d\n", txPin, rxPin);

        analogReadResolution(12);
        analogSetAttenuation(ADC_11db);
        pinMode(PIN_ADC_VOLT, INPUT);
        pinMode(PIN_ADC_CURR, INPUT);
        printLog("[POWER] ADC Bateria 6S ativo nos pinos VOLT=GPIO%d, CURR=GPIO%d\n", PIN_ADC_VOLT, PIN_ADC_CURR);
    }

    // -------------------------------------------------------------
    // INICIALIZAÇÃO LORA SX1278 (Nos dois nós C6: #1 e #2)
    // -------------------------------------------------------------
    if (currentRole == ROLE_SLAVE_C6_LOCAL || currentRole == ROLE_REMOTE_C6) {
        printLog("\n--- A INICIALIZAR HARDWARE LORA SX1278 (SPI) ---\n");
        pinMode(LORA_NSS, OUTPUT);
        digitalWrite(LORA_NSS, HIGH);
        pinMode(LORA_DIO0, INPUT);

        loraSpiBus.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
        delay(50);

        loraVersion = readRawRegister(0x42);
        printLog("[SPI] Leitura RegVersion (0x42): 0x%02X (Esperado: 0x12) -> %s\n",
                 loraVersion, (loraVersion == 0x12) ? "OK (SPI comunicando com perfeicao!)" : "AVISO (Verificar fios SPI)");

        printLog("[LORA] A inicializar RadioLib (433.0 MHz, BW=250 kHz, SF=7)...\n");
        int state = radio.begin(433.0, 250.0, 7, 7, 0x12, 10, 8, 0);
        if (state == RADIOLIB_ERR_NONE) {
            loraInitialized = true;
            radio.startReceive();
            printLog("[LORA] ★★★ SX1278 INICIALIZADO E A ESCUTAR EM MODO RX! ★★★\n");
            setRgb(0, 50, 0); // Verde
        } else {
            loraInitialized = false;
            printLog("[LORA] Falha na inicializacao do RadioLib (codigo: %d)\n", state);
            setRgb(50, 0, 0); // Vermelho
        }
    }
}

void loop() {
    uint32_t now = millis();

    // =============================================================
    // 1. COMPORTAMENTO DO MASTER LAFVIN
    // =============================================================
    if (currentRole == ROLE_MASTER_LAFVIN) {
        // Receção UART do C6 #1
        while (interSerial.available()) {
            char c = (char)interSerial.read();
            static char rxBuf[256];
            static size_t rxIdx = 0;

            if (c == '\n' || c == '\r') {
                if (rxIdx > 0) {
                    rxBuf[rxIdx] = '\0';
                    rxCount++;

                    if (strncmp(rxBuf, "PONG:", 5) == 0) {
                        uint32_t seq = 0, sentTs = 0;
                        char rfStat[32] = "";
                        float rssi = 0.0f, snr = 0.0f, v = 0.0f, a = 0.0f;
                        int p = 0;

                        // PONG:seq:ts:STATUS:RSSI=...:SNR=...:BAT=...
                        int n = sscanf(rxBuf + 5, "%u:%u:%31[^:]:RSSI=%f:SNR=%f:BAT=%fV:%fA:%d%%",
                                       &seq, &sentTs, rfStat, &rssi, &snr, &v, &a, &p);

                        lastRtt = (now >= sentTs) ? (now - sentTs) : 0;
                        strncpy(lastLoraStatus, rfStat, sizeof(lastLoraStatus) - 1);
                        if (n >= 5) {
                            lastRssi = rssi;
                            lastSnr = snr;
                        }
                        if (n >= 8) {
                            liveBatVolts = v;
                            liveBatAmps = a;
                            liveBatPct = p;
                        }

                        printLog("[LAFVIN MASTER] <<< %s | RTT=%ums | LoRa=%s | RSSI=%.1f | Bat=%.2fV\n",
                                 rxBuf, lastRtt, lastLoraStatus, lastRssi, liveBatVolts);
                        setRgb(0, 50, 0);
                    } else {
                        printLog("[LAFVIN MASTER RX] <<< %s\n", rxBuf);
                    }
                    rxIdx = 0;
                }
            } else {
                if (rxIdx < sizeof(rxBuf) - 1) rxBuf[rxIdx++] = c;
            }
        }

        // Transmissão periódica de PING (a cada 750ms)
        if (now - lastTxTime >= 750) {
            lastTxTime = now;
            txCount++;
            char msg[64];
            snprintf(msg, sizeof(msg), "PING:%u:%u\n", txCount, now);
            interSerial.print(msg);
            printLog("[LAFVIN MASTER TX] >>> %s", msg);
        }

        // Atualização do ecrã LCD (a cada 200ms)
        if (now - lastLcdUpdateTime >= 200) {
            lastLcdUpdateTime = now;
            renderLafvinLcd();
        }

        // Gravação periódica de log no cartão MicroSD (a cada 5s)
        if (sdOk && (now - lastSdAccessTime >= 5000)) {
            lastSdAccessTime = now;
            char logLine[160];
            snprintf(logLine, sizeof(logLine), "t=%u,tx=%u,rx=%u,rtt=%u,lora=%s,rssi=%.1f,snr=%.1f,bat=%.2fV,curr=%.2fA\n",
                     now, txCount, rxCount, lastRtt, lastLoraStatus, lastRssi, lastSnr, liveBatVolts, liveBatAmps);
            sdCard.appendFile("/telemetry_test.log", logLine);
            printLog("[SD] Log registado no MicroSD: %s", logLine);
        }
    }

    // =============================================================
    // 2. COMPORTAMENTO DO C6 #1 LOCAL (PONTE UART <---> LORA RF)
    // =============================================================
    else if (currentRole == ROLE_SLAVE_C6_LOCAL) {
        measureBattery();

        // Verificar receção de pacotes LoRa vindos do C6 #2
        if (digitalRead(LORA_DIO0) == HIGH) {
            String rfData;
            int state = radio.readData(rfData);
            if (state == RADIOLIB_ERR_NONE) {
                lastRssi = radio.getRSSI();
                lastSnr  = radio.getSNR();
                rfPacketsRx++;
                strncpy(lastLoraStatus, "RF_OK", sizeof(lastLoraStatus) - 1);
                printLog("[C6 #1 LoRa RX] <<< %s | RSSI=%5.1f dBm | SNR=%4.1f dB\n", rfData.c_str(), lastRssi, lastSnr);
                setRgb(0, 0, 50); // Azul
            }
            radio.startReceive();
        }

        // Receção UART da Lafvin
        while (interSerial.available()) {
            char c = (char)interSerial.read();
            static char uartRxBuf[128];
            static size_t uartRxIdx = 0;

            if (c == '\n' || c == '\r') {
                if (uartRxIdx > 0) {
                    uartRxBuf[uartRxIdx] = '\0';
                    rxCount++;

                    if (strncmp(uartRxBuf, "PING:", 5) == 0) {
                        uint32_t seq = 0, sentTs = 0;
                        sscanf(uartRxBuf + 5, "%u:%u", &seq, &sentTs);

                        // 1. Emitir pacote LoRa pelo ar para o C6 #2
                        char rfMsg[64];
                        snprintf(rfMsg, sizeof(rfMsg), "WIND_PING:%u", seq);
                        radio.transmit(rfMsg);
                        radio.startReceive();
                        rfPacketsTx++;
                        printLog("[C6 #1 LoRa TX] >>> %s (Enviado pelo ar a 433MHz)\n", rfMsg);

                        // 2. Responder imediatamente à Lafvin com o estado atual + Bateria
                        const char* rfStatus = (rfPacketsRx > 0) ? "RF_LINK_OK" : (loraInitialized ? "LORA_READY" : "LORA_ERR");
                        char uartReply[160];
                        snprintf(uartReply, sizeof(uartReply), "PONG:%u:%u:%s:RSSI=%.1f:SNR=%.1f:BAT=%.2fV:%.2fA:%d%%\n",
                                 seq, sentTs, rfStatus, lastRssi, lastSnr, liveBatVolts, liveBatAmps, liveBatPct);
                        interSerial.print(uartReply);
                        txCount++;
                        printLog("[C6 #1 UART TX] >>> %s", uartReply);
                        setRgb(0, 50, 0); // Verde
                    }
                    uartRxIdx = 0;
                }
            } else {
                if (uartRxIdx < sizeof(uartRxBuf) - 1) uartRxBuf[uartRxIdx++] = c;
            }
        }
    }

    // =============================================================
    // 3. COMPORTAMENTO DO C6 #2 REMOTO (LORA RF AUTÓNOMO)
    // =============================================================
    else if (currentRole == ROLE_REMOTE_C6) {
        if (digitalRead(LORA_DIO0) == HIGH) {
            String rfData;
            int state = radio.readData(rfData);
            if (state == RADIOLIB_ERR_NONE) {
                float rssi = radio.getRSSI();
                float snr  = radio.getSNR();
                rfPacketsRx++;
                printLog("[C6 #2 REMOTO RX] <<< %s | RSSI=%5.1f dBm | SNR=%4.1f dB\n", rfData.c_str(), rssi, snr);
                setRgb(50, 50, 0); // Amarelo

                // Se recebeu ping do C6 #1, responde pelo ar com PONG!
                if (rfData.startsWith("WIND_PING:")) {
                    delay(30); // Pequeno compasso de espera RF
                    char reply[64];
                    snprintf(reply, sizeof(reply), "WIND_PONG:%s", rfData.c_str() + 10);
                    radio.transmit(reply);
                    radio.startReceive();
                    rfPacketsTx++;
                    printLog("[C6 #2 REMOTO TX] >>> %s (Resposta RF 433MHz enviada!)\n", reply);
                    setRgb(0, 50, 0); // Verde
                }
            } else {
                radio.startReceive();
            }
        }
    }
}
