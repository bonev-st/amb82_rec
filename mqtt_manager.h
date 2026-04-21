#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <WiFi.h>
#include <NTPClient.h>
#include "src/PubSubClient.h"  // Force SDK-patched version over user-installed stock copy
#include "config.h"

#if MQTT_USE_TLS
#include <WiFiSSLClient.h>
#include "mqtt_certs.h"
#endif

class MqttManager {
public:
    // Accepts Client& -- works with both WiFiClient and WiFiSSLClient
    void begin(Client& netClient, NTPClient& timeClient);
    bool ensureConnected();
    // Cheap state check for the blue status LED. Wraps PubSubClient::connected().
    bool connected();

    void publishStatus(int batteryPct, float batteryV, int rssi);
    // Returns true iff the publish was accepted by the TCP stack. On false,
    // caller should hold off on any state transition that depends on the
    // recorder actually knowing about the event.
    bool publishMotionEvent(bool motionActive, const char* rtspUrl);
    void publishBatteryAlert(const char* level, int percentage, float voltage);

    unsigned long getEpochTime();
    void checkConfig();  // Call from main loop during idle -- polls for config updates

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
