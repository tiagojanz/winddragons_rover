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
#include "rover_version.h"

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
char queuedWebCommand[256] = "NONE";

// Controlo do ecrã e botão de navegação
volatile bool flagPageChange = false;
volatile uint32_t lastBtnPressTime = 0;

// Calibração do Joystick Analógico
int joyCenterX = 2048;
int joyCenterY = 2048;

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
}

void setup() {
    Serial.begin(115200);
    Serial.setTxTimeoutMs(0);
    delay(500);

    printLog("\n=======================================================\n");
    printLog("  WINDDRAGONS ROVER-%02d: LAFVIN MASTER (LCD+SD+WIFI)\n", ROVER_ID);
    printLog("  Firmware: Master Lafvin %s (%s)\n", ROVER_LAFVIN_VERSION, ROVER_BUILD_DATE);
    printLog("=======================================================\n");

    // Desativar CS do SD durante boot
    pinMode(PIN_SD_CS, OUTPUT);
    digitalWrite(PIN_SD_CS, HIGH);

    // 1. Inicializar LCD ST7789
    roverDisplay.begin();
    roverDisplay.bootLogf(UI_CYAN, 10, "[SYS ] Master Lafvin %s", ROVER_LAFVIN_VERSION);

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
    actuators.setAnchorCalibration(roverConfig.getAnchorDrumDiamMm(), roverConfig.getAnchorPulsesPerRev());
    webServer.setCommandQueueCallback([](const char* cmd) {
        if (cmd && strlen(cmd) > 0) {
            if (strncmp(cmd, "CMD:OTA:", 8) == 0) {
                // Envio imediato repetido para máxima fiabilidade no início do OTA
                for (int i = 0; i < 3; i++) {
                    interSerial.println(cmd);
                    interSerial.flush();
                    delay(25);
                }
                printLog("[LAFVIN] >>> DISPARO OTA DIRETO UART C6 (x3): %s\n", cmd);
            } else {
                interSerial.println(cmd);
                interSerial.flush();
                printLog("[LAFVIN] >>> COMANDO UART DIRETO: %s\n", cmd);
            }
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

    // 8. Configurar Joystick Analógico (Pad 3=GPIO 3, Pad 6=GPIO 0)
    analogReadResolution(12);
    pinMode(PIN_JOYSTICK_X, INPUT);
    pinMode(PIN_JOYSTICK_Y, INPUT);

    // Auto-calibração do Ponto Neutro / Centro com o joystick em repouso
    long sumX = 0, sumY = 0;
    for (int i = 0; i < 30; i++) {
        sumX += analogRead(PIN_JOYSTICK_X);
        sumY += analogRead(PIN_JOYSTICK_Y);
        delay(5);
    }
    joyCenterX = (int)(sumX / 30);
    joyCenterY = (int)(sumY / 30);
    printLog("[JOYSTICK] Auto-calibracao de centro concluida: X0=%d, Y0=%d\n", joyCenterX, joyCenterY);

    // 9. Configurar GPS Quectel LC29H no Header J2 (Pad 9=GPIO 17 [TX do GPS], Pad 8=GPIO 16 [RX do GPS])
    gps.begin(true, GPS_BAUDRATE, 17, 16);
    printLog("[GPS ] Quectel LC29H inicializado no Lafvin (RX=GPIO 17, TX=GPIO 16 a %d baud)\n",
             GPS_BAUDRATE);

    roverDisplay.bootLogf(UI_GREEN, 100, "[BOOT] Sistema Pronto!");
    roverDisplay.endBootScreen(1200);
}

void loop() {
    uint32_t now = millis();

    // 1. Atualizar GPS Quectel LC29H
    gps.update();

    // 1. Processar clientes HTTP Web
    webServer.handleClient();

    // 2. Interrupção do Botão BOOT para troca de página
    bool forceDisplay = false;
    if (flagPageChange) {
        flagPageChange = false;
        roverDisplay.nextPage();
        forceDisplay = true;
    }

    // 3. Leitura do Joystick Analógico com Curso Completo (100% no fim mecânico)
    static uint32_t lastJoyReadTime = 0;
    static int lastSentThr = 0;
    static int lastSentRud = 0;
    static bool joyActive = false;

    if (now - lastJoyReadTime >= 50) {
        lastJoyReadTime = now;
        int rawX = analogRead(PIN_JOYSTICK_X);
        int rawY = analogRead(PIN_JOYSTICK_Y);

        int deadband = 100; // Zona morta equilibrada
        int diffX = rawX - joyCenterX;
        int diffY = rawY - joyCenterY;

        static int maxSeenRight = 1450;
        static int maxSeenLeft  = 1450;
        static int maxSeenDown  = 1450;
        static int maxSeenUp    = 1450;

        int rud = 0;
        if (diffX > deadband) {
            // Estibordo (Right)
            int d = diffX - deadband;
            if (d > maxSeenRight) maxSeenRight = d;
            rud = constrain(d * 100 / maxSeenRight, 0, 100);
        } else if (diffX < -deadband) {
            // Bombordo (Left)
            int d = (-diffX) - deadband;
            if (d > maxSeenLeft) maxSeenLeft = d;
            rud = -constrain(d * 100 / maxSeenLeft, 0, 100);
        }

        int thr = 0;
        if (diffY > deadband) {
            // Marcha-atrás / Ré (Pull back)
            int d = diffY - deadband;
            if (d > maxSeenDown) maxSeenDown = d;
            thr = -constrain(d * 100 / maxSeenDown, 0, 100);
        } else if (diffY < -deadband) {
            // Avanço / Frente (Push forward)
            int d = (-diffY) - deadband;
            if (d > maxSeenUp) maxSeenUp = d;
            thr = constrain(d * 100 / maxSeenUp, 0, 100);
        }

        if (rud != 0 || thr != 0) {
            joyActive = true;
            actuators.setRudder((int8_t)rud);
            actuators.setThrottle((int8_t)thr);
            if (abs(rud - lastSentRud) >= 2 || abs(thr - lastSentThr) >= 2) {
                lastSentRud = rud;
                lastSentThr = thr;
                interSerial.printf("CMD:JOY:%d:%d\n", thr, rud);
                interSerial.flush();
                snprintf(queuedWebCommand, sizeof(queuedWebCommand), "CMD:JOY:%d:%d", thr, rud);
                printLog("[JOYSTICK] Ativo (X=%d Y=%d) -> Leme: %d%% | Motor: %d%%\n", rawX, rawY, rud, thr);
            }
        } else if (joyActive) {
            joyActive = false;
            lastSentRud = 0;
            lastSentThr = 0;
            actuators.setRudder(0);
            actuators.setThrottle(0);
            interSerial.println("CMD:JOY:0:0");
            interSerial.flush();
            snprintf(queuedWebCommand, sizeof(queuedWebCommand), "CMD:JOY:0:0");
            printLog("[JOYSTICK] Centro (X=%d Y=%d) -> Leme: 0% | Motor: 0%\n", rawX, rawY);
        }
    }

    // Consulta periódica da versão do Coprocessador C6 até obter resposta
    static uint32_t lastVerCheck = 0;
    if (strcmp(queuedWebCommand, "NONE") == 0 && strcmp(roverDisplay.getC6Version(), "--") == 0) {
        if (now - lastVerCheck >= 2000) {
            lastVerCheck = now;
            strncpy(queuedWebCommand, "CMD:GET_VER", sizeof(queuedWebCommand) - 1);
        }
    }

    // 4. Comunicação Inter-ESP UART (Ping periódico a cada 200ms ou rápido a cada 50ms se houver comando pendente)
    bool hasPendingCmd = (strcmp(queuedWebCommand, "NONE") != 0);
    if ((now - lastUartPingTime >= 200) || (hasPendingCmd && (now - lastUartPingTime >= 50))) {
        lastUartPingTime = now;
        uartSeq++;

        // Se houver comando pendente, envia-o como linha direta dedicada CMD:...
        if (hasPendingCmd) {
            interSerial.println(queuedWebCommand);
            interSerial.flush();
            printLog("[LAFVIN] >>> COMANDO PENDENTE UART: %s\n", queuedWebCommand);
            strncpy(queuedWebCommand, "NONE", sizeof(queuedWebCommand) - 1);
        }

        // PING de telemetria periódico limpo (sem colidir colons em scansets)
        char pingMsg[160];
        snprintf(pingMsg, sizeof(pingMsg), "PING:%u:%u:NONE:%ld:%ld:%.1f:%.1f:%u:%u\n",
                 uartSeq, roverConfig.getRoverId(),
                 (long)gps.getLatDeg7(), (long)gps.getLngDeg7(),
                 gps.getSpeedKnots(), gps.getHeading(),
                 gps.getSatellites(), gps.hasFix() ? 1 : 0);
        interSerial.print(pingMsg);
    }

    // 4. Receção e interpretação de respostas PONG do C6 Coprocessador
    while (interSerial.available()) {
        char c = (char)interSerial.read();
        static char rxBuf[256];
        static size_t rxIdx = 0;

        if (c == '\n' || c == '\r') {
            if (rxIdx > 0) {
                rxBuf[rxIdx] = '\0';
                
                if (strncmp(rxBuf, "PONG:OTA:", 9) == 0) {
                    lastUartRxTime = now;
                    printLog("[LAFVIN ROVER] ★ C6 OTA: %s\n", rxBuf + 9);
                    webServer.setC6OtaStatus(rxBuf + 9);
                } else if (strncmp(rxBuf, "PONG:VER:", 9) == 0) {
                    lastUartRxTime = now;
                    printLog("[LAFVIN ROVER] ★ Versao Coprocessador C6: %s\n", rxBuf + 9);
                    webServer.setC6Version(rxBuf + 9);
                    roverDisplay.setC6Version(rxBuf + 9);
                } else if (strncmp(rxBuf, "PONG:", 5) == 0) {
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
                        if (!gps.isHardwareLocked() && !gps.hasFix()) {
                            gps.setValues((double)lat7 / 10000000.0, (double)lng7 / 10000000.0, spd, head, (uint8_t)sats, fix > 0);
                        }
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

    // 6. Gravação de log no cartão MicroSD (a cada 5000ms se o servidor web não estiver ocupado)
    static uint32_t lastSdLogTime = 0;
    if (sdCard.isReady() && !webServer.isBusy() && (now - lastSdLogTime >= 5000)) {
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
