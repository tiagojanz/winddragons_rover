#include <Arduino.h>

// ==============================================================================
// TESTE POWER MODULE GM v1.0 - CALIBRADO (ESP32-C6 DevKitC-1)
// ==============================================================================
// - GPIO 2 (ADC1_CH2): Tensão da Bateria (Rácio calibrado: 10.803)
// - GPIO 3 (ADC1_CH3): Corrente da Bateria (Sensibilidade: 27.32 A/V)
// - GND: Pino 15 (GND)
// ==============================================================================

#define PIN_ADC_VOLT    2   // GPIO 2
#define PIN_ADC_CURR    3   // GPIO 3

// Calibração exata obtida: 23.61V / 2.1855V = 10.803
const float VOLT_DIVIDER_RATIO = 10.803f;
const float AMPS_PER_VOLT      = 27.32f;
const float CURR_OFFSET_MV     = 6.5f; // Offset residual medido a vazio (0A)

void printLog(const char* format, ...) {
    char buf[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    Serial.print(buf);
    Serial0.print(buf);
}

void setup() {
    Serial.begin(115200);
    Serial0.begin(115200);
    uint32_t t = millis();
    while (!Serial && !Serial0 && millis() - t < 2000) delay(10);

    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    pinMode(PIN_ADC_VOLT, INPUT);
    pinMode(PIN_ADC_CURR, INPUT);

    printLog("\n=======================================================================\n");
    printLog("    POWER MODULE GM v1.0 - TELEMETRIA EM TEMPO REAL CALIBRADA         \n");
    printLog("=======================================================================\n");
    printLog("Configuração: GPIO 2 = BATT_VOLT | GPIO 3 = BATT_CURR | GND = J1-15\n\n");
}

void loop() {
    uint32_t sumVoltMv = 0;
    uint32_t sumCurrMv = 0;
    const int N = 30;

    for (int i = 0; i < N; i++) {
        sumVoltMv += analogReadMilliVolts(PIN_ADC_VOLT);
        sumCurrMv += analogReadMilliVolts(PIN_ADC_CURR);
        delayMicroseconds(400);
    }

    float mvV = (float)sumVoltMv / N;
    float mvI = (float)sumCurrMv / N;

    // Tensão calibrada da bateria
    float batVolts = (mvV * VOLT_DIVIDER_RATIO) / 1000.0f;
    float cell6S   = batVolts / 6.0f;
    float cell4S   = batVolts / 4.0f;

    // Corrente real em Amperes com cancelamento do offset de repouso
    float currAmps = 0.0f;
    if (mvI > CURR_OFFSET_MV) {
        currAmps = ((mvI - CURR_OFFSET_MV) / 1000.0f) * AMPS_PER_VOLT;
    }

    // Estado da Bateria LiPo 6S (corte seguro: 19.8V [3.3V/cell] -> cheia: 25.2V [4.2V/cell])
    int pct6S = 0;
    if (batVolts >= 25.2f) pct6S = 100;
    else if (batVolts <= 19.8f) pct6S = 0;
    else pct6S = (int)(((batVolts - 19.8f) / (25.2f - 19.8f)) * 100.0f);

    printLog("[BATERIA 6S] Tensao: %5.2f V (%4.2f V/cel | %3d%%) | Corrente: %5.2f A | P:%6.1f W [ADC_V: %5.1f mV | ADC_I: %5.1f mV]\n",
             batVolts, cell6S, pct6S, currAmps, (batVolts * currAmps), mvV, mvI);

    delay(500);
}
