#include "led_manager.h"

void LedManager::begin(int greenPin, int bluePin) {
    _green.pin = greenPin;
    _blue.pin  = bluePin;
    pinMode(_green.pin, OUTPUT);
    pinMode(_blue.pin,  OUTPUT);
    digitalWrite(_green.pin, LOW);
    digitalWrite(_blue.pin,  LOW);
    _green.mode = LED_OFF;
    _blue.mode  = LED_OFF;
}

void LedManager::setGreen(Mode m) { _setMode(_green, m); }
void LedManager::setBlue(Mode m)  { _setMode(_blue,  m); }

void LedManager::_setMode(Channel& c, Mode m) {
    if (c.mode == m) return;
    c.mode = m;
    c.lastToggle = millis();
    // Reset phase so the new mode starts at a deterministic point:
    // LED_OFF/BLINK_*: start LOW; LED_ON: start HIGH.
    bool high = (m == LED_ON);
    c.phase = high;
    digitalWrite(c.pin, high ? HIGH : LOW);
}

void LedManager::_apply(Channel& c, unsigned long now) {
    switch (c.mode) {
        case LED_OFF:
            if (c.phase) { digitalWrite(c.pin, LOW); c.phase = false; }
            return;
        case LED_ON:
            if (!c.phase) { digitalWrite(c.pin, HIGH); c.phase = true; }
            return;
        case LED_BLINK_FAST: {
            const unsigned long target = 500;  // same on/off
            if (now - c.lastToggle >= target) {
                c.phase = !c.phase;
                digitalWrite(c.pin, c.phase ? HIGH : LOW);
                c.lastToggle = now;
            }
            return;
        }
        case LED_BLINK_SLOW: {
            const unsigned long onMs  = 500;
            const unsigned long offMs = 3000;
            const unsigned long target = c.phase ? onMs : offMs;
            if (now - c.lastToggle >= target) {
                c.phase = !c.phase;
                digitalWrite(c.pin, c.phase ? HIGH : LOW);
                c.lastToggle = now;
            }
            return;
        }
    }
}

void LedManager::update() {
    unsigned long now = millis();
    _apply(_green, now);
    _apply(_blue,  now);
}
