#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include "config_common.h"
#include "lora_protocol.h"

class ActuatorController {
public:
    ActuatorController() 
        : pwm(PCA9685_I2C_ADDR, Wire),
          currentThrottle(0), currentRudder(0),
          winchNeutralUs(1500), currentWinchPulseUs(0),
          anchorState(ANCHOR_RETRACTED), currentDepthCm(0),
          targetDepthCm(0), winchRetractSpeedCmPerSec(20.0f),
          gravityDropSpeedCmPerSec(80.0f),
          lastWinchUpdate(0), isRackLifted(false),
          alarmActive(false), alarmEndTimeMs(0),
          lastAlarmToggleMs(0), alarmStrobeState(false),
          pwmAvailable(false) {}

    void begin() {
        Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
        
        // Inicializa o sensor de fim de curso do içamento da âncora (J5 - GPIO 13)
        #if ARDUINO_USB_CDC_ON_BOOT
        // No ESP32-C6, GPIO 13 é USB D+. Ativar apenas se não estiver em conflito
        if (!HWCDC::isPlugged()) {
            pinMode(PIN_ANCHOR_DETECTOR, INPUT_PULLUP);
        }
        #else
        pinMode(PIN_ANCHOR_DETECTOR, INPUT_PULLUP);
        #endif

        if (pwm.begin()) {
            pwm.setPWMFreq(50); // 50 Hz para servos e ESCs normais (período de 20ms)
            pwmAvailable = true;
            Serial.println(F("[ACT] PCA9685 16-Ch PWM detectado com sucesso em 0x40!"));
            Serial.printf("      - Motor ESC: Canal PCA %d\n", PCA_CH_MOTOR);
            Serial.printf("      - Leme: Canal PCA %d\n", PCA_CH_RUDDER);
            Serial.printf("      - Guincho: Canal PCA %d\n", PCA_CH_WINCH);
            Serial.printf("      - Cremalheira / Largar: Canal PCA %d\n", PCA_CH_RACK);
            Serial.printf("      - Detector Âncora Fim de Curso: GPIO %d\n", PIN_ANCHOR_DETECTOR);
        } else {
            pwmAvailable = false;
            Serial.println(F("[ACT-AVISO] PCA9685 nao encontrado em 0x40! Verifique as ligacoes I2C."));
        }

        stopMotor();
        centerRudder();
        engageRack(); // Trava a cremalheira na inicialização
        stopWinch();
        stopAlarm();
    }

    // --- Cremalheira / Rack Release Servo Controls ---
    // Levanta a cremalheira desengatando o carretel para a âncora cair livremente por gravidade
    void liftRack() {
        setRackAngle(RACK_POS_RELEASED);
        isRackLifted = true;
        Serial.printf("[ACT-SERVO] LIFT RACK (LARGAR ANCORA) -> PCA CH %d enviado %d graus\n", PCA_CH_RACK, RACK_POS_RELEASED);
    }
    void releaseRack() { liftRack(); }

    // Baixa a cremalheira travando/engatando o carretel para travar a profundidade ou recolher
    void engageRack() {
        setRackAngle(RACK_POS_ENGAGED);
        isRackLifted = false;
        Serial.printf("[ACT-SERVO] ENGAGE RACK (TRAVAR ANCORA) -> PCA CH %d enviado %d graus\n", PCA_CH_RACK, RACK_POS_ENGAGED);
    }

    // Define ângulo direto para calibração mecânica (0 a 180 graus)
    void setRackAngle(uint8_t angle) {
        if (angle > 180) angle = 180;
        int pulseUs = map(angle, 0, 180, PWM_PULSE_MIN, PWM_PULSE_MAX);
        if (pwmAvailable) {
            pwm.writeMicroseconds(PCA_CH_RACK, pulseUs);
        }
        isRackLifted = (angle > 45);
        Serial.printf("[ACT-SERVO] CALIBRACAO -> PCA CH %d enviado %d graus (%d us)\n", PCA_CH_RACK, angle, pulseUs);
    }

    // Desativa o pulso do servo para relaxar o motor e evitar aquecimento em stall
    void relaxRack() {
        if (pwmAvailable) {
            pwm.setPin(PCA_CH_RACK, 0, false); // 0V flat
        }
        Serial.printf("[ACT-SERVO] RELAX -> PCA CH %d pulso PWM desligado (motor solto/frio)\n", PCA_CH_RACK);
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
    }

    void stopAlarm() {
        alarmActive = false;
        alarmStrobeState = false;
    }

    bool isAlarmActive() const { return alarmActive; }
    bool getAlarmStrobeState() const { return alarmStrobeState; }

    // Throttle: -100 to +100 (%)
    void setThrottle(int8_t throttlePct) {
        currentThrottle = constrain(throttlePct, -100, 100);
        int pulseUs = map(currentThrottle, -100, 100, PWM_PULSE_MIN, PWM_PULSE_MAX);
        if (pwmAvailable) {
            pwm.writeMicroseconds(PCA_CH_MOTOR, pulseUs);
        }
        Serial.printf("[ACT-MOTOR] ESC Throttle: %d%% (%d us no PCA CH %d)\n", currentThrottle, pulseUs, PCA_CH_MOTOR);
    }

    // Rudder: -100 (full left) to +100 (full right)
    void setRudder(int8_t rudderPct) {
        currentRudder = constrain(rudderPct, -100, 100);
        int pulseUs = map(currentRudder, -100, 100, PWM_PULSE_MIN, PWM_PULSE_MAX);
        if (pwmAvailable) {
            pwm.writeMicroseconds(PCA_CH_RUDDER, pulseUs);
        }
        Serial.printf("[ACT-LEME] Leme: %d%% (%d us no PCA CH %d)\n", currentRudder, pulseUs, PCA_CH_RUDDER);
    }

