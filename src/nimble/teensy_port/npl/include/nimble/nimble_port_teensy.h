#ifndef NIMBLE_PORT_TEENSY_H
#define NIMBLE_PORT_TEENSY_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Runs one non-blocking pass of: draining/parsing the HCI UART, firing any
 * expired ble_npl_callout timers, and dispatching everything currently
 * sitting on NimBLE's default host event queue.
 *
 * Teensyduino has no RTOS, so there's no separate task running the NimBLE
 * host in the background the way there is on ESP32/nRF52 (see
 * nimble/porting/npl/freertos). Instead this cooperative NPL (see
 * npl_os_teensy.cpp) calls this function itself from every blocking wait
 * (ble_npl_sem_pend, ble_npl_eventq_get, ble_npl_time_delay), so synchronous
 * NimBLE-Arduino calls (e.g. NimBLEClient::connect()) keep the host alive
 * while they block. But you must ALSO call it once per loop() iteration
 * yourself, or the host won't do anything while your sketch isn't inside a
 * blocking BLE call -- e.g. it won't notice new connections, incoming data,
 * or advertising timeouts.
 */
void nimble_port_teensy_pump(void);

#ifdef __cplusplus
}
#endif

#endif // NIMBLE_PORT_TEENSY_H
