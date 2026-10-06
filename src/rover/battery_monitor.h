#pragma once

#include <Arduino.h>
#include "config_common.h"

// ==============================================================================
// APM Power Module (28V / 90A) Monitor for 4S LiPo Battery
// ==============================================================================
// - Bateria 4S: 13.2V (vazia / corte seguro) a 16.8V (totalmente carregada)
// - Divisor de tensão APM: rácio 10.101 (16.8V entra -> ~1.66V no pino V)
// - Sensor de corrente APM: ~27.32 A/V (90A a 3.3V)
// ==============================================================================

class BatteryMonitor {
public:
    BatteryMonitor()
        : voltageMv(16800),
          batteryPct(100),
          currentAmps(0.0f),
          consumedMah(0.0f),
          lastUpdateMs(0) {}

    void begin() {
        analogReadResolution(12); // ADC a 12 bits (0-4095)
        pinMode(PIN_BATTERY_VOLT_ADC, INPUT);
        pinMode(PIN_BATTERY_CURR_ADC, INPUT);
        lastUpdateMs = millis();
        update(); // Primeira leitura inicial
    }

    void update() {
        uint32_t now = millis();
        if (lastUpdateMs == 0) lastUpdateMs = now;
        float dtHours = (now - lastUpdateMs) / 3600000.0f;
        lastUpdateMs = now;

        // 1. Filtragem por sobreamostragem (16 leituras) para rejeitar ruído PWM
        const int SAMPLES = 16;
        uint32_t sumVoltMv = 0;
        uint32_t sumCurrMv = 0;

        for (int i = 0; i < SAMPLES; i++) {
            sumVoltMv += analogReadMilliVolts(PIN_BATTERY_VOLT_ADC);
            sumCurrMv += analogReadMilliVolts(PIN_BATTERY_CURR_ADC);
            delayMicroseconds(50);
        }

        float adcVoltV = (sumVoltMv / (float)SAMPLES) / 1000.0f;
        float adcCurrV = (sumCurrMv / (float)SAMPLES) / 1000.0f;

        // 2. Fatores de conversão do Módulo APM 90A
        const float VOLT_DIVIDER_RATIO = 10.101f;
        const float AMPS_PER_VOLT      = 27.32f;
        const float CURR_OFFSET_V      = 0.015f; // Pequeno offset residual típico a 0A

        // 3. Tensão real da bateria 4S
        float batteryV = adcVoltV * VOLT_DIVIDER_RATIO;
        voltageMv = static_cast<uint16_t>(batteryV * 1000.0f);

        // 4. Corrente em Amperes
        if (adcCurrV > CURR_OFFSET_V) {
            currentAmps = (adcCurrV - CURR_OFFSET_V) * AMPS_PER_VOLT;
        } else {
            currentAmps = 0.0f;
        }

        // 5. Integração temporal do consumo acumulado (mAh)
        consumedMah += (currentAmps * 1000.0f) * dtHours;

        // 6. Percentagem para LiPo 4S (13.2V = 0%, 16.8V = 100%)
        const uint16_t V_MIN_4S_MV = 13200; // 3.30V por célula
        const uint16_t V_MAX_4S_MV = 16800; // 4.20V por célula

        if (voltageMv >= V_MAX_4S_MV) {
            batteryPct = 100;
        } else if (voltageMv <= V_MIN_4S_MV) {
            batteryPct = 0;
        } else {
            batteryPct = static_cast<uint8_t>(
                ((uint32_t)(voltageMv - V_MIN_4S_MV) * 100) / (V_MAX_4S_MV - V_MIN_4S_MV)
            );
        }
    }

    uint16_t getVoltageMv() const { return voltageMv; }
    uint8_t  getPercentage() const { return batteryPct; }
    float    getCurrentAmps() const { return currentAmps; }
    float    getConsumedMah() const { return consumedMah; }
    float    getCellAverageV() const { return (voltageMv / 1000.0f) / 4.0f; }

private:
    uint16_t voltageMv;
    uint8_t  batteryPct;
    float    currentAmps;
    float    consumedMah;
    uint32_t lastUpdateMs;
};
