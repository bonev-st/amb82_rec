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

bool WifiManager::ensureConnected() {
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }

    unsigned long now = millis();
    if (now - _lastReconnectAttempt < WIFI_RECONNECT_INTERVAL_MS) {
        return false;
    }
    _lastReconnectAttempt = now;

    LOG("[WiFi] Reconnecting...");
    WiFi.disconnect();
    delay(100);
    return connect();
}

int WifiManager::getRSSI() {
    return WiFi.RSSI();
}

bool WifiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}
