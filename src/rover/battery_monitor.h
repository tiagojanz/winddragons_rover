#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include "config_common.h"

// ==============================================================================
// APM Power Module (28V / 90A) Monitor via ADS1115 16-Bit I2C ADC (0x48)
// ==============================================================================
// - Baterias LiPo Suportadas:
//   * 3S: 9.9V (3.30V/célula) a 12.6V (4.20V/célula) - Nominal 11.1V
//   * 4S: 13.2V (3.30V/célula) a 16.8V (4.20V/célula) - Nominal 14.8V
//   * 6S: 19.8V (3.30V/célula) a 25.2V (4.20V/célula) - Nominal 22.2V
// - Divisor de tensão APM: rácio ~10.8
// - Sensor de corrente APM: ~27.32 A/V (90A a 3.3V)
// - Ganho ADS1115: GAIN_ONE (+/- 4.096V, 1 LSB = 0.125mV)
// ==============================================================================

class BatteryMonitor {
public:
    BatteryMonitor()
        : voltageMv(16800),
          batteryPct(100),
          currentAmps(0.0f),
          consumedMah(0.0f),
          lastUpdateMs(0),
          adsAvailable(false),
          cellCount(4),
          capacityMah(2650) {}

    void setCellCount(uint8_t cells) {
        if (cells == 3 || cells == 4 || cells == 6) {
            cellCount = cells;
            calculatePercentage();
        }
    }

    uint8_t getCellCount() const { return cellCount; }

    void setCapacityMah(uint16_t cap) {
        if (cap >= 500 && cap <= 50000) {
            capacityMah = cap;
        }
    }

    uint16_t getCapacityMah() const { return capacityMah; }

    float getRemainingMah() const {
        return (consumedMah < (float)capacityMah) ? ((float)capacityMah - consumedMah) : 0.0f;
    }

    float getEstimatedHoursRemaining() const {
        if (currentAmps > 0.05f) {
            return (getRemainingMah() / 1000.0f) / currentAmps;
        }
        return -1.0f;
    }

    void resetConsumed() {
        consumedMah = 0.0f;
    }

    void begin() {
        Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
        if (ads.begin(ADS1115_I2C_ADDR, &Wire)) {
            ads.setGain(GAIN_ONE); // +/- 4.096V 1-bit = 0.125mV
            adsAvailable = true;
            Serial.println(F("[BATT] ADS1115 16-Bit ADC detectado com sucesso em 0x48!"));
        } else {
            adsAvailable = false;
            Serial.println(F("[BATT-AVISO] ADS1115 nao encontrado em 0x48! A usar valores nominais."));
        }
        lastUpdateMs = millis();
        update(); // Primeira leitura inicial
    }

    void beginDirectAdc() {
        useDirectAdc = true;
        analogReadResolution(12);
        analogSetAttenuation(ADC_11db);
        pinMode(PIN_C6_ADC_VOLT, INPUT);
        pinMode(PIN_C6_ADC_CURR, INPUT);
        lastUpdateMs = millis();
        update();
    }

    void setValues(uint16_t vMv, float cAmps, uint8_t pct) {
        voltageMv = vMv;
        currentAmps = cAmps;
        calculatePercentage();
        consumedMah += (cAmps * 1000.0f) * (0.2f / 3600.0f);
    }

    void calculatePercentage() {
        uint16_t vMin = cellCount * 3300; // 3.30V min safe cut-off
        uint16_t vMax = cellCount * 4200; // 4.20V 100% full
        if (voltageMv >= vMax) {
            batteryPct = 100;
        } else if (voltageMv <= vMin) {
            batteryPct = 0;
        } else {
            batteryPct = static_cast<uint8_t>(((uint32_t)(voltageMv - vMin) * 100) / (vMax - vMin));
        }
    }

    void update() {
        uint32_t now = millis();
        if (lastUpdateMs == 0) lastUpdateMs = now;
        float dtHours = (now - lastUpdateMs) / 3600000.0f;
        lastUpdateMs = now;

        if (useDirectAdc) {
            uint32_t sumV = 0, sumI = 0;
            for (int i = 0; i < 15; i++) {
                sumV += analogReadMilliVolts(PIN_C6_ADC_VOLT);
                sumI += analogReadMilliVolts(PIN_C6_ADC_CURR);
                delayMicroseconds(100);
            }
            float mvV = (float)sumV / 15.0f;
            float mvI = (float)sumI / 15.0f;
            float batteryV = (mvV * BATT_VOLT_DIVIDER) / 1000.0f;
            voltageMv = static_cast<uint16_t>(batteryV * 1000.0f);

            if (mvI > BATT_CURR_OFFSET_MV) {
                currentAmps = ((mvI - BATT_CURR_OFFSET_MV) / 1000.0f) * BATT_AMPS_PER_VOLT;
            } else {
                currentAmps = 0.0f;
            }

            consumedMah += (currentAmps * 1000.0f) * dtHours;
            calculatePercentage();
            return;
        }

        if (!adsAvailable) {
            voltageMv = cellCount * 3850; // Tensão nominal de armazenamento (~3.85V/cel)
            calculatePercentage();
            currentAmps = 0.0f;
            return;
        }

        // 1. Leituras via ADS1115 (16-bit)
        int16_t rawVolt = ads.readADC_SingleEnded(ADS_CH_BATT_VOLT);
        int16_t rawCurr = ads.readADC_SingleEnded(ADS_CH_BATT_CURR);

        float adcVoltV = ads.computeVolts(rawVolt);
        float adcCurrV = ads.computeVolts(rawCurr);

        // 2. Fatores de conversão do Módulo APM 90A
        const float VOLT_DIVIDER_RATIO = 10.803f;
        const float AMPS_PER_VOLT      = 27.32f;
        const float CURR_OFFSET_V      = 0.015f;

        // 3. Tensão real da bateria
        float batteryV = adcVoltV * VOLT_DIVIDER_RATIO;
        if (batteryV < 0.0f) batteryV = 0.0f;
        voltageMv = static_cast<uint16_t>(batteryV * 1000.0f);

        // 4. Corrente em Amperes
        if (adcCurrV > CURR_OFFSET_V) {
            currentAmps = (adcCurrV - CURR_OFFSET_V) * AMPS_PER_VOLT;
        } else {
            currentAmps = 0.0f;
        }

        consumedMah += (currentAmps * 1000.0f) * dtHours;
        calculatePercentage();
    }

    uint16_t getVoltageMv() const { return voltageMv; }
    uint8_t  getPercentage() const { return batteryPct; }
    float    getCurrentAmps() const { return currentAmps; }
    float    getConsumedMah() const { return consumedMah; }
    float    getCellAverageV() const { return (cellCount > 0) ? (voltageMv / 1000.0f) / (float)cellCount : 0.0f; }
    bool     isSensorReady() const { return useDirectAdc || adsAvailable; }

private:
    Adafruit_ADS1115 ads;
    uint16_t voltageMv;
    uint8_t  batteryPct;
    float    currentAmps;
    float    consumedMah;
    uint32_t lastUpdateMs;
    bool     adsAvailable;
    bool     useDirectAdc = false;
    uint8_t  cellCount = 4;
    uint16_t capacityMah = 2650;
};
