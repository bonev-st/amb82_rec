#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include "config.h"

class BatteryMonitor {
public:
    void begin();
    void update();  // Call periodically in loop

    float getVoltage();
    int getPercentage();
    bool isLow();
    bool isCritical();

    // Returns true only on the transition from normal→low or low→critical
    bool hasNewAlert();
    const char* getAlertLevel();

private:
    float readVoltage();
    int voltageToPercent(float voltage);

    float _voltage = 0;
    int _percentage = 100;
    int _prevAlertState = 0;  // 0=normal, 1=low, 2=critical
    int _currentAlertState = 0;
    bool _newAlert = false;
    unsigned long _lastCheck = 0;
};

#endif // BATTERY_MONITOR_H
