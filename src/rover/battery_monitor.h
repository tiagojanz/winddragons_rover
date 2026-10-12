#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include "config_common.h"

// ==============================================================================
// APM Power Module (28V / 90A) Monitor via ADS1115 16-Bit I2C ADC (0x48)
// ==============================================================================
// - Bateria 4S: 13.2V (vazia / corte seguro) a 16.8V (totalmente carregada)
// - Divisor de tensão APM: rácio 10.101 (16.8V entra -> ~1.66V no pino V)
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
          adsAvailable(false) {}

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

    void update() {
        uint32_t now = millis();
        if (lastUpdateMs == 0) lastUpdateMs = now;
        float dtHours = (now - lastUpdateMs) / 3600000.0f;
        lastUpdateMs = now;

        if (!adsAvailable) {
            // Se o módulo ADS1115 ainda não estiver fisicamente conectado na bancada:
            // mantem valores nominais saudáveis para testes
            voltageMv = 16400;
            batteryPct = 90;
            currentAmps = 0.0f;
            return;
        }

        // 1. Leituras via ADS1115 (16-bit)
        int16_t rawVolt = ads.readADC_SingleEnded(ADS_CH_BATT_VOLT);
        int16_t rawCurr = ads.readADC_SingleEnded(ADS_CH_BATT_CURR);

        float adcVoltV = ads.computeVolts(rawVolt);
        float adcCurrV = ads.computeVolts(rawCurr);

        // 2. Fatores de conversão do Módulo APM 90A
        const float VOLT_DIVIDER_RATIO = 10.101f;
        const float AMPS_PER_VOLT      = 27.32f;
        const float CURR_OFFSET_V      = 0.015f; // Pequeno offset residual típico a 0A

        // 3. Tensão real da bateria 4S
        float batteryV = adcVoltV * VOLT_DIVIDER_RATIO;
        if (batteryV < 0.0f) batteryV = 0.0f;
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
    bool     isSensorReady() const { return adsAvailable; }

private:
    Adafruit_ADS1115 ads;
    uint16_t voltageMv;
    uint8_t  batteryPct;
    float    currentAmps;
    float    consumedMah;
    uint32_t lastUpdateMs;
    bool     adsAvailable;
};
