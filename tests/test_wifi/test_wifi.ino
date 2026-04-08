/*
 * Test 4: WiFi Connection
 * Verifies: connect, status, IP, RSSI, disconnect, reconnect
 * Needs: Board + WiFi network
 */

#include <WiFi.h>

#define SERIAL_BAUD 115200

// ---- CHANGE THESE ----
#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
// -----------------------

#define CONNECT_TIMEOUT_MS 15000

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

bool connectWiFi() {
    WiFi.begin((char*)WIFI_SSID, WIFI_PASSWORD);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > CONNECT_TIMEOUT_MS) {
            return false;
        }
        delay(500);
        printf(".");
    }
    printf("\n");
    return true;
}

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(2000);

    printf("\n========================================\n");
    printf(" Test 4: WiFi Connection\n");
    printf("========================================\n\n");

    // Test 1: Initial connect
    printf("[TEST] Connecting to \"%s\"...", WIFI_SSID);
    bool connected = connectWiFi();
    reportTest("WiFi.begin() connects", connected);

    if (!connected) {
        printf("[ABORT] Cannot continue without WiFi\n");
        return;
    }

    // Test 2: Status check
    uint8_t status = WiFi.status();
    printf("[TEST] WiFi.status(): %d (expect %d = WL_CONNECTED)\n", status, WL_CONNECTED);
    reportTest("status() == WL_CONNECTED", status == WL_CONNECTED);

    // Test 3: Local IP
    char* ip = WiFi.localIP().get_address();
    printf("[TEST] Local IP: %s\n", ip);
    reportTest("localIP().get_address() non-empty", ip != NULL && strlen(ip) > 0);

    // Test 4: RSSI
    int32_t rssi = WiFi.RSSI();
    printf("[TEST] RSSI: %ld dBm\n", rssi);
    reportTest("RSSI() returns negative value", rssi < 0);

    // Test 5: SSID
    char* ssid = WiFi.SSID();
    printf("[TEST] Connected SSID: %s\n", ssid);
    reportTest("SSID() returns network name", ssid != NULL && strlen(ssid) > 0);

    // Test 6: Disconnect
    printf("[TEST] Disconnecting...\n");
    WiFi.disconnect();
    delay(1000);
    uint8_t statusAfter = WiFi.status();
    printf("[TEST] Status after disconnect: %d\n", statusAfter);
    reportTest("disconnect() changes status", statusAfter != WL_CONNECTED);

    // Test 7: Reconnect
    printf("[TEST] Reconnecting...");
    bool reconnected = connectWiFi();
    reportTest("Reconnect after disconnect", reconnected);

    if (reconnected) {
        char* ip2 = WiFi.localIP().get_address();
        printf("[TEST] IP after reconnect: %s\n", ip2);
        reportTest("IP available after reconnect", ip2 != NULL && strlen(ip2) > 0);
    }

    // Summary
    printf("\n========================================\n");
    printf(" Results: %d PASS, %d FAIL\n", passCount, failCount);
    printf("========================================\n");
}

void loop() {
    // Print RSSI every 5 seconds to monitor stability
    if (WiFi.status() == WL_CONNECTED) {
        printf("  RSSI: %ld dBm\n", WiFi.RSSI());
    } else {
        printf("  WiFi disconnected!\n");
    }
    delay(5000);
}
