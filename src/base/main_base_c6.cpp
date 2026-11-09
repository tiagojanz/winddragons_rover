#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <Preferences.h>
#include <RadioLib.h>
#include <SPI.h>
#include <WiFi.h>

#include "base_version.h"
#include "config_common.h"
#include "lora_protocol.h"

// =========================================================================
// WINDDRAGONS BASE STATION - C6 COPROCESSOR (LORA 433MHz + INTER-ESP UART)
// =========================================================================

// Instância de comunicação Inter-ESP UART com o Master Lafvin
HardwareSerial interSerial(1);

// Hardware SPI dedicado para LoRa SX1278 no ESP32-C6
SPIClass loraSpiBus(FSPI);

SX1278 radio = new Module(PIN_C6_LORA_NSS, PIN_C6_LORA_DIO0, PIN_C6_LORA_RST,
                          RADIOLIB_NC, loraSpiBus);
bool loraReady = false;

// Flag de interrupção DIO0
volatile bool packetReceived = false;

#if defined(ESP8266) || defined(ESP32)
ICACHE_RAM_ATTR
#endif
void onDio0Rx() { packetReceived = true; }

// Estatísticas e Estado de Comunicação LoRa (Base Station)
uint32_t txPollCount = 0;
uint32_t rxTelemCount = 0;
uint32_t lastRxTelemMs = 0;
float lastRxRssi = 0.0f;
float lastRxSnr = 0.0f;
uint16_t pollSeq = 0;
uint32_t lastAutoPollMs = 0;
uint8_t selectedRoverTarget = BROADCAST_ROVER_ID; // 0xFF por defeito
bool autoPollingEnabled = true;

// LED RGB NeoPixel onboard
Adafruit_NeoPixel rgbLed(1, PIN_RGB_LED, NEO_GRB + NEO_KHZ800);

Preferences basePrefs;
uint8_t baseStationId = 1;

// Configuração do LED RGB do Coprocessador C6 (Estação Base)
//   0: BASE_OP_STORAGE        -> #A855F7 (Roxo)
//   1: BASE_OP_WAITING_ROVERS -> #F59E0B (Âmbar)
//   2: BASE_OP_IDLE           -> #06B6D4 (Ciano)
//   3: BASE_OP_GO             -> #10B981 (Verde)
//   4: RF_ERROR               -> #EF4444 (Vermelho 2 Hz)
static constexpr uint8_t C6_LED_MODE_COUNT = 5;
uint8_t c6LedBrightPct = 80; // 0..100%
uint32_t c6ModeColors[C6_LED_MODE_COUNT] = {
    0xA855F7, // 0: Storage (Roxo)
    0xF59E0B, // 1: Waiting Rovers (Âmbar)
    0x06B6D4, // 2: Idle (Ciano)
    0x10B981, // 3: Go / In Mission (Verde)
    0xEF4444  // 4: Erro RF / LoRa Offline (Vermelho)
};
uint8_t currentBaseOpState =
    0; // 0..3 (recebido do Master via PING ou CMD:OP_STATE)
uint32_t c6PreviewColor = 0;
uint32_t c6PreviewUntilMs = 0;
bool c6LedForceRefresh = true;

void printLog(const char *format, ...);

void setRgbColor(uint8_t r, uint8_t g, uint8_t b) {
  if (r == 0 && g == 0 && b == 0) {
    rgbLed.clear();
  } else {
    rgbLed.setPixelColor(0, rgbLed.Color(r, g, b));
  }
  rgbLed.show();
}

void setRgbHexScaled(uint32_t hexColor, uint8_t pct) {
  if (pct == 0 || hexColor == 0) {
    setRgbColor(0, 0, 0);
    return;
  }
  if (pct > 100)
    pct = 100;
  uint8_t r = (uint8_t)((((hexColor >> 16) & 0xFF) * (uint32_t)pct) / 100);
  uint8_t g = (uint8_t)((((hexColor >> 8) & 0xFF) * (uint32_t)pct) / 100);
  uint8_t b = (uint8_t)(((hexColor & 0xFF) * (uint32_t)pct) / 100);
  setRgbColor(r, g, b);
}

