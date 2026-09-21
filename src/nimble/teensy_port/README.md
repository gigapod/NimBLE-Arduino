# Teensy 4.1 + Murata Type 1YN port

Runs the NimBLE **host** stack (GAP/GATT central+peripheral) on a Teensy 4.1,
talking to a Murata Type 1YN Wi-Fi+BT module (Infineon/Cypress CYW43439
silicon) over its 4-wire H4 HCI UART. There is no local BLE radio driver here
(unlike the nRF52 port) and no VHCI shim (unlike ESP32) -- this is a real
wire to a real external controller chip.

## Why this port looks different from ESP32/nRF52

Both existing NimBLE-Arduino targets assume a real RTOS is available (ESP-IDF
bundles FreeRTOS; Adafruit's nRF52 core does too) and run the host on its own
task, blocking on an event queue. Stock Teensyduino has no RTOS. Rather than
add a FreeRTOS-for-Teensy dependency, this port's NimBLE Porting Layer
(`npl/src/npl_os_teensy.cpp`) is cooperative and single-threaded: the host
runs on your sketch's own call stack, driven by repeated calls to
`nimble_port_teensy_pump()`.

**You must call `nimble_port_teensy_pump()` once per `loop()` iteration** (see
`examples/NimBLE_Teensy_Server`), or the host won't notice new connections,
incoming writes, or advertising timeouts while your sketch is doing something
else. Blocking NimBLE-Arduino calls made from your own code (e.g.
`NimBLEClient::connect()`) still work as expected without you doing anything
extra -- the NPL's semaphore/eventqueue waits call the pump internally too.

## Wiring (SparkFun Teensy Wireless Shield defaults)

Pin choices are borrowed from
[SparkFun_Teensy_BTStack_Library](https://github.com/sparkfun/SparkFun_Teensy_BTStack_Library)'s
`port/arduino-teensy41-cyw43439`, which targets the same shield:

| Signal       | Teensy pin | Notes                                      |
|--------------|:----------:|---------------------------------------------|
| HCI UART TX  | 35         | Serial8 (LPUART6) TX -> module RX           |
| HCI UART RX  | 34         | Serial8 RX <- module TX                     |
| HCI UART RTS | 36         | hardware flow control, Teensy -> module CTS |
| HCI UART CTS | 33         | hardware flow control, module -> Teensy     |
| BT_ON        | 28         | module REG_ON / power enable, active high   |

Override via build flags if your wiring differs: `NIMBLE_TEENSY_UART_PORT`,
`NIMBLE_TEENSY_UART_RTS_PIN`, `NIMBLE_TEENSY_UART_CTS_PIN`,
`NIMBLE_TEENSY_UART_OPERATING_BAUD`, `NIMBLE_TEENSY_BT_ON_PIN`,
`NIMBLE_TEENSY_BT_ON_SETTLE_MS`.

## Firmware (patchram)

The CYW43439 needs its Bluetooth init script (patchram) loaded over HCI
before it does anything -- see `firmware/teensy_cyw43_patchram.cpp` for the
standard Broadcom/Cypress "download minidriver" load sequence, and
`firmware/cyw43_btfw_1yn.h` for the firmware blob itself, copied from
[`georgerobotics/cyw43-driver`](https://github.com/georgerobotics/cyw43-driver)
by way of SparkFun's port (see that file's header comment for full
provenance). **This firmware blob is Murata/Infineon IP, not covered by
NimBLE-Arduino's Apache-2.0 license and not tested against real 1YN hardware
as part of this port** -- verify it against your module's revision, and read
cyw43-driver's/Murata's license before shipping a product with it.

## Known limitations

- **No persistence**: bonds/CCCDs live in RAM only (`MYNEWT_VAL_BLE_STORE_CONFIG_PERSIST`
  is forced off) -- every reboot forgets paired devices. Wiring up a flash
  backend is future work.
- **Fixed 115200 baud**: the UART never negotiates a higher operating baud
  with the module after patchram load (some ports switch to e.g. 3 Mbps for
  throughput). Fine for GATT-sized traffic; a bottleneck for anything
  higher-throughput.
- **No ISO channels**: `ble_transport_to_ll_iso_impl` just discards BIS/CIS
  traffic.
- **Classic Bluetooth (A2DP/HFP) is out of scope** for this port -- see
  SparkFun_Teensy_BTStack_Library (a separate BTstack-based library) if you
  need that on the same hardware.
- **Not yet validated against real 1YN hardware** as part of this port --
  the transport, NPL, and patchram sequence are believed correct against
  public protocol documentation and the SparkFun reference implementation,
  but this needs on-hardware bring-up/debugging to confirm.
