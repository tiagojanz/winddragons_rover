#include <Arduino.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <Adafruit_NeoPixel.h>

#include "config_common.h"
#include "lora_protocol.h"
#include "gps_tracker.h"
#include "actuators.h"
#include "battery_monitor.h"
#include "rover_display.h"
#include "sd_card_manager.h"
#include "wifi_config_manager.h"
#include "rover_config_manager.h"
#include "rover_web_server.h"

// =========================================================================
// WINDDRAGONS ROVER - LAFVIN MASTER FIRMWARE
// Placa: Lafvin ESP32-C6 LCD 1.47" (IPS ST7789 320x172)
// Responsabilidade:
//  - Ecrã LCD ST7789 com interface gráfica e telemetria
//  - Leitor MicroSD (TF Card) para registo em ficheiro e configurações
//  - Ponto de Acesso WiFi e Servidor Web móvel
//  - Botão BOOT (GPIO 9) para navegação entre ecrãs
//  - Ponte UART com o Coprocessador C6 (TX=GPIO 2, RX=GPIO 1)
// =========================================================================

#ifndef ROVER_ID
#define ROVER_ID 1
#endif

// Periféricos e módulos
GPSTracker gps;
ActuatorController actuators;
BatteryMonitor battery;
RoverDisplay roverDisplay;
SDCardManager sdCard;
Adafruit_NeoPixel rgbLed(1, PIN_RGB_LED, NEO_GRB + NEO_KHZ800);
WiFiConfigManager wifiConfig;
RoverConfigManager roverConfig;

uint8_t currentMotorStatus = MOTOR_IDLE;
uint32_t lastControlReceivedTime = 0;
int16_t currentLoraRssi = 0;
float currentLoraSnr = 0.0f;
char loraStatusStr[32] = "A aguardar...";

RoverWebServer webServer(wifiConfig, gps, battery, actuators, currentMotorStatus, lastControlReceivedTime, roverConfig, sdCard, &roverDisplay);

// UART Inter-ESP (Lafvin Master -> C6 Coprocessor)
HardwareSerial interSerial(1);
uint32_t uartSeq = 0;
uint32_t lastUartPingTime = 0;
uint32_t lastUartRxTime = 0;
char queuedWebCommand[64] = "NONE";

// Controlo do ecrã e botão de navegação
volatile bool flagPageChange = false;
volatile uint32_t lastBtnPressTime = 0;

void IRAM_ATTR isrBootButton() {
    uint32_t now = millis();
    if (now - lastBtnPressTime > 200) {
        lastBtnPressTime = now;
        flagPageChange = true;
    }
}

