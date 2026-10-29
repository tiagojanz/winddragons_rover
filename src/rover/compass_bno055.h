#pragma once

#include <Arduino.h>
#include <Wire.h>
#include "config_common.h"

// =========================================================================
// DRIVER I2C NATIVO BOSCH BNO055 (GY-BNO055 9-DOF IMU + BÚSSOLA NDOF)
// Conector J_EXP na PCB: SDA = GPIO 1 (IO1), SCL = GPIO 11 (IO11)
// Modo de operação: NDOF (0x0C) — Fusão 9-eixos com compensação de
// inclinação (Pitch/Roll) das ondas integrada no processador do sensor.
// =========================================================================

class CompassBNO055 {
public:
    static constexpr uint8_t REG_CHIP_ID     = 0x00;
    static constexpr uint8_t REG_PAGE_ID     = 0x07;
    static constexpr uint8_t REG_EULER_H_LSB = 0x1A;
    static constexpr uint8_t REG_CALIB_STAT  = 0x35;
    static constexpr uint8_t REG_UNIT_SEL    = 0x3B;
    static constexpr uint8_t REG_OPR_MODE    = 0x3D;
    static constexpr uint8_t REG_PWR_MODE    = 0x3E;
    static constexpr uint8_t REG_SYS_TRIGGER = 0x3F;

    static constexpr uint8_t BNO055_ID       = 0xA0;
    static constexpr uint8_t MODE_CONFIG     = 0x00;
    static constexpr uint8_t MODE_NDOF       = 0x0C;

    bool begin(int sdaPin = PIN_C6_EXP_SDA, int sclPin = PIN_C6_EXP_SCL) {
        _sdaPin = sdaPin;
        _sclPin = sclPin;
        _busInitialized = true;

        Wire.begin(_sdaPin, _sclPin, 100000); // 100 kHz para suportar Clock Stretching do BNO055
        Wire.setTimeOut(20);                  // Timeout curto (20ms) para nunca bloquear o loop

        return tryInitializeSensor(true);
    }

    void update() {
        uint32_t now = millis();

        // Se o sensor não estiver presente (ex: ainda não ligado em J_EXP),
        // tenta detetá-lo silenciosamente a cada 5 segundos (suporta Hot-Plug)
        if (!_ready) {
            if (now - _lastRetryMs >= 5000) {
                _lastRetryMs = now;
                if (!_busInitialized) {
                    begin(_sdaPin, _sclPin);
                } else {
                    tryInitializeSensor(false);
                }
            }
            return;
        }

        // Leitura a 20 Hz (a cada 50ms)
        if (now - _lastReadMs < 50) {
            return;
        }
        _lastReadMs = now;

        uint8_t buf[6] = {0};
        if (!readBytes(REG_EULER_H_LSB, buf, 6)) {
            _failCount++;
            if (_failCount >= 10) {
                _ready = false;
                _failCount = 0;
                Serial.println("[COMPASS] AVISO: BNO055 desligado em J_EXP! Fallback para GPS COG.");
            }
            return;
        }
        _failCount = 0;

        int16_t rawH = (int16_t)((uint16_t)buf[0] | ((uint16_t)buf[1] << 8));
        int16_t rawR = (int16_t)((uint16_t)buf[2] | ((uint16_t)buf[3] << 8));
        int16_t rawP = (int16_t)((uint16_t)buf[4] | ((uint16_t)buf[5] << 8));

        // 1 grau = 16 LSB no BNO055
        _rawHeadingDeg = (float)rawH / 16.0f;
        _rollDeg       = (float)rawR / 16.0f;
        _pitchDeg      = (float)rawP / 16.0f;

        float h = _rawHeadingDeg + _headingOffsetDeg;
        while (h >= 360.0f) h -= 360.0f;
        while (h < 0.0f)    h += 360.0f;
        _headingDeg = h;

        // Ler estado de calibração a cada 500ms
        if (now - _lastCalReadMs >= 500) {
            _lastCalReadMs = now;
            uint8_t cal = 0;
            if (readByte(REG_CALIB_STAT, cal)) {
                _calSys   = (cal >> 6) & 0x03;
                _calGyro  = (cal >> 4) & 0x03;
                _calAccel = (cal >> 2) & 0x03;
                _calMag   = cal & 0x03;
            }
        }
    }

    bool isReady() const { return _ready; }
    uint8_t getI2cAddress() const { return _addr; }
    float getHeading() const { return _headingDeg; }
    float getRawHeading() const { return _rawHeadingDeg; }
    float getRoll() const { return _rollDeg; }
    float getPitch() const { return _pitchDeg; }
    float getHeadingOffset() const { return _headingOffsetDeg; }