bool parseHex24(const char *s, uint32_t &outVal) {
  if (!s)
    return false;
  while (*s == ' ' || *s == '#')
    s++;
  if (strlen(s) < 6)
    return false;
  char buf[7];
  for (int i = 0; i < 6; i++) {
    char c = s[i];
    bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F');
    if (!ok)
      return false;
    buf[i] = c;
  }
  buf[6] = '\0';
  outVal = (uint32_t)strtoul(buf, nullptr, 16);
  return true;
}

void updateC6StatusLed(uint32_t now) {
  static uint32_t lastLedTickMs = 0;
  static bool blinkPhase = false;
  static uint32_t lastAppliedColor = 0xFFFFFFFF;
  static uint8_t lastAppliedPct = 255;

  if (!c6LedForceRefresh && (now - lastLedTickMs < 120))
    return;
  if (now - lastLedTickMs >= 250) {
    blinkPhase = !blinkPhase;
    lastLedTickMs = now;
  }

  uint32_t activeHex = 0;
  uint8_t activePct = (c6LedBrightPct > 100) ? 100 : c6LedBrightPct;

  if (activePct == 0) {
    activeHex = 0;
  } else if (c6PreviewUntilMs > now) {
    activeHex = c6PreviewColor;
  } else if (!loraReady) {
    // Estado 4: Falha LoRa / Erro RF (Pisca a 2 Hz)
    activeHex = blinkPhase ? c6ModeColors[4] : 0x000000;
  } else {
    uint8_t idx = (currentBaseOpState <= 3) ? currentBaseOpState : 0;
    activeHex = c6ModeColors[idx];
  }

  if (c6LedForceRefresh || activeHex != lastAppliedColor ||
      activePct != lastAppliedPct) {
    c6LedForceRefresh = false;
    lastAppliedColor = activeHex;
    lastAppliedPct = activePct;
    setRgbHexScaled(activeHex, activePct);
  }
}

void sendLoraStatToMaster() {
  uint32_t agoSec =
      (lastRxTelemMs > 0) ? (millis() - lastRxTelemMs) / 1000 : 999999;
  char statLine[128];
  // Formato:
  // LORA_STAT:<status>:<txPolls>:<rxTelems>:<lastRxSecAgo>:<rssi>:<snr>:<isPolling>
  snprintf(statLine, sizeof(statLine), "LORA_STAT:%s:%u:%u:%lu:%.1f:%.1f:%d\n",
           loraReady ? "OK" : "ERR", txPollCount, rxTelemCount,
           (unsigned long)agoSec, lastRxRssi, lastRxSnr,
           autoPollingEnabled ? 1 : 0);
  interSerial.print(statLine);
}

void sendLoRaPoll(uint8_t targetId = BROADCAST_ROVER_ID) {
  if (!loraReady)
    return;
  PacketPollRequest pollPkt;
  memset(&pollPkt, 0, sizeof(pollPkt));
  pollPkt.magic = LORA_MAGIC_BYTE;
  pollPkt.msg_type = MSG_POLL_REQUEST;
  pollPkt.target_rover_id = targetId;
  pollPkt.poll_seq = ++pollSeq;
  pollPkt.checksum =
      calculate_checksum(reinterpret_cast<const uint8_t *>(&pollPkt),
                         sizeof(pollPkt) - sizeof(pollPkt.checksum));

  radio.standby();
  radio.transmit(reinterpret_cast<uint8_t *>(&pollPkt), sizeof(pollPkt));
  packetReceived = false;
  radio.startReceive();

  txPollCount++;
  printLog("[BASE C6] >>> LoRa Poll #%u enviado (Alvo: %u, 433MHz)\n",
           pollPkt.poll_seq, targetId);
  sendLoraStatToMaster();
}

void printLog(const char *format, ...) {
  char buf[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);
  Serial.print(buf);
  Serial0.print(buf);
}

