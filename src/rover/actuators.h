#pragma once

#include "config_common.h"
#include "lora_protocol.h"
#include <Adafruit_PWMServoDriver.h>
#include <Arduino.h>
#include <Wire.h>

// Direct LEDC PWM Configuration for ESP32-C6 (50 Hz, 14-bit resolution:
// 0..16383)
static const uint32_t ACT_PWM_FREQ = 50;
static const uint8_t ACT_PWM_RES = 14;
static inline uint32_t actUsToDuty(int us) {
  if (us <= 0)
    return 0;
  return (uint32_t)(((uint64_t)us * 16383ULL) / 20000ULL);
}

// Static ISR variables for DRV5032 Hall Effect Sensor (Drum rotation counter)
static volatile long _anchorHallPulseCount = 0;
static volatile uint32_t _lastHallPulseTimeMs = 0;

static void IRAM_ATTR isrAnchorHallPulse() {
  uint32_t now = millis();
  if (now - _lastHallPulseTimeMs >=
      10) { // 10ms debounce (permite ate 100 rot/seg)
    _lastHallPulseTimeMs = now;
    _anchorHallPulseCount = _anchorHallPulseCount + 1;
  }
}

class ActuatorController {
public:
  ActuatorController()
      : pwm(PCA9685_I2C_ADDR, Wire), currentThrottle(0), currentRudder(0),
        winchNeutralUs(1500), currentWinchPulseUs(0),
        anchorState(ANCHOR_RETRACTED), currentDepthCm(0), targetDepthCm(0),
        winchRetractSpeedCmPerSec(20.0f), gravityDropSpeedCmPerSec(80.0f),
        lastWinchUpdate(0), isRackLifted(false),
        rackAngleEngaged(RACK_POS_ENGAGED),
        rackAngleReleased(RACK_POS_RELEASED),
        rackInverted(RACK_SERVO_INVERTED_DEFAULT),
        currentRackAngle(RACK_POS_ENGAGED), anchorDrumDiamMm(30.0f),
        anchorPulsesPerRev(1), anchorHallPulses(0), anchorRevolutions(0.0f),
        _lastProcessedHallPulseCount(0), alarmActive(false), alarmEndTimeMs(0),
        lastAlarmToggleMs(0), alarmStrobeState(false), pwmAvailable(false),
        _isInitialized(false), _throttleApplied(false), _rudderApplied(false),
  _rackMoveTimeMs(0), _rackRelaxed(false), _ledMode("OFF"), _ledR(255),
      _ledG(255), _ledB(255), _ledBrightness(100), _ledCount(NUM_STROBE_LEDS) {}

  int rackAngleToPulseUs(uint8_t angle) const {
    if (angle > 180)
      angle = 180;
    uint8_t effectiveAngle = rackInverted ? (180 - angle) : angle;
    return map(effectiveAngle, 0, 180, PWM_PULSE_MIN, PWM_PULSE_MAX);
  }

