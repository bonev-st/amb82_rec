#include "wifi_manager.h"

void WifiManager::begin() {
    LOG("[WiFi] Connecting...");
    connect();
}

bool WifiManager::connect() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) {
            LOG("[WiFi] Connection timeout");
            return false;
        }
        delay(500);
    }

    LOGF("[WiFi] Connected, IP: %s, RSSI: %ld dBm\n",
         WiFi.localIP().get_address(), WiFi.RSSI());
    return true;
}

// Non-blocking reconnect state machine. Each call returns in microseconds
// regardless of link state — a single WiFi.status() check, never a blocking
// wait loop. Used from the main loop so motion detection is not delayed
// when the AP goes away. The blocking `connect()` path is retained for
// boot-time use via begin().
bool WifiManager::ensureConnected() {
    if (WiFi.status() == WL_CONNECTED) {
        _attemptInFlight = false;
        return true;
    }

    unsigned long now = millis();

    if (!_attemptInFlight) {
        if (now - _lastReconnectAttempt < WIFI_RECONNECT_INTERVAL_MS) {
            return false;
        }
        _lastReconnectAttempt = now;
        LOG("[WiFi] Reconnecting (non-blocking)...");
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        _attemptInFlight = true;
        _attemptStart = now;
        return false;
    }

    // Attempt already in progress — single non-blocking poll per call.
    if (WiFi.status() == WL_CONNECTED) {
        LOGF("[WiFi] Connected, IP: %s, RSSI: %ld dBm\n",
             WiFi.localIP().get_address(), WiFi.RSSI());
        _attemptInFlight = false;
        return true;
    }
    if (now - _attemptStart > WIFI_CONNECT_TIMEOUT_MS) {
        LOG("[WiFi] Reconnect timeout");
        _attemptInFlight = false;
    }
    return false;
}

int WifiManager::getRSSI() {
    return WiFi.RSSI();
}

bool WifiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}
