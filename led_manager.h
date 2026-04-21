#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include <Arduino.h>

// Non-blocking LED driver for the two on-board indicators.
//
//   Green (REC_LED_PIN, LED_G / PE_6): motion/record state.
//     LED_OFF           = idle (no motion)
//     LED_BLINK_FAST    = motion detected but server not yet notified
//     LED_ON            = motion notified to server (server is/was recording)
//
//   Blue (STATUS_LED_PIN, LED_BUILTIN / PF_9): system health.
//     LED_ON            = WiFi not associated
//     LED_BLINK_FAST    = WiFi OK, RTC not yet set from NTP
//     LED_BLINK_SLOW = WiFi + NTP OK, MQTT broker not connected
//     LED_OFF           = all OK
//
// Call begin() once from setup(), setGreen/setBlue() whenever state
// changes (cheap if the mode is unchanged), and update() every loop
// iteration. update() is O(1) and never blocks.
class LedManager {
public:
    enum Mode {
        LED_OFF = 0,
        LED_ON,
        LED_BLINK_FAST,     // 500 ms on / 500 ms off
        LED_BLINK_SLOW,  // 500 ms on / 3000 ms off
    };

    void begin(int greenPin, int bluePin);
    void setGreen(Mode m);
    void setBlue(Mode m);
    void update();

private:
    struct Channel {
        int  pin = -1;
        Mode mode = LED_OFF;
        bool phase = false;            // current pin state: true=HIGH
        unsigned long lastToggle = 0;  // millis() of last phase change
    };

    static void _setMode(Channel& c, Mode m);
    static void _apply(Channel& c, unsigned long now);

    Channel _green;
    Channel _blue;
};

#endif  // LED_MANAGER_H
