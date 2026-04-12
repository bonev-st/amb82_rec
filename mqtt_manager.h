#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <WiFi.h>
#include <NTPClient.h>
#include "src/PubSubClient.h"  // Force SDK-patched version over user-installed stock copy
#include "config.h"

class MqttManager {
public:
    void begin(WiFiClient& wifiClient, NTPClient& timeClient);
    bool ensureConnected();
    void loop();  // Call in main loop for keepalive

    void publishStatus(int batteryPct, float batteryV, int rssi);
    void publishMotionEvent(bool motionActive, const char* rtspUrl);
    void publishBatteryAlert(const char* level, int percentage, float voltage);

    unsigned long getEpochTime();
    void checkConfig();  // Call from main loop during idle — polls for config updates

private:
    bool connect();
    void pullConfig();
    static void mqttCallback(char* topic, uint8_t* payload, unsigned int length);

    PubSubClient _client;
    NTPClient* _timeClient = nullptr;
    unsigned long _lastReconnectAttempt = 0;
    unsigned long _lastConfigCheck = 0;

    static MqttManager* _instance;
};

#endif // MQTT_MANAGER_H
