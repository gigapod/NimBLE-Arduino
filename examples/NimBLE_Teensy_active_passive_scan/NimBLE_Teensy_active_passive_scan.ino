/*
 * NimBLE Teensy Scan active/passive switching demo
 *
 * Demonstrates the use of the scan callbacks while alternating between
 * passive and active scanning, on Teensy 4.1 + Murata Type 1YN.
 *
 * nimble_port_teensy_pump() must be called every loop() iteration for the
 * BLE host to run (no RTOS task does it for you on Teensy).
 */

#include <Arduino.h>
#include <NimBLEDevice.h>

extern "C" void nimble_port_teensy_pump(void);

int         scanTime = 5 * 1000; // In milliseconds, 0 = scan forever
NimBLEScan* pBLEScan;

bool active = false;

class ScanCallbacks : public NimBLEScanCallbacks {
    void onDiscovered(const NimBLEAdvertisedDevice* advertisedDevice) override {
        Serial.printf("Discovered Advertised Device: %s \n", advertisedDevice->toString().c_str());
    }

    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
        Serial.printf("Advertised Device Result: %s \n", advertisedDevice->toString().c_str());
    }

    void onScanEnd(const NimBLEScanResults& results, int reason) override {
        Serial.print("Scan Ended; reason = ");
        Serial.println(reason);
        active = !active;
        pBLEScan->setActiveScan(active);
        Serial.printf("scan start, active = %u\n", active);
        pBLEScan->start(scanTime);
    }
} scanCallbacks;

void setup() {
    Serial.begin(115200);
    Serial.println("Scanning...");

    NimBLEDevice::init("active-passive-scan");
    pBLEScan = NimBLEDevice::getScan();
    pBLEScan->setScanCallbacks(&scanCallbacks);
    pBLEScan->setActiveScan(active);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(100);
    pBLEScan->start(scanTime);
}

void loop() {
    nimble_port_teensy_pump();
}
