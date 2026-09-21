#ifndef NIMBLE_TEENSY_BT_CONTROL_H
#define NIMBLE_TEENSY_BT_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Drives BT_ON (Murata Type 1YN REG_ON). Overridable via
 * NIMBLE_TEENSY_BT_ON_PIN / NIMBLE_TEENSY_BT_ON_SETTLE_MS build flags -- see
 * README.md for the default SparkFun Teensy Wireless Shield wiring. */
void teensy_bt_control_init(void);
void teensy_bt_control_on(void);
void teensy_bt_control_off(void);

#ifdef __cplusplus
}
#endif

#endif // NIMBLE_TEENSY_BT_CONTROL_H
