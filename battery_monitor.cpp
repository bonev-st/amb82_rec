#include "battery_monitor.h"
#include <Arduino.h>

void BatteryMonitor::begin() {
    pinMode(BATTERY_ADC_PIN, INPUT);
    // Take initial reading
    _voltage = readVoltage();
    _percentage = voltageToPercent(_voltage);
    LOGF("[Battery] Initial: %.2fV (%d%%)\n", _voltage, _percentage);
}

void BatteryMonitor::update() {
    unsigned long now = millis();
    if (now - _lastCheck < BATTERY_CHECK_INTERVAL_MS) {
        return;
    }
    _lastCheck = now;

    _voltage = readVoltage();
    _percentage = voltageToPercent(_voltage);

    // Determine alert state
    _prevAlertState = _currentAlertState;
    if (_percentage <= BATTERY_CRITICAL_PCT) {
        _currentAlertState = 2;
    } else if (_percentage <= BATTERY_LOW_THRESHOLD_PCT) {
        _currentAlertState = 1;
    } else {
        _currentAlertState = 0;
    }

    // Fire on any state transition (including recovery). The alert is
    // published retained, so recovery -> OK overwrites a stale CRITICAL on
    // the broker. HA filters actionable alerts via the "LOW"/"CRITICAL"
    // condition server-side.
    _newAlert = (_currentAlertState != _prevAlertState);

    if (_newAlert) {
        LOGF("[Battery] ALERT: %.2fV (%d%%) -- %s\n",
             _voltage, _percentage, getAlertLevel());
    }
}

float BatteryMonitor::readVoltage() {
    long sum = 0;
    for (int i = 0; i < BATTERY_SAMPLES; i++) {
        sum += analogRead(BATTERY_ADC_PIN);
        delay(2);
    }
    float adcAvg = (float)sum / BATTERY_SAMPLES;
    float voltage = (adcAvg / BATTERY_ADC_RESOLUTION) * BATTERY_ADC_REF_VOLTAGE * BATTERY_VOLTAGE_DIVIDER;
    return voltage;
}

int BatteryMonitor::voltageToPercent(float voltage) {
    if (voltage >= BATTERY_FULL_VOLTAGE) return 100;
    if (voltage <= BATTERY_EMPTY_VOLTAGE) return 0;
    return (int)(((voltage - BATTERY_EMPTY_VOLTAGE) /
                  (BATTERY_FULL_VOLTAGE - BATTERY_EMPTY_VOLTAGE)) * 100.0f);
}

float BatteryMonitor::getVoltage() { return _voltage; }
int BatteryMonitor::getPercentage() { return _percentage; }
bool BatteryMonitor::isLow() { return _currentAlertState >= 1; }
bool BatteryMonitor::isCritical() { return _currentAlertState >= 2; }

bool BatteryMonitor::hasNewAlert() {
    bool alert = _newAlert;
    _newAlert = false;  // Clear after reading
    return alert;
}

const char* BatteryMonitor::getAlertLevel() {
    switch (_currentAlertState) {
        case 2:  return "CRITICAL";
        case 1:  return "LOW";
        default: return "OK";
    }
}
