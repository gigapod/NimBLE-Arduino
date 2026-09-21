// BT_ON / REG_ON control for the Murata Type 1YN.
//
// Adapted from SparkFun_Teensy_BTStack_Library's teensy_bt_control.cpp
// (port/arduino-teensy41-cyw43439/teensy_bt_control.cpp): BT_ON enables the
// module's internal regulators; it is not a reset strobe, so we just drive it
// and hold it. The module needs some time after BT_ON goes high before its
// UART is ready to receive the first HCI command.

#ifdef ARDUINO_TEENSY41

#include <Arduino.h>

#include "nimble/teensy_port/board/teensy_bt_control.h"

#ifndef NIMBLE_TEENSY_BT_ON_PIN
#define NIMBLE_TEENSY_BT_ON_PIN 28
#endif

#ifndef NIMBLE_TEENSY_BT_ON_SETTLE_MS
#define NIMBLE_TEENSY_BT_ON_SETTLE_MS 150
#endif

extern "C" void teensy_bt_control_init(void) {
    pinMode(NIMBLE_TEENSY_BT_ON_PIN, OUTPUT);
    digitalWrite(NIMBLE_TEENSY_BT_ON_PIN, LOW);
}

extern "C" void teensy_bt_control_on(void) {
    digitalWrite(NIMBLE_TEENSY_BT_ON_PIN, HIGH);
    delay(NIMBLE_TEENSY_BT_ON_SETTLE_MS);
}

extern "C" void teensy_bt_control_off(void) {
    digitalWrite(NIMBLE_TEENSY_BT_ON_PIN, LOW);
}

#endif // ARDUINO_TEENSY41
