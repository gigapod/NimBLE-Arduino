/**
 *  NimBLE_Teensy_Scan_Continuous Demo:
 *
 *  Continuously scans for BLE devices on Teensy 4.1 + Murata Type 1YN
 *  (Infineon CYW43439), wired as the SparkFun Teensy Wireless Shield does by
 *  default -- see src/nimble/teensy_port/README.md if your wiring differs.
 *
 *  When devices are found the onDiscovered and onResult callbacks are called
 *  with the device data. The scan doesn't store results, only the callbacks
 *  are used. When the scan timeout is reached, onScanEnd restarts it (this
 *  also clears the duplicate-filter cache so the same devices get reported
 *  again).
 *
 *  Unlike ESP32/nRF52, Teensy has no RTOS to run the NimBLE host's own task in
 *  the background, so this port drives it cooperatively: nimble_port_teensy_pump()
 *  must be called often (every loop() iteration here) for anything to happen
 *  -- discovered devices, scan restarts -- see
 *  nimble/teensy_port/npl/include/nimble/nimble_port_teensy.h for why.
 */

#include <Arduino.h>
#include <NimBLEDevice.h>

static constexpr uint32_t scanTimeMs = 30 * 1000; // 30 seconds scan time.

class ScanCallbacks : public NimBLEScanCallbacks {
    /** Initial discovery, advertisement data only. */
    void onDiscovered(const NimBLEAdvertisedDevice* advertisedDevice) override {
        Serial.printf("Discovered Device: %s\n", advertisedDevice->toString().c_str());
    }

    /**
     *  If active scanning the result here will have the scan response data.
     *  If not active scanning then this will be the same as onDiscovered.
     */
    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
        Serial.printf("Device result: %s\n", advertisedDevice->toString().c_str());
    }

    void onScanEnd(const NimBLEScanResults& results, int reason) override {
        Serial.printf("Scan ended reason = %d; restarting scan\n", reason);
        NimBLEDevice::getScan()->start(scanTimeMs, false, true);
    }
} scanCallbacks;

void setup() {
    Serial.begin(115200);
    Serial.println("Starting NimBLE Teensy Scan");

    NimBLEDevice::init("");                             // Initialize the device, you can specify a device name if you want.
    NimBLEScan* pBLEScan = NimBLEDevice::getScan();      // Create the scan object.
    pBLEScan->setScanCallbacks(&scanCallbacks, false);   // Set the callback for when devices are discovered, no duplicates.
    pBLEScan->setActiveScan(true);                       // Set active scanning, this will get more data from the advertiser.
    pBLEScan->setMaxResults(0);                          // Do not store the scan results, use callback only.
    pBLEScan->start(scanTimeMs, false, true);            // duration, not a continuation of last scan, restart to get all devices again.

    Serial.println("Scanning...");
}

void loop() {
    // Keep the BLE host alive -- see the file header comment above.
    nimble_port_teensy_pump();
}
