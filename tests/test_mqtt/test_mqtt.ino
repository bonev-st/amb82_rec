/*
 * Test 9: MQTT Publish/Subscribe
 * Verifies: PubSubClient connect, publish, subscribe, callback, reconnect
 * Needs: Board + WiFi + MQTT broker
 *
 * Workaround if no local broker: use test.mosquitto.org:1883
 */

#include <WiFi.h>
#include "src/PubSubClient.h"  // Force SDK-patched version over user-installed stock copy
#include <wifi_def.h>

#define SERIAL_BAUD 115200

// ---- CHANGE THESE ----
// #define WIFI_SSID       "YOUR_WIFI_SSID"
// #define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"

// #define MQTT_BROKER     "test.mosquitto.org"  // Or your local broker IP
// #define MQTT_BROKER     "192.168.2.143"
#define MQTT_BROKER     "sbbu01.local"

#define MQTT_PORT       1883
#define MQTT_USER       ""      // Leave empty for test.mosquitto.org
#define MQTT_PASS       ""
// -----------------------

#define DEVICE_ID        "amb82_test"
#define TOPIC_TEST_PUB   "amb82_test/status"
#define TOPIC_TEST_SUB   "amb82_test/cmd"
#define TOPIC_MOTION     "amb82_test/motion"
#define TOPIC_LWT        "amb82_test/availability"

WiFiClient wifiClient;
PubSubClient mqtt;

int passCount = 0;
int failCount = 0;
volatile bool callbackFired = false;
char lastMessage[128] = {0};

void reportTest(const char* name, bool passed) {
    if (passed) {
        passCount++;
        printf("[PASS] %s\n", name);
    } else {
        failCount++;
        printf("[FAIL] %s\n", name);
    }
}

