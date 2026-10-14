#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>

// =========================================================================
// DEFINIÇÃO DE PINOS - ESP32-C6-DevKitC-1 V1.2 para SX1278 (Ra-02 433MHz)
// =========================================================================
// Header J3 (Lado Direito da placa):
#define LORA_DIO0   23  // J3 Pin 5  (DIO0 do LoRa)
#define LORA_RST    22  // J3 Pin 6  (RST do LoRa)
#define LORA_MISO   21  // J3 Pin 7  (MISO do LoRa)
#define LORA_MOSI   20  // J3 Pin 8  (MOSI do LoRa)
#define LORA_SCK    19  // J3 Pin 9  (SCK do LoRa)
#define LORA_NSS    18  // J3 Pin 10 (NSS / CS do LoRa)
// J3 Pin 12: GND       // J3 Pin 12 (GND do LoRa)
// Header J1 (Lado Esquerdo da placa):
// J1 Pin 1:  3V3       // J1 Pin 1  (3V3 do LoRa - NUNCA LIGAR A 5V!)

#define RGB_LED_PIN 8   // LED RGB WS2812 onboard do ESP32-C6 DevKitC-1

// Barramento SPI de hardware FSPI
SPIClass spiBus(FSPI);

// Instância do SX1278 no RadioLib
// Module(cs, irq/dio0, rst, gpio/dio1, spi)
SX1278 radio = new Module(LORA_NSS, LORA_DIO0, LORA_RST, RADIOLIB_NC, spiBus);

