#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>

#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <Preferences.h>
#include <WiFi.h>

#include "actuators.h"
#include "battery_monitor.h"
#include "compass_bno055.h"
#include "config_common.h"
#include "gps_tracker.h"
#include "lora_protocol.h"
#include "rover_version.h"

// =========================================================================
// WINDDRAGONS ROVER - C6 COPROCESSOR FIRMWARE
// Placa: ESP32-C6-DevKitC-1
// Responsabilidade:
//  - LoRa SX1278 (Ra-02 433MHz em barramento FSPI de hardware)
//  - Power Module (Bateria LiPo 6S nos pinos analógicos ADC)
//  - Atuadores & Servos (ESC=21, Leme=20, Guincho=19, Rack=18)
//  - Ponte UART de alta velocidade com o Lafvin Master (TX=13, RX=22)
// =========================================================================

// Configuração NVS para o Rover ID
Preferences c6Prefs;
uint8_t roverId = 1;
bool joystickEnabled = false;

// Periféricos
GPSTracker gps;
CompassBNO055 compass;
ActuatorController actuators;
BatteryMonitor battery;
Adafruit_NeoPixel rgbLed(1, PIN_RGB_LED, NEO_GRB + NEO_KHZ800);
#if defined(PIN_STROBE_LEDS) && (PIN_STROBE_LEDS >= 0)
Adafruit_NeoPixel strobeLeds(NUM_STROBE_LEDS, PIN_STROBE_LEDS,
                             NEO_GRB + NEO_KHZ800);
enum LedStripTestMode : uint8_t {
  LED_MODE_OFF = 0,
  LED_MODE_STROBE = 1,
  LED_MODE_RAINBOW = 2,
  LED_MODE_CHASE = 3,
  LED_MODE_NAV = 4,
  LED_MODE_BEACON = 5,
  LED_MODE_POLICE = 6,
  LED_MODE_SOLID = 7
};
static LedStripTestMode ledTestMode = LED_MODE_OFF;
static uint8_t ledSolidR = 255, ledSolidG = 255, ledSolidB = 255;
static uint8_t ledStripCount = NUM_STROBE_LEDS;
static uint8_t ledStripBrightPct = 100;
static bool ledForceRefresh = false;
#endif

// Hardware SPI FSPI para LoRa SX1278
SPIClass loraSpiBus(FSPI);
SX1278 radio = new Module(PIN_C6_LORA_NSS, PIN_C6_LORA_DIO0, PIN_C6_LORA_RST,
                          RADIOLIB_NC, loraSpiBus);
bool loraReady = false;
float lastRssi = 0.0f;
float lastSnr = 0.0f;

// Flag de interrupção DIO0
volatile bool packetReceived = false;

#if defined(ESP8266) || defined(ESP32)
ICACHE_RAM_ATTR
#endif
void onDio0Rx() { packetReceived = true; }

// Estatísticas LoRa Rover (Comunicações com a Base Station)
uint32_t rxPollCount = 0;
uint32_t txTelemCount = 0;
uint32_t lastRxBaseMs = 0;

// UART Inter-ESP (DevKitC -> Lafvin)
HardwareSerial interSerial(1);

void sendLoraStatToMaster() {
  uint32_t agoSec =
      (lastRxBaseMs > 0) ? (millis() - lastRxBaseMs) / 1000 : 999999;
  char statLine[128];
  // Formato:
  // LORA_STAT:<status>:<txTelems>:<rxPolls>:<lastRxSecAgo>:<rssi>:<snr>:<hasContact>
  snprintf(statLine, sizeof(statLine), "LORA_STAT:%s:%u:%u:%lu:%.1f:%.1f:%d\n",
           loraReady ? "OK" : "ERR", txTelemCount, rxPollCount,
           (unsigned long)agoSec, lastRssi, lastSnr,
           (lastRxBaseMs > 0 && agoSec < 10) ? 1 : 0);
  interSerial.print(statLine);
}

void sendCompassStatToMaster() {
  char cmpLine[128];
  // Formato:
  // COMPASS_STAT:<ready>:<heading>:<roll>:<pitch>:<calSys>:<calMag>:<offset>
  snprintf(cmpLine, sizeof(cmpLine),
           "COMPASS_STAT:%d:%.1f:%.1f:%.1f:%u:%u:%.1f\n",
           compass.isReady() ? 1 : 0, compass.getHeading(), compass.getRoll(),
           compass.getPitch(), compass.getCalSys(), compass.getCalMag(),
           compass.getHeadingOffset());
  interSerial.print(cmpLine);
}

// Estado de controlo e navegação
uint16_t telemetrySeq = 0;
uint32_t lastTelemetryTime = 0;
uint32_t lastControlReceivedTime = 0;
uint32_t lastLoRaControlMs = 0;
const uint32_t FAILSAFE_TIMEOUT_MS = 2500;
uint8_t currentMotorStatus = MOTOR_IDLE;
uint16_t lastExecutedCommandId = 0;

// Coordenadas autónomas
double homeLat = 0.0;
double homeLng = 0.0;
bool hasHomeSet = false;
double stationTargetLat = 0.0;
double stationTargetLng = 0.0;
double navTargetLat = 0.0;
double navTargetLng = 0.0;
const float STATION_KEEPING_TOLERANCE_M = 8.0f;
const float WAYPOINT_ARRIVAL_RADIUS_M = 4.0f;

void printLog(const char *format, ...) {
  char buf[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);
  Serial.print(buf);
  Serial0.print(buf);
}

void setRgbColor(uint8_t r, uint8_t g, uint8_t b) {
  rgbLed.setPixelColor(0, rgbLed.Color(r, g, b));
  rgbLed.show();
}

