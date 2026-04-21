#include "mqtt_manager.h"

MqttManager* MqttManager::_instance = nullptr;

// When TLS is enabled we MUST connect PubSubClient by IPAddress (not hostname
// string). Reason: Ameba's WiFiSSLClient::connect(const char*, port) sets the
// SNI hostname for mbedtls_ssl_set_hostname(); mbedTLS 2.28 then checks that
// string against DNS SAN entries (not IP SAN). Passing an IP literal as a
// "hostname" therefore always fails verification with -0x2700. The IPAddress
// overload of connect() leaves _sni_hostname NULL, which makes mbedTLS skip
// hostname matching entirely (chain-of-trust verification still happens).
#if MQTT_USE_TLS
// Ameba's IPAddress has no fromString() -- parse "a.b.c.d" manually.
// MQTT_USE_TLS requires an IPv4 literal because mbedTLS 2.28 on this
// platform checks DNS SAN strings, not IP SAN; passing a hostname as SNI
// always fails -0x2700 (see begin()). If someone sets MQTT_BROKER to a
// hostname, sscanf returns 0 and the silent 0.0.0.0 fallback would hide
// the error -- so check and log loudly.
static IPAddress mqttBrokerIP() {
    unsigned int a = 0, b = 0, c = 0, d = 0;
    int n = sscanf(MQTT_BROKER, "%u.%u.%u.%u", &a, &b, &c, &d);
    if (n != 4 || a > 255 || b > 255 || c > 255 || d > 255) {
        LOGF("[MQTT] FATAL: MQTT_BROKER='%s' is not an IPv4 literal. TLS path "
             "requires a.b.c.d because hostname SNI is not supported on this "
             "mbedTLS build. Set MQTT_BROKER in secrets.h to an IP and reflash.\n",
             MQTT_BROKER);
        return IPAddress(0, 0, 0, 0);
    }
    return IPAddress((uint8_t)a, (uint8_t)b, (uint8_t)c, (uint8_t)d);
}
#endif

