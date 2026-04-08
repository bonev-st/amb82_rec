#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include "config.h"

class WifiManager {
public:
    void begin();
    bool ensureConnected();
    int getRSSI();
    bool isConnected();

private:
    bool connect();
    unsigned long _lastReconnectAttempt = 0;
};

#endif // WIFI_MANAGER_H