void updateStrobeLeds(uint32_t now) {
#if defined(PIN_STROBE_LEDS) && (PIN_STROBE_LEDS >= 0)
  static uint32_t lastStrobeRender = 0;
    if (!ledForceRefresh && (now - lastStrobeRender < 25))
      return;
  lastStrobeRender = now;

  static bool wasStrobeOn = false;
  static bool wasLit = false;
  static LedStripTestMode lastRenderedMode = LED_MODE_OFF;
  uint16_t nLeds = strobeLeds.numPixels();
  if (nLeds == 0)
    nLeds = ledStripCount;

  // Prioridade 1: Alarme ativo (manual ou comando) -> Estroboscópio náutico
  // branco de alta intensidade
  if (actuators.isAlarmActive()) {
    // Prioridade 1: Alarme ativo (manual ou comando) ou Teste Strobe Náutico ->
    // Estroboscópio náutico branco duplo-flash
    if (actuators.isAlarmActive() || ledTestMode == LED_MODE_STROBE) {
      // Padrão náutico duplo-flash a cada 600ms (40ms ON, 60ms OFF, 40ms ON,
      // 460ms OFF)
      uint32_t phase = now % 600;
      bool isOn = (phase < 40) || (phase >= 100 && phase < 140);
      if (isOn != wasStrobeOn) {
        wasStrobeOn = isOn;
        if (isOn != wasLit || lastRenderedMode != LED_MODE_STROBE ||
            ledForceRefresh) {
          wasLit = isOn;
          lastRenderedMode = LED_MODE_STROBE;
          ledForceRefresh = false;
          uint32_t color = isOn ? strobeLeds.Color(255, 255, 255) : 0;
          for (int i = 0; i < NUM_STROBE_LEDS; i++) {
            for (uint16_t i = 0; i < nLeds; i++) {
              strobeLeds.setPixelColor(i, color);
            }
            strobeLeds.show();
          }
        }
        // Prioridade 2: Modo Failsafe -> Flashing Âmbar/Vermelho a 2 Hz (250ms
        // ON, Prioridade 2: Modos de Teste e Sinalização Interativos
        else if (ledTestMode == LED_MODE_SOLID) {
          if (lastRenderedMode != LED_MODE_SOLID || ledForceRefresh ||
              !wasLit) {
            wasLit = true;
            lastRenderedMode = LED_MODE_SOLID;
            ledForceRefresh = false;
            uint32_t color = strobeLeds.Color(ledSolidR, ledSolidG, ledSolidB);
            for (uint16_t i = 0; i < nLeds; i++) {
              strobeLeds.setPixelColor(i, color);
            }
            strobeLeds.show();
          }
        } else if (ledTestMode == LED_MODE_RAINBOW) {
          wasLit = true;
          lastRenderedMode = LED_MODE_RAINBOW;
          ledForceRefresh = false;
          uint16_t baseHue = (uint16_t)((now * 24UL) & 0xFFFF);
          for (uint16_t i = 0; i < nLeds; i++) {
            uint16_t pixelHue =
                baseHue + (i * 65536UL / (nLeds > 0 ? nLeds : 1));
            strobeLeds.setPixelColor(
                i, strobeLeds.gamma32(strobeLeds.ColorHSV(pixelHue)));
          }
          strobeLeds.show();
        } else if (ledTestMode == LED_MODE_CHASE) {
          // Scanner bidirecional (Knight Rider) com cauda suave para testar
          // cada LED
          wasLit = true;
          lastRenderedMode = LED_MODE_CHASE;
          ledForceRefresh = false;
          uint16_t periodSteps = (nLeds > 1) ? ((nLeds - 1) * 2) : 1;
          uint16_t step = (now / 70) % periodSteps;
          int head = (step < nLeds) ? step : (periodSteps - step);
          for (uint16_t i = 0; i < nLeds; i++) {
            int dist = abs((int)i - head);
            if (dist == 0) {
              strobeLeds.setPixelColor(
                  i, strobeLeds.Color(ledSolidR, ledSolidG, ledSolidB));
            } else if (dist == 1) {
              strobeLeds.setPixelColor(i, strobeLeds.Color(ledSolidR / 4,
                                                           ledSolidG / 4,
                                                           ledSolidB / 4));
            } else if (dist == 2) {
              strobeLeds.setPixelColor(i, strobeLeds.Color(ledSolidR / 16,
                                                           ledSolidG / 16,
                                                           ledSolidB / 16));
            } else {
              strobeLeds.setPixelColor(i, 0);
            }
          }
          strobeLeds.show();
        } else if (ledTestMode == LED_MODE_NAV) {
          // Luzes de Navegação Náutica: Bombordo (Vermelho) + Estibordo (Verde)
          if (lastRenderedMode != LED_MODE_NAV || ledForceRefresh || !wasLit) {
            wasLit = true;
            lastRenderedMode = LED_MODE_NAV;
            ledForceRefresh = false;
            uint16_t half = nLeds / 2;
            for (uint16_t i = 0; i < nLeds; i++) {
              if (i < half) {
                strobeLeds.setPixelColor(
                    i, strobeLeds.Color(255, 0, 0)); // Bombordo
              } else {
                strobeLeds.setPixelColor(
                    i, strobeLeds.Color(0, 255, 0)); // Estibordo
              }
            }
            strobeLeds.show();
          }
        } else if (ledTestMode == LED_MODE_BEACON) {
          // Flash Âmbar / Laranja de aviso a 2 Hz
          uint32_t phase = now % 500;
          bool isOn = (phase < 250);
          if (isOn != wasLit || lastRenderedMode != LED_MODE_BEACON ||
              ledForceRefresh) {
            wasLit = isOn;
            lastRenderedMode = LED_MODE_BEACON;
            ledForceRefresh = false;
            uint32_t color = isOn ? strobeLeds.Color(255, 80, 0) : 0;
            for (uint16_t i = 0; i < nLeds; i++) {
              strobeLeds.setPixelColor(i, color);
            }
            strobeLeds.show();
          }
        } else if (ledTestMode == LED_MODE_POLICE) {
          // Emergência Vermelho / Azul alternado
          wasLit = true;
          lastRenderedMode = LED_MODE_POLICE;
          ledForceRefresh = false;
          bool side = ((now / 120) % 2) == 0;
          uint16_t half = nLeds / 2;
          if (half == 0)
            half = 1;
          for (uint16_t i = 0; i < nLeds; i++) {
            bool firstHalf = (i < half);
            if (firstHalf == side) {
              strobeLeds.setPixelColor(i, strobeLeds.Color(255, 0, 0));
            } else {
              strobeLeds.setPixelColor(i, strobeLeds.Color(0, 40, 255));
            }
          }
          strobeLeds.show();
        }
        // Prioridade 3: Modo Failsafe -> Flashing Âmbar/Vermelho a 2 Hz (250ms
        // ON, 250ms OFF)
        else if (currentMotorStatus == MOTOR_FAILSAFE) {
          uint32_t phase = now % 500;
          bool isOn = (phase < 250);
          if (isOn != wasStrobeOn) {
            wasStrobeOn = isOn;
            if (isOn != wasLit || lastRenderedMode != LED_MODE_OFF ||
                ledForceRefresh) {
              wasLit = isOn;
              lastRenderedMode = LED_MODE_OFF;
              ledForceRefresh = false;
              uint32_t color = isOn ? strobeLeds.Color(255, 60, 0) : 0;
              for (int i = 0; i < NUM_STROBE_LEDS; i++) {
                for (uint16_t i = 0; i < nLeds; i++) {
                  strobeLeds.setPixelColor(i, color);
                }
                strobeLeds.show();
              }
            }
            // Estado Normal: LEDs apagados (standby silencioso)
            else {
              if (wasStrobeOn) {
                wasStrobeOn = false;
                if (wasLit || lastRenderedMode != LED_MODE_OFF ||
                    ledForceRefresh) {
                  wasLit = false;
                  lastRenderedMode = LED_MODE_OFF;
                  ledForceRefresh = false;
                  strobeLeds.clear();
                  strobeLeds.show();
                }
              }
#endif
            }

            void sendLoRaTelemetry() {
              if (!loraReady)
                return;

              PacketTelemetry packet;
              memset(&packet, 0, sizeof(packet));

              packet.magic = LORA_MAGIC_BYTE;
              packet.msg_type = MSG_TELEMETRY;
              packet.rover_id = roverId;
              packet.seq_num = telemetrySeq++;

              packet.lat_deg7 = gps.getLatDeg7();
              packet.lng_deg7 = gps.getLngDeg7();
              packet.speed_cm_s =
                  static_cast<uint16_t>(gps.getSpeedKnots() * 51.4444f);
              packet.heading_deg10 =
                  static_cast<uint16_t>(gps.getHeading() * 10.0f);
              packet.satellites = gps.getSatellites();
              packet.fix_quality = gps.getFixQuality();

              packet.battery_mv = battery.getVoltageMv();
              packet.battery_pct = battery.getPercentage();
              packet.motor_status = currentMotorStatus;
              packet.anchor_status = actuators.getAnchorStatus();
              packet.anchor_depth_cm = actuators.getAnchorDepthCm();
              packet.last_cmd_id = lastExecutedCommandId;
              packet.checksum =
                  calculate_checksum(reinterpret_cast<const uint8_t *>(&packet),
                                     sizeof(packet) - sizeof(packet.checksum));

              txTelemCount++;
              radio.standby();
              radio.transmit(reinterpret_cast<uint8_t *>(&packet),
                             sizeof(packet));
              packetReceived = false;
              radio.startReceive();
              sendLoraStatToMaster();
            }

            void processIncomingLoRa(uint8_t *buffer, int len) {
              if (len < 4)
                return;
              if (buffer[0] != LORA_MAGIC_BYTE)
                return;

              uint8_t msgType = buffer[1];
              uint8_t targetRoverId = buffer[2];
              if (targetRoverId != 0xFF && targetRoverId != roverId)
                return;

              lastRssi = radio.getRSSI();
              lastSnr = radio.getSNR();
              rxPollCount++;
              lastRxBaseMs = millis();
              lastControlReceivedTime = millis();

              if (msgType == MSG_POLL_REQUEST) {
                printLog("[C6 ROVER] <<< LoRa Poll da Base recebido! A "
                         "responder com "
                         "telemetria (TX #%u)\n",
                         telemetrySeq);
                sendLoRaTelemetry();
                sendLoraStatToMaster();
                return;
              }

              if (msgType == MSG_MANUAL_CONTROL &&
                  len >= (int)sizeof(PacketManualControl)) {
                PacketManualControl *ctrl =
                    reinterpret_cast<PacketManualControl *>(buffer);
                uint16_t calc =
                    calculate_checksum(buffer, sizeof(PacketManualControl) -
                                                   sizeof(ctrl->checksum));
                if (ctrl->checksum != 0 && calc != ctrl->checksum)
                  return;

                lastLoRaControlMs = millis();
                actuators.setThrottle(ctrl->throttle);
                actuators.setRudder(ctrl->rudder);
                static int8_t lastLoRaJog = 0;
                if (ctrl->anchor_jog != lastLoRaJog || ctrl->anchor_jog != 0) {
                  actuators.jogWinch(ctrl->anchor_jog);
                  lastLoRaJog = ctrl->anchor_jog;
                }
                if (ctrl->throttle != 0 || ctrl->rudder != 0 ||
                    ctrl->mode_request == 0) {
                  currentMotorStatus = MOTOR_MANUAL;
                } else if (ctrl->mode_request == 1) {
                  if (currentMotorStatus != MOTOR_HOLDING_STATION) {
                    stationTargetLat = gps.getLatitude();
                    stationTargetLng = gps.getLongitude();
                    currentMotorStatus = MOTOR_HOLDING_STATION;
                  }
                }
                static uint32_t lastCtrlRgbMs = 0;
                if (millis() - lastCtrlRgbMs >= 1000) {
                  lastCtrlRgbMs = millis();
                  setRgbColor(0, 50, 0); // Green: Control active
                }
                printLog("[C6 ROVER] <<< LoRa CTRL recebido: Thr=%d%%, "
                         "Rud=%d%%, Jog=%d "
                         "(RSSI=%.1f dBm)\n",
                         ctrl->throttle, ctrl->rudder, ctrl->anchor_jog,
                         lastRssi);
                sendLoraStatToMaster();
              } else if (msgType == MSG_ACTION_COMMAND &&
                         len >= (int)sizeof(PacketActionCommand)) {
                PacketActionCommand *cmd =
                    reinterpret_cast<PacketActionCommand *>(buffer);
                uint16_t calc =
                    calculate_checksum(buffer, sizeof(PacketActionCommand) -
                                                   sizeof(cmd->checksum));
                if (cmd->checksum != 0 && calc != cmd->checksum)
                  return;

                lastExecutedCommandId = cmd->command_id;
                printLog("[C6 ROVER] <<< LoRa ACAO recebida: CmdID=#%u | "
                         "Acao=%u (%s) | "
                         "Depth=%u cm\n",
                         cmd->command_id, cmd->action,
                         get_action_str(cmd->action), cmd->target_depth_cm);

                switch (cmd->action) {
                case ACTION_HOLD_STATION:
                  if (cmd->target_lat_deg7 != 0 && cmd->target_lng_deg7 != 0) {
                    stationTargetLat = cmd->target_lat_deg7 / 10000000.0;
                    stationTargetLng = cmd->target_lng_deg7 / 10000000.0;
                  } else {
                    stationTargetLat = gps.getLatitude();
                    stationTargetLng = gps.getLongitude();
                  }
                  currentMotorStatus = MOTOR_HOLDING_STATION;
                  break;
                case ACTION_STOP:
                  actuators.emergencyStop();
                  currentMotorStatus = MOTOR_IDLE;
                  break;
                case ACTION_DROP_ANCHOR:
                  actuators.commandDropAnchor(
                      cmd->target_depth_cm > 0 ? cmd->target_depth_cm : 300);
                  currentMotorStatus = MOTOR_ANCHORED;
                  break;
                case ACTION_RETRIEVE_ANCHOR:
                  actuators.commandRetractAnchor();
                  currentMotorStatus = MOTOR_IDLE;
                  break;
                case ACTION_STOP_ANCHOR:
                  actuators.stopWinch();
                  break;
                case ACTION_NAVIGATE:
                  if (cmd->target_lat_deg7 != 0 && cmd->target_lng_deg7 != 0) {
                    navTargetLat = cmd->target_lat_deg7 / 10000000.0;
                    navTargetLng = cmd->target_lng_deg7 / 10000000.0;
                    currentMotorStatus = MOTOR_MOVING_TO_TARGET;
                  }
                  break;
                case ACTION_RTL:
                  if (hasHomeSet) {
                    navTargetLat = homeLat;
                    navTargetLng = homeLng;
                    currentMotorStatus = MOTOR_RTL;
                  }
                  break;
                case ACTION_ALARM:
                  actuators.triggerAlarm(cmd->duration_s > 0 ? cmd->duration_s
                                                             : 10);
                  break;
                case ACTION_PING:
                  break;
                case ACTION_MANUAL:
                  currentMotorStatus = MOTOR_MANUAL;
                  lastControlReceivedTime = millis();
                  break;
                case 13: // RELAX RACK
                  actuators.relaxRack();
                  break;
                case 14: // WINCH LOWER
                  actuators.setWinchMicroseconds(1200);
                  break;
                case 15: // ZERO ANCHOR
                  actuators.zeroAnchorCounter();
                  break;
                case 16: // ENGAGE RACK
                  actuators.engageRack();
                  break;
                case 17: // RELEASE RACK
                  actuators.releaseRack();
                  break;
                default:
                  break;
                }

                // Responder imediatamente com Telemetria LoRa atualizada para a
                // Base confirmar a ação
                delay(15);
                sendLoRaTelemetry();
                lastTelemetryTime = millis();
                sendLoraStatToMaster();
              }
            }

            void updateAutonomousNavigation() {
              if (currentMotorStatus != MOTOR_HOLDING_STATION &&
                  currentMotorStatus != MOTOR_MOVING_TO_TARGET &&
                  currentMotorStatus != MOTOR_RTL) {
                return;
              }

              if (!gps.hasFix()) {
                actuators.stopMotor();
                actuators.centerRudder();
                return;
              }

              if (!hasHomeSet) {
                homeLat = gps.getLatitude();
                homeLng = gps.getLongitude();
                hasHomeSet = true;
              }

              if (currentMotorStatus == MOTOR_HOLDING_STATION) {
                double distM =
                    gps.distanceTo(stationTargetLat, stationTargetLng);
                if (distM > STATION_KEEPING_TOLERANCE_M) {
                  double targetCourse =
                      gps.courseTo(stationTargetLat, stationTargetLng);
                  double headingError = targetCourse - gps.getHeading();
                  while (headingError > 180.0)
                    headingError -= 360.0;
                  while (headingError < -180.0)
                    headingError += 360.0;

                  int8_t rudder = constrain(
                      static_cast<int>(headingError * 1.5f), -100, 100);
                  actuators.setRudder(rudder);
                  int8_t throttle = (abs(headingError) > 50.0) ? 20 : 35;
                  actuators.setThrottle(throttle);
                } else {
                  actuators.stopMotor();
                  actuators.centerRudder();
                }
              } else if (currentMotorStatus == MOTOR_MOVING_TO_TARGET ||
                         currentMotorStatus == MOTOR_RTL) {
                double distM = gps.distanceTo(navTargetLat, navTargetLng);
                if (distM <= WAYPOINT_ARRIVAL_RADIUS_M) {
                  actuators.stopMotor();
                  actuators.centerRudder();
                  if (currentMotorStatus == MOTOR_MOVING_TO_TARGET) {
                    stationTargetLat = navTargetLat;
                    stationTargetLng = navTargetLng;
                    currentMotorStatus = MOTOR_HOLDING_STATION;
                  } else {
                    currentMotorStatus = MOTOR_IDLE;
                  }
                } else {
                  double targetCourse =
                      gps.courseTo(navTargetLat, navTargetLng);
                  double headingError = targetCourse - gps.getHeading();
                  while (headingError > 180.0)
                    headingError -= 360.0;
                  while (headingError < -180.0)
                    headingError += 360.0;

                  int8_t rudder = constrain(
                      static_cast<int>(headingError * 1.8f), -100, 100);
                  actuators.setRudder(rudder);
                  int8_t throttle = (abs(headingError) > 60.0) ? 25 : 50;
                  actuators.setThrottle(throttle);
                }
              }
            }

            void performHttpOta(const char *ssid, const char *pass,
                                const char *url) {
              printLog("\n[OTA] ==========================================\n");
              printLog(
                  "[OTA] Inicio de Atualizacao OTA no Coprocessador C6!\n");
              printLog("[OTA] Versao Atual: %s\n", ROVER_C6_VERSION);
              printLog("[OTA] Rede WiFi    : %s\n", ssid);
              printLog("[OTA] URL Alvo     : %s\n", url);
              printLog("[OTA] ==========================================\n");

              // PASSO 1: Preparação & Paragem de Atuadores
              // Cor: ROXO / MAGENTA (40, 0, 40)
              setRgbColor(40, 0, 40);
              actuators.stopMotor();
              actuators.centerRudder();
              actuators.stopWinch();
              currentMotorStatus = MOTOR_IDLE;

              // Libertar LoRa
              if (loraReady) {
                radio.sleep();
              }

              // Notificar Master pela UART que o OTA foi iniciado
              interSerial.println("PONG:OTA:STARTING");
              interSerial.flush();
              delay(100);

              // PASSO 2: Conexão à rede Wi-Fi
              // Cor: LARANJA / AMARELO (50, 25, 0)
              setRgbColor(50, 25, 0);
              interSerial.println("PONG:OTA:WIFI_CONNECTING");
              interSerial.flush();

              WiFi.mode(WIFI_STA);
              WiFi.disconnect(false);
              delay(100);
              WiFi.begin(ssid, (pass && strlen(pass) > 0) ? pass : nullptr);

              uint32_t startConn = millis();
              bool connected = false;
              while (millis() - startConn < 10000) {
                if (WiFi.status() == WL_CONNECTED) {
                  connected = true;
                  break;
                }
                delay(200);
                printLog(".");
              }

              if (!connected) {
                printLog("\n[OTA] ERRO: Falha ao conectar a rede WiFi '%s'\n",
                         ssid);
                // PASSO ERRO: VERMELHO INTENSO (70, 0, 0)
                setRgbColor(70, 0, 0);
                interSerial.println("PONG:OTA:FAIL_WIFI");
                interSerial.flush();
                WiFi.disconnect(true);
                WiFi.mode(WIFI_OFF);
                if (loraReady)
                  radio.startReceive();
                return;
              }

              printLog("\n[OTA] Conectado ao WiFi! IP: %s\n",
                       WiFi.localIP().toString().c_str());

              // PASSO 3: Wi-Fi Conectado / A Contactar Servidor Web
              // Cor: CIANO (0, 45, 50)
              setRgbColor(0, 45, 50);
              interSerial.println("PONG:OTA:DOWNLOADING");
              interSerial.flush();

              WiFiClient client;
              client.setTimeout(25000);
              httpUpdate.rebootOnUpdate(
                  false); // Manter controlo para confirmar ao Master antes de
                          // reiniciar

              // Registar callbacks para cada fase do processo
              httpUpdate.onStart([&]() {
                printLog("[OTA] Download e gravacao da Flash iniciados!\n");
                // PASSO 4: Download & Flash Write
                // Cor: AZUL PROFUNDO (0, 0, 60)
                setRgbColor(0, 0, 60);
                interSerial.println("PONG:OTA:FLASH_START");
                interSerial.flush();
              });

              httpUpdate.onProgress([&](int current, int total) {
                static int lastPct = -1;
                static bool blink = false;
                int pct = (total > 0) ? (current * 100 / total) : 0;
                if (pct != lastPct && (pct % 5 == 0 || pct == 100)) {
                  lastPct = pct;
                  blink = !blink;
                  // Alternar tom de azul durante a escrita na Flash para
                  // feedback visual dinâmico
                  if (blink) {
                    setRgbColor(0, 0, 60); // Azul profundo
                  } else {
                    setRgbColor(0, 30, 60); // Azul-claro
                  }
                  char progMsg[32];
                  snprintf(progMsg, sizeof(progMsg), "PONG:OTA:PROGRESS:%d",
                           pct);
                  interSerial.println(progMsg);
                  interSerial.flush();
                  printLog("[OTA] Progresso Flash: %d%% (%d / %d bytes)\n", pct,
                           current, total);
                }
              });

              httpUpdate.onEnd([&]() {
                printLog("[OTA] Gravacao da Flash terminada com integridade "
                         "verificada!\n");
                // PASSO 5: Sucesso / Concluído
                // Cor: VERDE BRILHANTE (0, 70, 0)
                setRgbColor(0, 70, 0);
                interSerial.println("PONG:OTA:PROGRESS:100");
                interSerial.flush();
              });

              httpUpdate.onError([&](int err) {
                printLog("[OTA] Erro durante atualizacao HTTP: %d\n", err);
                // PASSO ERRO: VERMELHO INTENSO (70, 0, 0)
                setRgbColor(70, 0, 0);
              });

              t_httpUpdate_return ret = httpUpdate.update(client, url);

              if (ret == HTTP_UPDATE_OK) {
                printLog(
                    "[OTA] ★★★ SUCESSO! Novo firmware gravado na Flash! ★★★\n");
                // PASSO 5: SUCESSO TOTAL
                // Cor: VERDE BRILHANTE (0, 80, 0)
                setRgbColor(0, 80, 0);
                interSerial.println("PONG:OTA:SUCCESS");
                interSerial.flush();
                delay(1200);
                ESP.restart();
              } else {
                int errCode = httpUpdate.getLastError();
                String errStr = httpUpdate.getLastErrorString();
                printLog("[OTA] Falha na gravacao OTA: (%d) %s\n", errCode,
                         errStr.c_str());
                // PASSO ERRO: VERMELHO INTENSO (80, 0, 0)
                setRgbColor(80, 0, 0);
                char failMsg[64];
                snprintf(failMsg, sizeof(failMsg), "PONG:OTA:FAIL:%d", errCode);
                interSerial.println(failMsg);
                interSerial.flush();
                WiFi.disconnect(true);
                WiFi.mode(WIFI_OFF);
                if (loraReady)
                  radio.startReceive();
              }
            }

            void setup() {
              Serial.begin(115200);
              Serial0.begin(115200);
              Serial.setTxTimeoutMs(0);
              delay(500);

              // Carregar ID e Bateria da memória NVS
              c6Prefs.begin("rover_c6", false);
              roverId = c6Prefs.getUChar("id", 1);
              joystickEnabled = c6Prefs.getBool("joy_en", false);
              uint8_t c6Cells = c6Prefs.getUChar("cells", 4);
              if (c6Cells == 3 || c6Cells == 4 || c6Cells == 6) {
                battery.setCellCount(c6Cells);
              }

              printLog("\n====================================================="
                       "==\n");
              printLog(
                  "  WINDDRAGONS ROVER-%02d: C6 COPROCESSOR (LORA+NAV+ACT)\n",
                  roverId);
              printLog("  Firmware: Coprocessador C6 %s (%s)\n",
                       ROVER_C6_VERSION, ROVER_BUILD_DATE);
              printLog("  Identificador NVS ativo: ROVER-%02d | Bateria: %uS\n",
                       roverId, battery.getCellCount());
              printLog(
                  "=======================================================\n");

              // 1. LED RGB NeoPixel
              rgbLed.begin();
              setRgbColor(30, 30, 0); // Yellow: Booting

              // 2. Inter-ESP UART
              interSerial.setRxBufferSize(512);
              interSerial.begin(INTER_ESP_BAUDRATE, SERIAL_8N1,
                                PIN_INTER_ESP_C6_RX, PIN_INTER_ESP_C6_TX);
              interSerial.println("PONG:VER:" ROVER_C6_VERSION);
              interSerial.flush();
              Serial.printf("[UART] Canal serie com Lafvin iniciado nos pinos "
                            "TX=%d, RX=%d "
                            "(%d baud) | FW: %s\n",
                            PIN_INTER_ESP_C6_TX, PIN_INTER_ESP_C6_RX,
                            INTER_ESP_BAUDRATE, ROVER_C6_VERSION);

              // 3. Monitor de Bateria LiPo (3S, 4S ou 6S)
              battery.beginDirectAdc();
              Serial.printf("[BATT] Monitor %uS LiPo ativo nos pinos "
                            "VOLT=GPIO%d, CURR=GPIO%d\n",
                            battery.getCellCount(), PIN_C6_ADC_VOLT,
                            PIN_C6_ADC_CURR);

              // 4. GPS Quectel LC29H (em bancada desligado para não colidir com
              // UART0 de debug)
              gps.begin(false);
              Serial.println(
                  "[GPS ] GPS Quectel preparado (modo bancada/simulado)");

              // 4b. Bússola 9-DOF GY-BNO055 na ilha J_EXP (SDA=GPIO1,
              // SCL=GPIO11)
              float cmpOffset = c6Prefs.getFloat("cmp_off", 0.0f);
              compass.setHeadingOffset(cmpOffset);
              compass.begin(PIN_C6_EXP_SDA, PIN_C6_EXP_SCL);

              // 5. Atuadores (Direct PWM / PCA9685)
              float ancDiam = c6Prefs.getFloat("anc_diam", 30.0f);
              uint8_t ancPulses = c6Prefs.getUChar("anc_pulses", 1);
              uint8_t rckEng = c6Prefs.getUChar("rck_eng", RACK_POS_ENGAGED);
              uint8_t rckRel = c6Prefs.getUChar("rck_rel", RACK_POS_RELEASED);
              bool rckInv =
                  c6Prefs.getBool("rck_inv", RACK_SERVO_INVERTED_DEFAULT);
              actuators.setRackConfig(rckEng, rckRel, rckInv, false);
              actuators.begin();
              actuators.setAnchorCalibration(ancDiam, ancPulses);
              Serial.printf(
                  "[ACT ] Atuadores inicializados (ESC=GPIO%d, Leme=GPIO%d, "
                  "Guincho=GPIO%d, Rack=GPIO%d [Encosto=%u°, Livre=%u°, "
                  "Inv=%d], "
                  "FimCurso=GPIO%d, Hall=GPIO%d | Tambor=%.1fmm)\n",
                  PIN_C6_MOTOR_ESC, PIN_C6_SERVO_RUD, PIN_C6_WINCH,
                  PIN_C6_SERVO_RACK, rckEng, rckRel, rckInv ? 1 : 0,
                  PIN_ANCHOR_DETECTOR, PIN_ANCHOR_HALL, ancDiam);

// 6. Fita / Módulo de LEDs WS2812B Strobe
#if defined(PIN_STROBE_LEDS) && (PIN_STROBE_LEDS >= 0)
              ledStripCount = c6Prefs.getUChar("led_cnt", NUM_STROBE_LEDS);
              if (ledStripCount < 1 || ledStripCount > 120)
                ledStripCount = NUM_STROBE_LEDS;
              ledStripBrightPct = c6Prefs.getUChar("led_brt", 100);
              if (ledStripBrightPct < 5 || ledStripBrightPct > 100)
                ledStripBrightPct = 100;
              strobeLeds.begin();
              if (ledStripCount != strobeLeds.numPixels()) {
                strobeLeds.updateLength(ledStripCount);
              }
              strobeLeds.setBrightness(map(ledStripBrightPct, 0, 100, 0, 255));
              strobeLeds.clear();
              strobeLeds.show();
              Serial.printf("[STROBE] LEDs WS2812B Strobe iniciados no GPIO %d "
                            "(%d LEDs)\n",
                            PIN_STROBE_LEDS, NUM_STROBE_LEDS);
              Serial.printf("[STROBE] LEDs WS2812B Strobe iniciados no GPIO %d "
                            "(%u LEDs, Brilho=%u%%)\n",
                            PIN_STROBE_LEDS, ledStripCount, ledStripBrightPct);
#endif

              // 7. LoRa SX1278 (FSPI Hardware)
              pinMode(PIN_C6_LORA_NSS, OUTPUT);
              digitalWrite(PIN_C6_LORA_NSS, HIGH);
              pinMode(PIN_C6_LORA_DIO0, INPUT);

              loraSpiBus.begin(PIN_C6_LORA_SCK, PIN_C6_LORA_MISO,
                               PIN_C6_LORA_MOSI, PIN_C6_LORA_NSS);
              delay(50);

              Serial.println(
                  "[LORA] A inicializar RadioLib SX1278 (433.0 MHz)...");
              int state = radio.begin(LORA_FREQUENCY, LORA_BANDWIDTH,
                                      LORA_SPREAD_FACTOR, LORA_CODING_RATE,
                                      LORA_SYNC_WORD, LORA_OUTPUT_POWER,
                                      LORA_PREAMBLE_LEN, 0);
              if (state == RADIOLIB_ERR_NONE) {
                loraReady = true;
                radio.setPacketReceivedAction(onDio0Rx);
                packetReceived = false;
                radio.startReceive();
                Serial.println("[LORA] ★★★ SX1278 PRONTO E EM ESCUTA RX! ★★★");
                setRgbColor(0, 40, 0); // Green
              } else {
                loraReady = false;
                Serial.printf("[LORA] Falha na inicialização (código: %d)\n",
                              state);
                setRgbColor(40, 0, 0); // Red
              }
            }

            void handleIncomingCommand(const char *cmdStr) {
              if (!cmdStr || strlen(cmdStr) == 0 || strcmp(cmdStr, "NONE") == 0)
                return;

              uint32_t now = millis();
              if (strncmp(cmdStr, "CMD:JOY:", 8) == 0) {
                int thr = 0, rud = 0;
                if (sscanf(cmdStr + 8, "%d:%d", &thr, &rud) == 2) {
                  // Ignora joystick local se estiver desativado ou se houver
                  // controlo LoRa ativo da Base
                  if (!joystickEnabled || (lastLoRaControlMs > 0 &&
                                           (now - lastLoRaControlMs < 3000))) {
                    return;
                  }
                  actuators.setThrottle(thr);
                  actuators.setRudder(rud);
                  if (thr != 0 || rud != 0) {
                    currentMotorStatus = MOTOR_MANUAL;
                  }
                  lastControlReceivedTime = now;
                }
              } else if (strncmp(cmdStr, "CMD:JOY_EN:", 11) == 0) {
                int en = atoi(cmdStr + 11);
                bool wasEnabled = joystickEnabled;
                joystickEnabled = (en != 0);
                c6Prefs.putBool("joy_en", joystickEnabled);
                if (!joystickEnabled && wasEnabled) {
                  actuators.stopMotor();
                  actuators.centerRudder();
                  if (currentMotorStatus == MOTOR_MANUAL) {
                    currentMotorStatus = MOTOR_IDLE;
                  }
                }
                printLog("[C6 ROVER] Joystick fisico onboard %s\n",
                         joystickEnabled ? "ATIVADO" : "DESATIVADO (ignorado)");
              } else if (strcmp(cmdStr, "CMD:MODE:MANUAL") == 0) {
                currentMotorStatus = MOTOR_MANUAL;
                lastControlReceivedTime = now;
                printLog("[C6 ROVER] Modo Manual ativado via Web/Lafvin\n");
              } else if (strncmp(cmdStr, "CMD:TX_TELEM", 12) == 0) {
                sendLoRaTelemetry();
              } else if (strncmp(cmdStr, "CMD:GET_LORA", 12) == 0) {
                sendLoraStatToMaster();
              } else if (strncmp(cmdStr, "CMD:LEME:", 9) == 0) {
                int val = atoi(cmdStr + 9);
                actuators.setRudder(val);
                lastControlReceivedTime = now;
              } else if (strncmp(cmdStr, "CMD:MOTOR:", 10) == 0) {
                int val = atoi(cmdStr + 10);
                actuators.setThrottle(val);
                lastControlReceivedTime = now;
              } else if (strncmp(cmdStr, "CMD:WINCH:", 10) == 0) {
                int pulse = atoi(cmdStr + 10);
                if (pulse <= 0) {
                  actuators.stopWinch();
                } else {
                  actuators.setWinchMicroseconds(pulse);
                }
              } else if (strcmp(cmdStr, "CMD:RACK:RELEASE") == 0) {
                actuators.releaseRack();
              } else if (strcmp(cmdStr, "CMD:RACK:ENGAGE") == 0) {
                actuators.engageRack();
              } else if (strcmp(cmdStr, "CMD:RACK:RELAX") == 0) {
                actuators.relaxRack();
              } else if (strncmp(cmdStr, "CMD:RACK:ANGLE:", 15) == 0) {
                int ang = atoi(cmdStr + 15);
                actuators.setRackAngle((uint8_t)constrain(ang, 0, 180));
              } else if (strncmp(cmdStr, "CMD:RACK_CFG:", 13) == 0) {
                int eng = RACK_POS_ENGAGED, rel = RACK_POS_RELEASED,
                    inv = RACK_SERVO_INVERTED_DEFAULT ? 1 : 0;
                if (sscanf(cmdStr + 13, "%d:%d:%d", &eng, &rel, &inv) >= 2) {
                  eng = constrain(eng, 0, 180);
                  rel = constrain(rel, 0, 180);
                  bool invBool = (inv != 0);
                  actuators.setRackConfig((uint8_t)eng, (uint8_t)rel, invBool,
                                          true);
                  c6Prefs.putUChar("rck_eng", (uint8_t)eng);
                  c6Prefs.putUChar("rck_rel", (uint8_t)rel);
                  c6Prefs.putBool("rck_inv", invBool);
                  printLog("[C6 ROVER] Config Cremalheira gravada: "
                           "Encosto=%d°, Livre=%d°, "
                           "Invertido=%s\n",
                           eng, rel, invBool ? "SIM" : "NAO");
                }
              } else if (strcmp(cmdStr, "CMD:EMERGENCY_STOP") == 0) {
                actuators.emergencyStop();
                currentMotorStatus = MOTOR_IDLE;
              } else if (strcmp(cmdStr, "CMD:DROP_ANCHOR") == 0) {
                actuators.commandDropAnchor(300);
                currentMotorStatus = MOTOR_ANCHORED;
              } else if (strcmp(cmdStr, "CMD:RETRACT_ANCHOR") == 0) {
                actuators.commandRetractAnchor();
                currentMotorStatus = MOTOR_IDLE;
              } else if (strcmp(cmdStr, "CMD:STOP_ANCHOR") == 0) {
                actuators.stopWinch();
              } else if (strncmp(cmdStr, "CMD:BATT:", 9) == 0) {
                int cells = atoi(cmdStr + 9);
                if (cells == 3 || cells == 4 || cells == 6) {
                  battery.setCellCount((uint8_t)cells);
                  c6Prefs.putUChar("cells", (uint8_t)cells);
                  printLog("[C6 ROVER] ★ Bateria configurada para %uS e "
                           "gravada em NVS\n",
                           cells);
                }
              } else if (strcmp(cmdStr, "CMD:BATT_DIAG") == 0 ||
                         strcmp(cmdStr, "BATT") == 0) {
                battery.update();
                char diagMsg[180];
                snprintf(diagMsg, sizeof(diagMsg),
                         "[BATT DIAG] Pino VOLT (GPIO%d/J_BATT-3)=%.1f mV -> "
                         "%.2f V | "
                         "Pino CURR (GPIO%d/J_BATT-2)=%.1f mV -> %.2f A\n",
                         PIN_C6_ADC_VOLT, battery.getRawVoltAdcMv(),
                         battery.getVoltageMv() / 1000.0f, PIN_C6_ADC_CURR,
                         battery.getRawCurrAdcMv(), battery.getCurrentAmps());
                printLog("%s", diagMsg);
                interSerial.print(diagMsg);
              } else if (strncmp(cmdStr, "CMD:ALARM:", 10) == 0) {
                int dur = atoi(cmdStr + 10);
                if (dur > 0) {
                  actuators.triggerAlarm((uint16_t)dur);
                  printLog(
                      "[C6 ROVER] Alarme/Strobe WS2812B ATIVADO (%d seg)\n",
                      dur);
                } else {
                  actuators.stopAlarm();
#if defined(PIN_STROBE_LEDS) && (PIN_STROBE_LEDS >= 0)
                  ledTestMode = LED_MODE_OFF;
                  ledForceRefresh = true;
#endif
                  printLog("[C6 ROVER] Alarme/Strobe WS2812B DESLIGADO\n");
                }
#if defined(PIN_STROBE_LEDS) && (PIN_STROBE_LEDS >= 0)
              } else if (strcmp(cmdStr, "CMD:LED:OFF") == 0) {
                actuators.stopAlarm();
                ledTestMode = LED_MODE_OFF;
                ledForceRefresh = true;
                updateStrobeLeds(now);
                printLog(
                    "[C6 ROVER] ★ Fita de LEDs WS2812B DESLIGADA (Standby)\n");
              } else if (strncmp(cmdStr, "CMD:LED:MODE:", 13) == 0) {
                const char *m = cmdStr + 13;
                actuators.stopAlarm();
                if (strcasecmp(m, "OFF") == 0) {
                  ledTestMode = LED_MODE_OFF;
                } else if (strcasecmp(m, "STROBE") == 0) {
                  ledTestMode = LED_MODE_STROBE;
                } else if (strcasecmp(m, "RAINBOW") == 0) {
                  ledTestMode = LED_MODE_RAINBOW;
                } else if (strcasecmp(m, "CHASE") == 0) {
                  if (ledSolidR == 0 && ledSolidG == 0 && ledSolidB == 0) {
                    ledSolidR = 6;
                    ledSolidG = 182;
                    ledSolidB = 212;
                  }
                  ledTestMode = LED_MODE_CHASE;
                } else if (strcasecmp(m, "NAV") == 0) {
                  ledTestMode = LED_MODE_NAV;
                } else if (strcasecmp(m, "BEACON") == 0) {
                  ledTestMode = LED_MODE_BEACON;
                } else if (strcasecmp(m, "POLICE") == 0) {
                  ledTestMode = LED_MODE_POLICE;
                } else if (strcasecmp(m, "SOLID") == 0) {
                  ledTestMode = LED_MODE_SOLID;
                }
                ledForceRefresh = true;
                updateStrobeLeds(now);
                printLog("[C6 ROVER] ★ Fita de LEDs WS2812B Modo='%s' (%u LEDs "
                         "no GPIO %d)\n",
                         m, ledStripCount, PIN_STROBE_LEDS);
              } else if (strncmp(cmdStr, "CMD:LED:SOLID:", 14) == 0) {
                int r = 255, g = 255, b = 255;
                if (sscanf(cmdStr + 14, "%d:%d:%d", &r, &g, &b) == 3) {
                  actuators.stopAlarm();
                  ledSolidR = (uint8_t)constrain(r, 0, 255);
                  ledSolidG = (uint8_t)constrain(g, 0, 255);
                  ledSolidB = (uint8_t)constrain(b, 0, 255);
                  if (ledSolidR == 0 && ledSolidG == 0 && ledSolidB == 0) {
                    ledTestMode = LED_MODE_OFF;
                  } else {
                    ledTestMode = LED_MODE_SOLID;
                  }
                  ledForceRefresh = true;
                  updateStrobeLeds(now);
                  printLog("[C6 ROVER] ★ Fita de LEDs WS2812B Cor Solida "
                           "RGB(%u, %u, %u)\n",
                           ledSolidR, ledSolidG, ledSolidB);
                }
              } else if (strncmp(cmdStr, "CMD:LED:BRIGHT:", 15) == 0) {
                int brt = atoi(cmdStr + 15);
                ledStripBrightPct = (uint8_t)constrain(brt, 5, 100);
                strobeLeds.setBrightness(
                    map(ledStripBrightPct, 0, 100, 0, 255));
                c6Prefs.putUChar("led_brt", ledStripBrightPct);
                ledForceRefresh = true;
                updateStrobeLeds(now);
                printLog(
                    "[C6 ROVER] ★ Brilho da Fita de LEDs ajustado para %u%%\n",
                    ledStripBrightPct);
              } else if (strncmp(cmdStr, "CMD:LED:CFG:", 12) == 0) {
                int cnt = NUM_STROBE_LEDS, brt = 100;
                if (sscanf(cmdStr + 12, "%d:%d", &cnt, &brt) >= 1) {
                  ledStripCount = (uint8_t)constrain(cnt, 1, 120);
                  ledStripBrightPct = (uint8_t)constrain(brt, 5, 100);
                  if (ledStripCount != strobeLeds.numPixels()) {
                    strobeLeds.clear();
                    strobeLeds.show();
                    strobeLeds.updateLength(ledStripCount);
                  }
                  strobeLeds.setBrightness(
                      map(ledStripBrightPct, 0, 100, 0, 255));
                  c6Prefs.putUChar("led_cnt", ledStripCount);
                  c6Prefs.putUChar("led_brt", ledStripBrightPct);
                  ledForceRefresh = true;
                  updateStrobeLeds(now);
                  printLog("[C6 ROVER] ★ Config Fita LEDs gravada: %u LEDs | "
                           "Brilho=%u%%\n",
                           ledStripCount, ledStripBrightPct);
                }
#endif
              } else if (strcmp(cmdStr, "CMD:ZERO_ANCHOR") == 0) {
                actuators.zeroAnchorCounter();
                printLog("[C6 ROVER] Contador da ancora ZERADO (0.0 cm / 0 "
                         "voltas)\n");
              } else if (strncmp(cmdStr, "CMD:ANC_CFG:", 12) == 0) {
                float d = 30.0f;
                int p = 1;
                if (sscanf(cmdStr + 12, "%f:%d", &d, &p) >= 1) {
                  actuators.setAnchorCalibration(d, (uint8_t)p);
                  c6Prefs.putFloat("anc_diam", d);
                  c6Prefs.putUChar("anc_pulses", (uint8_t)p);
                  printLog("[C6 ROVER] Calibracao da ancora gravada: "
                           "Diam=%.1fmm, Pulsos=%d\n",
                           d, p);
                }
              } else if (strcmp(cmdStr, "CMD:COMPASS_ZERO") == 0) {
                float newOff = compass.zeroCurrentHeading();
                c6Prefs.putFloat("cmp_off", newOff);
                sendCompassStatToMaster();
                printLog("[C6 ROVER] ★ Bussola BNO055 alinhada a Proa (0.0°) | "
                         "Offset=%.1f° gravado em NVS\n",
                         newOff);
              } else if (strncmp(cmdStr, "CMD:COMPASS_OFFSET:", 19) == 0) {
                float off = atof(cmdStr + 19);
                compass.setHeadingOffset(off);
                c6Prefs.putFloat("cmp_off", compass.getHeadingOffset());
                sendCompassStatToMaster();
                printLog("[C6 ROVER] ★ Offset da bussola BNO055 gravado em "
                         "NVS: %.1f°\n",
                         compass.getHeadingOffset());
              } else if (strncmp(cmdStr, "CMD:OTA:", 8) == 0) {
                // Formato esperado: CMD:OTA:<ssid>|<pass>|<url>
                char otaSsid[64] = "";
                char otaPass[64] = "";
                char otaUrl[160] = "";
                const char *p = cmdStr + 8;
                const char *sep1 = strchr(p, '|');
                if (sep1) {
                  size_t lenSsid = sep1 - p;
                  if (lenSsid > sizeof(otaSsid) - 1)
                    lenSsid = sizeof(otaSsid) - 1;
                  strncpy(otaSsid, p, lenSsid);
                  otaSsid[lenSsid] = '\0';

                  const char *p2 = sep1 + 1;
                  const char *sep2 = strchr(p2, '|');
                  if (sep2) {
                    size_t lenPass = sep2 - p2;
                    if (lenPass > sizeof(otaPass) - 1)
                      lenPass = sizeof(otaPass) - 1;
                    strncpy(otaPass, p2, lenPass);
                    otaPass[lenPass] = '\0';

                    strncpy(otaUrl, sep2 + 1, sizeof(otaUrl) - 1);
                    otaUrl[sizeof(otaUrl) - 1] = '\0';
                  }
                }
                printLog("[C6 ROVER] >>> Comando OTA recebido! SSID='%s', "
                         "URL='%s'\n",
                         otaSsid, otaUrl);
                if (strlen(otaSsid) > 0 && strlen(otaUrl) > 0) {
                  performHttpOta(otaSsid, otaPass, otaUrl);
                }
              } else if (strncmp(cmdStr, "CMD:GET_VER", 11) == 0 ||
                         strncmp(cmdStr, "CMD:VER", 7) == 0) {
                interSerial.println("PONG:VER:" ROVER_C6_VERSION);
                interSerial.flush();
              }
            }

            void loop() {
              uint32_t now = millis();

              // 1. Atualizar sensores e atuadores
              gps.update();
              compass.update();
              gps.setCompassData(compass.isReady(), compass.getHeading(),
                                 compass.getRoll(), compass.getPitch(),
                                 compass.getCalSys(), compass.getCalMag(),
                                 compass.getHeadingOffset());
              battery.update();
              actuators.update();
              updateStrobeLeds(now);

              // 2. Navegação autónoma (10 Hz)
              static uint32_t lastNavUpdate = 0;
              if (now - lastNavUpdate >= 100) {
                lastNavUpdate = now;
                updateAutonomousNavigation();
              }

              // 3. Receção LoRa SX1278 (Interrupção DIO0 ou pino HIGH)
              bool hasPacket = packetReceived;
              if (!hasPacket && digitalRead(PIN_C6_LORA_DIO0) == HIGH) {
                hasPacket = true;
              }

              if (loraReady && hasPacket) {
                packetReceived = false;
                uint8_t buffer[64];
                int len = radio.getPacketLength();
                if (len > 0 && len <= (int)sizeof(buffer)) {
                  int state = radio.readData(buffer, len);
                  if (state == RADIOLIB_ERR_NONE) {
                    processIncomingLoRa(buffer, len);
                  }
                }
                packetReceived = false;
                radio.startReceive();
              }

              // Atualizar 'now' após leitura LoRa para evitar underflow
              // unsigned (now < lastControlReceivedTime)
              now = millis();

              // 4. Failsafe Watchdog (Apenas corta se o motor principal estiver
              // ativo em modo manual e perder sinal)
              if (currentMotorStatus == MOTOR_MANUAL &&
                  actuators.getThrottle() != 0 &&
                  ((int32_t)(now - lastControlReceivedTime) >
                   (int32_t)FAILSAFE_TIMEOUT_MS)) {
                actuators.stopMotor();
                actuators.stopWinch();
                currentMotorStatus = MOTOR_FAILSAFE;
                setRgbColor(40, 10, 0);
              }

              // 5. Envio periódico de telemetria LoRa para a Base (a cada
              // 1000ms)
              if (now - lastTelemetryTime >= 1000) {
                lastTelemetryTime = now;
                sendLoRaTelemetry();
                printLog("[C6 ROVER] >>> LoRa Telemetria #%u enviada pelo ar "
                         "(433MHz) | "
                         "Bat=%.2fV (%u%%) | Total TX: %u | RX Polls: %u\n",
                         telemetrySeq, battery.getVoltageMv() / 1000.0f,
                         battery.getPercentage(), txTelemCount, rxPollCount);
              }

              // 6. Comunicação UART com o Lafvin Master
              while (interSerial.available()) {
                char c = (char)interSerial.read();
                static char rxBuf[256];
                static size_t rxIdx = 0;

                if (c == '\n' || c == '\r') {
                  if (rxIdx > 0) {
                    rxBuf[rxIdx] = '\0';

                    if (strncmp(rxBuf, "CMD:", 4) == 0 ||
                        strcmp(rxBuf, "BATT") == 0 ||
                        strcmp(rxBuf, "BATTERY") == 0) {
                      // Comando direto via UART recebido do Master Lafvin
                      lastControlReceivedTime = now;
                      printLog("[C6 ROVER] <<< %s\n", rxBuf);
                      handleIncomingCommand(rxBuf);
                    } else if (strncmp(rxBuf, "PING:", 5) == 0) {
                      uint32_t seq = 0;
                      uint32_t incomingId = 0;
                      char cmdStr[64] = "NONE";
                      long lat7 = 0, lng7 = 0;
                      float spd = 0.0f, head = 0.0f;
                      uint32_t sats = 0, fix = 0;
                      int parsed =
                          sscanf(rxBuf + 5, "%u:%u:%63[^:]:%ld:%ld:%f:%f:%u:%u",
                                 &seq, &incomingId, cmdStr, &lat7, &lng7, &spd,
                                 &head, &sats, &fix);
                      if (parsed >= 2 && incomingId > 0 &&
                          incomingId != roverId) {
                        roverId = (uint8_t)incomingId;
                        c6Prefs.putUChar("id", roverId);
                        printLog("[C6 ROVER] ★ ID sincronizado com Lafvin e "
                                 "gravado em "
                                 "NVS: %u\n",
                                 roverId);
                      }
                      if (parsed >= 9) {
                        gps.setValues((double)lat7 / 10000000.0,
                                      (double)lng7 / 10000000.0, spd, head,
                                      (uint8_t)sats, fix > 0);
                      }

                      // PING recebido do Lafvin Master: ligação local UART está
                      // 100% viva
                      lastControlReceivedTime = now;

                      // Se vier um comando embutido no PING:
                      if (strcmp(cmdStr, "NONE") != 0) {
                        handleIncomingCommand(cmdStr);
                      }

                      // Resposta rica de telemetria para o Lafvin Master:
                      // PONG:<seq>:<batMv>:<batA>:<batPct>:<lat7>:<lng7>:<spd>:<head>:<sats>:<fix>:<motSt>:<rud>:<thr>:<winch>:<rack>:<depth>:<loraSt>:<rssi>:<snr>:<ancSt>
                      char pong[256];
                      snprintf(pong, sizeof(pong),
                               "PONG:%u:%u:%.2f:%u:%ld:%ld:%.1f:%.1f:%u:%u:%u:%"
                               "d:%d:%d:%d:%"
                               "u:%s:%.1f:%.1f:%u\n",
                               seq, battery.getVoltageMv(),
                               battery.getCurrentAmps(),
                               battery.getPercentage(), (long)gps.getLatDeg7(),
                               (long)gps.getLngDeg7(), gps.getSpeedKnots(),
                               gps.getHeading(), gps.getSatellites(),
                               gps.getFixQuality(), currentMotorStatus,
                               actuators.getRudder(), actuators.getThrottle(),
                               actuators.getWinchPulseUs(),
                               actuators.isRackReleased() ? 1 : 0,
                               actuators.getAnchorDepthCm(),
                               loraReady ? "RF_OK" : "RF_ERR", lastRssi,
                               lastSnr, actuators.getAnchorStatus());
                      interSerial.print(pong);
                      sendLoraStatToMaster();
                      sendCompassStatToMaster();
                      printLog("[C6 ROVER] <<< %s | >>> PONG: Bat=%.2fV / "
                               "%.2fA (%d%%) "
                               "LoRa=%s | Head=%.1f° (%s) | Winch=%dus | "
                               "Depth=%ucm\n",
                               rxBuf, battery.getVoltageMv() / 1000.0f,
                               battery.getCurrentAmps(),
                               battery.getPercentage(),
                               loraReady ? "RF_OK" : "RF_ERR", gps.getHeading(),
                               compass.isReady() ? "BNO055" : "GPS_COG",
                               actuators.getWinchPulseUs(),
                               actuators.getAnchorDepthCm());
                    }
                    rxIdx = 0;
                  }
                } else {
                  if (rxIdx < sizeof(rxBuf) - 1)
                    rxBuf[rxIdx++] = c;
                }
              }

              // 7. Consola de comandos Serial (ex: SET_ID 2, WINCH 1800, ALARM
              // 10, LIMIT, ZERO, ANC_CFG)
              Stream *cmdStream = nullptr;
              if (Serial.available())
                cmdStream = &Serial;
              else if (Serial0.available())
                cmdStream = &Serial0;

              if (cmdStream != nullptr) {
                String cmd = cmdStream->readStringUntil('\n');
                cmd.trim();
                if (cmd.startsWith("SET_ID ") || cmd.startsWith("ID ")) {
                  int newId = cmd.substring(cmd.indexOf(' ') + 1).toInt();
                  if (newId >= 1 && newId <= 254) {
                    roverId = (uint8_t)newId;
                    c6Prefs.putUChar("id", roverId);
                    printLog("[C6] ★ ID guardado em NVS: %d\n", newId);
                  }
                } else if (cmd.startsWith("WINCH ")) {
                  int pulse = cmd.substring(6).toInt();
                  if (pulse <= 0) {
                    actuators.stopWinch();
                    printLog("[C6] Guincho parado (Sinal 0V no GPIO %d)\n",
                             PIN_C6_WINCH);
                  } else {
                    actuators.setWinchMicroseconds(pulse);
                    printLog("[C6] Guincho acionado: %d us no GPIO %d\n", pulse,
                             PIN_C6_WINCH);
                  }
                } else if (cmd == "WINCH_STOP" || cmd == "STOP") {
                  actuators.stopWinch();
                  printLog("[C6] Guincho parado (0V)\n");
                } else if (cmd == "RACK RELEASE") {
                  actuators.releaseRack();
                  printLog(
                      "[C6] Trinco da cremalheira SOLTO (%u graus, inv=%d)\n",
                      actuators.getRackAngleReleased(),
                      actuators.isRackInverted() ? 1 : 0);
                } else if (cmd == "RACK ENGAGE") {
                  actuators.engageRack();
                  printLog(
                      "[C6] Trinco da cremalheira TRAVADO (%u graus, inv=%d)\n",
                      actuators.getRackAngleEngaged(),
                      actuators.isRackInverted() ? 1 : 0);
                } else if (cmd == "RACK RELAX") {
                  actuators.relaxRack();
                  printLog("[C6] Trinco relaxado\n");
                } else if (cmd.startsWith("RACK_CFG ")) {
                  int eng = RACK_POS_ENGAGED, rel = RACK_POS_RELEASED,
                      inv = RACK_SERVO_INVERTED_DEFAULT ? 1 : 0;
                  if (sscanf(cmd.c_str() + 9, "%d %d %d", &eng, &rel, &inv) >=
                      2) {
                    eng = constrain(eng, 0, 180);
                    rel = constrain(rel, 0, 180);
                    bool invBool = (inv != 0);
                    actuators.setRackConfig((uint8_t)eng, (uint8_t)rel, invBool,
                                            true);
                    c6Prefs.putUChar("rck_eng", (uint8_t)eng);
                    c6Prefs.putUChar("rck_rel", (uint8_t)rel);
                    c6Prefs.putBool("rck_inv", invBool);
                    printLog(
                        "[C6] Calibracao Cremalheira: Encosto=%d°, Livre=%d°, "
                        "Invertido=%d\n",
                        eng, rel, invBool ? 1 : 0);
                  }
                } else if (cmd.startsWith("RACK ")) {
                  int ang = cmd.substring(5).toInt();
                  actuators.setRackAngle((uint8_t)constrain(ang, 0, 180));
                  printLog("[C6] Trinco da cremalheira movido para %d graus\n",
                           constrain(ang, 0, 180));
                } else if (cmd.startsWith("ALARM ") || cmd == "ALARM") {
                  int dur = 10;
                  if (cmd.startsWith("ALARM "))
                    dur = cmd.substring(6).toInt();
                  if (dur > 0) {
                    actuators.triggerAlarm((uint16_t)dur);
                    printLog("[C6] Alarme/Strobe WS2812B ativado por %d seg\n",
                             dur);
                  } else {
                    actuators.stopAlarm();
#if defined(PIN_STROBE_LEDS) && (PIN_STROBE_LEDS >= 0)
                    ledTestMode = LED_MODE_OFF;
                    ledForceRefresh = true;
#endif
                    printLog("[C6] Alarme/Strobe WS2812B desligado\n");
                  }
#if defined(PIN_STROBE_LEDS) && (PIN_STROBE_LEDS >= 0)
                } else if (cmd.startsWith("LED ")) {
                  String sub = cmd.substring(4);
                  sub.trim();
                  if (sub.equalsIgnoreCase("OFF") || sub == "0") {
                    handleIncomingCommand("CMD:LED:OFF");
                  } else if (sub.equalsIgnoreCase("STROBE") ||
                             sub.equalsIgnoreCase("RAINBOW") ||
                             sub.equalsIgnoreCase("CHASE") ||
                             sub.equalsIgnoreCase("NAV") ||
                             sub.equalsIgnoreCase("BEACON") ||
                             sub.equalsIgnoreCase("POLICE")) {
                    String c = "CMD:LED:MODE:" + sub;
                    handleIncomingCommand(c.c_str());
                  } else {
                    int r = 255, g = 255, b = 255;
                    if (sscanf(sub.c_str(), "%d %d %d", &r, &g, &b) == 3) {
                      char c[48];
                      snprintf(c, sizeof(c), "CMD:LED:SOLID:%d:%d:%d", r, g, b);
                      handleIncomingCommand(c);
                    }
                  }
#endif
                } else if (cmd == "LIMIT" || cmd == "ANCHOR") {
                  printLog("[C6] Fim de curso da ancora (GPIO %d): %s (Nivel "
                           "digital: %d)\n",
                           PIN_ANCHOR_DETECTOR,
                           actuators.isAnchorAtTopLimit() ? "ACIONADO (NO TOPO)"
                                                          : "LIVRE",
                           digitalRead(PIN_ANCHOR_DETECTOR));
                  printLog("[C6] Sensor Hall DRV5032 (GPIO %d): Nivel=%d | "
                           "Pulsos=%ld | "
                           "Voltas=%.2f | Profundidade=%u cm (Tambor=%.1fmm)\n",
                           PIN_ANCHOR_HALL, digitalRead(PIN_ANCHOR_HALL),
                           actuators.getAnchorHallPulses(),
                           actuators.getAnchorRevolutions(),
                           actuators.getAnchorDepthCm(),
                           actuators.getAnchorDrumDiamMm());
                } else if (cmd == "ZERO_ANCHOR" || cmd == "ZERO") {
                  actuators.zeroAnchorCounter();
                  printLog("[C6] Contador de cabo da ancora ZERADO (0.0 cm)\n");
                } else if (cmd.startsWith("ANC_CFG ")) {
                  float d = 30.0f;
                  int p = 1;
                  if (sscanf(cmd.c_str() + 8, "%f %d", &d, &p) >= 1) {
                    actuators.setAnchorCalibration(d, (uint8_t)p);
                    c6Prefs.putFloat("anc_diam", d);
                    c6Prefs.putUChar("anc_pulses", (uint8_t)p);
                    printLog("[C6] Calibracao da ancora definida: Diam=%.1fmm, "
                             "Pulsos=%d\n",
                             d, p);
                  }
                } else if (cmd == "BATT" || cmd == "BATTERY") {
                  battery.update();
                  printLog(
                      "[C6] Bateria (%uS): %.2f V (%u%%) | Corrente: %.2f A | "
                      "ADC Bruto: VOLT(GPIO%d/J_BATT-3)=%.1f mV, "
                      "CURR(GPIO%d/J_BATT-2)=%.1f mV\n",
                      battery.getCellCount(), battery.getVoltageMv() / 1000.0f,
                      battery.getPercentage(), battery.getCurrentAmps(),
                      PIN_C6_ADC_VOLT, battery.getRawVoltAdcMv(),
                      PIN_C6_ADC_CURR, battery.getRawCurrAdcMv());
                } else if (cmd == "COMPASS" || cmd == "BNO055") {
                  printLog("[C6] Bussola GY-BNO055 (J_EXP SDA=GPIO%d, "
                           "SCL=GPIO%d): %s\n",
                           PIN_C6_EXP_SDA, PIN_C6_EXP_SCL,
                           compass.isReady() ? "ONLINE (Modo NDOF 9-Eixos)"
                                             : "OFFLINE (Fallback GPS COG)");
                  if (compass.isReady()) {
                    printLog("[C6]   I2C=0x%02X | Proa=%.1f° (Bruto=%.1f°, "
                             "Offset=%.1f°) | "
                             "Roll=%.1f° Pitch=%.1f° | Cal[Sys=%u Gyro=%u "
                             "Acc=%u Mag=%u]\n",
                             compass.getI2cAddress(), compass.getHeading(),
                             compass.getRawHeading(),
                             compass.getHeadingOffset(), compass.getRoll(),
                             compass.getPitch(), compass.getCalSys(),
                             compass.getCalGyro(), compass.getCalAccel(),
                             compass.getCalMag());
                  }
                } else if (cmd == "COMPASS_ZERO") {
                  float newOff = compass.zeroCurrentHeading();
                  c6Prefs.putFloat("cmp_off", newOff);
                  printLog("[C6] ★ Bussola alinhada a Proa (0.0°) | "
                           "Offset=%.1f° guardado "
                           "em NVS\n",
                           newOff);
                } else if (cmd.startsWith("COMPASS_OFFSET ")) {
                  float off = cmd.substring(15).toFloat();
                  compass.setHeadingOffset(off);
                  c6Prefs.putFloat("cmp_off", compass.getHeadingOffset());
                  printLog("[C6] ★ Offset da bussola definido para %.1f° e "
                           "guardado em NVS\n",
                           compass.getHeadingOffset());
                }
              }
            }
