#pragma once

#include <Arduino.h>
#include <RadioLib.h>

/**
 * @brief Dedicated Software SPI Hardware Abstraction Layer for RadioLib.
 * 
 * Bypasses the ESP32-C6 hardware SPI (FSPI) controller entirely, eliminating:
 * 1. Duplicate APB clock change callbacks (`addApbChangeCallback`)
 * 2. Bus conflicts with the ST7789 display controller
 * 3. Bus conflicts with the onboard MicroSD TF controller
 * 
 * Implements SPI Mode 0 (CPOL=0, CPHA=0) via fast GPIO bit-banging.
 */
class SoftSpiHal : public ArduinoHal {
private:
    int8_t _sck;
    int8_t _miso;
    int8_t _mosi;

public:
    SoftSpiHal(int8_t sck, int8_t miso, int8_t mosi)
        : ArduinoHal(), _sck(sck), _miso(miso), _mosi(mosi) {}

    void spiBegin() override {
        if (_sck >= 0)  { pinMode(_sck, OUTPUT); digitalWrite(_sck, HIGH); }
        if (_mosi >= 0) { pinMode(_mosi, OUTPUT); digitalWrite(_mosi, LOW); }
        if (_miso >= 0) { pinMode(_miso, INPUT_PULLUP); }
    }

    void spiBeginTransaction() override {
        if (_sck >= 0) digitalWrite(_sck, HIGH);
    }

    void spiTransfer(uint8_t* out, size_t len, uint8_t* in) override {
        for (size_t i = 0; i < len; i++) {
            uint8_t b_out = out ? out[i] : 0x00;
            uint8_t b_in = 0;
            for (int bit = 7; bit >= 0; bit--) {
                if (_mosi >= 0) {
                    digitalWrite(_mosi, (b_out & (1 << bit)) ? HIGH : LOW);
                }
                delayMicroseconds(5);
                if (_sck >= 0) digitalWrite(_sck, LOW);
                delayMicroseconds(5);
                b_in = (b_in << 1) | ((_miso >= 0 && digitalRead(_miso)) ? 1 : 0);
                if (_sck >= 0) digitalWrite(_sck, HIGH);
                delayMicroseconds(5);
            }
            if (in) {
                in[i] = b_in;
            }
        }
    }

    void spiEndTransaction() override {
        if (_sck >= 0) digitalWrite(_sck, HIGH);
    }

    void spiEnd() override {
        if (_sck >= 0)  pinMode(_sck, INPUT);
        if (_mosi >= 0) pinMode(_mosi, INPUT);
        if (_miso >= 0) pinMode(_miso, INPUT);
    }

    // Direct diagnostic function to test reading SX1278 RegVersion (0x42)
    uint8_t readReg(uint8_t reg, int8_t csPin) {
        if (csPin < 0) return 0xFF;
        pinMode(csPin, OUTPUT);
        digitalWrite(csPin, HIGH);
        delayMicroseconds(20);
        digitalWrite(csPin, LOW);
        delayMicroseconds(20);

        uint8_t tx[2] = { (uint8_t)(reg & 0x7F), 0x00 };
        uint8_t rx[2] = { 0, 0 };

        spiTransfer(tx, 2, rx);

        digitalWrite(csPin, HIGH);
        delayMicroseconds(20);
        return rx[1];
    }
};