    uint8_t getCalSys() const { return _calSys; }
    uint8_t getCalGyro() const { return _calGyro; }
    uint8_t getCalAccel() const { return _calAccel; }
    uint8_t getCalMag() const { return _calMag; }

    void setHeadingOffset(float offsetDeg) {
        while (offsetDeg > 180.0f)  offsetDeg -= 360.0f;
        while (offsetDeg < -180.0f) offsetDeg += 360.0f;
        _headingOffsetDeg = offsetDeg;
    }

    // Define a orientação atual da proa como 0.0° (Norte de referência)
    float zeroCurrentHeading() {
        float newOff = -_rawHeadingDeg;
        setHeadingOffset(newOff);
        return _headingOffsetDeg;
    }

private:
    int _sdaPin = PIN_C6_EXP_SDA;
    int _sclPin = PIN_C6_EXP_SCL;
    uint8_t _addr = BNO055_I2C_ADDR_A;
    bool _busInitialized = false;
    bool _ready = false;
    uint8_t _failCount = 0;
    uint32_t _lastReadMs = 0;
    uint32_t _lastCalReadMs = 0;
    uint32_t _lastRetryMs = 0;

    float _rawHeadingDeg = 0.0f;
    float _headingDeg = 0.0f;
    float _rollDeg = 0.0f;
    float _pitchDeg = 0.0f;
    float _headingOffsetDeg = 0.0f;

    uint8_t _calSys = 0;
    uint8_t _calGyro = 0;
    uint8_t _calAccel = 0;
    uint8_t _calMag = 0;

    bool tryInitializeSensor(bool verbose) {
        const uint8_t candidates[2] = { BNO055_I2C_ADDR_A, BNO055_I2C_ADDR_B };
        uint8_t foundAddr = 0;

        for (uint8_t a : candidates) {
            _addr = a;
            uint8_t id = 0;
            if (readByte(REG_CHIP_ID, id) && id == BNO055_ID) {
                foundAddr = a;
                break;
            }
        }

        if (foundAddr == 0) {
            _ready = false;
            if (verbose) {
                Serial.printf("[COMPASS] GY-BNO055 ainda nao detetado em J_EXP (SDA=GPIO%d, SCL=GPIO%d) -> Fallback GPS COG ativo\n",
                              _sdaPin, _sclPin);
            }
            return false;
        }

        _addr = foundAddr;

        // 1. Entrar em CONFIGMODE
        writeByte(REG_OPR_MODE, MODE_CONFIG);
        delay(25);

        // 2. Selecionar Página 0 de registos
        writeByte(REG_PAGE_ID, 0x00);

        // 3. Modo de energia Normal
        writeByte(REG_PWR_MODE, 0x00);
        delay(10);

        // 4. Unidades padrão (Graus, m/s^2, dps)
        writeByte(REG_UNIT_SEL, 0x00);

        // 5. Ativar cristal externo de 32.768 kHz presente na placa GY-BNO055
        writeByte(REG_SYS_TRIGGER, 0x80);
        delay(10);

        // 6. Ativar modo NDOF (Fusão 9-DOF: Acelerómetro + Giroscópio + Magnetómetro)
        writeByte(REG_OPR_MODE, MODE_NDOF);
        delay(25);

        _ready = true;
        _failCount = 0;
        Serial.printf("[COMPASS] ★★★ GY-BNO055 DETETADO E ATIVO em J_EXP (I2C 0x%02X, SDA=GPIO%d, SCL=GPIO%d | Modo NDOF 9-Eixos | Offset=%.1f°) ★★★\n",
                      _addr, _sdaPin, _sclPin, _headingOffsetDeg);
        return true;
    }

    bool writeByte(uint8_t reg, uint8_t val) {
        Wire.beginTransmission(_addr);
        Wire.write(reg);
        Wire.write(val);
        return (Wire.endTransmission() == 0);
    }

    bool readByte(uint8_t reg, uint8_t &val) {
        Wire.beginTransmission(_addr);
        Wire.write(reg);
        if (Wire.endTransmission(false) != 0) {
            return false;
        }
        if (Wire.requestFrom(_addr, (uint8_t)1) != 1) {
            return false;
        }
        val = Wire.read();
        return true;
    }

    bool readBytes(uint8_t reg, uint8_t *buf, uint8_t len) {
        Wire.beginTransmission(_addr);
        Wire.write(reg);
        if (Wire.endTransmission(false) != 0) {
            return false;
        }
        if (Wire.requestFrom(_addr, len) != len) {
            return false;
        }
        for (uint8_t i = 0; i < len; i++) {
            buf[i] = Wire.read();
        }
        return true;
    }
};

