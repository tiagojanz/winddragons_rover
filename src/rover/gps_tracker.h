#pragma once

#include <Arduino.h>
#include <TinyGPSPlus.h>
#include "config_common.h"

class GPSTracker {
public:
    GPSTracker(HardwareSerial& port = Serial0) 
        : gpsSerial(port), isHwEnabled(false),
          currentRxPin(17), currentTxPin(16),
          currentBaud(GPS_BAUDRATE), isLocked(false),
          lastScanTime(0), scanStep(0), rawIdx(0),
          totalBytesReceived(0),
          manualLat(0.0), manualLng(0.0), manualSpeed(0.0f),
          manualHeading(0.0f), manualSats(0), manualFix(false),
          hasManualData(false) {
        rawSentence[0] = '\0';
        lastValidSentence[0] = '\0';
    }

    void begin(bool enableHw = true, uint32_t baud = GPS_BAUDRATE, int rxPin = 17, int txPin = 16) {
        isHwEnabled = enableHw;
        currentBaud = baud;
        currentRxPin = rxPin;
        currentTxPin = txPin;
        isLocked = false;
        scanStep = 0;
        rawIdx = 0;
        totalBytesReceived = 0;
        lastScanTime = millis();
        if (isHwEnabled) {
            gpsSerial.begin(currentBaud, SERIAL_8N1, currentRxPin, currentTxPin);
            Serial.printf("[GPS] Serial0 UART ativo: RX=GPIO%d, TX=GPIO%d (%lu baud)\n",
                          currentRxPin, currentTxPin, (unsigned long)currentBaud);
        }
    }

    void update() {
        if (!isHwEnabled) return;

        while (gpsSerial.available() > 0) {
            char c = (char)gpsSerial.read();
            totalBytesReceived++;

            // Captura de texto NMEA raw para validação imediata
            if (c == '$') {
                rawIdx = 0;
                rawSentence[rawIdx++] = c;
            } else if (rawIdx > 0 && rawIdx < sizeof(rawSentence) - 1) {
                if (c == '\n' || c == '\r') {
                    rawSentence[rawIdx] = '\0';
                    if (rawIdx >= 6 && rawSentence[0] == '$' && 
                        (rawSentence[1] == 'G' || rawSentence[1] == 'P' || rawSentence[1] == 'B')) {
                        strncpy(lastValidSentence, rawSentence, sizeof(lastValidSentence) - 1);
                        lastValidSentence[sizeof(lastValidSentence) - 1] = '\0';
                        if (!isLocked) {
                            isLocked = true;
                            Serial.printf("[GPS] ★★★ NMEA detectado: %s (RX=GPIO%d @ %lu baud) ★★★\n",
                                          lastValidSentence, currentRxPin, (unsigned long)currentBaud);
                        }
                    }
                    rawIdx = 0;
                } else {
                    rawSentence[rawIdx++] = c;
                }
            }

            gps.encode(c);
        }

        // Se após 6 segundos não detetar NMEA válido na configuração padrão (RX=17 @ 115200),
        // testa as alternativas (9600 baud ou pinos invertidos)
        if (!isLocked) {
            uint32_t now = millis();
            if (now - lastScanTime >= 6000) {
                lastScanTime = now;
                scanStep = (scanStep + 1) % 4;
                switch (scanStep) {
                    case 0: currentRxPin = 17; currentTxPin = 16; currentBaud = 115200; break;
                    case 1: currentRxPin = 17; currentTxPin = 16; currentBaud = 9600; break;
                    case 2: currentRxPin = 16; currentTxPin = 17; currentBaud = 115200; break;
                    case 3: currentRxPin = 16; currentTxPin = 17; currentBaud = 9600; break;
                }
                rawIdx = 0;
                gpsSerial.end();
                delay(50);
                gpsSerial.begin(currentBaud, SERIAL_8N1, currentRxPin, currentTxPin);
                Serial.printf("[GPS] A tentar alternativa: RX=GPIO%d, TX=GPIO%d a %lu baud...\n",
                              currentRxPin, currentTxPin, (unsigned long)currentBaud);
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
            return gps.location.isValid() && gps.location.age() < 3000;
        }
        if (hasManualData) return manualFix;
        return gps.location.isValid() && gps.location.age() < 3000;
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
            return gps.satellites.isValid() ? gps.satellites.value() : 0;
        }
        if (hasManualData) return manualSats;
        return gps.satellites.isValid() ? gps.satellites.value() : 0;
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
    bool isLocked;
    uint32_t lastScanTime;
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
