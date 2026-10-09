#pragma once

#include <Arduino.h>
#include <ESP32Servo.h>
#include "config_common.h"
#include "lora_protocol.h"

class ActuatorController {
public:
    ActuatorController() 
        : currentThrottle(0), currentRudder(0),
          winchNeutralUs(1500), currentWinchPulseUs(1500),
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

        int chMotor = motorServo.attach(PIN_MOTOR_ESC, PWM_PULSE_MIN, PWM_PULSE_MAX);
        int chRud   = rudderServo.attach(PIN_SERVO_RUD, PWM_PULSE_MIN, PWM_PULSE_MAX);
        int chRack  = rackServo.attach(PIN_SERVO_RACK, PWM_PULSE_MIN, PWM_PULSE_MAX);

        // O guincho arranca 100% desligado (INPUT, sem periférico PWM ligado, igual ao estado de Reset)
        pinMode(PIN_WINCH, INPUT);
        currentWinchPulseUs = 0;

        Serial.printf("[ACT] Servos iniciados:\n");
        Serial.printf("      - Motor ESC: GPIO %d (Canal %d)\n", PIN_MOTOR_ESC, chMotor);
        Serial.printf("      - Leme: GPIO %d (Canal %d)\n", PIN_SERVO_RUD, chRud);
        Serial.printf("      - Guincho: GPIO %d (Modo ON/OFF - Desligado no arranque)\n", PIN_WINCH);
        Serial.printf("      - Cremalheira / Largar: GPIO %d (Canal %d)\n", PIN_SERVO_RACK, chRack);

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
        Serial.printf("[ACT-SERVO] LIFT RACK (LARGAR ANCORA) -> GPIO %d enviado %d graus\n", PIN_SERVO_RACK, RACK_POS_RELEASED);
    }
    void releaseRack() { liftRack(); }

    // Baixa a cremalheira travando/engatando o carretel para travar a profundidade ou recolher
    void engageRack() {
        rackServo.write(RACK_POS_ENGAGED);
        isRackLifted = false;
        Serial.printf("[ACT-SERVO] ENGAGE RACK (TRAVAR ANCORA) -> GPIO %d enviado %d graus\n", PIN_SERVO_RACK, RACK_POS_ENGAGED);
    }

    // Define ângulo direto para calibração mecânica (0 a 180 graus)
    void setRackAngle(uint8_t angle) {
        if (angle > 180) angle = 180;
        rackServo.write(angle);
        isRackLifted = (angle > 45);
        Serial.printf("[ACT-SERVO] CALIBRACAO -> GPIO %d enviado %d graus\n", PIN_SERVO_RACK, angle);
    }

    // Desativa o pulso do servo para relaxar o motor e evitar aquecimento em stall
    void relaxRack() {
        rackServo.release();
        Serial.printf("[ACT-SERVO] RELAX -> GPIO %d pulso PWM desligado (motor solto/frio)\n", PIN_SERVO_RACK);
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
        Serial.printf("[ACT-MOTOR] ESC Throttle: %d%% (%d us no GPIO %d)\n", currentThrottle, pulseUs, PIN_MOTOR_ESC);
    }

    // Rudder: -100 (full left) to +100 (full right)
    void setRudder(int8_t rudderPct) {
        currentRudder = constrain(rudderPct, -100, 100);
        int pulseUs = map(currentRudder, -100, 100, PWM_PULSE_MIN, PWM_PULSE_MAX);
        rudderServo.writeMicroseconds(pulseUs);
        Serial.printf("[ACT-LEME] Leme: %d%% (%d us no GPIO %d)\n", currentRudder, pulseUs, PIN_SERVO_RUD);
    }

    void stopMotor() {
        setThrottle(0);
    }

    void centerRudder() {
        setRudder(0);
    }

    // Corta por completo o sinal PWM no GPIO 19 (0V flat / sem pulsos / exatamente como no Reset)
    void releaseWinch() {
        if (winchServo.attached()) {
            winchServo.detach();
        }
        pinMode(PIN_WINCH, INPUT);
        currentWinchPulseUs = 0;
        Serial.printf("[ACT-GUINCHO] SINAL TOTALMENTE CORTADO (DETACH / INPUT / 0V no GPIO %d)\n", PIN_WINCH);
    }

    // Manual winch jog: -1 = desenrolar/descer cabo (1200us), 0 = desligar sinal (OFF), 1 = enrolar/subir (1800us)
    void jogWinch(int8_t direction) {
        if (direction < 0) {
            setWinchMicroseconds(1200);
            Serial.printf("[ACT-GUINCHO] LIGADO (DESCE / DESENROLAR) -> 1200 us no GPIO %d\n", PIN_WINCH);
        } else if (direction > 0) {
            setWinchMicroseconds(1800);
            Serial.printf("[ACT-GUINCHO] LIGADO (SOBE / ENROLAR) -> 1800 us no GPIO %d\n", PIN_WINCH);
        } else {
            stopWinch();
        }
    }

    void setWinchMicroseconds(int pulseUs) {
        if (pulseUs <= 0) {
            releaseWinch();
            return;
        }
        pulseUs = constrain(pulseUs, 800, 2200);
        currentWinchPulseUs = pulseUs;
        if (!winchServo.attached()) {
            winchServo.attach(PIN_WINCH, 800, 2200);
        }
        winchServo.writeMicroseconds(pulseUs);
        Serial.printf("[ACT-GUINCHO] SINAL ATIVO (ON): %d us no GPIO %d\n", pulseUs, PIN_WINCH);
    }

    void setWinchNeutral(int neutralUs) {
        winchNeutralUs = constrain(neutralUs, 900, 2100);
        Serial.printf("[ACT-GUINCHO] PONTO NEUTRO DEFINIDO: %d us no GPIO %d\n", winchNeutralUs, PIN_WINCH);
        stopWinch();
    }

    int getWinchNeutral() const { return winchNeutralUs; }
    int getWinchPulseUs() const { return currentWinchPulseUs; }
    bool isWinchRunning() const { return (currentWinchPulseUs > 0); }

    void stopWinch() {
        releaseWinch(); // Corta o sinal a 100% (0V) para o motor parar
        if (anchorState == ANCHOR_DEPLOYING || anchorState == ANCHOR_RETRACTING) {
            anchorState = (currentDepthCm > 10) ? ANCHOR_DEPLOYED : ANCHOR_RETRACTED;
        }
    }

    // Lança a âncora levantando a cremalheira para queda por gravidade
    void commandDropAnchor(uint16_t depthCm) {
        targetDepthCm = (depthCm > 0) ? depthCm : 300; // default 3 meters
        currentDepthCm = 0; // Reinicia para permitir descida completa
        anchorState = ANCHOR_DEPLOYING;
        winchServo.writeMicroseconds(PWM_PULSE_MID); // Motor livre
        liftRack(); // Levanta a cremalheira para soltar o carretel
        lastWinchUpdate = millis();
        Serial.printf("[ACT-ANCORA] DROP ANCHOR -> Alvo: %u cm | Cremalheira ABERTA no GPIO %d\n", targetDepthCm, PIN_SERVO_RACK);
    }

    // Recolhe a âncora: engata a cremalheira e enrola o cabo
    void commandRetractAnchor() {
        targetDepthCm = 0;
        anchorState = ANCHOR_RETRACTING;
        if (currentDepthCm <= 0) {
            currentDepthCm = 300; // Margem para teste de bancada não desligar instantaneamente
        }
        engageRack(); // Engata a cremalheira
        winchServo.writeMicroseconds(1800);
        lastWinchUpdate = millis();
        Serial.printf("[ACT-ANCORA] RETRACT ANCHOR -> Guincho 1800us no GPIO %d | Cremalheira TRAVADA no GPIO %d\n", PIN_WINCH, PIN_SERVO_RACK);
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
                Serial.printf("[ACT-ANCORA] DROP CONCLUIDO -> Profundidade %u cm atingida. A travar cremalheira.\n", targetDepthCm);
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
                Serial.printf("[ACT-ANCORA] RECOLHA CONCLUIDA -> Âncora no topo. Guincho parado.\n");
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
    int winchNeutralUs;
    int currentWinchPulseUs;

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
