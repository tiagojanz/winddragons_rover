#pragma once

#include "config_common.h"
#include <Arduino.h>
#include <driver/gpio.h>

class BaseJoystick {
public:
  BaseJoystick()
      : rawX(1595), rawY(1595), centerX(1595), centerY(1595),
        axisXConnected(true), axisYConnected(true), throttle(0), rudder(0),
        deadband(220) {}

  void begin() {
    analogReadResolution(12); // 0 to 4095
    pinMode(PIN_JOYSTICK_X, INPUT);
    pinMode(PIN_JOYSTICK_Y, INPUT);

    if (PIN_JOYSTICK_SW >= 0) {
      pinMode(PIN_JOYSTICK_SW, INPUT_PULLUP);
    }

    // Optional anchor and mode buttons
    pinMode(PIN_BTN_BOOT, INPUT_PULLUP);
    pinMode(PIN_BTN_ANCHOR_UP, INPUT_PULLUP);
    pinMode(PIN_BTN_ANCHOR_DOWN, INPUT_PULLUP);
    pinMode(PIN_BTN_MODE, INPUT_PULLUP);

    // Auto-calibração do centro neutro em repouso (30 amostras)
    long sumX = 0, sumY = 0;
    for (int i = 0; i < 30; i++) {
      sumX += analogRead(PIN_JOYSTICK_X);
      sumY += analogRead(PIN_JOYSTICK_Y);
      delay(3);
    }
    centerX = (int)(sumX / 30);
    centerY = (int)(sumY / 30);
    axisXConnected = (centerX >= 1100 && centerX <= 2900);
    axisYConnected = (centerY >= 1100 && centerY <= 2900);
    if (!axisXConnected)
      centerX = 1618;
    if (!axisYConnected)
      centerY = 1594;
    rawX = centerX;
    rawY = centerY;
  }

  bool isAxisXConnected() const { return axisXConnected; }
  bool isAxisYConnected() const { return axisYConnected; }
  int getCenterX() const { return centerX; }
  int getCenterY() const { return centerY; }

