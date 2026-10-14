#include <Arduino.h>
#include <esp_mac.h>

// =========================================================================
// TESTE DE COMUNICAÇÃO INTER-ESP UART BIDIRECIONAL DEFINITIVO
// 
// Ligações:
// Preto:   Lafvin GND <---> C6 GND
// Amarelo: Lafvin pino 2 (RX) <---> C6 pino 13 (TX) [JÁ COMPROVADO 100%]
// Verde:   Lafvin pino 4 (TX) <---> C6 pino 22 (RX)
// =========================================================================

#define RGB_LED_PIN 8

HardwareSerial interSerial(1);

bool isMaster = false;
char boardRole[32] = "";
int txPin = -1;
int rxPin = -1;
const char* portName = "";

uint32_t txCount = 0;
uint32_t rxCount = 0;
uint32_t lastTxTime = 0;

void setRgb(uint8_t r, uint8_t g, uint8_t b) {
#ifdef RGB_BUILTIN
    rgbLedWrite(RGB_LED_PIN, r, g, b);
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

void setup() {
    Serial.begin(115200);
    Serial0.begin(115200);

    setRgb(30, 20, 0); // Amarelo
    delay(1000);

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    if (mac[0] == 0x98 || (mac[4] == 0xAA && mac[5] == 0xB0)) {
        isMaster = true;
        snprintf(boardRole, sizeof(boardRole), "LAFVIN MASTER (LCD)");
        txPin = 4;  // Pino 4 da serigrafia -> TX (Cabo Verde)
        rxPin = 2;  // Pino 2 da serigrafia -> RX (Cabo Amarelo)
        portName = "Lafvin [TX=GPIO4 (Verde), RX=GPIO2 (Amarelo)]";
    } else {
        isMaster = false;
        snprintf(boardRole, sizeof(boardRole), "C6 SLAVE (DEVKITC)");
        txPin = 13; // Pino 13 da serigrafia -> TX (Cabo Amarelo)
        rxPin = 22; // Pino 22 da serigrafia -> RX (Cabo Verde)
        portName = "DevKitC [TX=GPIO13 (Amarelo), RX=GPIO22 (Verde)]";
    }

    interSerial.setRxBufferSize(512);
    interSerial.begin(115200, SERIAL_8N1, rxPin, txPin);

    printLog("\n\n=======================================================\n");
    printLog("  TESTE INTER-ESP UART BIDIRECIONAL DEFINITIVO\n");
    printLog("  Placa: [%s]\n", boardRole);
    printLog("  MAC:   %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    printLog("  Pinos: TX=GPIO%d, RX=GPIO%d @ 115200 bps\n", txPin, rxPin);
    printLog("  Info:  %s\n", portName);
    printLog("=======================================================\n");

    if (isMaster) {
        setRgb(0, 0, 30); // Azul
    } else {
        setRgb(0, 20, 20); // Ciano
    }
}

void loop() {
    uint32_t now = millis();

    // -------------------------------------------------------------
    // RECEÇÃO NÃO-BLOQUEANTE
    // -------------------------------------------------------------
    static char rxBuffer[256];
    static size_t rxBufIdx = 0;

    while (interSerial.available()) {
        char c = (char)interSerial.read();

        if (c == '\n' || c == '\r') {
            if (rxBufIdx > 0) {
                rxBuffer[rxBufIdx] = '\0';
                String msg = String(rxBuffer);
                rxBufIdx = 0;
                rxCount++;

                setRgb(0, 50, 0); // Verde de sucesso

                if (msg.startsWith("PING:")) {
                    printLog("[%s RX SUCESSO ★★★] <<< %s\n", boardRole, msg.c_str());
                    // Responder imediatamente com PONG
                    String pongMsg = "PONG:" + msg.substring(5) + "\n";
                    interSerial.print(pongMsg);
                    printLog("[%s TX PONG] >>> %s", boardRole, pongMsg.c_str());
                } else if (msg.startsWith("PONG:")) {
                    int firstColon = msg.indexOf(':');
                    int secondColon = msg.indexOf(':', firstColon + 1);
                    uint32_t rtt = 0;
                    if (secondColon != -1) {
                        uint32_t sendTime = msg.substring(secondColon + 1).toInt();
                        rtt = now - sendTime;
                    }
                    printLog("[%s ★★★ SUCESSO BIDIRECIONAL COMPLETO ★★★] <<< %s (RTT=%lums, Total RX=%lu)\n", 
                             boardRole, msg.c_str(), rtt, rxCount);
                } else {
                    printLog("[%s RX SUCESSO ★★★] <<< %s\n", boardRole, msg.c_str());
                }
            }
        } else {
            if (rxBufIdx < sizeof(rxBuffer) - 1) {
                rxBuffer[rxBufIdx++] = c;
            }
        }
    }

    // -------------------------------------------------------------
    // TRANSMISSÃO PERIÓDICA: O Master (Lafvin) envia PING a cada 500ms
    // -------------------------------------------------------------
    if (isMaster) {
        if (now - lastTxTime >= 500) {
            lastTxTime = now;
            txCount++;
            String pingMsg = "PING:" + String(txCount) + ":" + String(now) + "\n";
            interSerial.print(pingMsg);
            printLog("[%s TX] >>> %s", boardRole, pingMsg.c_str());
        }
    } else {
        // C6 Slave também envia Heartbeat a cada 1000ms
        if (now - lastTxTime >= 1000) {
            lastTxTime = now;
            txCount++;
            String c6Msg = "MSG:C6_ALIVE:" + String(txCount) + ":" + String(now) + "\n";
            interSerial.print(c6Msg);
            printLog("[%s TX] >>> %s", boardRole, c6Msg.c_str());
        }
    }

    // Desligar o LED após breve flash de atividade
    static uint32_t lastLedReset = 0;
    if (now - lastLedReset >= 80) {
        lastLedReset = now;
        if (isMaster) {
            setRgb(0, 0, 5);
        } else {
            setRgb(0, 5, 5);
        }
    }

    delay(2);
}
