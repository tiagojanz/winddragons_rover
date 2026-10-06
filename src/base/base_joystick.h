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
        pinMode(PIN_JOYSTICK_SW, INPUT_PULLUP);

        // Optional anchor buttons
        pinMode(PIN_BTN_ANCHOR_UP, INPUT_PULLUP);
        pinMode(PIN_BTN_ANCHOR_DOWN, INPUT_PULLUP);
    }

    bool isClicked() const {
        return digitalRead(PIN_JOYSTICK_SW) == LOW;
    }

    bool wasShortClicked() {
        if (shortPressOccurred) {
            shortPressOccurred = false;
            return true;
        }
        return false;
    }

    bool wasLongClicked() {
        if (longPressOccurred) {
            longPressOccurred = false;
            return true;
        }
        return false;
    }

    // Menu navigation helper: returns -1 (UP), +1 (DOWN), or 0 (neutral)
    int8_t getMenuNavStep() {
        uint32_t now = millis();
        if (now - lastMenuStepMs < 250) return 0; // Repeat rate limit

        if (throttle > 45) { // Pushed forward / up
            lastMenuStepMs = now;
            return -1;
        } else if (throttle < -45) { // Pushed backward / down
            lastMenuStepMs = now;
            return 1;
        }
        return 0;
    }

    void update() {
        // 1. Read Joystick ADC
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

        // 2. Handle Joystick Switch Click (Debounced Short & Long Press)
        bool btnState = (digitalRead(PIN_JOYSTICK_SW) == LOW);
        uint32_t now = millis();

        if (btnState && !lastBtnState) {
            // Button just pressed down
            pressStartTime = now;
            longPressFired = false;
        } else if (btnState && lastBtnState) {
            // Button currently held down
            if (!longPressFired && (now - pressStartTime >= 700)) {
                longPressFired = true;
                longPressOccurred = true;
            }
        } else if (!btnState && lastBtnState) {
            // Button just released
            if (!longPressFired && (now - pressStartTime >= 40)) {
                shortPressOccurred = true;
            }
        }
        lastBtnState = btnState;
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

    // Switch press tracking
    bool lastBtnState = false;
    bool longPressFired = false;
    bool shortPressOccurred = false;
    bool longPressOccurred = false;
    uint32_t pressStartTime = 0;
    uint32_t lastMenuStepMs = 0;
};
