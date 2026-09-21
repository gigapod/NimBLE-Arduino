// HCI transport ("LL side") for Teensy 4.1 + Murata Type 1YN, implementing
// nimble/nimble/transport/include/nimble/transport_impl.h against a real H4
// UART instead of ESP32's VHCI-to-builtin-controller shim or nRF52's direct
// radio driver. Wiring/pin defaults match the SparkFun Teensy Wireless Shield
// (see SparkFun_Teensy_BTStack_Library's port/arduino-teensy41-cyw43439,
// which this borrows its BT_ON control and UART pin choices from); override
// via the NIMBLE_TEENSY_UART_* / NIMBLE_TEENSY_BT_ON_* build flags if yours
// differs.

#ifdef ARDUINO_TEENSY41

#include <Arduino.h>
#include <string.h>

extern "C" {
#include "nimble/nimble/transport/include/nimble/transport.h"
#include "nimble/nimble/transport/include/nimble/transport_impl.h"
#include "nimble/porting/nimble/include/os/os_mbuf.h"
}

#include "nimble/teensy_port/transport/include/nimble/teensy_hci_uart.h"
#include "nimble/teensy_port/board/teensy_bt_control.h"
#include "nimble/teensy_port/firmware/teensy_cyw43_patchram.h"

#ifndef NIMBLE_TEENSY_UART_PORT
#define NIMBLE_TEENSY_UART_PORT Serial8
#endif
#ifndef NIMBLE_TEENSY_UART_RTS_PIN
#define NIMBLE_TEENSY_UART_RTS_PIN 36
#endif
#ifndef NIMBLE_TEENSY_UART_CTS_PIN
#define NIMBLE_TEENSY_UART_CTS_PIN 33
#endif
#ifndef NIMBLE_TEENSY_UART_OPERATING_BAUD
#define NIMBLE_TEENSY_UART_OPERATING_BAUD 115200
#endif

#define BT_UART NIMBLE_TEENSY_UART_PORT

namespace {

// H4 RX framing state machine -- see Bluetooth Core Spec Vol 4, Part A.
enum RxState {
    RX_TYPE,
    RX_EVT_CODE,
    RX_EVT_LEN,
    RX_EVT_PARAMS,
    RX_ACL_H0,
    RX_ACL_H1,
    RX_ACL_H2,
    RX_ACL_H3,
    RX_ACL_DATA,
};

// Serial8's built-in hardware RX ring buffer is tiny (64 bytes) and
// teensy_hci_uart_poll_rx() only drains it once per pump() call. Some
// sketches call pump() from a tight loop(), but any that block for a while
// (a long delay(), a busy-wait elsewhere) risk a burst of HCI bytes (e.g.
// several advertising reports, or an L2CAP burst) overflowing that buffer
// and silently dropping bytes -- which desyncs the H4 framing state machine.
// Give the UART a much larger backing buffer so bursts have somewhere to
// sit until polled (same fix SparkFun_Teensy_BTStack_Library's
// hal_teensy_uart.cpp uses for the same reason).
uint8_t rx_overflow_buffer[2048];

RxState rx_state = RX_TYPE;

uint8_t evt_code  = 0;
uint8_t evt_len   = 0;
uint8_t evt_pos   = 0;
uint8_t evt_params[255];

uint16_t acl_handle_flags = 0;
uint16_t acl_data_len     = 0;
uint16_t acl_pos          = 0;
struct os_mbuf *acl_om    = nullptr;

void reset_rx_state() {
    rx_state = RX_TYPE;
    if (acl_om != nullptr) {
        os_mbuf_free_chain(acl_om);
        acl_om = nullptr;
    }
}

void finish_event() {
    void *buf = ble_transport_alloc_evt(0);
    if (buf != nullptr) {
        uint8_t *p = (uint8_t *)buf;
        p[0] = evt_code;
        p[1] = evt_len;
        memcpy(p + 2, evt_params, evt_len);
        ble_transport_to_hs_evt(buf);
    }
    rx_state = RX_TYPE;
}

void finish_acl() {
    struct os_mbuf *om = acl_om;
    acl_om = nullptr;
    rx_state = RX_TYPE;
    if (om != nullptr) {
        ble_transport_to_hs_acl(om);
    }
}

void process_rx_byte(uint8_t b) {
    switch (rx_state) {
        case RX_TYPE:
            if (b == 0x04) {
                rx_state = RX_EVT_CODE;
            } else if (b == 0x02) {
                rx_state = RX_ACL_H0;
            }
            // Anything else (e.g. a stray SCO byte) is ignored; we stay in
            // RX_TYPE and effectively resync on the next byte.
            break;

        case RX_EVT_CODE:
            evt_code = b;
            rx_state = RX_EVT_LEN;
            break;

        case RX_EVT_LEN:
            evt_len = b;
            evt_pos = 0;
            if (evt_len == 0) {
                finish_event();
            } else {
                rx_state = RX_EVT_PARAMS;
            }
            break;

        case RX_EVT_PARAMS:
            evt_params[evt_pos++] = b;
            if (evt_pos >= evt_len) {
                finish_event();
            }
            break;

        case RX_ACL_H0:
            acl_handle_flags = b;
            rx_state = RX_ACL_H1;
            break;

        case RX_ACL_H1:
            acl_handle_flags |= (uint16_t)b << 8;
            rx_state = RX_ACL_H2;
            break;

        case RX_ACL_H2:
            acl_data_len = b;
            rx_state = RX_ACL_H3;
            break;

        case RX_ACL_H3: {
            acl_data_len |= (uint16_t)b << 8;
            acl_pos = 0;
            acl_om = ble_transport_alloc_acl_from_ll();
            if (acl_om != nullptr) {
                uint8_t hdr[4] = {
                    (uint8_t)acl_handle_flags,
                    (uint8_t)(acl_handle_flags >> 8),
                    (uint8_t)acl_data_len,
                    (uint8_t)(acl_data_len >> 8),
                };
                os_mbuf_append(acl_om, hdr, sizeof(hdr));
            }
            if (acl_data_len == 0) {
                finish_acl();
            } else {
                rx_state = RX_ACL_DATA;
            }
            break;
        }

        case RX_ACL_DATA:
            if (acl_om != nullptr) {
                uint8_t byte = b;
                os_mbuf_append(acl_om, &byte, 1);
            }
            if (++acl_pos >= acl_data_len) {
                finish_acl();
            }
            break;
    }
}

} // namespace

