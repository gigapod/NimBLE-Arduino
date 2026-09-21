#ifdef ARDUINO_TEENSY41

#include <Arduino.h>
#include <string.h>

#include "nimble/teensy_port/firmware/teensy_cyw43_patchram.h"
#include "nimble/teensy_port/firmware/cyw43_btfw_1yn.h"

namespace {

constexpr uint16_t kHciOpReset            = 0x0C03; // HCI_Reset
constexpr uint16_t kHciOpDownloadMinidrv  = 0xFC2E; // BCM/CYW vendor: Download_Minidriver

bool read_byte_blocking(HardwareSerial &uart, uint8_t *out, uint32_t timeout_ms) {
    uint32_t start = millis();
    while ((uint32_t)(millis() - start) < timeout_ms) {
        if (uart.available() > 0) {
            *out = (uint8_t)uart.read();
            return true;
        }
    }
    return false;
}

// Waits for an HCI Command Complete event matching `opcode`, ignoring/
// discarding anything else seen in the meantime (there shouldn't be
// anything else this early, but a stray event shouldn't wedge bring-up).
bool wait_for_command_complete(HardwareSerial &uart, uint16_t opcode, uint32_t timeout_ms) {
    uint32_t start = millis();
    while ((uint32_t)(millis() - start) < timeout_ms) {
        uint8_t type;
        if (!read_byte_blocking(uart, &type, timeout_ms)) {
            return false;
        }
        if (type != 0x04) { // not an HCI event packet -- resync
            continue;
        }

        uint8_t evcode, plen;
        if (!read_byte_blocking(uart, &evcode, timeout_ms)) return false;
        if (!read_byte_blocking(uart, &plen, timeout_ms)) return false;

        uint8_t params[255];
        for (uint8_t i = 0; i < plen; i++) {
            if (!read_byte_blocking(uart, &params[i], timeout_ms)) {
                return false;
            }
        }

        constexpr uint8_t kEvtCommandComplete = 0x0E;
        if (evcode == kEvtCommandComplete && plen >= 4) {
            uint16_t got_opcode = (uint16_t)params[1] | ((uint16_t)params[2] << 8);
            uint8_t status = params[3];
            if (got_opcode == opcode) {
                return status == 0x00;
            }
        }
        // Anything else (unexpected event, CC for a different opcode):
        // keep waiting for the one we asked for.
    }
    return false;
}

bool send_cmd_wait_cc(HardwareSerial &uart, uint16_t opcode, const uint8_t *payload,
                       uint8_t len, uint32_t timeout_ms) {
    uint8_t hdr[4] = {
        0x01, // H4 packet type: HCI Command
        (uint8_t)(opcode & 0xFF),
        (uint8_t)(opcode >> 8),
        len,
    };
    uart.write(hdr, sizeof(hdr));
    if (len > 0 && payload != nullptr) {
        uart.write(payload, len);
    }
    uart.flush();
    return wait_for_command_complete(uart, opcode, timeout_ms);
}

} // namespace

bool teensy_cyw43_patchram_load(HardwareSerial &uart) {
    if (!send_cmd_wait_cc(uart, kHciOpReset, nullptr, 0, 500)) {
        return false;
    }

    if (!send_cmd_wait_cc(uart, kHciOpDownloadMinidrv, nullptr, 0, 500)) {
        return false;
    }
    delay(50); // let the chip switch into download mode

    // cyw43_btfw_1yn[] is a verbatim stream of complete HCI command packets
    // (2-byte opcode LE, 1-byte length, payload) -- mostly BCM/CYW
    // "Write_RAM" (opcode 0xFC4C) records, terminated by one "Launch_RAM"
    // (0xFC4E) record. Replaying it is opcode-agnostic: send each record,
    // wait for its Command Complete, move to the next.
    const uint8_t *p = cyw43_btfw_1yn;
    uint32_t remaining = cyw43_btfw_1yn_len;
    while (remaining >= 3) {
        uint16_t opcode = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
        uint8_t len = p[2];
        if (3u + len > remaining) {
            break; // malformed trailing data -- shouldn't happen
        }
        if (!send_cmd_wait_cc(uart, opcode, p + 3, len, 1000)) {
            return false;
        }
        p += 3 + len;
        remaining -= 3 + len;
    }

    // Launch_RAM jumps into the patched firmware -- give it time to boot,
    // then re-sync with a fresh reset.
    delay(250);
    return send_cmd_wait_cc(uart, kHciOpReset, nullptr, 0, 1000);
}

#endif // ARDUINO_TEENSY41
