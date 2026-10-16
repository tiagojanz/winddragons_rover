#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include "config_common.h"
#include "lora_protocol.h"

// Direct LEDC PWM Configuration for ESP32-C6 (50 Hz, 14-bit resolution: 0..16383)
static const uint32_t ACT_PWM_FREQ = 50;
static const uint8_t  ACT_PWM_RES  = 14;
static inline uint32_t actUsToDuty(int us) {
    if (us <= 0) return 0;
    return (uint32_t)(((uint64_t)us * 16383ULL) / 20000ULL);
}

// Static ISR variables for DRV5032 Hall Effect Sensor (Drum rotation counter)
static volatile long _anchorHallPulseCount = 0;
static volatile uint32_t _lastHallPulseTimeMs = 0;

static void IRAM_ATTR isrAnchorHallPulse() {
    uint32_t now = millis();
    if (now - _lastHallPulseTimeMs >= 10) { // 10ms debounce (permite ate 100 rot/seg)
        _lastHallPulseTimeMs = now;
        _anchorHallPulseCount = _anchorHallPulseCount + 1;
    }
}

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
          anchorDrumDiamMm(30.0f), anchorPulsesPerRev(1),
          anchorHallPulses(0), anchorRevolutions(0.0f),
          _lastProcessedHallPulseCount(0),
          alarmActive(false), alarmEndTimeMs(0),
          lastAlarmToggleMs(0), alarmStrobeState(false),
          pwmAvailable(false) {}

    void begin() {
        // 1. Configurar Canais PWM Diretos LEDC no ESP32-C6 (Hardware DevKitC)
        #if defined(PIN_C6_WINCH) && (PIN_C6_WINCH >= 0)
        ledcAttach(PIN_C6_WINCH, ACT_PWM_FREQ, ACT_PWM_RES);
        ledcWrite(PIN_C6_WINCH, actUsToDuty(winchNeutralUs)); // Inicia no ponto neutro ativo (1500us hold)
        #endif

        #if defined(PIN_C6_WINCH_ALT) && (PIN_C6_WINCH_ALT >= 0)
        ledcAttach(PIN_C6_WINCH_ALT, ACT_PWM_FREQ, ACT_PWM_RES);
        ledcWrite(PIN_C6_WINCH_ALT, actUsToDuty(winchNeutralUs)); // Inicia no ponto neutro ativo (1500us hold)
        #endif

        #if defined(PIN_C6_SERVO_RACK) && (PIN_C6_SERVO_RACK >= 0)
        ledcAttach(PIN_C6_SERVO_RACK, ACT_PWM_FREQ, ACT_PWM_RES);
        ledcWrite(PIN_C6_SERVO_RACK, actUsToDuty(map(RACK_POS_ENGAGED, 0, 180, PWM_PULSE_MIN, PWM_PULSE_MAX)));
        #endif

        #if defined(PIN_C6_SERVO_RUD) && (PIN_C6_SERVO_RUD >= 0)
        ledcAttach(PIN_C6_SERVO_RUD, ACT_PWM_FREQ, ACT_PWM_RES);
        ledcWrite(PIN_C6_SERVO_RUD, actUsToDuty(PWM_PULSE_MID));
        #endif

        #if defined(PIN_C6_MOTOR_ESC) && (PIN_C6_MOTOR_ESC >= 0)
        ledcAttach(PIN_C6_MOTOR_ESC, ACT_PWM_FREQ, ACT_PWM_RES);
        ledcWrite(PIN_C6_MOTOR_ESC, actUsToDuty(PWM_PULSE_MIN));
        #endif

        // 2. Tentar detetar chip opcional PCA9685 no barramento I2C
        Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
        
        // Inicializa o sensor de fim de curso do içamento da âncora (se configurado)
        if (PIN_ANCHOR_DETECTOR >= 0) {
            pinMode(PIN_ANCHOR_DETECTOR, INPUT_PULLUP);
            Serial.printf("[ACT-ANCORA] Fim de curso ativo no GPIO %d (INPUT_PULLUP)\n", PIN_ANCHOR_DETECTOR);
        }

        // Inicializa o sensor Hall de contagem de voltas do tambor (DRV5032)
        #if defined(PIN_ANCHOR_HALL) && (PIN_ANCHOR_HALL >= 0)
        pinMode(PIN_ANCHOR_HALL, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(PIN_ANCHOR_HALL), isrAnchorHallPulse, FALLING);
        Serial.printf("[ACT-ANCORA] Sensor Hall DRV5032 ativo no GPIO %d (INPUT_PULLUP)\n", PIN_ANCHOR_HALL);
        #endif

        if (pwm.begin()) {
            pwm.setPWMFreq(50);
            pwmAvailable = true;
            Serial.println(F("[ACT] PCA9685 16-Ch PWM detectado com sucesso em 0x40!"));
        } else {
            pwmAvailable = false;
            Serial.println(F("[ACT] Controlo Direto LEDC ativo nos pinos GPIO 21, 20, 19, 18"));
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
        #if defined(PIN_C6_SERVO_RACK) && (PIN_C6_SERVO_RACK >= 0)
        ledcWrite(PIN_C6_SERVO_RACK, actUsToDuty(pulseUs));
        #endif
        isRackLifted = (angle > 45);
        Serial.printf("[ACT-SERVO] CALIBRACAO -> PCA CH %d / GPIO %d enviado %d graus (%d us)\n", PCA_CH_RACK, PIN_C6_SERVO_RACK, angle, pulseUs);
    }

    // Desativa o pulso do servo para relaxar o motor e evitar aquecimento em stall
    void relaxRack() {
        if (pwmAvailable) {
            pwm.setPin(PCA_CH_RACK, 0, false); // 0V flat
        }
        #if defined(PIN_C6_SERVO_RACK) && (PIN_C6_SERVO_RACK >= 0)
        ledcWrite(PIN_C6_SERVO_RACK, 0);
        #endif
        Serial.printf("[ACT-SERVO] RELAX -> pulso PWM desligado (motor solto/frio)\n");
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
        #if defined(PIN_C6_MOTOR_ESC) && (PIN_C6_MOTOR_ESC >= 0)
        ledcWrite(PIN_C6_MOTOR_ESC, actUsToDuty(pulseUs));
        #endif
        Serial.printf("[ACT-MOTOR] ESC Throttle: %d%% (%d us no GPIO %d)\n", currentThrottle, pulseUs, PIN_C6_MOTOR_ESC);
    }

    // Rudder: -100 (full left) to +100 (full right)
    void setRudder(int8_t rudderPct) {
        currentRudder = constrain(rudderPct, -100, 100);
        int pulseUs = map(currentRudder, -100, 100, PWM_PULSE_MIN, PWM_PULSE_MAX);
        if (pwmAvailable) {
            pwm.writeMicroseconds(PCA_CH_RUDDER, pulseUs);
        }
        #if defined(PIN_C6_SERVO_RUD) && (PIN_C6_SERVO_RUD >= 0)
        ledcWrite(PIN_C6_SERVO_RUD, actUsToDuty(pulseUs));
        #endif
        Serial.printf("[ACT-LEME] Leme: %d%% (%d us no GPIO %d)\n", currentRudder, pulseUs, PIN_C6_SERVO_RUD);
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
        #if defined(PIN_C6_WINCH) && (PIN_C6_WINCH >= 0)
        ledcWrite(PIN_C6_WINCH, 0); // 0V plano, sinal totalmente cortado
        #endif
        #if defined(PIN_C6_WINCH_ALT) && (PIN_C6_WINCH_ALT >= 0)
        ledcWrite(PIN_C6_WINCH_ALT, 0);
        Serial.printf("[ACT-GUINCHO] SINAL TOTALMENTE CORTADO (0V nos GPIOs %d e %d)\n", PIN_C6_WINCH, PIN_C6_WINCH_ALT);
        #else
        Serial.printf("[ACT-GUINCHO] SINAL TOTALMENTE CORTADO (0V no GPIO %d)\n", PIN_C6_WINCH);
        #endif
    }

    // Manual winch jog: -1 = desenrolar/descer cabo (1200us), 0 = desligar sinal (OFF), 1 = enrolar/subir (1800us)
    void jogWinch(int8_t direction) {
        if (direction < 0) {
            setWinchMicroseconds(1200);
            Serial.printf("[ACT-GUINCHO] LIGADO (DESCE / DESENROLAR) -> 1200 us\n");
        } else if (direction > 0) {
            // Verificar detector de topo da âncora
            if (isAnchorAtTopLimit()) {
                Serial.printf("[ACT-GUINCHO] BLOQUEADO: Âncora já está no topo!\n");
                stopWinch();
                return;
            }
            setWinchMicroseconds(1800);
            Serial.printf("[ACT-GUINCHO] LIGADO (SOBE / ENROLAR) -> 1800 us\n");
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

        // Proteção mecânica: se a âncora já está no topo (fim de curso acionado), impede recolher mais (> 1500us)
        if (pulseUs > winchNeutralUs && isAnchorAtTopLimit()) {
            Serial.printf("[ACT-GUINCHO] BLOQUEADO: Fim de curso acionado no GPIO %d! Impedida recolha em excesso.\n", PIN_ANCHOR_DETECTOR);
            pulseUs = winchNeutralUs;
        }

        currentWinchPulseUs = pulseUs;
        if (pwmAvailable) {
            pwm.writeMicroseconds(PCA_CH_WINCH, pulseUs);
        }
        #if defined(PIN_C6_WINCH) && (PIN_C6_WINCH >= 0)
        ledcWrite(PIN_C6_WINCH, actUsToDuty(pulseUs));
        #endif
        #if defined(PIN_C6_WINCH_ALT) && (PIN_C6_WINCH_ALT >= 0)
        ledcWrite(PIN_C6_WINCH_ALT, actUsToDuty(pulseUs));
        #endif
        Serial.printf("[ACT-GUINCHO] SINAL ATIVO (ON): %d us no GPIO %d\n", pulseUs, PIN_C6_WINCH);
    }

    void setWinchNeutral(int neutralUs) {
        winchNeutralUs = constrain(neutralUs, 900, 2100);
        Serial.printf("[ACT-GUINCHO] PONTO NEUTRO DEFINIDO: %d us\n", winchNeutralUs);
        stopWinch();
    }

    int getWinchNeutral() const { return winchNeutralUs; }
    int getWinchPulseUs() const { return currentWinchPulseUs; }
    bool isWinchRunning() const { return (currentWinchPulseUs > 0); }

    bool isAnchorAtTopLimit() const {
        if (PIN_ANCHOR_DETECTOR < 0) return false;
        return (digitalRead(PIN_ANCHOR_DETECTOR) == LOW);
    }

    void stopWinch() {
        setWinchMicroseconds(winchNeutralUs); // Ponto morto ativo (1500us) para travar o carretel contra correntes de maré
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

        // 1. Processamento de pulsos reais do sensor Hall DRV5032 no tambor da âncora
        if (_anchorHallPulseCount != _lastProcessedHallPulseCount) {
            long diff = _anchorHallPulseCount - _lastProcessedHallPulseCount;
            _lastProcessedHallPulseCount = _anchorHallPulseCount;

            float cmPerPulse = (3.14159265f * (anchorDrumDiamMm / 10.0f)) / (float)(anchorPulsesPerRev > 0 ? anchorPulsesPerRev : 1);

            // Determinação do sentido pelo comando do guincho:
            if (currentWinchPulseUs > winchNeutralUs) {
                // Guincho a recolher (puxar) -> diminui o cabo solto
                currentDepthCm -= (diff * cmPerPulse);
                if (currentDepthCm < 0) currentDepthCm = 0;
                anchorHallPulses -= diff;
                if (anchorHallPulses < 0) anchorHallPulses = 0;
            } else {
                // Queda livre, desenrolar ou teste de rotação em bancada -> aumenta o cabo solto
                currentDepthCm += (diff * cmPerPulse);
                anchorHallPulses += diff;
            }
            anchorRevolutions = (float)anchorHallPulses / (float)(anchorPulsesPerRev > 0 ? anchorPulsesPerRev : 1);

            if (anchorState == ANCHOR_RETRACTED && currentDepthCm > 5.0f) {
                anchorState = ANCHOR_DEPLOYED;
            }
        }

        // Queda por gravidade (cremalheira levantada)
        if (anchorState == ANCHOR_DEPLOYING) {
            if (_anchorHallPulseCount == 0 && targetDepthCm > 0) {
                currentDepthCm += (gravityDropSpeedCmPerSec * dt);
            }
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
                zeroAnchorCounter();
                if (hitLimit) {
                    Serial.printf("[ACT-ANCORA] RECOLHA CONCLUIDA -> Fim de curso acionado (detector topo)! Guincho parado.\n");
                } else {
                    Serial.printf("[ACT-ANCORA] RECOLHA CONCLUIDA -> Alvo atingido. Guincho parado.\n");
                }
                stopWinch();
                anchorState = ANCHOR_RETRACTED;
            } else if (_anchorHallPulseCount == 0) {
                currentDepthCm -= (winchRetractSpeedCmPerSec * dt);
            }
        }

        // Proteção contínua: se o guincho estiver a recolher (> winchNeutralUs) e acionar o fim de curso mecânico, corta imediatamente
        if (isAnchorAtTopLimit()) {
            if (currentWinchPulseUs > winchNeutralUs) {
                Serial.printf("[ACT-GUINCHO] CORTE IMEDIATO: Fim de curso atingido no GPIO %d! Guincho parado no topo.\n", PIN_ANCHOR_DETECTOR);
                stopWinch();
            }
            zeroAnchorCounter();
            anchorState = ANCHOR_RETRACTED;
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

    void zeroAnchorCounter() {
        _anchorHallPulseCount = 0;
        _lastProcessedHallPulseCount = 0;
        anchorHallPulses = 0;
        anchorRevolutions = 0.0f;
        currentDepthCm = 0.0f;
    }

    void setAnchorCalibration(float diamMm, uint8_t pulsesPerRev) {
        if (diamMm >= 5.0f && diamMm <= 500.0f) anchorDrumDiamMm = diamMm;
        if (pulsesPerRev >= 1 && pulsesPerRev <= 24) anchorPulsesPerRev = pulsesPerRev;
        float cmPerPulse = (3.14159265f * (anchorDrumDiamMm / 10.0f)) / (float)anchorPulsesPerRev;
        currentDepthCm = anchorHallPulses * cmPerPulse;
        anchorRevolutions = (float)anchorHallPulses / (float)anchorPulsesPerRev;
    }

    float getAnchorDrumDiamMm() const { return anchorDrumDiamMm; }
    uint8_t getAnchorPulsesPerRev() const { return anchorPulsesPerRev; }
    long getAnchorHallPulses() const { return anchorHallPulses; }
    float getAnchorRevolutions() const { return anchorRevolutions; }

    uint8_t getAnchorStatus() const {
        return anchorState;
    }

    uint16_t getAnchorDepthCm() const {
        return static_cast<uint16_t>(currentDepthCm);
    }

    int8_t getThrottle() const { return currentThrottle; }
    int8_t getRudder() const { return currentRudder; }
    bool   isDriverReady() const { return pwmAvailable; }

    void setValues(int8_t thr, int8_t rud, int winchUs, bool rackLifted, uint8_t anchorSt, uint16_t depth) {
        currentThrottle = thr;
        currentRudder = rud;
        currentWinchPulseUs = winchUs;
        isRackLifted = rackLifted;
        anchorState = anchorSt;
        currentDepthCm = depth;
        float cmPerPulse = (3.14159265f * (anchorDrumDiamMm / 10.0f)) / (float)(anchorPulsesPerRev > 0 ? anchorPulsesPerRev : 1);
        if (cmPerPulse > 0.001f) {
            anchorHallPulses = (long)(currentDepthCm / cmPerPulse);
            anchorRevolutions = (float)anchorHallPulses / (float)(anchorPulsesPerRev > 0 ? anchorPulsesPerRev : 1);
        }
    }

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

    float anchorDrumDiamMm;
    uint8_t anchorPulsesPerRev;
    long anchorHallPulses;
    float anchorRevolutions;
    long _lastProcessedHallPulseCount;

    bool alarmActive;
    uint32_t alarmEndTimeMs;
    uint32_t lastAlarmToggleMs;
    bool alarmStrobeState;
    bool pwmAvailable;
};
