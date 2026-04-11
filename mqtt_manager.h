#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <WiFi.h>
#include "src/PubSubClient.h"  // Force SDK-patched version over user-installed stock copy
#include "config.h"

class MqttManager {
public:
    void begin(WiFiClient& wifiClient);
    bool ensureConnected();
    void loop();  // Call in main loop for keepalive

    void publishStatus(int batteryPct, float batteryV, int rssi);
    void publishMotionEvent(bool motionActive, const char* rtspUrl);
    void publishBatteryAlert(const char* level, int percentage, float voltage);

private:
    bool connect();

    PubSubClient _client;
    unsigned long _lastReconnectAttempt = 0;
};

#endif // MQTT_MANAGER_H