void setRgbColor(uint8_t r, uint8_t g, uint8_t b) {
    rgbLed.setPixelColor(0, rgbLed.Color(r, g, b));
    rgbLed.show();
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
    Serial.setTxTimeoutMs(0);
    delay(500);

    printLog("\n=======================================================\n");
    printLog("  WINDDRAGONS ROVER-%02d: LAFVIN MASTER (LCD+SD+WIFI)\n", ROVER_ID);
    printLog("=======================================================\n");

    // Desativar CS do SD durante boot
    pinMode(PIN_SD_CS, OUTPUT);
    digitalWrite(PIN_SD_CS, HIGH);

    // 1. Inicializar LCD ST7789
    roverDisplay.begin();

    // 2. LED RGB NeoPixel
    rgbLed.begin();
    setRgbColor(20, 20, 0); // Yellow: Booting

    // 3. Inicializar MicroSD Card (Partilha SPI com o LCD)
    roverDisplay.bootLogf(UI_LIGHTGREY, 20, "[SD  ] A inicializar leitor TF...");
    if (sdCard.begin()) {
        roverDisplay.bootLogf(UI_GREEN, 35, "[SD  ] Cartao TF 100%% OK");
        printLog("[SD  ] Cartao TF detetado e pronto (%llu MB)!\n", sdCard.getCardSizeMB());
    } else {
        roverDisplay.bootLogf(UI_YELLOW, 35, "[SD  ] Nenhum cartao detetado");
    }

    // 4. Carregar configurações da memória NVS (Flash) e MicroSD
    roverConfig.begin(&sdCard);
    uint8_t currentRoverId = roverConfig.getRoverId();
    webServer.setRoverId(currentRoverId);
    battery.setCellCount(roverConfig.getBatteryCells());
    battery.setCapacityMah(roverConfig.getBatteryCapacityMah());
    webServer.setCommandQueueCallback([](const char* cmd) {
        if (cmd && strlen(cmd) > 0) {
            strncpy(queuedWebCommand, cmd, sizeof(queuedWebCommand) - 1);
            queuedWebCommand[sizeof(queuedWebCommand) - 1] = '\0';
        }
    });

    printLog("\n=======================================================\n");
    printLog("  WINDDRAGONS ROVER-%02d: LAFVIN MASTER (LCD+SD+WIFI)\n", currentRoverId);
    printLog("  Identificador NVS ativo: ROVER-%02d | Bateria: %uS (%u mAh)\n", 
             currentRoverId, roverConfig.getBatteryCells(), roverConfig.getBatteryCapacityMah());
    printLog("=======================================================\n");

    roverDisplay.showBootScreen(currentRoverId);
    roverDisplay.bootLogf(UI_CYAN, 50, "[BOOT] WindDragons Master (ID NVS: %u | %uS %umAh)", 
                          currentRoverId, roverConfig.getBatteryCells(), roverConfig.getBatteryCapacityMah());

    // 5. Configurar WiFi e Servidor Web
    char apName[32];
    snprintf(apName, sizeof(apName), "WINDDRAGONS-ROVER-%02d", currentRoverId);
    wifiConfig.begin(&sdCard);

    roverDisplay.bootLogf(UI_YELLOW, 65, "[WIFI] A procurar redes...");
    printLog("[WIFI] A procurar redes no cartao SD / NVS...\n");

    // Tentar conectar às redes configuradas em /wifi_networks.json ou NVS (timeout de 6 segundos)
    bool wifiOk = wifiConfig.autoConnect(6000, [](const char* ssid) {
        roverDisplay.bootLogf(UI_YELLOW, 70, "[WIFI] A testar '%s'...", ssid);
        printLog("[WIFI] A testar ligacao a: %s\n", ssid);
    });

    if (wifiOk) {
        printLog("[WIFI] Ligado com sucesso! SSID: %s | IP: %s\n", 
                 wifiConfig.getSSID().c_str(), wifiConfig.getIPAddress().c_str());
        roverDisplay.bootLogf(UI_GREEN, 75, "[WIFI] STA: %s", wifiConfig.getSSID().c_str());
        roverDisplay.bootLogf(UI_GREEN, 80, "[WIFI] IP: %s", wifiConfig.getIPAddress().c_str());
    } else {
        // Fallback automático para Ponto de Acesso
        wifiConfig.startAccessPoint(apName, "winddragon");
        printLog("[WIFI] Sem rede STA. Modo Ponto de Acesso: %s (192.168.4.1)\n", apName);
        roverDisplay.bootLogf(UI_YELLOW, 75, "[WIFI] Sem rede -> Modo AP");
        roverDisplay.bootLogf(UI_CYAN, 80, "[WIFI] AP: %s (192.168.4.1)", apName);
    }

    webServer.begin();

    // 6. Configurar UART Inter-ESP (TX=GPIO 2, RX=GPIO 1)
    interSerial.setRxBufferSize(512);
    interSerial.begin(INTER_ESP_BAUDRATE, SERIAL_8N1, PIN_INTER_ESP_LAFVIN_RX, PIN_INTER_ESP_LAFVIN_TX);
    roverDisplay.bootLogf(UI_GREEN, 90, "[UART] Inter-ESP TX=2, RX=1");
    Serial.printf("[UART] Canal serie com C6 iniciado nos pinos TX=%d, RX=%d (%d baud)\n",
                  PIN_INTER_ESP_LAFVIN_TX, PIN_INTER_ESP_LAFVIN_RX, INTER_ESP_BAUDRATE);

    // 7. Botão BOOT para alternar ecrãs
    pinMode(PIN_BTN_BOOT, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_BTN_BOOT), isrBootButton, FALLING);

    roverDisplay.bootLogf(UI_GREEN, 100, "[BOOT] Sistema Pronto!");
    roverDisplay.endBootScreen(1200);
}

