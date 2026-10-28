#pragma once

#include <Arduino.h>
#include <TinyGPSPlus.h>
#include "config_common.h"

class GPSTracker {
public:
    GPSTracker(HardwareSerial& port = Serial0) 
        : gpsSerial(port), isHwEnabled(false),
          currentRxPin(PIN_LAFVIN_GPS_RX), currentTxPin(PIN_LAFVIN_GPS_TX),
          currentBaud(GPS_BAUDRATE), isLocked(true),
          lastScanTime(0), lastRxByteTime(0), scanStep(0), rawIdx(0),
          totalBytesReceived(0),
          manualLat(0.0), manualLng(0.0), manualSpeed(0.0f),
          manualHeading(0.0f), manualSats(0), manualFix(false),
          hasManualData(false) {
        rawSentence[0] = '\0';
        lastValidSentence[0] = '\0';
    }

    void begin(bool enableHw = true, uint32_t baud = GPS_BAUDRATE, int rxPin = PIN_LAFVIN_GPS_RX, int txPin = PIN_LAFVIN_GPS_TX) {
        isHwEnabled = enableHw;
        currentBaud = baud;
        currentRxPin = rxPin;
        currentTxPin = txPin;
        isLocked = true;
        scanStep = 0;
        rawIdx = 0;
        totalBytesReceived = 0;
        lastScanTime = millis();
        lastRxByteTime = millis();
        if (isHwEnabled) {
            // Desacopla UART0 padrão do ESP32-C6 (TX=16, RX=17) e liberta GPIO 16 para ser RX de entrada
            gpsSerial.end();
            delay(50);
            pinMode(currentRxPin, INPUT_PULLUP);
            pinMode(currentTxPin, OUTPUT);
            digitalWrite(currentTxPin, HIGH);
            gpsSerial.setRxBufferSize(1024);
            gpsSerial.begin(currentBaud, SERIAL_8N1, currentRxPin, currentTxPin);
            Serial.printf("[GPS] Serial0 UART ativo: RX=GPIO%d, TX=GPIO%d (%lu baud)\n",
                          currentRxPin, currentTxPin, (unsigned long)currentBaud);
        }
    }

    void update() {
        if (!isHwEnabled) return;

        static uint32_t lastDiagMs = 0;
        static uint32_t lastBytesCount = 0;
        static uint32_t lastNmeaPrint = 0;

        while (gpsSerial.available() > 0) {
            char c = (char)gpsSerial.read();
            totalBytesReceived++;
            lastRxByteTime = millis();

            // Captura e imprime sentenças NMEA
            if (c == '$') {
                rawIdx = 0;
                rawSentence[rawIdx++] = c;
            } else if (rawIdx > 0 && rawIdx < sizeof(rawSentence) - 1) {
                if (c == '\n' || c == '\r') {
                    rawSentence[rawIdx] = '\0';
                    if (rawIdx >= 6 && rawSentence[0] == '$') {
                        strncpy(lastValidSentence, rawSentence, sizeof(lastValidSentence) - 1);
                        lastValidSentence[sizeof(lastValidSentence) - 1] = '\0';
                        if (millis() - lastNmeaPrint >= 1000) {
                            lastNmeaPrint = millis();
                            Serial.printf("[NMEA] %s\n", lastValidSentence);
                        }
                        if (!isLocked && (strncmp(rawSentence, "$GN", 3) == 0 ||
                                          strncmp(rawSentence, "$GP", 3) == 0 ||
                                          strncmp(rawSentence, "$BD", 3) == 0)) {
                            isLocked = true;
                            Serial.printf("\n★★★ GPS NMEA CONFIRMADO: RX=GPIO%d, TX=GPIO%d @ %lu baud ★★★\n\n",
                                          currentRxPin, currentTxPin, (unsigned long)currentBaud);
                        }
                    }
                    rawIdx = 0;
                } else {
                    rawSentence[rawIdx++] = c;
                }
            }

            gps.encode(c);
            if (!isLocked && (gps.sentencesWithFix() > 0 || gps.location.isValid() || gps.passedChecksum() >= 2)) {
                isLocked = true;
                Serial.printf("\n★★★ GPS FIX DETETADO: RX=GPIO%d, TX=GPIO%d @ %lu baud ★★★\n\n",
                              currentRxPin, currentTxPin, (unsigned long)currentBaud);
            }
        }

        uint32_t now = millis();

        // Watchdog de recuperação: se passarem >5s sem receber bytes, re-inicializa UART0
        if (now - lastRxByteTime >= 5000) {
            lastRxByteTime = now;
            gpsSerial.end();
            delay(50);
            pinMode(currentRxPin, INPUT_PULLUP);
            pinMode(currentTxPin, OUTPUT);
            digitalWrite(currentTxPin, HIGH);
            gpsSerial.setRxBufferSize(1024);
            gpsSerial.begin(currentBaud, SERIAL_8N1, currentRxPin, currentTxPin);
            Serial.printf("[GPS] Watchdog: UART reinicializada RX=GPIO%d, TX=GPIO%d (%lu baud)\n",
                          currentRxPin, currentTxPin, (unsigned long)currentBaud);
        }

        // Relatório de status a cada 2.0s
        if (now - lastDiagMs >= 2000) {
            lastDiagMs = now;
            uint32_t dBytes = totalBytesReceived - lastBytesCount;
            lastBytesCount = totalBytesReceived;

            if (hasFix()) {
                Serial.printf("[GPS] ★ FIX 3D: Sats=%u | Lat=%.6f Lng=%.6f | Vel=%.1f nós | Chars=%lu | CS_OK=%lu\n",
                              gps.satellites.value(), gps.location.lat(), gps.location.lng(),
                              gps.speed.knots(), (unsigned long)gps.charsProcessed(), (unsigned long)gps.passedChecksum());
            } else {
                Serial.printf("[GPS] A procurar satelites... | Sats=%u | Bytes=%lu (+%lu) | CS_OK=%lu\n",
                              gps.satellites.isValid() ? gps.satellites.value() : 0,
                              (unsigned long)totalBytesReceived, (unsigned long)dBytes,
                              (unsigned long)gps.passedChecksum());
            }
        }
    }