  void begin() {
    _isInitialized = true;
// 1. Configurar Canais PWM Diretos LEDC no ESP32-C6 (Hardware DevKitC)
#if defined(PIN_C6_WINCH) && (PIN_C6_WINCH >= 0)
    ledcAttach(PIN_C6_WINCH, ACT_PWM_FREQ, ACT_PWM_RES);
    ledcWrite(
        PIN_C6_WINCH,
        actUsToDuty(
            winchNeutralUs)); // Inicia no ponto neutro ativo (1500us hold)
#endif

#if defined(PIN_C6_WINCH_ALT) && (PIN_C6_WINCH_ALT >= 0)
    ledcAttach(PIN_C6_WINCH_ALT, ACT_PWM_FREQ, ACT_PWM_RES);
    ledcWrite(
        PIN_C6_WINCH_ALT,
        actUsToDuty(
            winchNeutralUs)); // Inicia no ponto neutro ativo (1500us hold)
#endif

#if defined(PIN_C6_SERVO_RACK) && (PIN_C6_SERVO_RACK >= 0)
    ledcAttach(PIN_C6_SERVO_RACK, ACT_PWM_FREQ, ACT_PWM_RES);
    ledcWrite(PIN_C6_SERVO_RACK,
              actUsToDuty(rackAngleToPulseUs(rackAngleEngaged)));
#endif

#if defined(PIN_C6_SERVO_RUD) && (PIN_C6_SERVO_RUD >= 0)
    ledcAttach(PIN_C6_SERVO_RUD, ACT_PWM_FREQ, ACT_PWM_RES);
    ledcWrite(PIN_C6_SERVO_RUD, actUsToDuty(PWM_PULSE_MID));
#endif

#if defined(PIN_C6_MOTOR_ESC) && (PIN_C6_MOTOR_ESC >= 0)
    ledcAttach(PIN_C6_MOTOR_ESC, ACT_PWM_FREQ, ACT_PWM_RES);
    ledcWrite(PIN_C6_MOTOR_ESC, actUsToDuty(PWM_PULSE_MIN));
#endif

    // 2. Inicializa o sensor de fim de curso do içamento da âncora (se
    // configurado)
    if (PIN_ANCHOR_DETECTOR >= 0) {
      pinMode(PIN_ANCHOR_DETECTOR, INPUT_PULLUP);
      Serial.printf(
          "[ACT-ANCORA] Fim de curso ativo no GPIO %d (INPUT_PULLUP)\n",
          PIN_ANCHOR_DETECTOR);
    }

// Inicializa o sensor Hall de contagem de voltas do tambor (DRV5032)
#if defined(PIN_ANCHOR_HALL) && (PIN_ANCHOR_HALL >= 0)
    pinMode(PIN_ANCHOR_HALL, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_ANCHOR_HALL), isrAnchorHallPulse,
                    FALLING);
    Serial.printf(
        "[ACT-ANCORA] Sensor Hall DRV5032 ativo no GPIO %d (INPUT_PULLUP)\n",
        PIN_ANCHOR_HALL);
#endif

    // Na placa Dual_ESP_Carrier os servos são controlados diretamente via LEDC
    // hardware (GPIO 0 é reservado a PIN_C6_LORA_DIO0)
    pwmAvailable = false;
    Serial.printf("[ACT] Controlo Direto LEDC ativo (ESC=GPIO%d, Leme=GPIO%d, "
                  "Guincho=GPIO%d, Rack=GPIO%d)\n",
                  PIN_C6_MOTOR_ESC, PIN_C6_SERVO_RUD, PIN_C6_WINCH,
                  PIN_C6_SERVO_RACK);

