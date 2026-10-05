#pragma once

#include <Arduino.h>
#include <ESP32Servo.h>
#include "config_common.h"
#include "lora_protocol.h"

class ActuatorController {
public:
    ActuatorController() 
        : currentThrottle(0), currentRudder(0),
          anchorState(ANCHOR_RETRACTED), currentDepthCm(0),
          targetDepthCm(0), winchSpeedCmPerSec(20.0f),
          lastWinchUpdate(0) {}

    void begin() {
        // Allocate timers for ESP32Servo
        ESP32PWM::allocateTimer(0);
        ESP32PWM::allocateTimer(1);
        ESP32PWM::allocateTimer(2);

        motorServo.setPeriodHertz(50);
        rudderServo.setPeriodHertz(50);
        winchServo.setPeriodHertz(50);

        motorServo.attach(PIN_MOTOR_ESC, PWM_PULSE_MIN, PWM_PULSE_MAX);
        rudderServo.attach(PIN_SERVO_RUD, PWM_PULSE_MIN, PWM_PULSE_MAX);
        winchServo.attach(PIN_WINCH, PWM_PULSE_MIN, PWM_PULSE_MAX);

        stopMotor();
        centerRudder();
        stopWinch();
    }

    // Throttle: -100 to +100 (%)
    void setThrottle(int8_t throttlePct) {
        currentThrottle = constrain(throttlePct, -100, 100);
        // Map -100..+100 to 1000..2000 us (1500 is neutral)
        int pulseUs = map(currentThrottle, -100, 100, PWM_PULSE_MIN, PWM_PULSE_MAX);
        motorServo.writeMicroseconds(pulseUs);
    }

    // Rudder: -100 (full left) to +100 (full right)
    void setRudder(int8_t rudderPct) {
        currentRudder = constrain(rudderPct, -100, 100);
        int pulseUs = map(currentRudder, -100, 100, PWM_PULSE_MIN, PWM_PULSE_MAX);
        rudderServo.writeMicroseconds(pulseUs);
    }

    void stopMotor() {
        setThrottle(0);
    }

    void centerRudder() {
        setRudder(0);
    }

    // Manual winch jog: -1 = lower/drop, 0 = stop, 1 = raise/retract
    void jogWinch(int8_t direction) {
        if (direction < 0) {
            // Drop anchor
            winchServo.writeMicroseconds(1200); // Continuous forward
            anchorState = ANCHOR_DEPLOYING;
        } else if (direction > 0) {
            // Retract anchor
            winchServo.writeMicroseconds(1800); // Continuous reverse
            anchorState = ANCHOR_RETRACTING;
        } else {
            stopWinch();
        }
    }

    void stopWinch() {
        winchServo.writeMicroseconds(PWM_PULSE_MID);
        if (anchorState == ANCHOR_DEPLOYING || anchorState == ANCHOR_RETRACTING) {
            anchorState = (currentDepthCm > 10) ? ANCHOR_DEPLOYED : ANCHOR_RETRACTED;
        }
    }

    void commandDropAnchor(uint16_t depthCm) {
        targetDepthCm = (depthCm > 0) ? depthCm : 300; // default 3 meters
        anchorState = ANCHOR_DEPLOYING;
        winchServo.writeMicroseconds(1200);
        lastWinchUpdate = millis();
    }

    void commandRetractAnchor() {
        targetDepthCm = 0;
        anchorState = ANCHOR_RETRACTING;
        winchServo.writeMicroseconds(1800);
        lastWinchUpdate = millis();
    }

    void update() {
        uint32_t now = millis();
        if (lastWinchUpdate == 0) lastWinchUpdate = now;
        float dt = (now - lastWinchUpdate) / 1000.0f;
        lastWinchUpdate = now;

        if (anchorState == ANCHOR_DEPLOYING) {
            currentDepthCm += (winchSpeedCmPerSec * dt);
            if (currentDepthCm >= targetDepthCm) {
                currentDepthCm = targetDepthCm;
                stopWinch();
                anchorState = ANCHOR_DEPLOYED;
            }
        } else if (anchorState == ANCHOR_RETRACTING) {
            if (currentDepthCm > (winchSpeedCmPerSec * dt)) {
                currentDepthCm -= (winchSpeedCmPerSec * dt);
            } else {
                currentDepthCm = 0;
                stopWinch();
                anchorState = ANCHOR_RETRACTED;
            }
        }
    }

    uint8_t getAnchorStatus() const {
        return anchorState;
    }

    uint16_t getAnchorDepthCm() const {
        return static_cast<uint16_t>(currentDepthCm);
    }

    int8_t getThrottle() const { return currentThrottle; }
    int8_t getRudder() const { return currentRudder; }

private:
    Servo motorServo;
    Servo rudderServo;
    Servo winchServo;

    int8_t currentThrottle;
    int8_t currentRudder;

    uint8_t anchorState;
    float currentDepthCm;
    uint16_t targetDepthCm;
    float winchSpeedCmPerSec;
    uint32_t lastWinchUpdate;
};