    void setValues(double lat, double lng, float spd, float head, uint8_t sats, bool fix) {
        manualLat = lat;
        manualLng = lng;
        manualSpeed = spd;
        manualHeading = head;
        manualSats = sats;
        manualFix = fix;
        hasManualData = true;
    }

    bool hasFix() {
        if (isHwEnabled && (isLocked || gps.charsProcessed() > 0)) {
            return gps.location.isValid() && gps.location.age() < 5000;
        }
        if (hasManualData) return manualFix;
        return gps.location.isValid() && gps.location.age() < 5000;
    }

    double getLatitude() {
        if (isHwEnabled && (isLocked || gps.charsProcessed() > 0)) {
            return gps.location.isValid() ? gps.location.lat() : 0.0;
        }
        if (hasManualData) return manualLat;
        return gps.location.isValid() ? gps.location.lat() : 0.0;
    }

    double getLongitude() {
        if (isHwEnabled && (isLocked || gps.charsProcessed() > 0)) {
            return gps.location.isValid() ? gps.location.lng() : 0.0;
        }
        if (hasManualData) return manualLng;
        return gps.location.isValid() ? gps.location.lng() : 0.0;
    }

    int32_t getLatDeg7() {
        return static_cast<int32_t>(getLatitude() * 10000000.0);
    }

    int32_t getLngDeg7() {
        return static_cast<int32_t>(getLongitude() * 10000000.0);
    }

    float getSpeedKnots() {
        if (isHwEnabled && (isLocked || gps.charsProcessed() > 0)) {
            return gps.speed.isValid() ? gps.speed.knots() : 0.0f;
        }
        if (hasManualData) return manualSpeed;
        return gps.speed.isValid() ? gps.speed.knots() : 0.0f;
    }

    float getHeading() {
        if (isHwEnabled && (isLocked || gps.charsProcessed() > 0)) {
            return gps.course.isValid() ? gps.course.deg() : 0.0f;
        }
        if (hasManualData) return manualHeading;
        return gps.course.isValid() ? gps.course.deg() : 0.0f;
    }