  bool isClicked() const {
    int swPin = (PIN_JOYSTICK_SW >= 0) ? PIN_JOYSTICK_SW : PIN_BTN_BOOT;
    return digitalRead(swPin) == LOW;
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

  bool wasBootHeld3s() {
    if (hold3sOccurred) {
      hold3sOccurred = false;
      return true;
    }
    return false;
  }

  uint8_t getBootHoldProgressPct(uint32_t targetMs = 3000) const {
    if (!lastBtnState || hold3sFired) return 0;
    uint32_t elapsed = millis() - pressStartTime;
    if (elapsed < 200) return 0;
    uint32_t pct = (elapsed * 100) / targetMs;
    return (pct > 100) ? 100 : (uint8_t)pct;
  }

  // Menu navigation helper (legacy compatibility): returns -1 (UP), +1 (DOWN), or 0 (neutral)
  int8_t getMenuNavStep() {
    return getNavY();
  }

  // Navegação Vertical (Eixo Y do Joystick): -1 = Cima (Anterior), +1 = Baixo (Seguinte), 0 = Neutro
  int8_t getNavY() {
    uint32_t now = millis();
    bool upPressed = (throttle > 45 || digitalRead(PIN_BTN_ANCHOR_UP) == LOW);
    bool downPressed = (throttle < -45 || digitalRead(PIN_BTN_ANCHOR_DOWN) == LOW);

    if (!upPressed && !downPressed) {
      if (abs(throttle) < 20) {
        navYArmed = true;
      }
      return 0;
    }

    if (navYArmed || (now - lastNavYMs >= 260)) {
      navYArmed = false;
      lastNavYMs = now;
      return upPressed ? -1 : 1;
    }
    return 0;
  }

  // Navegação Horizontal (Eixo X do Joystick): -1 = Esquerda (Sair/Voltar), +1 = Direita (Entrar/Confirmar), 0 = Neutro
  // Exige retorno ao centro entre ações para evitar saltar múltiplos níveis de menu de uma só vez.
  int8_t getNavX() {
    uint32_t now = millis();
    bool leftPressed = (rudder < -50);
    bool rightPressed = (rudder > 50);

    if (!leftPressed && !rightPressed) {
      if (abs(rudder) < 25) {
        navXArmed = true;
      }
      leftHoldStartMs = 0;
      return 0;
    }

    if (navXArmed && (now - lastNavXMs >= 220)) {
      navXArmed = false;
      lastNavXMs = now;
      leftHoldStartMs = leftPressed ? now : 0;
      return leftPressed ? -1 : 1;
    }
    return 0;
  }

  // Navegação Horizontal com auto-repeat (para cursores 2D como teclado QWERTY)
  int8_t getNavXRepeat() {
    uint32_t now = millis();
    bool leftPressed = (rudder < -45);
    bool rightPressed = (rudder > 45);

    if (!leftPressed && !rightPressed) {
      if (abs(rudder) < 20) {
        navXArmed = true;
      }
      return 0;
    }

    if (navXArmed || (now - lastNavXMs >= 200)) {
      navXArmed = false;
      lastNavXMs = now;
      return leftPressed ? -1 : 1;
    }
    return 0;
  }

  // Deteta se o utilizador manteve X para a esquerda (com Y próximo de neutro) para sair de ecrãs interativos (Joystick/Comando)
  bool isHoldingLeftToExit(uint32_t holdMs = 350) {
    uint32_t now = millis();
    if (rudder < -55 && abs(throttle) < 35) {
      if (leftHoldStartMs == 0) {
        leftHoldStartMs = now;
      } else if (!leftHoldFired && (now - leftHoldStartMs >= holdMs)) {
        leftHoldFired = true;
        navXArmed = false;
        lastNavXMs = now;
        return true;
      }
    } else {
      leftHoldStartMs = 0;
      if (abs(rudder) < 25) {
        leftHoldFired = false;
      }
    }
    return false;
  }

  void disarmNavX() {
    navXArmed = false;
    leftHoldFired = true;
    lastNavXMs = millis();
  }

  void disarmNav() {
    navXArmed = false;
    navYArmed = false;
    leftHoldFired = true;
    lastNavXMs = millis();
    lastNavYMs = millis();
  }

  void update() {
    // 1. Read Joystick ADC (PIN_JOYSTICK_Y = IO0 = Eixo Vertical/Propulsão,
    // PIN_JOYSTICK_X = IO3 = Eixo Horizontal/Leme)
    int currentX = analogRead(PIN_JOYSTICK_X);
    int currentY = analogRead(PIN_JOYSTICK_Y);

    // Low-pass filter
    rawX = (rawX * 3 + currentX) / 4;
    rawY = (rawY * 3 + currentY) / 4;

    // Mapear Eixo Y (IO0 -> Throttle / Propulsão) com escala adaptativa
    static int maxUpY = 1200;
    static int maxDownY = 1200;
    int diffY = rawY - centerY;
    if (!axisYConnected && rawY > 350 && rawY < 1350) {
      throttle = 0;
    } else if (diffY > deadband) {
      int d = diffY - deadband;
      if (d > maxUpY)
        maxUpY = d;
      throttle = constrain(d * 100 / maxUpY, 0, 100);
    } else if (diffY < -deadband) {
      int d = (-diffY) - deadband;
      if (d > maxDownY)
        maxDownY = d;
      throttle = -constrain(d * 100 / maxDownY, 0, 100);
    } else {
      throttle = 0;
    }

    // Mapear Eixo X (IO3 -> Rudder / Leme) com proteção contra pino em aberto
    static int maxRightX = 1200;
    static int maxLeftX = 1200;
    int diffX = rawX - centerX;
    if (!axisXConnected) {
      // Se o contacto elétrico no pino 3 da Lafvin for restabelecido (~1.65V ->
      // ADC ~1595 estável por 20 leituras), ativa o eixo X em tempo real
      if (currentX >= 1350 && currentX <= 2200) {
        if (++validXCount >= 20) {
          centerX = rawX;
          axisXConnected = true;
        }
      } else {
        validXCount = 0;
      }
      rudder = 0;
    } else if (diffX > deadband) {
      int d = diffX - deadband;
      if (d > maxRightX)
        maxRightX = d;
      rudder = constrain(d * 100 / maxRightX, 0, 100);
    } else if (diffX < -deadband) {
      int d = (-diffX) - deadband;
      if (d > maxLeftX)
        maxLeftX = d;
      rudder = -constrain(d * 100 / maxLeftX, 0, 100);
    } else {
      rudder = 0;
    }

    // 2. Handle Joystick Switch / BOOT Button Click (Debounced Short & Long
    // Press on GPIO 9)
    int swPin = (PIN_JOYSTICK_SW >= 0) ? PIN_JOYSTICK_SW : PIN_BTN_BOOT;
    bool btnState = (digitalRead(swPin) == LOW);
    uint32_t now = millis();

    if (btnState && !lastBtnState) {
      pressStartTime = now;
      longPressFired = false;
      hold3sFired = false;
    } else if (btnState && lastBtnState) {
      if (!longPressFired && (now - pressStartTime >= 700)) {
        longPressFired = true;
        longPressOccurred = true;
      }
      if (!hold3sFired && (now - pressStartTime >= 3000)) {
        hold3sFired = true;
        hold3sOccurred = true;
      }
    } else if (!btnState && lastBtnState) {
      if (!longPressFired && (now - pressStartTime >= 40)) {
        shortPressOccurred = true;
      }
    }
    lastBtnState = btnState;

    // 3. Track button clicks on BOOT / Joystick push switch (GPIO 9)
    bool currentBoot = (digitalRead(PIN_BTN_BOOT) == LOW);
    if (currentBoot && !lastBootRawState) {
      joyClickCount++;
    }
    lastBootRawState = currentBoot;
  }

  int8_t getThrottle() const { return throttle; }
  int8_t getRudder() const { return rudder; }
  int getRawX() const { return rawX; }
  int getRawY() const { return rawY; }
  int getDeadband() const { return deadband; }

  bool isBootPressed() const { return digitalRead(PIN_BTN_BOOT) == LOW; }
  bool isModePressed() const { return digitalRead(PIN_BTN_MODE) == LOW; }
  bool isAnchorUpPressed() const {
    return digitalRead(PIN_BTN_ANCHOR_UP) == LOW;
  }
  bool isAnchorDownPressed() const {
    return digitalRead(PIN_BTN_ANCHOR_DOWN) == LOW;
  }
  uint16_t getJoyClickCount() const { return joyClickCount; }

  int8_t getAnchorJog() const {
    if (digitalRead(PIN_BTN_ANCHOR_DOWN) == LOW)
      return -1; // Lower
    if (digitalRead(PIN_BTN_ANCHOR_UP) == LOW)
      return 1; // Raise
    return 0;   // Stop
  }

private:
  int rawX;
  int rawY;
  int centerX;
  int centerY;
  bool axisXConnected;
  bool axisYConnected;
  int8_t throttle;
  int8_t rudder;
  int deadband;

  // Switch press & menu navigation tracking
  bool lastBtnState = false;
  bool longPressFired = false;
  bool hold3sFired = false;
  bool shortPressOccurred = false;
  bool longPressOccurred = false;
  bool hold3sOccurred = false;
  uint32_t pressStartTime = 0;
  bool navYArmed = true;
  bool navXArmed = true;
  uint32_t lastNavYMs = 0;
  uint32_t lastNavXMs = 0;
  uint32_t leftHoldStartMs = 0;
  bool leftHoldFired = false;

  bool lastBootRawState = false;
  uint16_t joyClickCount = 0;
  uint8_t validXCount = 0;
};
