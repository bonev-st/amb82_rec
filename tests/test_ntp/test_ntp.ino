/*
 * Test 5: NTP Time Sync
 * Verifies: NTPClient begin, update, epoch time, formatted time/date
 * Needs: Board + WiFi + internet access
 */

#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <wifi_def.h>

#define SERIAL_BAUD 115200

#define NTP_SERVER          "pool.ntp.org"
#define NTP_OFFSET          0       // UTC offset in seconds
#define NTP_UPDATE_INTERVAL 60000   // ms

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_SERVER, NTP_OFFSET, NTP_UPDATE_INTERVAL);

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

    printf("\n========================================\n");
    printf(" Test 5: NTP Time Sync\n");
    printf("========================================\n\n");

    // Connect WiFi first
    printf("[SETUP] Connecting WiFi...");
    WiFi.begin((char*)WIFI_SSID, WIFI_PASSWORD);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > 15000) {
            printf("\n[ABORT] WiFi connection failed\n");
            return;
        }
        delay(500);
        printf(".");
    }
    printf(" OK\n");

    // Test 1: Begin
    timeClient.begin();
    reportTest("timeClient.begin()", true);  // No return value, success if no crash

    // Test 2: Force update (initial sync)
    bool forced = timeClient.forceUpdate();
    printf("[TEST] forceUpdate(): %s\n", forced ? "true" : "false");
    reportTest("forceUpdate() returns true", forced);

    // Test 3: Update (returns false when interval not elapsed -- expected after forceUpdate)
    bool updated = timeClient.update();
    printf("[TEST] timeClient.update(): %s (false is OK -- interval not elapsed)\n", updated ? "true" : "false");
    reportTest("timeClient.update() runs without error", true);

    // Test 4: Epoch time
    unsigned long epoch = timeClient.getEpochTime();
    printf("[TEST] Epoch time: %lu\n", epoch);
    reportTest("getEpochTime() > 1700000000 (after 2023)", epoch > 1700000000UL);

    // Test 5: Formatted time
    String timeStr = timeClient.getFormattedTime();
    printf("[TEST] Formatted time: %s\n", timeStr.c_str());
    reportTest("getFormattedTime() non-empty", timeStr.length() > 0);

    // Test 6: Formatted date
    String dateStr = timeClient.getFormattedDate();
    printf("[TEST] Formatted date: %s\n", dateStr.c_str());
    reportTest("getFormattedDate() non-empty", dateStr.length() > 0);

    // Test 7: Date components
    int year = timeClient.getYear();
    int month = timeClient.getMonth();
    int day = timeClient.getMonthDay();
    int hours = timeClient.getHours();
    int minutes = timeClient.getMinutes();
    int seconds = timeClient.getSeconds();
    printf("[TEST] Date parts: %04d-%02d-%02d %02d:%02d:%02d\n",
           year, month, day, hours, minutes, seconds);
    reportTest("getYear() >= 2025", year >= 2025);
    reportTest("getMonth() in 1-12", month >= 1 && month <= 12);
    reportTest("getMonthDay() in 1-31", day >= 1 && day <= 31);
    reportTest("getHours() in 0-23", hours >= 0 && hours <= 23);

    // Summary
    printf("\n========================================\n");
    printf(" Results: %d PASS, %d FAIL\n", passCount, failCount);
    printf("========================================\n");
    printf("\n[TEST] Continuous time (every 5s):\n");
}

void loop() {
    timeClient.update();
    printf("  %s  epoch=%lu\n", timeClient.getFormattedTime().c_str(), timeClient.getEpochTime());
    delay(5000);
}