void performHttpOta(const char *ssid, const char *pass, const char *url) {
  printLog("\n[OTA] ==========================================\n");
  printLog("[OTA] Inicio de Atualizacao OTA no Coprocessador Base C6!\n");
  printLog("[OTA] Versao Atual: %s\n", BASE_C6_VERSION);
  printLog("[OTA] Rede WiFi    : %s\n", ssid);
  printLog("[OTA] URL Alvo     : %s\n", url);
  printLog("[OTA] ==========================================\n");

  // PASSO 1: Preparação
  // Cor: ROXO / MAGENTA (40, 0, 40)
  setRgbColor(40, 0, 40);

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
    printLog("\n[OTA] ERRO: Falha ao conectar a rede WiFi '%s'\n", ssid);
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

  // PASSO 3: Download via HTTP
  setRgbColor(0, 45, 50); // Ciano
  interSerial.println("PONG:OTA:DOWNLOADING");
  interSerial.flush();

  WiFiClient client;
  client.setTimeout(25000);
  httpUpdate.rebootOnUpdate(false);

  httpUpdate.onStart([&]() {
    printLog("[OTA] Download e gravacao da Flash iniciados!\n");
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
      setRgbColor(0, blink ? 0 : 30, 60);
      char progMsg[32];
      snprintf(progMsg, sizeof(progMsg), "PONG:OTA:PROGRESS:%d", pct);
      interSerial.println(progMsg);
      interSerial.flush();
      printLog("[OTA] Progresso Flash: %d%% (%d / %d bytes)\n", pct, current,
               total);
    }
  });

  httpUpdate.onEnd([&]() {
    printLog("[OTA] Gravacao da Flash terminada com integridade verificada!\n");
    setRgbColor(0, 70, 0); // Verde
    interSerial.println("PONG:OTA:PROGRESS:100");
    interSerial.flush();
  });

  httpUpdate.onError([&](int err) {
    printLog("[OTA] Erro durante atualizacao HTTP: %d\n", err);
    setRgbColor(70, 0, 0); // Vermelho
  });

  t_httpUpdate_return ret = httpUpdate.update(client, url);

  if (ret == HTTP_UPDATE_OK) {
    printLog("[OTA] ★★★ SUCESSO! Novo firmware gravado na Flash! ★★★\n");
    setRgbColor(0, 80, 0);
    interSerial.println("PONG:OTA:SUCCESS");
    interSerial.flush();
    delay(1200);
    ESP.restart();
  } else {
    int errCode = httpUpdate.getLastError();
    String errStr = httpUpdate.getLastErrorString();
    printLog("[OTA] Falha na gravacao OTA: (%d) %s\n", errCode, errStr.c_str());
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

void handleIncomingCommand(const char *cmdStr) {
  if (!cmdStr || strlen(cmdStr) == 0 || strcmp(cmdStr, "NONE") == 0)
    return;

  if (strncmp(cmdStr, "CMD:ACTION:", 11) == 0) {
    // CMD:ACTION:<target_id>:<action>:<cmd_id>:<depth>:<dur>:<lat7>:<lng7>
    uint32_t targetId = 0, action = 0, cmdId = 0, depth = 0, dur = 0;
    long lat7 = 0, lng7 = 0;
    if (sscanf(cmdStr + 11, "%u:%u:%u:%u:%u:%ld:%ld", &targetId, &action,
               &cmdId, &depth, &dur, &lat7, &lng7) >= 3) {
      if (targetId == 0) {
        targetId = (selectedRoverTarget != BROADCAST_ROVER_ID &&
                    selectedRoverTarget > 0)
                       ? selectedRoverTarget
                       : 1;
      }
      if (loraReady) {
        PacketActionCommand packet;
        memset(&packet, 0, sizeof(packet));
        packet.magic = LORA_MAGIC_BYTE;
        packet.msg_type = MSG_ACTION_COMMAND;
        packet.target_rover_id = (uint8_t)targetId;
        packet.action = (uint8_t)action;
        packet.command_id = (uint16_t)cmdId;
        packet.target_depth_cm = (uint16_t)depth;
        packet.duration_s = (uint16_t)dur;
        packet.target_lat_deg7 = lat7;
        packet.target_lng_deg7 = lng7;
        packet.checksum =
            calculate_checksum(reinterpret_cast<const uint8_t *>(&packet),
                               sizeof(packet) - sizeof(packet.checksum));

        if (targetId != BROADCAST_ROVER_ID) {
          selectedRoverTarget = (uint8_t)targetId;
        }
        lastAutoPollMs =
            millis(); // Evita colisão de Poll logo após enviar comando de ação
        radio.standby();
        radio.transmit(reinterpret_cast<uint8_t *>(&packet), sizeof(packet));
        delay(25);
        // Retransmissão rápida para garantir entrega em canal half-duplex
        radio.transmit(reinterpret_cast<uint8_t *>(&packet), sizeof(packet));
        packetReceived = false;
        radio.startReceive();
        printLog("[BASE C6] >>> LoRa Acao enviada (2x): Rover #%u | Acao=%u "
                 "(%s) | CmdID=%u | Depth=%u\n",
                 targetId, action, get_action_str((uint8_t)action), cmdId,
                 depth);
      }
    }
  } else if (strncmp(cmdStr, "CMD:CTRL:", 9) == 0) {
    // CMD:CTRL:<target_id>:<thr>:<rud>:<jog>:<mode>
    int targetId = 0, thr = 0, rud = 0, jog = 0, mode = 0;
    if (sscanf(cmdStr + 9, "%d:%d:%d:%d:%d", &targetId, &thr, &rud, &jog,
               &mode) >= 3) {
      if (targetId <= 0) {
        targetId = (selectedRoverTarget != BROADCAST_ROVER_ID &&
                    selectedRoverTarget > 0)
                       ? selectedRoverTarget
                       : 1;
      }
      if (loraReady) {
        PacketManualControl packet;
        memset(&packet, 0, sizeof(packet));
        packet.magic = LORA_MAGIC_BYTE;
        packet.msg_type = MSG_MANUAL_CONTROL;
        packet.target_rover_id = (uint8_t)targetId;
        packet.throttle = (int8_t)thr;
        packet.rudder = (int8_t)rud;
        packet.anchor_jog = (int8_t)jog;
        packet.mode_request = (uint8_t)mode;
        packet.checksum =
            calculate_checksum(reinterpret_cast<const uint8_t *>(&packet),
                               sizeof(packet) - sizeof(packet.checksum));

        selectedRoverTarget = (uint8_t)targetId;
        radio.standby();
        radio.transmit(reinterpret_cast<uint8_t *>(&packet), sizeof(packet));
        packetReceived = false;
        radio.startReceive();
        printLog("[BASE C6] >>> LoRa CTRL -> Rover #%d: Thr=%d%%, Rud=%d%%, "
                 "Jog=%d\n",
                 targetId, thr, rud, jog);
      }
    }
  } else if (strncmp(cmdStr, "CMD:POLL", 8) == 0) {
    // CMD:POLL ou CMD:POLL:<id>
    uint32_t tId = selectedRoverTarget;
    if (strlen(cmdStr) > 9) {
      sscanf(cmdStr + 9, "%u", &tId);
    }
    sendLoRaPoll((uint8_t)tId);
  } else if (strncmp(cmdStr, "CMD:GET_LORA", 12) == 0) {
    sendLoraStatToMaster();
  } else if (strncmp(cmdStr, "CMD:OTA:", 8) == 0) {
    // CMD:OTA:<ssid>|<pass>|<url>
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
    printLog("[BASE C6] >>> Comando OTA recebido! SSID='%s', URL='%s'\n",
             otaSsid, otaUrl);
    if (strlen(otaSsid) > 0 && strlen(otaUrl) > 0) {
      performHttpOta(otaSsid, otaPass, otaUrl);
    }
  } else if (strncmp(cmdStr, "CMD:GET_VER", 11) == 0 ||
             strncmp(cmdStr, "CMD:VER", 7) == 0) {
    interSerial.println("PONG:VER:" BASE_C6_VERSION);
    interSerial.flush();
  } else if (strncmp(cmdStr, "CMD:OP_STATE:", 13) == 0) {
    unsigned int st = 0;
    if (sscanf(cmdStr + 13, "%u", &st) == 1 && st <= 3) {
      if (currentBaseOpState != (uint8_t)st) {
        currentBaseOpState = (uint8_t)st;
        c6LedForceRefresh = true;
        printLog("[BASE C6] Estado de Operacao atualizado para: %u\n",
                 currentBaseOpState);
      }
    }
  } else if (strncmp(cmdStr, "CMD:C6LED:BRIGHT:", 17) == 0) {
    int val = atoi(cmdStr + 17);
    if (val < 0)
      val = 0;
    if (val > 100)
      val = 100;
    c6LedBrightPct = (uint8_t)val;
    basePrefs.putUChar("c6l_brt", c6LedBrightPct);
    c6LedForceRefresh = true;
    updateC6StatusLed(millis());
    printLog("[C6 LED] Intensidade ajustada para %u%%\n", c6LedBrightPct);
  } else if (strncmp(cmdStr, "CMD:C6LED:MODE:", 15) == 0) {
    // CMD:C6LED:MODE:<idx>:<hex>
    int idx = -1;
    const char *p = cmdStr + 15;
    idx = atoi(p);
    const char *sep = strchr(p, ':');
    if (sep && idx >= 0 && idx < C6_LED_MODE_COUNT) {
      uint32_t col = 0;
      if (parseHex24(sep + 1, col)) {
        c6ModeColors[idx] = col;
        char key[12];
        snprintf(key, sizeof(key), "c6l_m%d", idx);
        basePrefs.putUInt(key, col);
        c6PreviewColor = col;
        c6PreviewUntilMs = millis() + 2000;
        c6LedForceRefresh = true;
        updateC6StatusLed(millis());
        printLog("[C6 LED] Modo %d -> #%06X\n", idx, (unsigned)col);
      }
    }
  } else if (strncmp(cmdStr, "CMD:C6LED:TEST:", 15) == 0) {
    uint32_t col = 0;
    if (parseHex24(cmdStr + 15, col)) {
      c6PreviewColor = col;
      c6PreviewUntilMs = millis() + 3000;
      c6LedForceRefresh = true;
      updateC6StatusLed(millis());
      printLog("[C6 LED] Teste de cor -> #%06X (3s)\n", (unsigned)col);
    }
  } else if (strncmp(cmdStr, "CMD:C6LED:CFG:", 14) == 0) {
    // CMD:C6LED:CFG:<brt>:<c0>:<c1>:<c2>:<c3>:<c4>
    const char *p = cmdStr + 14;
    int brt = atoi(p);
    if (brt < 0)
      brt = 0;
    if (brt > 100)
      brt = 100;
    c6LedBrightPct = (uint8_t)brt;
    basePrefs.putUChar("c6l_brt", c6LedBrightPct);
    const char *tok = strchr(p, ':');
    for (int i = 0; i < C6_LED_MODE_COUNT && tok; i++) {
      tok++; // avança ':'
      uint32_t col = 0;
      if (parseHex24(tok, col)) {
        c6ModeColors[i] = col;
        char key[12];
        snprintf(key, sizeof(key), "c6l_m%d", i);
        basePrefs.putUInt(key, col);
      }
      tok = strchr(tok, ':');
    }
    c6LedForceRefresh = true;
    updateC6StatusLed(millis());
    printLog("[C6 LED] Config completa sincronizada (Brilho=%u%%)\n",
             c6LedBrightPct);
  }
}

void setup() {
  Serial.begin(115200);
  Serial0.begin(115200);
  Serial.setTxTimeoutMs(0);
  delay(300);

  basePrefs.begin("base_c6", false);
  baseStationId = basePrefs.getUChar("id", 1);
  c6LedBrightPct = basePrefs.getUChar("c6l_brt", 80);
  if (c6LedBrightPct > 100)
    c6LedBrightPct = 100;
  for (int i = 0; i < C6_LED_MODE_COUNT; i++) {
    char key[12];
    snprintf(key, sizeof(key), "c6l_m%d", i);
    c6ModeColors[i] = basePrefs.getUInt(key, c6ModeColors[i]);
  }

  printLog("\n=======================================================\n");
  printLog("  WINDDRAGONS BASE STATION - C6 COPROCESSOR (LORA 433MHz)\n");
  printLog("  Firmware: Base C6 %s (%s)\n", BASE_C6_VERSION, BASE_BUILD_DATE);
  printLog("=======================================================\n");

  // 1. LED RGB NeoPixel
  rgbLed.begin();
  setRgbHexScaled(0xF59E0B, c6LedBrightPct); // Amarelo: arranque

  // 2. Inter-ESP UART
  interSerial.setRxBufferSize(512);
  interSerial.begin(INTER_ESP_BAUDRATE, SERIAL_8N1, PIN_INTER_ESP_C6_RX,
                    PIN_INTER_ESP_C6_TX);
  printLog("[UART] Inter-ESP ligada a %d baud (RX=GPIO%d, TX=GPIO%d)\n",
           INTER_ESP_BAUDRATE, PIN_INTER_ESP_C6_RX, PIN_INTER_ESP_C6_TX);

  // 3. LoRa SX1278 (Hardware FSPI)
  pinMode(PIN_C6_LORA_NSS, OUTPUT);
  digitalWrite(PIN_C6_LORA_NSS, HIGH);
  pinMode(PIN_C6_LORA_DIO0, INPUT);

  loraSpiBus.begin(PIN_C6_LORA_SCK, PIN_C6_LORA_MISO, PIN_C6_LORA_MOSI,
                   PIN_C6_LORA_NSS);
  delay(50);

  printLog("[LORA] A inicializar RadioLib SX1278 (433.0 MHz)...\n");
  int state = radio.begin(LORA_FREQUENCY, LORA_BANDWIDTH, LORA_SPREAD_FACTOR,
                          LORA_CODING_RATE, LORA_SYNC_WORD, LORA_OUTPUT_POWER,
                          LORA_PREAMBLE_LEN, 0);
  if (state == RADIOLIB_ERR_NONE) {
    loraReady = true;
    radio.setPacketReceivedAction(onDio0Rx);
    packetReceived = false;
    radio.startReceive();
    printLog("[LORA] ★★★ SX1278 BASE PRONTO E EM ESCUTA RX! ★★★\n");
    c6LedForceRefresh = true;
    updateC6StatusLed(millis());
  } else {
    loraReady = false;
    printLog("[LORA] Falha na inicialização SX1278 (código: %d)\n", state);
    c6LedForceRefresh = true;
    updateC6StatusLed(millis());
  }
}

void loop() {
  uint32_t now = millis();
  updateC6StatusLed(now);

  // 1. Receção de pacotes LoRa 433 MHz dos Rovers (Interrupção DIO0 ou pino
  // HIGH)
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
        if ((len == sizeof(PacketTelemetry) ||
             len == LORA_LEGACY_TELEMETRY_SIZE) &&
            buffer[0] == LORA_MAGIC_BYTE && buffer[1] == MSG_TELEMETRY) {
          PacketTelemetry pktVal;
          memset(&pktVal, 0, sizeof(pktVal));
          uint16_t rxChecksum = 0;
          uint16_t calc = 0;
          if (len == sizeof(PacketTelemetry)) {
            memcpy(&pktVal, buffer, sizeof(PacketTelemetry));
            rxChecksum = pktVal.checksum;
            calc = calculate_checksum(buffer, sizeof(PacketTelemetry) -
                                                  sizeof(pktVal.checksum));
          } else {
            // Pacote legado de 28 bytes (sem campos BNO055)
            memcpy(&pktVal, buffer,
                   LORA_LEGACY_TELEMETRY_SIZE - sizeof(uint16_t));
            memcpy(&rxChecksum,
                   buffer + (LORA_LEGACY_TELEMETRY_SIZE - sizeof(uint16_t)),
                   sizeof(uint16_t));
            pktVal.checksum = rxChecksum;
            calc = calculate_checksum(buffer, LORA_LEGACY_TELEMETRY_SIZE -
                                                  sizeof(uint16_t));
          }
          PacketTelemetry *pkt = &pktVal;
          if (pkt->checksum == 0 || calc == pkt->checksum) {
            float rssi = radio.getRSSI();
            float snr = radio.getSNR();

            rxTelemCount++;
            lastRxTelemMs = now;
            lastRxRssi = rssi;
            lastRxSnr = snr;
            if (pkt->rover_id > 0 && pkt->rover_id != BROADCAST_ROVER_ID) {
              selectedRoverTarget = pkt->rover_id;
            }

            // Transmitir telemetria do Rover para o Lafvin Master via UART:
            // TELEM:<id>:<voltMv>:<pct>:<lat7>:<lng7>:<spdCmS>:<head10>:<sats>:<fix>:<motSt>:<ancSt>:<depthCm>:<cmdId>:<rssi>:<snr>:<roll10>:<pitch10>:<cmpFlags>
            char telemLine[220];
            snprintf(telemLine, sizeof(telemLine),
                     "TELEM:%u:%u:%u:%ld:%ld:%u:%u:%u:%u:%u:%u:%u:%u:%.1f:%.1f:"
                     "%d:%d:%u\n",
                     pkt->rover_id, pkt->battery_mv, pkt->battery_pct,
                     (long)pkt->lat_deg7, (long)pkt->lng_deg7, pkt->speed_cm_s,
                     pkt->heading_deg10, pkt->satellites, pkt->fix_quality,
                     pkt->motor_status, pkt->anchor_status,
                     pkt->anchor_depth_cm, pkt->last_cmd_id, rssi, snr,
                     (int)pkt->roll_deg10, (int)pkt->pitch_deg10,
                     (unsigned)pkt->compass_flags);
            interSerial.print(telemLine);
            printLog("[BASE C6] <<< RF Telemetria Rover #%u (Proa: %.1f° [%s] "
                     "Roll:%.1f° Pitch:%.1f° | RSSI: %.1f dBm, SNR: %.1f dB) | "
                     "Total RX: %u\n",
                     pkt->rover_id, pkt->heading_deg10 / 10.0f,
                     unpack_compass_active(pkt->compass_flags) ? "BNO055"
                                                               : "GPS",
                     pkt->roll_deg10 / 10.0f, pkt->pitch_deg10 / 10.0f, rssi,
                     snr, rxTelemCount);

            sendLoraStatToMaster();
          }
        }
      }
    }
    packetReceived = false;
    radio.startReceive();
  }

  // 2. Polling periódico de telemetria aos Rovers pelo ar (1 Hz)
  if (loraReady && autoPollingEnabled && (now - lastAutoPollMs >= 1000)) {
    lastAutoPollMs = now;
    sendLoRaPoll(selectedRoverTarget);
  }

  // 3. Comunicação Inter-ESP UART com o Master Lafvin
  static uint32_t lastDiagMs = 0;
  if (now - lastDiagMs >= 2000) {
    lastDiagMs = now;
    printLog("[DIAG] UART Pins: RX(GPIO%d)=%d, TX(GPIO%d)=%d | loraReady=%d\n",
             PIN_INTER_ESP_C6_RX, digitalRead(PIN_INTER_ESP_C6_RX),
             PIN_INTER_ESP_C6_TX, digitalRead(PIN_INTER_ESP_C6_TX),
             loraReady ? 1 : 0);
    // Enviar beacon proativo para acordar o Lafvin Master caso o handshake
    // inicial tenha falhado
    interSerial.println("PONG:VER:" BASE_C6_VERSION);
    sendLoraStatToMaster();
  }

  while (interSerial.available()) {
    char c = (char)interSerial.read();
    static char rxBuf[256];
    static size_t rxIdx = 0;

    if (c == '\n' || c == '\r') {
      if (rxIdx > 0) {
        rxBuf[rxIdx] = '\0';
        if (strncmp(rxBuf, "CMD:", 4) == 0) {
          printLog("[BASE C6] <<< %s\n", rxBuf);
          handleIncomingCommand(rxBuf);
        } else if (strncmp(rxBuf, "PING:", 5) == 0) {
          uint32_t seq = 0, opSt = currentBaseOpState;
          int parsed = sscanf(rxBuf + 5, "%u:%u", &seq, &opSt);
          if (parsed >= 2 && opSt <= 3 && currentBaseOpState != (uint8_t)opSt) {
            currentBaseOpState = (uint8_t)opSt;
            c6LedForceRefresh = true;
          }
          char pong[64];
          snprintf(pong, sizeof(pong), "PONG:%u:%s\n", seq,
                   loraReady ? "RF_OK" : "RF_ERR");
          interSerial.print(pong);
          sendLoraStatToMaster();
        }
        rxIdx = 0;
      }
    } else {
      if (rxIdx < sizeof(rxBuf) - 1) {
        rxBuf[rxIdx++] = c;
      }
    }
  }
}