extern "C" void teensy_hci_uart_poll_rx(void) {
    while (BT_UART.available() > 0) {
        process_rx_byte((uint8_t)BT_UART.read());
    }
}

extern "C" void ble_transport_ll_init(void) {
    teensy_bt_control_init();

    BT_UART.begin(NIMBLE_TEENSY_UART_OPERATING_BAUD);
    BT_UART.attachRts(NIMBLE_TEENSY_UART_RTS_PIN);
    BT_UART.attachCts(NIMBLE_TEENSY_UART_CTS_PIN);
    BT_UART.addMemoryForRead(rx_overflow_buffer, sizeof(rx_overflow_buffer));

    teensy_bt_control_on();
    reset_rx_state();

    // Best-effort: if this fails, the host's own startup HCI_Reset will
    // simply time out and NimBLEDevice::init() will hang waiting to sync --
    // see README.md's troubleshooting section.
    teensy_cyw43_patchram_load(BT_UART);
}

extern "C" void ble_transport_ll_deinit(void) {
    teensy_bt_control_off();
    reset_rx_state();
}

extern "C" int ble_transport_to_ll_cmd_impl(void *buf) {
    uint8_t *p = (uint8_t *)buf;
    uint8_t len = p[2];
    uint8_t type = 0x01;

    BT_UART.write(&type, 1);
    BT_UART.write(p, 3 + len);

    ble_transport_free(buf);
    return 0;
}

extern "C" int ble_transport_to_ll_acl_impl(struct os_mbuf *om) {
    uint8_t type = 0x02;
    BT_UART.write(&type, 1);
    for (struct os_mbuf *cur = om; cur != NULL; cur = SLIST_NEXT(cur, om_next)) {
        BT_UART.write(cur->om_data, cur->om_len);
    }

    os_mbuf_free_chain(om);
    return 0;
}

extern "C" int ble_transport_to_ll_iso_impl(struct os_mbuf *om) {
    // BIS/CIS are not supported by this port.
    os_mbuf_free_chain(om);
    return 0;
}

#endif // ARDUINO_TEENSY41