// Função auxiliar para imprimir tanto no USB nativo (Serial) como no CP2102 UART (Serial0)
void printLog(const char* format, ...) {
    char buf[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    Serial.print(buf);
    Serial0.print(buf);
}

void setRgb(uint8_t r, uint8_t g, uint8_t b) {
#ifdef RGB_BUILTIN
    rgbLedWrite(RGB_LED_PIN, r, g, b);
#else
    neopixelWrite(RGB_LED_PIN, r, g, b);
#endif
}

// Leitura direta de registo SPI sem passar pelo RadioLib (para diagnóstico bruto)
uint8_t readRawRegister(uint8_t reg) {
    digitalWrite(LORA_NSS, LOW);
    spiBus.transfer(reg & 0x7F); // MSB=0 para leitura
    uint8_t val = spiBus.transfer(0x00);
    digitalWrite(LORA_NSS, HIGH);
    return val;
}

// Escrita direta de registo SPI
void writeRawRegister(uint8_t reg, uint8_t val) {
    digitalWrite(LORA_NSS, LOW);
    spiBus.transfer(reg | 0x80); // MSB=1 para escrita
    spiBus.transfer(val);
    digitalWrite(LORA_NSS, HIGH);
}

bool loraReady = false;
uint32_t packetCounter = 0;

void setup() {
    Serial.begin(115200);
    Serial0.begin(115200);

    // Amarelo: a iniciar
    setRgb(30, 20, 0);

    delay(2000); // Aguardar estabilização do monitor série

    printLog("\n\n=======================================================\n");
    printLog("  TESTE SX1278 (Ra-02) NO ESP32-C6-DevKitC-1 V1.2       \n");
    printLog("=======================================================\n");
    printLog("MAPA DE PINOS SELECIONADOS:\n");
    printLog("  [SX1278]       -->  [ESP32-C6 DevKitC-1]\n");
    printLog("  3V3            -->  Header J1 - Pin 1 (3V3)\n");
    printLog("  GND            -->  Header J3 - Pin 12 (G)\n");
    printLog("  NSS (CS)       -->  Header J3 - Pin 10 (GPIO 18)\n");
    printLog("  SCK            -->  Header J3 - Pin 9  (GPIO 19)\n");
    printLog("  MOSI           -->  Header J3 - Pin 8  (GPIO 20)\n");
    printLog("  MISO           -->  Header J3 - Pin 7  (GPIO 21)\n");
    printLog("  RST            -->  Header J3 - Pin 6  (GPIO 22)\n");
    printLog("  DIO0           -->  Header J3 - Pin 5  (GPIO 23)\n");
    printLog("-------------------------------------------------------\n");

    // 1. Configurar pinos manuais
    pinMode(LORA_NSS, OUTPUT);
    digitalWrite(LORA_NSS, HIGH);

    pinMode(LORA_RST, OUTPUT);
    
    // Hardware Reset manual do SX1278
    printLog("[1/4] A executar Hardware Reset (RST LOW -> HIGH)...\n");
    digitalWrite(LORA_RST, LOW);
    delay(10);
    digitalWrite(LORA_RST, HIGH);
    delay(20);

    // 2. Iniciar barramento SPI de hardware (FSPI)
    printLog("[2/4] A inicializar barramento SPI (SCK=19, MISO=21, MOSI=20, CS=18)...\n");
    spiBus.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);

    // 3. Teste SPI Bruto: Leitura do RegVersion (Endereço 0x42)
    printLog("[3/4] Teste direto de leitura SPI do RegVersion (0x42)...\n");
    uint8_t regVersion = readRawRegister(0x42);
    uint8_t regOpMode  = readRawRegister(0x01);

    printLog("      -> RegVersion lido: 0x%02X (Valor esperado no SX1278: 0x12)\n", regVersion);
    printLog("      -> RegOpMode lido:  0x%02X\n", regOpMode);

    // Teste de escrita/leitura num registo de configuração (RegFifoTxBaseAddr - 0x0E)
    uint8_t oldTxBase = readRawRegister(0x0E);
    writeRawRegister(0x0E, 0x5A);
    uint8_t testTxBase = readRawRegister(0x0E);
    writeRawRegister(0x0E, oldTxBase); // restaurar
    printLog("      -> Teste Leitura/Escrita Reg 0x0E: gravado 0x5A, lido 0x%02X (%s)\n", 
             testTxBase, (testTxBase == 0x5A) ? "OK!" : "FALHA!");

    if (regVersion == 0x12) {
        printLog("      >>> SUCESSO! O chip SX1278 responde perfeitamente por SPI! <<<\n");
    } else {
        printLog("      >>> AVISO: Chip não respondeu 0x12. Verifica fios/alimentação! <<<\n");
    }

    // 4. Inicialização pelo RadioLib
    printLog("[4/4] A inicializar RadioLib (433.0 MHz, BW 250kHz, SF7, CR 4/5)...\n");
    int state = radio.begin(433.0, 250.0, 7, 5, 0x12, 17, 8);

    if (state == RADIOLIB_ERR_NONE) {
        printLog("\n=======================================================\n");
        printLog("  *** LORA INICIALIZADO COM SUCESSO NO ESP32-C6! ***  \n");
        printLog("=======================================================\n");
        loraReady = true;
        // Verde: Funcionamento perfeito!
        setRgb(0, 30, 0);
    } else {
        printLog("\n[ERRO] RadioLib falhou com código: %d\n", state);
        if (state == RADIOLIB_ERR_CHIP_NOT_FOUND) {
            printLog("       -> RADIOLIB_ERR_CHIP_NOT_FOUND (-2): Módulo não encontrado.\n");
        } else if (state == RADIOLIB_ERR_INVALID_BANDWIDTH) {
            printLog("       -> Largura de banda inválida.\n");
        }
        // Vermelho: Erro
        setRgb(40, 0, 0);
    }
}

void loop() {
    if (loraReady) {
        packetCounter++;
        char msg[64];
        snprintf(msg, sizeof(msg), "ESP32-C6 DevKit Ping #%lu", packetCounter);

        printLog("[TX] A transmitir pacote #%lu: \"%s\"...", packetCounter, msg);
        
        // Azul claro a piscar durante transmissão
        setRgb(0, 20, 40);
        uint32_t tStart = millis();
        int txState = radio.transmit(msg);
        uint32_t tDuration = millis() - tStart;

        if (txState == RADIOLIB_ERR_NONE) {
            printLog(" OK! (%lu ms)\n", tDuration);
            // Verde constante
            setRgb(0, 30, 0);
        } else {
            printLog(" Falha ao transmitir! Código: %d\n", txState);
            setRgb(40, 0, 0);
        }

        delay(3000);
    } else {
        // Se não conseguiu inicializar, tentar leitura bruta de 2 em 2 segundos
        delay(2000);
        uint8_t v = readRawRegister(0x42);
        printLog("[DIAGNÓSTICO] RegVersion lido agora: 0x%02X (esperado 0x12)\n", v);
        if (v == 0x12) {
            printLog(">>> O chip agora respondeu! Faz reset ao ESP32 para reiniciar. <<<\n");
            setRgb(0, 30, 0);
        } else {
            // Piscar vermelho
            setRgb(40, 0, 0);
            delay(200);
            setRgb(0, 0, 0);
        }
    }
}