void MqttManager::begin(Client& netClient, NTPClient& timeClient) {
    _timeClient = &timeClient;
    _instance = this;

    _client.setClient(netClient);
#if MQTT_USE_TLS
    _client.setServer(mqttBrokerIP(), MQTT_PORT);
#else
    _client.setServer(MQTT_BROKER, MQTT_PORT);
#endif
    // No setCallback on main client -- calling loop() on Ameba's WiFiClient
    // blocks indefinitely and corrupts the connection for subsequent publishes.
    // keepAlive=0 -> per MQTT 3.1.1 [MQTT-3.1.2-25], broker must not drop the
    // client for keepalive timeout. publishMotionEvent() force-reconnects on
    // demand if TCP breaks.
    _client.setKeepAlive(0);

    // Pull config BEFORE connecting the main client -- uses a separate,
    // disposable WiFiClient + PubSubClient so the main client stays pristine.
    unsigned long t0 = millis();
    pullConfig();
    LOGF("[MQTT] pullConfig took %lu ms\n", millis() - t0);
    _lastConfigCheck = millis();  // reset poll timer so checkConfig waits a full interval
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

void MqttManager::pullConfig() {
    // Read the retained config message using a SEPARATE, disposable MQTT
    // connection.  Calling subscribe()/loop() on the main PubSubClient
    // corrupts the Ameba WiFiClient state and breaks subsequent publishes.
    // A throwaway client avoids that entirely -- it connects, grabs the
    // retained message, disconnects, and goes out of scope.
#if MQTT_USE_TLS
    WiFiSSLClient cfgWifi;
    cfgWifi.setRootCA((unsigned char*)mqtt_ca_cert);
    cfgWifi.setClientCertificate((unsigned char*)mqtt_client_cert,
                                 (unsigned char*)mqtt_client_key);
#else
    WiFiClient cfgWifi;
#endif
    PubSubClient cfgClient;
    cfgClient.setClient(cfgWifi);
#if MQTT_USE_TLS
    cfgClient.setServer(mqttBrokerIP(), MQTT_PORT);
#else
    cfgClient.setServer(MQTT_BROKER, MQTT_PORT);
#endif
    cfgClient.setCallback(mqttCallback);
    cfgClient.setKeepAlive(15);
    cfgClient.setSocketTimeout(2);

    if (!cfgClient.connect(DEVICE_ID "_cfg", MQTT_USER, MQTT_PASSWORD)) {
        LOG("[MQTT] pullConfig: connect failed");
        return;
    }

    cfgClient.subscribe(MQTT_TOPIC_CONFIG);

    // Need 2+ loop() calls: one for SUBACK, one for retained PUBLISH.
    for (int i = 0; i < 3; i++) {
        cfgClient.loop();
        delay(50);
    }

    cfgClient.disconnect();
}

void MqttManager::checkConfig() {
    // Periodic config poll -- call from main loop during STATE_IDLE only.
    // Creates a disposable MQTT connection to read the retained config
    // message, so runtime timezone changes via MQTT take effect within
    // one polling interval without needing a reboot.
    unsigned long now = millis();
    if (now - _lastConfigCheck < CONFIG_CHECK_INTERVAL_MS) return;
    _lastConfigCheck = now;
    LOG("[MQTT] Checking for config update...");
    pullConfig();
}

void MqttManager::mqttCallback(char* topic, uint8_t* payload, unsigned int length) {
    if (!_instance || !_instance->_timeClient) return;
    if (strcmp(topic, MQTT_TOPIC_CONFIG) != 0) return;

    // Parse timezone_offset from JSON payload.
    // Expected format: {"timezone_offset": 7200}
    // Use strstr + strtol for robustness to whitespace/key ordering.
    char buf[128];
    unsigned int copyLen = (length < sizeof(buf) - 1) ? length : sizeof(buf) - 1;
    memcpy(buf, payload, copyLen);
    buf[copyLen] = '\0';

    const char* key = strstr(buf, "\"timezone_offset\"");
    if (!key) {
        LOGF("[MQTT] Config: no timezone_offset in payload: %s\n", buf);
        return;
    }

    // Skip past the key and colon
    const char* colon = strchr(key, ':');
    if (!colon) return;

    char* end = nullptr;
    long offset = strtol(colon + 1, &end, 10);
    if (end == colon + 1) {
        LOGF("[MQTT] Config: failed to parse timezone_offset value\n");
        return;
    }
    // Clamp to valid timezone range +-12h to reject garbage/overflow values.
    if (offset < -43200 || offset > 43200) {
        LOGF("[MQTT] Config: timezone_offset %ld out of range, ignoring\n", offset);
        return;
    }

    _instance->_timeClient->setTimeOffset((int)offset);
    LOGF("[MQTT] Config: timezone_offset=%ld (UTC%+.1fh)\n", offset, offset / 3600.0f);
}

unsigned long MqttManager::getEpochTime() {
    if (!_timeClient) return millis() / 1000;
    unsigned long epoch = _timeClient->getEpochTime();
    // If NTP hasn't synced yet, fall back to uptime seconds.
    if (epoch < NTP_VALID_EPOCH_MIN) return millis() / 1000;
    return epoch;
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

bool MqttManager::connected() {
    return _client.connected();
}

void MqttManager::publishStatus(int batteryPct, float batteryV, int rssi) {
    if (!_client.connected()) {
        ensureConnected();  // backoff-gated, non-blocking
        if (!_client.connected()) return;
    }

    char payload[320];
    snprintf(payload, sizeof(payload),
        "{\"device\":\"%s\",\"firmware\":\"%s\",\"build\":\"%s\","
        "\"timestamp\":%lu,\"battery_pct\":%d,\"battery_v\":%.2f,"
        "\"rssi\":%d,\"uptime\":%lu}",
        DEVICE_ID, FIRMWARE_VERSION, FIRMWARE_BUILD,
        getEpochTime(), batteryPct, batteryV, rssi, millis() / 1000);

    _client.publish(MQTT_TOPIC_STATUS, payload);
    LOGF("[MQTT] Status: %s\n", payload);
}

bool MqttManager::publishMotionEvent(bool motionActive, const char* rtspUrl) {
    // Motion events are important but not worth flooding the radio with.
    // If disconnected, attempt a reconnect -- but gate at 2 s so a broker-
    // down condition during streaming (100 ms loop cadence) doesn't spin
    // on blocking reconnects and starve the motion state machine.
    if (!_client.connected()) {
        unsigned long now = millis();
        if (now - _lastReconnectAttempt < MQTT_MOTION_RECONNECT_INTERVAL_MS) {
            LOG("[MQTT] motion publish: reconnect gate closed -- event dropped");
            return false;
        }
        _lastReconnectAttempt = now;
        LOG("[MQTT] motion publish: not connected, reconnecting...");
        if (!connect()) {
            LOG("[MQTT] motion publish: reconnect failed -- event lost");
            return false;
        }
    }

    char payload[256];
    if (motionActive && rtspUrl != NULL) {
        snprintf(payload, sizeof(payload),
            "{\"device\":\"%s\",\"motion\":true,\"rtsp\":\"%s\",\"timestamp\":%lu}",
            DEVICE_ID, rtspUrl, getEpochTime());
    } else {
        snprintf(payload, sizeof(payload),
            "{\"device\":\"%s\",\"motion\":false,\"timestamp\":%lu}",
            DEVICE_ID, getEpochTime());
    }

    bool ok = _client.publish(MQTT_TOPIC_MOTION, payload);
    LOGF("[MQTT] Motion: %s (publish=%s)\n",
         motionActive ? "STARTED" : "STOPPED", ok ? "ok" : "FAIL");
    return ok;
}

void MqttManager::publishBatteryAlert(const char* level, int percentage, float voltage) {
    if (!_client.connected()) {
        unsigned long now = millis();
        if (now - _lastReconnectAttempt < MQTT_MOTION_RECONNECT_INTERVAL_MS) {
            LOG("[MQTT] battery alert: reconnect gate closed -- alert dropped");
            return;
        }
        _lastReconnectAttempt = now;
        LOG("[MQTT] battery alert: not connected, reconnecting...");
        if (!connect()) {
            LOG("[MQTT] battery alert: reconnect failed -- alert lost");
            return;
        }
    }

    char payload[256];
    snprintf(payload, sizeof(payload),
        "{\"device\":\"%s\",\"alert\":\"%s\",\"battery_pct\":%d,\"battery_v\":%.2f,"
        "\"timestamp\":%lu}",
        DEVICE_ID, level, percentage, voltage, getEpochTime());

    _client.publish(MQTT_TOPIC_BATTERY, payload, true);
    LOGF("[MQTT] Battery alert: %s (%d%%)\n", level, percentage);
}
