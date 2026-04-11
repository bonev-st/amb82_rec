#include "mqtt_manager.h"

void MqttManager::begin(WiFiClient& wifiClient) {
    _client.setClient(wifiClient);
    _client.setServer(MQTT_BROKER, MQTT_PORT);
    // keepAlive=0 → per MQTT 3.1.1 [MQTT-3.1.2-25], broker must not drop the
    // client for keepalive timeout. This is critical on Ameba because
    // PubSubClient::loop() blocks the main task indefinitely on this SDK's
    // WiFiClient implementation — we cannot call it from the main loop
    // without freezing motion detection. Instead we skip loop() entirely,
    // rely on keepAlive=0 to keep the broker from kicking us, and
    // publishMotionEvent() force-reconnects on demand if TCP breaks anyway.
    _client.setKeepAlive(0);
    connect();
}

bool MqttManager::connect() {
    LOG("[MQTT] Connecting...");

    bool connected = _client.connect(
        DEVICE_ID,
        MQTT_USER,
        MQTT_PASSWORD,
        MQTT_TOPIC_LWT,
        1,
        true,
        "offline"
    );

    if (connected) {
        LOG("[MQTT] Connected");
        _client.publish(MQTT_TOPIC_LWT, "online", true);
    } else {
        LOGF("[MQTT] Connection failed, rc=%d\n", _client.state());
    }
    return connected;
}

bool MqttManager::ensureConnected() {
    if (_client.connected()) {
        return true;
    }

    unsigned long now = millis();
    if (now - _lastReconnectAttempt < MQTT_RECONNECT_INTERVAL_MS) {
        return false;
    }
    _lastReconnectAttempt = now;
    return connect();
}

void MqttManager::loop() {
    if (_client.connected()) {
        _client.loop();
    }
}

void MqttManager::publishStatus(int batteryPct, float batteryV, int rssi) {
    if (!_client.connected()) return;

    char payload[256];
    snprintf(payload, sizeof(payload),
        "{\"device\":\"%s\",\"battery_pct\":%d,\"battery_v\":%.2f,"
        "\"rssi\":%d,\"uptime\":%lu}",
        DEVICE_ID, batteryPct, batteryV, rssi, millis() / 1000);

    _client.publish(MQTT_TOPIC_STATUS, payload);
    LOGF("[MQTT] Status: %s\n", payload);
}

void MqttManager::publishMotionEvent(bool motionActive, const char* rtspUrl) {
    // Motion events must not be dropped. If the connection is down, force an
    // immediate reconnect attempt (bypassing the backoff gate in
    // ensureConnected) so the recorder never misses a start/stop.
    if (!_client.connected()) {
        LOG("[MQTT] motion publish: not connected, forcing reconnect...");
        _lastReconnectAttempt = millis();
        if (!connect()) {
            LOG("[MQTT] motion publish: reconnect failed — event lost");
            return;
        }
    }

    char payload[256];
    if (motionActive && rtspUrl != NULL) {
        snprintf(payload, sizeof(payload),
            "{\"device\":\"%s\",\"motion\":true,\"rtsp\":\"%s\",\"timestamp\":%lu}",
            DEVICE_ID, rtspUrl, millis() / 1000);
    } else {
        snprintf(payload, sizeof(payload),
            "{\"device\":\"%s\",\"motion\":false,\"timestamp\":%lu}",
            DEVICE_ID, millis() / 1000);
    }

    _client.publish(MQTT_TOPIC_MOTION, payload);
    LOGF("[MQTT] Motion: %s\n", motionActive ? "STARTED" : "STOPPED");
}

void MqttManager::publishBatteryAlert(const char* level, int percentage, float voltage) {
    if (!_client.connected()) return;

    char payload[256];
    snprintf(payload, sizeof(payload),
        "{\"device\":\"%s\",\"alert\":\"%s\",\"battery_pct\":%d,\"battery_v\":%.2f}",
        DEVICE_ID, level, percentage, voltage);

    _client.publish(MQTT_TOPIC_BATTERY, payload, true);
    LOGF("[MQTT] Battery alert: %s (%d%%)\n", level, percentage);
}