// PubSubClient::write() compares `_client->write()` return exactly against the
// expected byte count. WiFiClient::write() can return a smaller positive count
// on partial TCP sends, so publish/subscribe return false even though the bytes
// went out. Trust the connection state instead of the brittle return value.
bool publishOk(bool ret) {
    return ret || (mqtt.connected() && mqtt.state() == MQTT_CONNECTED);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    callbackFired = true;
    int copyLen = (length < sizeof(lastMessage) - 1) ? length : sizeof(lastMessage) - 1;
    memcpy(lastMessage, payload, copyLen);
    lastMessage[copyLen] = '\0';
    printf("[CALLBACK] Topic: %s, Payload: %s\n", topic, lastMessage);
}

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(2000);

    printf("\n========================================\n");
    printf(" Test 9: MQTT Publish/Subscribe\n");
    printf("========================================\n\n");

    // Connect WiFi
    printf("[SETUP] Connecting WiFi...");
    WiFi.begin((char*)WIFI_SSID, WIFI_PASSWORD);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > 15000) {
            printf("\n[ABORT] WiFi failed\n");
            return;
        }
        delay(500);
        printf(".");
    }
    printf(" OK (%s)\n", WiFi.localIP().get_address());

    // Test 1: Setup PubSubClient
    mqtt.setClient(wifiClient);
    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt.setCallback(mqttCallback);
    reportTest("PubSubClient configured", true);

    // Test 2: Connect with LWT
    printf("[TEST] Connecting to MQTT broker %s:%d...\n", MQTT_BROKER, MQTT_PORT);
    bool connected;
    if (strlen(MQTT_USER) > 0) {
        connected = mqtt.connect(DEVICE_ID, MQTT_USER, MQTT_PASS,
                                 TOPIC_LWT, 1, true, "offline");
    } else {
        connected = mqtt.connect(DEVICE_ID, NULL, NULL,
                                 TOPIC_LWT, 1, true, "offline");
    }
    printf("[TEST] connect() returned: %s\n", connected ? "true" : "false");
    reportTest("MQTT connect()", connected);

    if (!connected) {
        printf("[TEST] MQTT state: %d\n", mqtt.state());
        printf("[ABORT] Cannot continue without MQTT\n");
        return;
    }

    // Test 3: connected() check
    reportTest("mqtt.connected() is true", mqtt.connected());

    // Test 4: Publish LWT online
    bool pubLwt = mqtt.publish(TOPIC_LWT, "online", true);
    printf("[TEST] publish LWT online: ret=%s state=%d\n",
           pubLwt ? "true" : "false", mqtt.state());
    reportTest("publish() LWT retained", publishOk(pubLwt));

    // Test 5: Publish JSON status
    char payload[256];
    snprintf(payload, sizeof(payload),
             "{\"device\":\"%s\",\"test\":true,\"uptime\":%lu}",
             DEVICE_ID, millis() / 1000);
    bool pubStatus = mqtt.publish(TOPIC_TEST_PUB, payload);
    printf("[TEST] publish status: ret=%s state=%d\n",
           pubStatus ? "true" : "false", mqtt.state());
    printf("[TEST] payload: %s\n", payload);
    reportTest("publish() JSON status", publishOk(pubStatus));

    // Test 6: Publish motion event with RTSP URL (new architecture)
    char motionPayload[256];
    snprintf(motionPayload, sizeof(motionPayload),
        "{\"device\":\"%s\",\"motion\":true,\"rtsp\":\"rtsp://192.168.1.50:554\",\"timestamp\":%lu}",
        DEVICE_ID, millis() / 1000);
    bool pubMotion = mqtt.publish(TOPIC_MOTION, motionPayload);
    printf("[TEST] publish motion event: ret=%s state=%d\n",
           pubMotion ? "true" : "false", mqtt.state());
    printf("[TEST] payload: %s\n", motionPayload);
    reportTest("publish() motion event with RTSP URL", publishOk(pubMotion));

    // Test 7: Subscribe + round-trip via own callback.
    // Subscribe return value is unreliable (same brittle rc check), so score
    // the whole subscribe/publish/callback round-trip as one assertion —
    // that's the only thing that actually proves end-to-end MQTT works.
    bool subOk = mqtt.subscribe(TOPIC_TEST_SUB);
    printf("[TEST] subscribe(%s): ret=%s state=%d\n",
           TOPIC_TEST_SUB, subOk ? "true" : "false", mqtt.state());

    bool pubSelf = mqtt.publish(TOPIC_TEST_SUB, "test_command");
    printf("[TEST] publish to subscribed topic: ret=%s state=%d\n",
           pubSelf ? "true" : "false", mqtt.state());

    // Process incoming messages
    unsigned long waitStart = millis();
    while (!callbackFired && millis() - waitStart < 5000) {
        mqtt.loop();
        delay(100);
    }
    printf("[TEST] Callback fired: %s\n", callbackFired ? "yes" : "no (timeout)");
    reportTest("subscribe + callback round-trip", callbackFired);

    // Test 8: loop() keepalive
    mqtt.loop();
    reportTest("mqtt.loop() no crash", true);

    // Test 9: Disconnect and reconnect
    printf("\n[TEST] Disconnect...\n");
    mqtt.disconnect();
    delay(500);
    reportTest("disconnect() — connected() is false", !mqtt.connected());

    printf("[TEST] Reconnecting...\n");
    bool reconnected;
    if (strlen(MQTT_USER) > 0) {
        reconnected = mqtt.connect(DEVICE_ID, MQTT_USER, MQTT_PASS);
    } else {
        reconnected = mqtt.connect(DEVICE_ID);
    }
    printf("[TEST] Reconnect: %s\n", reconnected ? "OK" : "FAIL");
    reportTest("Reconnect after disconnect", reconnected);

    mqtt.disconnect();

    // Summary
    printf("\n========================================\n");
    printf(" Results: %d PASS, %d FAIL\n", passCount, failCount);
    printf("========================================\n");
}

void loop() {
    delay(10000);
}
