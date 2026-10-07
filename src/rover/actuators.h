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
          targetDepthCm(0), winchRetractSpeedCmPerSec(20.0f),
          gravityDropSpeedCmPerSec(80.0f),
          lastWinchUpdate(0), isRackLifted(false),
          alarmActive(false), alarmEndTimeMs(0),
          lastAlarmToggleMs(0), alarmStrobeState(false),
          buzzerHardwareEnabled(false) {}

    void begin() {
        // Reservar explicitamente Timers 0 e 1 para os servos (50Hz)
        // Isso impede a biblioteca ESP32Servo de se sobrepor ao Timer do display ST7789 (Canal 5, 1000Hz)
        ESP32PWM::allocateTimer(0);
        ESP32PWM::allocateTimer(1);

        motorServo.setPeriodHertz(50);
        rudderServo.setPeriodHertz(50);
        winchServo.setPeriodHertz(50);
        rackServo.setPeriodHertz(50);

        motorServo.attach(PIN_MOTOR_ESC, PWM_PULSE_MIN, PWM_PULSE_MAX);
        rudderServo.attach(PIN_SERVO_RUD, PWM_PULSE_MIN, PWM_PULSE_MAX);
        winchServo.attach(PIN_WINCH, PWM_PULSE_MIN, PWM_PULSE_MAX);
        rackServo.attach(PIN_SERVO_RACK, PWM_PULSE_MIN, PWM_PULSE_MAX);

        #if ARDUINO_USB_CDC_ON_BOOT
        // No ESP32-C6, GPIO 13 é USB D+. Se o cabo USB estiver ligado ao PC, protege o pino para não derrubar o CDC.
        // Se estiver em operação autónoma (alimentado por bateria), ativa o buzzer no pino 13!
        if (PIN_ALARM_BUZZER >= 0) {
            if (!HWCDC::isPlugged()) {
                buzzerHardwareEnabled = true;
                pinMode(PIN_ALARM_BUZZER, OUTPUT);
                digitalWrite(PIN_ALARM_BUZZER, LOW);
            } else {
                buzzerHardwareEnabled = false;
            }
        }
        #else
        if (PIN_ALARM_BUZZER >= 0) {
            buzzerHardwareEnabled = true;
            pinMode(PIN_ALARM_BUZZER, OUTPUT);
            digitalWrite(PIN_ALARM_BUZZER, LOW);
        }
        #endif

        stopMotor();
        centerRudder();
        engageRack(); // Trava a cremalheira na inicialização
        stopWinch();
        stopAlarm();
    }

    // --- Cremalheira / Rack Release Servo Controls ---
    // Levanta a cremalheira desengatando o carretel para a âncora cair livremente por gravidade
    void liftRack() {
        rackServo.write(RACK_POS_RELEASED);
        isRackLifted = true;
    }
    void releaseRack() { liftRack(); }

    // Baixa a cremalheira travando/engatando o carretel para travar a profundidade ou recolher
    void engageRack() {
        rackServo.write(RACK_POS_ENGAGED);
        isRackLifted = false;
    }

    bool isRackReleased() const { return isRackLifted; }

    void emergencyStop() {
        stopMotor();
        centerRudder();
        stopWinch();
    }

    // Trigger visual/acoustic alarm for a duration in seconds
    void triggerAlarm(uint16_t durationSeconds = 10) {
        alarmActive = true;
        alarmEndTimeMs = millis() + (static_cast<uint32_t>(durationSeconds) * 1000UL);
        lastAlarmToggleMs = millis();
        alarmStrobeState = true;
        if (buzzerHardwareEnabled && PIN_ALARM_BUZZER >= 0) {
            digitalWrite(PIN_ALARM_BUZZER, HIGH);
        }
    }

    void stopAlarm() {
        alarmActive = false;
        alarmStrobeState = false;
        if (buzzerHardwareEnabled && PIN_ALARM_BUZZER >= 0) {
            digitalWrite(PIN_ALARM_BUZZER, LOW);
        }
    }

    bool isAlarmActive() const { return alarmActive; }
    bool getAlarmStrobeState() const { return alarmStrobeState; }

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

    // Manual winch jog: -1 = drop por gravidade (levanta cremalheira), 0 = travar, 1 = içar/recolher
    void jogWinch(int8_t direction) {
        if (direction < 0) {
            // Levanta a cremalheira para queda livre por gravidade
            liftRack();
            winchServo.writeMicroseconds(PWM_PULSE_MID);
            anchorState = ANCHOR_DEPLOYING;
        } else if (direction > 0) {
            // Engata cremalheira e aciona motor do guincho para içar
            engageRack();
            winchServo.writeMicroseconds(1800); // Continuous reverse
            anchorState = ANCHOR_RETRACTING;
        } else {
            stopWinch();
        }
    }

    void stopWinch() {
        engageRack(); // Trava a cremalheira
        winchServo.writeMicroseconds(PWM_PULSE_MID);
        if (anchorState == ANCHOR_DEPLOYING || anchorState == ANCHOR_RETRACTING) {
            anchorState = (currentDepthCm > 10) ? ANCHOR_DEPLOYED : ANCHOR_RETRACTED;
        }
    }

    // Lança a âncora levantando a cremalheira para queda por gravidade
    void commandDropAnchor(uint16_t depthCm) {
        targetDepthCm = (depthCm > 0) ? depthCm : 300; // default 3 meters
        anchorState = ANCHOR_DEPLOYING;
        winchServo.writeMicroseconds(PWM_PULSE_MID); // Motor livre
        liftRack(); // Levanta a cremalheira para soltar o carretel
        lastWinchUpdate = millis();
    }

    // Recolhe a âncora: engata a cremalheira e enrola o cabo
    void commandRetractAnchor() {
        targetDepthCm = 0;
        anchorState = ANCHOR_RETRACTING;
        engageRack(); // Engata a cremalheira
        winchServo.writeMicroseconds(1800);
        lastWinchUpdate = millis();
    }

    void update() {
        uint32_t now = millis();
        if (lastWinchUpdate == 0) lastWinchUpdate = now;
        float dt = (now - lastWinchUpdate) / 1000.0f;
        lastWinchUpdate = now;

        // Queda por gravidade (cremalheira levantada)
        if (anchorState == ANCHOR_DEPLOYING) {
            currentDepthCm += (gravityDropSpeedCmPerSec * dt);
            if (currentDepthCm >= targetDepthCm) {
                currentDepthCm = targetDepthCm;
                engageRack(); // Baixa e engata a cremalheira para travar na profundidade atingida
                stopWinch();
                anchorState = ANCHOR_DEPLOYED;
            }
        } 
        // Içamento pelo guincho (cremalheira engatada)
        else if (anchorState == ANCHOR_RETRACTING) {
            if (currentDepthCm > (winchRetractSpeedCmPerSec * dt)) {
                currentDepthCm -= (winchRetractSpeedCmPerSec * dt);
            } else {
                currentDepthCm = 0;
                stopWinch();
                anchorState = ANCHOR_RETRACTED;
            }
        }

        // Handle alarm timeout and strobe/buzzer oscillation (100ms on / 100ms off)
        if (alarmActive) {
            if (now >= alarmEndTimeMs) {
                stopAlarm();
            } else if (now - lastAlarmToggleMs >= 100) {
                lastAlarmToggleMs = now;
                alarmStrobeState = !alarmStrobeState;
                if (buzzerHardwareEnabled && PIN_ALARM_BUZZER >= 0) {
                    digitalWrite(PIN_ALARM_BUZZER, alarmStrobeState ? HIGH : LOW);
                }
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
    Servo rackServo; // 3º servo: levanta a cremalheira para soltar a âncora

    int8_t currentThrottle;
    int8_t currentRudder;

    uint8_t anchorState;
    float currentDepthCm;
    uint16_t targetDepthCm;
    float winchRetractSpeedCmPerSec;
    float gravityDropSpeedCmPerSec;
    uint32_t lastWinchUpdate;
    bool isRackLifted;

    bool alarmActive;
    uint32_t alarmEndTimeMs;
    uint32_t lastAlarmToggleMs;
    bool alarmStrobeState;
    bool buzzerHardwareEnabled;
};