    stopMotor();
    centerRudder();
    engageRack(); // Trava a cremalheira na inicialização
    stopWinch();
    stopAlarm();
  }

  // --- Cremalheira / Rack Release Servo Controls ---
  // Levanta a cremalheira desengatando o carretel para a âncora cair livremente
  // por gravidade
  void liftRack() {
    setRackAngle(rackAngleReleased);
    isRackLifted = true;
    Serial.printf("[ACT-SERVO] LIFT RACK (LIVRE) -> PCA CH %d / GPIO %d "
                  "enviado %u graus (inv=%d, %d us)\n",
                  PCA_CH_RACK, PIN_C6_SERVO_RACK, rackAngleReleased,
                  rackInverted ? 1 : 0, rackAngleToPulseUs(rackAngleReleased));
  }
  void releaseRack() { liftRack(); }

  // Baixa a cremalheira travando/engatando o carretel para travar a
  // profundidade ou recolher
  void engageRack() {
    setRackAngle(rackAngleEngaged);
    isRackLifted = false;
    Serial.printf("[ACT-SERVO] ENGAGE RACK (TRAVAR / ENCOSTO) -> PCA CH %d / "
                  "GPIO %d enviado %u graus (inv=%d, %d us)\n",
                  PCA_CH_RACK, PIN_C6_SERVO_RACK, rackAngleEngaged,
                  rackInverted ? 1 : 0, rackAngleToPulseUs(rackAngleEngaged));
  }

  // Define ângulo direto para calibração mecânica (0 a 180 graus)
  void setRackAngle(uint8_t angle) {
    if (angle > 180)
      angle = 180;
    currentRackAngle = angle;
    _rackMoveTimeMs = millis();
    _rackRelaxed = false;
    int pulseUs = rackAngleToPulseUs(angle);
    if (pwmAvailable) {
      pwm.writeMicroseconds(PCA_CH_RACK, pulseUs);
    }
#if defined(PIN_C6_SERVO_RACK) && (PIN_C6_SERVO_RACK >= 0)
    if (_isInitialized) {
      ledcWrite(PIN_C6_SERVO_RACK, actUsToDuty(pulseUs));
    }
#endif
    isRackLifted = (abs((int)angle - (int)rackAngleReleased) <
                    abs((int)angle - (int)rackAngleEngaged));
    if (_isInitialized) {
      Serial.printf("[ACT-SERVO] CALIBRACAO -> PCA CH %d / GPIO %d enviado %u "
                    "graus (efetivo=%u°, inv=%d, %d us)\n",
                    PCA_CH_RACK, PIN_C6_SERVO_RACK, angle,
                    rackInverted ? (180 - angle) : angle, rackInverted ? 1 : 0,
                    pulseUs);
      Serial0.printf("[ACT-SERVO] CALIBRACAO -> PCA CH %d / GPIO %d enviado %u "
                     "graus (efetivo=%u°, inv=%d, %d us)\n",
                     PCA_CH_RACK, PIN_C6_SERVO_RACK, angle,
                     rackInverted ? (180 - angle) : angle, rackInverted ? 1 : 0,
                     pulseUs);
    }
  }

  void setRackConfig(uint8_t engagedAngle, uint8_t releasedAngle, bool inverted,
                     bool applyNow = true) {
    if (engagedAngle > 180)
      engagedAngle = 180;
    if (releasedAngle > 180)
      releasedAngle = 180;
    rackAngleEngaged = engagedAngle;
    rackAngleReleased = releasedAngle;
    rackInverted = inverted;
    if (applyNow && _isInitialized) {
      if (isRackLifted) {
        liftRack();
      } else {
        engageRack();
      }
    } else if (!_isInitialized) {
      currentRackAngle = isRackLifted ? rackAngleReleased : rackAngleEngaged;
    }
  }

  uint8_t getRackAngleEngaged() const { return rackAngleEngaged; }
  uint8_t getRackAngleReleased() const { return rackAngleReleased; }
  bool isRackInverted() const { return rackInverted; }
  uint8_t getCurrentRackAngle() const { return currentRackAngle; }
  int getCurrentRackPulseUs() const {
    return rackAngleToPulseUs(currentRackAngle);
  }

  // Desativa o pulso do servo para relaxar o motor e evitar aquecimento em
  // stall
  void relaxRack() {
    _rackRelaxed = true;
    if (pwmAvailable) {
      pwm.setPin(PCA_CH_RACK, 0, false); // 0V flat
    }
#if defined(PIN_C6_SERVO_RACK) && (PIN_C6_SERVO_RACK >= 0)
    if (_isInitialized) {
      ledcWrite(PIN_C6_SERVO_RACK, 0);
    }
#endif
    if (_isInitialized) {
      Serial.printf(
          "[ACT-SERVO] RELAX -> pulso PWM desligado (motor solto/frio)\n");
      Serial0.printf(
          "[ACT-SERVO] RELAX -> pulso PWM desligado (motor solto/frio)\n");
    }
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
    alarmEndTimeMs =
        millis() + (static_cast<uint32_t>(durationSeconds) * 1000UL);
    lastAlarmToggleMs = millis();
    alarmStrobeState = true;
  }

  void stopAlarm() {
    alarmActive = false;
    alarmStrobeState = false;
  }

  bool isAlarmActive() const { return alarmActive; }
  bool getAlarmStrobeState() const { return alarmStrobeState; }

  void setLedState(const String &mode, uint8_t r = 255, uint8_t g = 255,
                   uint8_t b = 255) {
    _ledMode = mode;
    _ledMode.toUpperCase();
    _ledR = r;
    _ledG = g;
    _ledB = b;
    if (_ledMode == "OFF") {
      alarmActive = false;
      alarmStrobeState = false;
    } else if (_ledMode == "ALARM" || _ledMode == "STROBE_ALARM") {
      alarmActive = true;
    }
  }

  void setLedConfigState(uint8_t count, uint8_t brightness) {
    if (count >= 1 && count <= 120)
      _ledCount = count;
    if (brightness >= 5 && brightness <= 100)
      _ledBrightness = brightness;
  }

  String getLedMode() const {
    return alarmActive ? String("STROBE") : _ledMode;
  }
  uint8_t getLedR() const { return _ledR; }
  uint8_t getLedG() const { return _ledG; }
  uint8_t getLedB() const { return _ledB; }
  uint8_t getLedBrightness() const { return _ledBrightness; }
  uint8_t getLedCount() const { return _ledCount; }

  // Throttle: -100 to +100 (%)
  void setThrottle(int8_t throttlePct) {
    int8_t clamped = constrain(throttlePct, -100, 100);
    if (_throttleApplied && clamped == currentThrottle) {
      return;
    }
    currentThrottle = clamped;
    _throttleApplied = true;
    int pulseUs = map(currentThrottle, -100, 100, PWM_PULSE_MIN, PWM_PULSE_MAX);
    if (pwmAvailable) {
      pwm.writeMicroseconds(PCA_CH_MOTOR, pulseUs);
    }
#if defined(PIN_C6_MOTOR_ESC) && (PIN_C6_MOTOR_ESC >= 0)
    if (_isInitialized) {
      ledcWrite(PIN_C6_MOTOR_ESC, actUsToDuty(pulseUs));
    }
#endif
    if (_isInitialized) {
      Serial.printf("[ACT-MOTOR] ESC Throttle: %d%% (%d us no GPIO %d)\n",
                    currentThrottle, pulseUs, PIN_C6_MOTOR_ESC);
      Serial0.printf("[ACT-MOTOR] ESC Throttle: %d%% (%d us no GPIO %d)\n",
                     currentThrottle, pulseUs, PIN_C6_MOTOR_ESC);
    }
  }

  // Rudder: -100 (full left) to +100 (full right)
  void setRudder(int8_t rudderPct) {
    int8_t clamped = constrain(rudderPct, -100, 100);
    if (_rudderApplied && clamped == currentRudder) {
      return;
    }
    currentRudder = clamped;
    _rudderApplied = true;
    int pulseUs = map(currentRudder, -100, 100, PWM_PULSE_MIN, PWM_PULSE_MAX);
    if (pwmAvailable) {
      pwm.writeMicroseconds(PCA_CH_RUDDER, pulseUs);
    }
#if defined(PIN_C6_SERVO_RUD) && (PIN_C6_SERVO_RUD >= 0)
    if (_isInitialized) {
      ledcWrite(PIN_C6_SERVO_RUD, actUsToDuty(pulseUs));
    }
#endif
    if (_isInitialized) {
      Serial.printf("[ACT-LEME] Leme: %d%% (%d us no GPIO %d)\n", currentRudder,
                    pulseUs, PIN_C6_SERVO_RUD);
      Serial0.printf("[ACT-LEME] Leme: %d%% (%d us no GPIO %d)\n",
                     currentRudder, pulseUs, PIN_C6_SERVO_RUD);
    }
  }

  void stopMotor() { setThrottle(0); }

  void centerRudder() { setRudder(0); }

  // Corta por completo o sinal PWM no canal do guincho (0V flat / sem pulsos)
  void releaseWinch() {
    currentWinchPulseUs = 0;
    if (pwmAvailable) {
      pwm.setPin(PCA_CH_WINCH, 0, false);
    }
#if defined(PIN_C6_WINCH) && (PIN_C6_WINCH >= 0)
    if (_isInitialized) {
      ledcWrite(PIN_C6_WINCH, 0); // 0V plano, sinal totalmente cortado
    }
#endif
#if defined(PIN_C6_WINCH_ALT) && (PIN_C6_WINCH_ALT >= 0)
    if (_isInitialized) {
      ledcWrite(PIN_C6_WINCH_ALT, 0);
    }
    if (_isInitialized) {
      Serial.printf(
          "[ACT-GUINCHO] SINAL TOTALMENTE CORTADO (0V nos GPIOs %d e %d)\n",
          PIN_C6_WINCH, PIN_C6_WINCH_ALT);
      Serial0.printf(
          "[ACT-GUINCHO] SINAL TOTALMENTE CORTADO (0V nos GPIOs %d e %d)\n",
          PIN_C6_WINCH, PIN_C6_WINCH_ALT);
    }
#else
    if (_isInitialized) {
      Serial.printf("[ACT-GUINCHO] SINAL TOTALMENTE CORTADO (0V no GPIO %d)\n",
                    PIN_C6_WINCH);
      Serial0.printf("[ACT-GUINCHO] SINAL TOTALMENTE CORTADO (0V no GPIO %d)\n",
                     PIN_C6_WINCH);
    }
#endif
  }

  // Manual winch jog: -1 = desenrolar/descer cabo (1200us), 0 = desligar sinal
  // (OFF), 1 = enrolar/subir (1800us)
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

    // Proteção mecânica: se a âncora já está no topo (fim de curso acionado),
    // impede recolher mais (> 1500us)
    if (pulseUs > winchNeutralUs && isAnchorAtTopLimit()) {
      Serial.printf("[ACT-GUINCHO] BLOQUEADO: Fim de curso acionado no GPIO "
                    "%d! Impedida recolha em excesso.\n",
                    PIN_ANCHOR_DETECTOR);
      pulseUs = winchNeutralUs;
    }

    currentWinchPulseUs = pulseUs;
    if (pwmAvailable) {
      pwm.writeMicroseconds(PCA_CH_WINCH, pulseUs);
    }