    void stopMotor() {
        setThrottle(0);
    }

    void centerRudder() {
        setRudder(0);
    }

    // Corta por completo o sinal PWM no canal do guincho (0V flat / sem pulsos)
    void releaseWinch() {
        currentWinchPulseUs = 0;
        if (pwmAvailable) {
            pwm.setPin(PCA_CH_WINCH, 0, false);
        }
        Serial.printf("[ACT-GUINCHO] SINAL TOTALMENTE CORTADO (0V no PCA CH %d)\n", PCA_CH_WINCH);
    }

    // Manual winch jog: -1 = desenrolar/descer cabo (1200us), 0 = desligar sinal (OFF), 1 = enrolar/subir (1800us)
    void jogWinch(int8_t direction) {
        if (direction < 0) {
            setWinchMicroseconds(1200);
            Serial.printf("[ACT-GUINCHO] LIGADO (DESCE / DESENROLAR) -> 1200 us no PCA CH %d\n", PCA_CH_WINCH);
        } else if (direction > 0) {
            // Verificar detector de topo da âncora
            if (isAnchorAtTopLimit()) {
                Serial.printf("[ACT-GUINCHO] BLOQUEADO: Âncora já está no topo (detector J5 acionado)!\n");
                stopWinch();
                return;
            }
            setWinchMicroseconds(1800);
            Serial.printf("[ACT-GUINCHO] LIGADO (SOBE / ENROLAR) -> 1800 us no PCA CH %d\n", PCA_CH_WINCH);
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
        if (pwmAvailable) {
            pwm.writeMicroseconds(PCA_CH_WINCH, pulseUs);
        }
        Serial.printf("[ACT-GUINCHO] SINAL ATIVO (ON): %d us no PCA CH %d\n", pulseUs, PCA_CH_WINCH);
    }

    void setWinchNeutral(int neutralUs) {
        winchNeutralUs = constrain(neutralUs, 900, 2100);
        Serial.printf("[ACT-GUINCHO] PONTO NEUTRO DEFINIDO: %d us no PCA CH %d\n", winchNeutralUs, PCA_CH_WINCH);
        stopWinch();
    }

    int getWinchNeutral() const { return winchNeutralUs; }
    int getWinchPulseUs() const { return currentWinchPulseUs; }
    bool isWinchRunning() const { return (currentWinchPulseUs > 0); }

    bool isAnchorAtTopLimit() const {
        #if ARDUINO_USB_CDC_ON_BOOT
        if (HWCDC::isPlugged()) return false; // Em USB CDC desativado para proteção
        #endif
        return (digitalRead(PIN_ANCHOR_DETECTOR) == LOW);
    }

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
        if (pwmAvailable) {
            pwm.writeMicroseconds(PCA_CH_WINCH, PWM_PULSE_MID);
        }
        liftRack(); // Levanta a cremalheira para soltar o carretel
        lastWinchUpdate = millis();
        Serial.printf("[ACT-ANCORA] DROP ANCHOR -> Alvo: %u cm | Cremalheira ABERTA no PCA CH %d\n", targetDepthCm, PCA_CH_RACK);
    }

    // Recolhe a âncora: engata a cremalheira e enrola o cabo
    void commandRetractAnchor() {
        if (isAnchorAtTopLimit()) {
            Serial.printf("[ACT-ANCORA] RECOLHA IGNORADA -> Âncora já está encostada ao topo!\n");
            anchorState = ANCHOR_RETRACTED;
            currentDepthCm = 0;
            engageRack();
            stopWinch();
            return;
        }

        targetDepthCm = 0;
        anchorState = ANCHOR_RETRACTING;
        if (currentDepthCm <= 0) {
            currentDepthCm = 300; // Margem para teste de bancada não desligar instantaneamente
        }
        engageRack(); // Engata a cremalheira
        setWinchMicroseconds(1800);
        lastWinchUpdate = millis();
        Serial.printf("[ACT-ANCORA] RETRACT ANCHOR -> Guincho 1800us no PCA CH %d | Cremalheira TRAVADA no PCA CH %d\n", PCA_CH_WINCH, PCA_CH_RACK);
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
            bool hitLimit = isAnchorAtTopLimit();
            if (hitLimit || currentDepthCm <= (winchRetractSpeedCmPerSec * dt)) {
                currentDepthCm = 0;
                if (hitLimit) {
                    Serial.printf("[ACT-ANCORA] RECOLHA CONCLUIDA -> Fim de curso acionado (detector topo)! Guincho parado.\n");
                } else {
                    Serial.printf("[ACT-ANCORA] RECOLHA CONCLUIDA -> Tempo decorrido. Guincho parado.\n");
                }
                stopWinch();
                anchorState = ANCHOR_RETRACTED;
            } else {
                currentDepthCm -= (winchRetractSpeedCmPerSec * dt);
            }
        }

        // Handle alarm timeout and strobe oscillation (100ms on / 100ms off)
        if (alarmActive) {
            if (now >= alarmEndTimeMs) {
                stopAlarm();
            } else if (now - lastAlarmToggleMs >= 100) {
                lastAlarmToggleMs = now;
                alarmStrobeState = !alarmStrobeState;
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
    bool   isDriverReady() const { return pwmAvailable; }

private:
    Adafruit_PWMServoDriver pwm;

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
    bool pwmAvailable;
};
