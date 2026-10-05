#pragma once

#include <Arduino.h>
#include "config_common.h"

class BaseJoystick {
public:
    BaseJoystick() 
        : rawX(2048), rawY(2048),
          throttle(0), rudder(0),
          deadband(180) {}

    void begin() {
        analogReadResolution(12); // 0 to 4095
        pinMode(PIN_JOYSTICK_X, INPUT);
        pinMode(PIN_JOYSTICK_Y, INPUT);

        // Optional anchor buttons
        pinMode(PIN_BTN_ANCHOR_UP, INPUT_PULLUP);
        pinMode(PIN_BTN_ANCHOR_DOWN, INPUT_PULLUP);
    }

    void update() {
        // Read ADC
        int currentX = analogRead(PIN_JOYSTICK_X);
        int currentY = analogRead(PIN_JOYSTICK_Y);

        // Low-pass filter
        rawX = (rawX * 3 + currentX) / 4;
        rawY = (rawY * 3 + currentY) / 4;

        // Calculate offset from center (2048)
        int diffX = rawX - 2048;
        int diffY = rawY - 2048;

        // Apply deadband
        if (abs(diffX) < deadband) diffX = 0;
        if (abs(diffY) < deadband) diffY = 0;

        // Map to -100 to +100
        throttle = constrain(diffX * 100 / (2048 - deadband), -100, 100);
        rudder   = constrain(diffY * 100 / (2048 - deadband), -100, 100);
    }

    int8_t getThrottle() const { return throttle; }
    int8_t getRudder() const { return rudder; }

    int8_t getAnchorJog() const {
        if (digitalRead(PIN_BTN_ANCHOR_DOWN) == LOW) return -1; // Lower
        if (digitalRead(PIN_BTN_ANCHOR_UP) == LOW)   return 1;  // Raise
        return 0; // Stop
    }

private:
    int rawX;
    int rawY;
    int8_t throttle;
    int8_t rudder;
    int deadband;
};