    uint8_t getSatellites() {
        if (isHwEnabled && (isLocked || gps.charsProcessed() > 0)) {
            return (gps.satellites.isValid() && gps.satellites.age() < 5000) ? (uint8_t)gps.satellites.value() : 0;
        }
        if (hasManualData) return manualSats;
        return (gps.satellites.isValid() && gps.satellites.age() < 5000) ? (uint8_t)gps.satellites.value() : 0;
    }

    uint8_t getFixQuality() {
        return hasFix() ? 1 : 0;
    }

    uint32_t getCharsProcessed() const { return gps.charsProcessed(); }
    uint32_t getTotalBytes() const { return totalBytesReceived; }
    uint32_t getSentencesWithFix() const { return gps.sentencesWithFix(); }
    uint32_t getFailedChecksum() const { return gps.failedChecksum(); }
    bool isHardwareLocked() const { return isLocked; }
    int getRxPin() const { return currentRxPin; }
    int getTxPin() const { return currentTxPin; }
    uint32_t getBaud() const { return currentBaud; }
    const char* getRawSentence() const { return lastValidSentence; }
    uint8_t getRateHz() const { return currentRateHz; }

    // Envio de instrução NMEA / PAIR com cálculo automático de Checksum XOR
    bool sendNmeaCommand(const char* payload) {
        if (!isHwEnabled) return false;
        const char* p = payload;
        while (*p == ' ' || *p == '$') p++; // Ignora espaços e '$' inicial

        // Remover asterisco e checksum antigo caso o utilizador já os tenha incluído
        const char* star = strchr(p, '*');
        char cleanCmd[96];
        if (star) {
            size_t len = star - p;
            if (len >= sizeof(cleanCmd)) len = sizeof(cleanCmd) - 1;
            strncpy(cleanCmd, p, len);
            cleanCmd[len] = '\0';
            p = cleanCmd;
        }

        // Calcula checksum XOR de todos os caracteres do payload
        uint8_t cs = 0;
        for (size_t i = 0; p[i] != '\0'; i++) {
            cs ^= (uint8_t)p[i];
        }

        char formatted[128];
        snprintf(formatted, sizeof(formatted), "$%s*%02X\r\n", p, cs);
        gpsSerial.print(formatted);
        gpsSerial.flush();
        Serial.printf("[GPS CMD] Enviado ao LC29H: %s", formatted);
        return true;
    }

    // Configura taxa de navegação (1, 2, 5 ou 10 Hz) e opcionalmente grava na Flash NVRAM
    bool setUpdateRate(uint8_t hz, bool saveToFlash = true) {
        if (hz < 1) hz = 1;
        if (hz > 10) hz = 10;
        uint32_t intervalMs = 1000 / hz;

        char cmd[32];
        snprintf(cmd, sizeof(cmd), "PAIR050,%lu", (unsigned long)intervalMs);
        bool ok = sendNmeaCommand(cmd);

        if (ok) {
            currentRateHz = hz;
            if (saveToFlash) {
                delay(50);
                sendNmeaCommand("PAIR513"); // Grava na Flash/NVRAM do Quectel LC29H
            }
        }
        return ok;
    }

    // Distância em metros entre o ponto atual e um ponto alvo
    double distanceTo(double targetLat, double targetLng) {
        if (!hasFix()) return 999999.0;
        return TinyGPSPlus::distanceBetween(getLatitude(), getLongitude(), targetLat, targetLng);
    }

    // Rumo/azimute em graus (0..359.9) para um ponto alvo
    double courseTo(double targetLat, double targetLng) {
        if (!hasFix()) return 0.0;
        return TinyGPSPlus::courseTo(getLatitude(), getLongitude(), targetLat, targetLng);
    }

private:
    HardwareSerial& gpsSerial;
    TinyGPSPlus gps;
    bool isHwEnabled;
    int currentRxPin;
    int currentTxPin;
    uint32_t currentBaud;
    uint8_t currentRateHz = 1;
    bool isLocked;
    uint32_t lastScanTime;
    uint32_t lastRxByteTime;
    uint8_t scanStep;
    char rawSentence[128];
    char lastValidSentence[128];
    size_t rawIdx;
    uint32_t totalBytesReceived;

    double manualLat;
    double manualLng;
    float manualSpeed;
    float manualHeading;
    uint8_t manualSats;
    bool manualFix;
    bool hasManualData;
};

