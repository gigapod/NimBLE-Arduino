#ifndef NIMBLE_TEENSY_HCI_UART_H
#define NIMBLE_TEENSY_HCI_UART_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Drains whatever bytes are currently available on the HCI UART and feeds
 * them through the H4 framing state machine, dispatching complete HCI
 * events/ACL packets up to the NimBLE host as they're assembled. Must be
 * called frequently (see nimble_port_teensy_pump()) -- it never blocks.
 */
void teensy_hci_uart_poll_rx(void);

#ifdef __cplusplus
}
#endif

#endif // NIMBLE_TEENSY_HCI_UART_H