#if defined(PIN_C6_WINCH) && (PIN_C6_WINCH >= 0)
    if (_isInitialized) {
      ledcWrite(PIN_C6_WINCH, actUsToDuty(pulseUs));
    }
#endif
#if defined(PIN_C6_WINCH_ALT) && (PIN_C6_WINCH_ALT >= 0)
    if (_isInitialized) {
      ledcWrite(PIN_C6_WINCH_ALT, actUsToDuty(pulseUs));
    }
#endif
    if (_isInitialized) {
      Serial.printf("[ACT-GUINCHO] SINAL ATIVO (ON): %d us no GPIO %d\n",
                    pulseUs, PIN_C6_WINCH);
      Serial0.printf("[ACT-GUINCHO] SINAL ATIVO (ON): %d us no GPIO %d\n",
                     pulseUs, PIN_C6_WINCH);
    }
  }

  void setWinchNeutral(int neutralUs) {
    winchNeutralUs = constrain(neutralUs, 900, 2100);
    Serial.printf("[ACT-GUINCHO] PONTO NEUTRO DEFINIDO: %d us\n",
                  winchNeutralUs);
    stopWinch();
  }

  int getWinchNeutral() const { return winchNeutralUs; }
  int getWinchPulseUs() const { return currentWinchPulseUs; }
  bool isWinchRunning() const { return (currentWinchPulseUs > 0); }

  bool isAnchorAtTopLimit() const {
    if (PIN_ANCHOR_DETECTOR < 0)
      return false;
    return (digitalRead(PIN_ANCHOR_DETECTOR) == LOW);
  }

  void stopWinch() {
    setWinchMicroseconds(
        winchNeutralUs); // Ponto morto ativo (1500us) para travar o carretel
                         // contra correntes de maré
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
    Serial.printf("[ACT-ANCORA] DROP ANCHOR -> Alvo: %u cm | Cremalheira "
                  "ABERTA no PCA CH %d\n",
                  targetDepthCm, PCA_CH_RACK);
  }

  // Recolhe a âncora: engata a cremalheira e enrola o cabo
  void commandRetractAnchor() {
    if (isAnchorAtTopLimit()) {
      Serial.printf("[ACT-ANCORA] RECOLHA IGNORADA -> Âncora já está encostada "
                    "ao topo!\n");
      anchorState = ANCHOR_RETRACTED;
      currentDepthCm = 0;
      engageRack();
      stopWinch();
      return;
    }

    targetDepthCm = 0;
    anchorState = ANCHOR_RETRACTING;
    if (currentDepthCm <= 0) {
      currentDepthCm =
          300; // Margem para teste de bancada não desligar instantaneamente
    }
    engageRack(); // Engata a cremalheira
    setWinchMicroseconds(1800);
    lastWinchUpdate = millis();
    Serial.printf("[ACT-ANCORA] RETRACT ANCHOR -> Guincho 1800us no PCA CH %d "
                  "| Cremalheira TRAVADA no PCA CH %d\n",
                  PCA_CH_WINCH, PCA_CH_RACK);
  }

  void update() {
    uint32_t now = millis();
    if (lastWinchUpdate == 0)
      lastWinchUpdate = now;
    float dt = (now - lastWinchUpdate) / 1000.0f;
    lastWinchUpdate = now;

    // 1. Processamento de pulsos reais do sensor Hall DRV5032 no tambor da
    // âncora
    if (_anchorHallPulseCount != _lastProcessedHallPulseCount) {
      long diff = _anchorHallPulseCount - _lastProcessedHallPulseCount;
      _lastProcessedHallPulseCount = _anchorHallPulseCount;

      float cmPerPulse =
          (3.14159265f * (anchorDrumDiamMm / 10.0f)) /
          (float)(anchorPulsesPerRev > 0 ? anchorPulsesPerRev : 1);

      // Determinação do sentido pelo comando do guincho:
      if (currentWinchPulseUs > winchNeutralUs) {
        // Guincho a recolher (puxar) -> diminui o cabo solto
        currentDepthCm -= (diff * cmPerPulse);
        if (currentDepthCm < 0)
          currentDepthCm = 0;
        anchorHallPulses -= diff;
        if (anchorHallPulses < 0)
          anchorHallPulses = 0;
      } else {
        // Queda livre, desenrolar ou teste de rotação em bancada -> aumenta o
        // cabo solto
        currentDepthCm += (diff * cmPerPulse);
        anchorHallPulses += diff;
      }
      anchorRevolutions =
          (float)anchorHallPulses /
          (float)(anchorPulsesPerRev > 0 ? anchorPulsesPerRev : 1);

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
        Serial.printf("[ACT-ANCORA] DROP CONCLUIDO -> Profundidade %u cm "
                      "atingida. A travar cremalheira.\n",
                      targetDepthCm);
        engageRack(); // Baixa e engata a cremalheira para travar na
                      // profundidade atingida
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
          Serial.printf("[ACT-ANCORA] RECOLHA CONCLUIDA -> Fim de curso "
                        "acionado (detector topo)! Guincho parado.\n");
        } else {
          Serial.printf("[ACT-ANCORA] RECOLHA CONCLUIDA -> Alvo atingido. "
                        "Guincho parado.\n");
        }
        stopWinch();
        anchorState = ANCHOR_RETRACTED;
      } else if (_anchorHallPulseCount == 0) {
        currentDepthCm -= (winchRetractSpeedCmPerSec * dt);
      }
    }

    // Proteção contínua: se o guincho estiver a recolher (> winchNeutralUs) e
    // acionar o fim de curso mecânico, corta imediatamente
    if (isAnchorAtTopLimit()) {
      if (currentWinchPulseUs > winchNeutralUs) {
        Serial.printf("[ACT-GUINCHO] CORTE IMEDIATO: Fim de curso atingido no "
                      "GPIO %d! Guincho parado no topo.\n",
                      PIN_ANCHOR_DETECTOR);
        stopWinch();
      }
      if (anchorState != ANCHOR_DEPLOYING && !isRackLifted) {
        zeroAnchorCounter();
        anchorState = ANCHOR_RETRACTED;
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

    // Auto-relaxar o servo da cremalheira 800ms após atingir a posição alvo
    // para evitar corrente de stall na linha +5V_SERVO
    if (_isInitialized && !_rackRelaxed && _rackMoveTimeMs > 0 &&
        (now - _rackMoveTimeMs >= 800)) {
      relaxRack();
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
    if (diamMm >= 5.0f && diamMm <= 500.0f)
      anchorDrumDiamMm = diamMm;
    if (pulsesPerRev >= 1 && pulsesPerRev <= 24)
      anchorPulsesPerRev = pulsesPerRev;
    float cmPerPulse =
        (3.14159265f * (anchorDrumDiamMm / 10.0f)) / (float)anchorPulsesPerRev;
    currentDepthCm = anchorHallPulses * cmPerPulse;
    anchorRevolutions = (float)anchorHallPulses / (float)anchorPulsesPerRev;
  }

  float getAnchorDrumDiamMm() const { return anchorDrumDiamMm; }
  uint8_t getAnchorPulsesPerRev() const { return anchorPulsesPerRev; }
  long getAnchorHallPulses() const { return anchorHallPulses; }
  float getAnchorRevolutions() const { return anchorRevolutions; }

  uint8_t getAnchorStatus() const { return anchorState; }

  uint16_t getAnchorDepthCm() const {
    return static_cast<uint16_t>(currentDepthCm);
  }

  int8_t getThrottle() const { return currentThrottle; }
  int8_t getRudder() const { return currentRudder; }
  bool isDriverReady() const { return pwmAvailable; }

  void setValues(int8_t thr, int8_t rud, int winchUs, bool rackLifted,
                 uint8_t anchorSt, uint16_t depth) {
    currentThrottle = thr;
    currentRudder = rud;
    currentWinchPulseUs = winchUs;
    isRackLifted = rackLifted;
    currentRackAngle = rackLifted ? rackAngleReleased : rackAngleEngaged;
    anchorState = anchorSt;
    currentDepthCm = depth;
    float cmPerPulse = (3.14159265f * (anchorDrumDiamMm / 10.0f)) /
                       (float)(anchorPulsesPerRev > 0 ? anchorPulsesPerRev : 1);
    if (cmPerPulse > 0.001f) {
      anchorHallPulses = (long)(currentDepthCm / cmPerPulse);
      anchorRevolutions =
          (float)anchorHallPulses /
          (float)(anchorPulsesPerRev > 0 ? anchorPulsesPerRev : 1);
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
  uint8_t rackAngleEngaged;
  uint8_t rackAngleReleased;
  bool rackInverted;
  uint8_t currentRackAngle;

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
  bool _isInitialized;
  bool _throttleApplied;
  bool _rudderApplied;
  uint32_t _rackMoveTimeMs;
  bool _rackRelaxed;

  String _ledMode;
  uint8_t _ledR;
  uint8_t _ledG;
  uint8_t _ledB;
  uint8_t _ledBrightness;
  uint8_t _ledCount;
};
