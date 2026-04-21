#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include "config.h"

class WifiManager {
public:
    // Returns the connect result so setup() can decide whether to retry
    // or reset -- rtsp.begin() later on the call chain hangs forever if
    // WiFi isn't up yet.
    bool begin();
    bool ensureConnected();
    int getRSSI();

private:
    bool connect(unsigned long timeoutMs = WIFI_CONNECT_TIMEOUT_MS);
    unsigned long _lastReconnectAttempt = 0;
    bool _attemptInFlight = false;
    unsigned long _attemptStart = 0;
};

#endif // WIFI_MANAGER_H
