/*
 * Test 1: Serial & Debug Output
 * Verifies: Serial.println(), printf(), LOG/LOGF macros, LED blink
 * Needs: Board + USB only
 */

#define DEBUG_ENABLED true
#define SERIAL_BAUD   115200

#if DEBUG_ENABLED
  #define LOG(msg)           Serial.println(msg)
  #define LOGF(fmt, ...)     printf(fmt, ##__VA_ARGS__)
#else
  #define LOG(msg)
  #define LOGF(fmt, ...)
#endif

#define LED_PIN LED_BUILTIN

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

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(2000);
    pinMode(LED_PIN, OUTPUT);

    printf("\n========================================\n");
    printf(" Test 1: Serial & Debug Output\n");
    printf("========================================\n\n");

    // Test 1: Serial.println
    Serial.println("[TEST] Serial.println works");
    reportTest("Serial.println()", true);  // If you see this, it works

    // Test 2: printf with integer
    int testInt = 42;
    printf("[TEST] printf integer: %d\n", testInt);
    reportTest("printf integer", true);

    // Test 3: printf with float
    float testFloat = 3.14f;
    printf("[TEST] printf float: %.2f\n", testFloat);
    reportTest("printf float", true);

    // Test 4: printf with string
    const char* testStr = "hello";
    printf("[TEST] printf string: %s\n", testStr);
    reportTest("printf string", true);

    // Test 5: printf with %% literal
    printf("[TEST] printf percent: 100%%\n");
    reportTest("printf percent literal", true);

    // Test 6: printf with multiple args
    printf("[TEST] printf multi: int=%d float=%.1f str=%s\n", testInt, testFloat, testStr);
    reportTest("printf multiple args", true);

    // Test 7: LOG macro
    LOG("[TEST] LOG macro works");
    reportTest("LOG() macro", true);

    // Test 8: LOGF macro
    LOGF("[TEST] LOGF macro: val=%d\n", 99);
    reportTest("LOGF() macro", true);

    // Test 9: unsigned long (millis)
    printf("[TEST] millis: %lu\n", millis());
    reportTest("printf unsigned long", true);

    // Summary
    printf("\n========================================\n");
    printf(" Results: %d PASS, %d FAIL\n", passCount, failCount);
    printf("========================================\n");
    printf("\n[TEST] LED blink starting — verify visually\n");
}

void loop() {
    // Blink LED to confirm board is alive
    digitalWrite(LED_PIN, HIGH);
    delay(500);
    digitalWrite(LED_PIN, LOW);
    delay(500);
}