void loop() {
    uint32_t now = millis();

    // 1. Processar clientes HTTP Web
    webServer.handleClient();

    // 2. Interrupção do Botão BOOT para troca de página
    bool forceDisplay = false;
    if (flagPageChange) {
        flagPageChange = false;
        roverDisplay.nextPage();
        forceDisplay = true;
    }

    // 3. Comunicação Inter-ESP UART (Ping periódico a cada 500ms)
    if (now - lastUartPingTime >= 500) {
        lastUartPingTime = now;
        uartSeq++;

        char pingMsg[128];
        snprintf(pingMsg, sizeof(pingMsg), "PING:%u:%u:%s\n", uartSeq, roverConfig.getRoverId(), queuedWebCommand);
        interSerial.print(pingMsg);
        printLog("[LAFVIN] >>> %s", pingMsg);
        strncpy(queuedWebCommand, "NONE", sizeof(queuedWebCommand) - 1);
    }

    // 4. Receção e interpretação de respostas PONG do C6 Coprocessador
    while (interSerial.available()) {
        char c = (char)interSerial.read();
        static char rxBuf[256];
        static size_t rxIdx = 0;

        if (c == '\n' || c == '\r') {
            if (rxIdx > 0) {
                rxBuf[rxIdx] = '\0';
                
                // Formato esperado:
                // PONG:<seq>:<batMv>:<batA>:<batPct>:<lat7>:<lng7>:<spd>:<head>:<sats>:<fix>:<motSt>:<rud>:<thr>:<winch>:<rack>:<depth>:<loraSt>:<rssi>:<snr>
                if (strncmp(rxBuf, "PONG:", 5) == 0) {
                    lastUartRxTime = now;
                    uint32_t seq = 0, batMv = 0, batPct = 0, sats = 0, fix = 0, motSt = 0, depth = 0;
                    long lat7 = 0, lng7 = 0;
                    float batA = 0.0f, spd = 0.0f, head = 0.0f, rssi = 0.0f, snr = 0.0f;
                    int rud = 0, thr = 0, winch = 0, rack = 0;
                    char rfSt[32] = "OK";

                    int parsed = sscanf(rxBuf + 5,
                                        "%u:%u:%f:%u:%ld:%ld:%f:%f:%u:%u:%u:%d:%d:%d:%d:%u:%31[^:]:%f:%f",
                                        &seq, &batMv, &batA, &batPct,
                                        &lat7, &lng7, &spd, &head,
                                        &sats, &fix, &motSt,
                                        &rud, &thr, &winch, &rack, &depth,
                                        rfSt, &rssi, &snr);
                    if (parsed >= 10) {
                        battery.setValues(batMv, batA, (uint8_t)batPct);
                        gps.setValues((double)lat7 / 10000000.0, (double)lng7 / 10000000.0, spd, head, (uint8_t)sats, fix > 0);
                        actuators.setValues((int8_t)thr, (int8_t)rud, winch, rack > 0, (uint8_t)motSt, (uint16_t)depth);
                        currentMotorStatus = (uint8_t)motSt;
                        currentLoraRssi = (int16_t)rssi;
                        currentLoraSnr  = snr;
                        strncpy(loraStatusStr, rfSt, sizeof(loraStatusStr) - 1);
                        printLog("[LAFVIN ROVER] <<< PONG #%u: Bat=%.2fV (%u%%) | LoRa=%s (RSSI=%.1f) | Fix=%u Sats=%u\n",
                                 seq, batMv / 1000.0f, batPct, rfSt, rssi, fix, sats);
                    }
                }
                rxIdx = 0;
            }
        } else {
            if (rxIdx < sizeof(rxBuf) - 1) rxBuf[rxIdx++] = c;
        }
    }

    // 5. Atualização do ecrã LCD ST7789 (a cada 200ms, se barramento SPI estiver livre)
    static uint32_t lastLcdUpdate = 0;
    if (!webServer.isBusy() && (forceDisplay || (now - lastLcdUpdate >= 200))) {
        lastLcdUpdate = now;
        bool failsafeActive = (currentMotorStatus == MOTOR_FAILSAFE);

        roverDisplay.update(roverConfig.getRoverId(), gps, battery, actuators, currentMotorStatus,
                            currentLoraRssi, failsafeActive, forceDisplay, sdCard.isReady(),
                            wifiConfig.isAPMode(), wifiConfig.getIPAddress().c_str(),
                            wifiConfig.getSSID().c_str(), wifiConfig.getRSSI(),
                            wifiConfig.getAPStationCount(),
                            wifiConfig.getAPPassword().c_str());
    }

    // 6. Gravação de log no cartão MicroSD (a cada 5000ms)
    static uint32_t lastSdLogTime = 0;
    if (sdCard.isReady() && (now - lastSdLogTime >= 5000)) {
        lastSdLogTime = now;
        char logBuf[160];
        snprintf(logBuf, sizeof(logBuf),
                 "%lu,%.6f,%.6f,%.1f,%.1f,%u,%.2fV,%.2fA,%u,%s,%d\n",
                 now, gps.getLatitude(), gps.getLongitude(), gps.getSpeedKnots(),
                 gps.getHeading(), battery.getPercentage(),
                 battery.getVoltageMv() / 1000.0f, battery.getCurrentAmps(),
                 currentMotorStatus, loraStatusStr, currentLoraRssi);
        sdCard.appendFile("/telemetry.csv", logBuf);
    }

    // 7. Consola de comandos Serial (ex: SET_ID 2)
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd.startsWith("SET_ID ") || cmd.startsWith("ID ")) {
            int newId = cmd.substring(cmd.indexOf(' ') + 1).toInt();
            if (newId >= 1 && newId <= 254) {
                roverConfig.setRoverId((uint8_t)newId);
                roverConfig.saveConfig();
                printLog("[LAFVIN] ★ ID guardado em NVS: %d. A reiniciar...\n", newId);
                delay(500);
                ESP.restart();
            }
        }
    }
}
