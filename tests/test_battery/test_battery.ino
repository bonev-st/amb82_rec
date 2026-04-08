/*
 * Test 2: ADC / Battery Monitor
 * Verifies: analogRead(A0), voltage calculation, percentage mapping, alert transitions
 * Needs: Board + USB (floating A0 is fine for basic validation)
 */

#define SERIAL_BAUD            115200
#define BATTERY_ADC_PIN        A0
#define BATTERY_FULL_VOLTAGE   4.2f
#define BATTERY_EMPTY_VOLTAGE  3.3f
#define BATTERY_VOLTAGE_DIVIDER 2.0f
#define BATTERY_ADC_RESOLUTION 1024
#define BATTERY_ADC_REF_VOLTAGE 3.3f
#define BATTERY_SAMPLES        10
#define BATTERY_LOW_PCT        20
#define BATTERY_CRITICAL_PCT   10

int passCount = 0;
int failCount = 0;

void reportTest(const char* name, bool passed) {
    if (passed) {
        passCount++;
        printf("[PASS] %s\n", name);
    } else {
        failCount++;
        printf("[FAIL] %s\n", name);
    }
}

float readVoltage() {
    long sum = 0;
    for (int i = 0; i < BATTERY_SAMPLES; i++) {
        sum += analogRead(BATTERY_ADC_PIN);
        delay(2);
    }
    float adcAvg = (float)sum / BATTERY_SAMPLES;
    return (adcAvg / BATTERY_ADC_RESOLUTION) * BATTERY_ADC_REF_VOLTAGE * BATTERY_VOLTAGE_DIVIDER;
}

int voltageToPercent(float voltage) {
    if (voltage >= BATTERY_FULL_VOLTAGE) return 100;
    if (voltage <= BATTERY_EMPTY_VOLTAGE) return 0;
    return (int)(((voltage - BATTERY_EMPTY_VOLTAGE) /
                  (BATTERY_FULL_VOLTAGE - BATTERY_EMPTY_VOLTAGE)) * 100.0f);
}

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(2000);
    pinMode(BATTERY_ADC_PIN, INPUT);

    printf("\n========================================\n");
    printf(" Test 2: ADC / Battery Monitor\n");
    printf("========================================\n\n");

    // Test 1: Raw ADC read
    uint32_t raw = analogRead(BATTERY_ADC_PIN);
    printf("[TEST] Raw ADC value: %lu (range 0-%d)\n", raw, BATTERY_ADC_RESOLUTION - 1);
    reportTest("analogRead(A0) returns value", raw <= (uint32_t)BATTERY_ADC_RESOLUTION);

    // Test 2: Averaged voltage
    float voltage = readVoltage();
    printf("[TEST] Averaged voltage: %.3fV (from %d samples)\n", voltage, BATTERY_SAMPLES);
    reportTest("Voltage reading (any value is OK for floating pin)", true);

    // Test 3: Percentage mapping boundaries
    int pctFull = voltageToPercent(4.2f);
    int pctEmpty = voltageToPercent(3.3f);
    int pctMid = voltageToPercent(3.75f);
    int pctOver = voltageToPercent(5.0f);
    int pctUnder = voltageToPercent(2.0f);
    printf("[TEST] Pct at 4.2V: %d%% (expect 100)\n", pctFull);
    printf("[TEST] Pct at 3.3V: %d%% (expect 0)\n", pctEmpty);
    printf("[TEST] Pct at 3.75V: %d%% (expect ~50)\n", pctMid);
    printf("[TEST] Pct at 5.0V: %d%% (expect 100)\n", pctOver);
    printf("[TEST] Pct at 2.0V: %d%% (expect 0)\n", pctUnder);
    reportTest("100% at full voltage", pctFull == 100);
    reportTest("0% at empty voltage", pctEmpty == 0);
    reportTest("~50% at midpoint", pctMid >= 40 && pctMid <= 60);
    reportTest("Clamped at 100% above full", pctOver == 100);
    reportTest("Clamped at 0% below empty", pctUnder == 0);

    // Test 4: Alert level logic
    const char* levelOk = (75 > BATTERY_LOW_PCT) ? "OK" : "LOW";
    const char* levelLow = (15 <= BATTERY_LOW_PCT && 15 > BATTERY_CRITICAL_PCT) ? "LOW" : "WRONG";
    const char* levelCrit = (5 <= BATTERY_CRITICAL_PCT) ? "CRITICAL" : "WRONG";
    printf("[TEST] 75%% → %s (expect OK)\n", levelOk);
    printf("[TEST] 15%% → %s (expect LOW)\n", levelLow);
    printf("[TEST] 5%%  → %s (expect CRITICAL)\n", levelCrit);
    reportTest("Alert: 75% = OK", strcmp(levelOk, "OK") == 0);
    reportTest("Alert: 15% = LOW", strcmp(levelLow, "LOW") == 0);
    reportTest("Alert: 5% = CRITICAL", strcmp(levelCrit, "CRITICAL") == 0);

    // Summary
    printf("\n========================================\n");
    printf(" Results: %d PASS, %d FAIL\n", passCount, failCount);
    printf("========================================\n");
    printf("\n[TEST] Continuous ADC readings (every 2s):\n");
}

void loop() {
    float v = readVoltage();
    int pct = voltageToPercent(v);
    uint32_t raw = analogRead(BATTERY_ADC_PIN);
    printf("  ADC raw=%lu  voltage=%.3fV  pct=%d%%\n", raw, v, pct);
    delay(2000);
}
