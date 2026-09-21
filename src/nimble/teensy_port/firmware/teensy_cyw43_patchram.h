#ifndef NIMBLE_TEENSY_CYW43_PATCHRAM_H
#define NIMBLE_TEENSY_CYW43_PATCHRAM_H

#include <stdbool.h>

class HardwareSerial;

/*
 * Loads the CYW43439/Murata 1YN Bluetooth patchram over an already-opened,
 * already-powered (BT_ON high) HCI H4 UART, using the standard
 * Broadcom/Cypress "download minidriver" protocol: HCI_Reset, a vendor
 * Download_Minidriver command, then a verbatim replay of the firmware's
 * pre-packaged HCI command stream (Write_RAM records terminated by a
 * Launch_RAM record -- see firmware/cyw43_btfw_1yn.h), followed by a settle
 * delay and a final HCI_Reset once the patched firmware is running.
 *
 * This is a blocking call (typically ~1-2s): it's meant to run once, from
 * ble_transport_ll_init(), before the NimBLE host has anything else to do.
 * Returns false on any command timeout/NAK -- the caller can still proceed;
 * the host's own startup HCI_Reset will simply fail/time out and NimBLE
 * won't reach the sync callback (see README.md's troubleshooting section).
 */
bool teensy_cyw43_patchram_load(HardwareSerial &uart);

#endif // NIMBLE_TEENSY_CYW43_PATCHRAM_H
