/**
 * NimBLE_Teensy_Secure_Server Demo:
 *
 * This example demonstrates the secure passkey protected connection and
 * communication between a Teensy 4.1 + Murata Type 1YN server and a client.
 *
 * Note: this port doesn't have a persistence backend wired up yet (bonds
 * live in RAM only, see src/nimble/teensy_port/README.md's "Known
 * limitations"), so a passkey change takes effect immediately on every
 * reboot -- there's no stale bond to clear like on ESP32's NVS.
 *
 * nimble_port_teensy_pump() must be called every loop() iteration for the
 * BLE host to run (no RTOS task does it for you on Teensy).
 */

#include <Arduino.h>
#include <NimBLEDevice.h>

void setup() {
    Serial.begin(115200);
    Serial.println("Starting NimBLE Teensy Secure Server");
    NimBLEDevice::init("NimBLE");
    NimBLEDevice::setPower(3); /** +3db */

    NimBLEDevice::setSecurityAuth(true, true, false); /** bonding, MITM, don't need BLE secure connections as we are using passkey pairing */
    NimBLEDevice::setSecurityPasskey(123456);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY); /** Display only passkey */
    NimBLEServer*         pServer                  = NimBLEDevice::createServer();
    NimBLEService*        pService                 = pServer->createService("ABCD");
    NimBLECharacteristic* pNonSecureCharacteristic = pService->createCharacteristic("1234", NIMBLE_PROPERTY::READ);
    NimBLECharacteristic* pSecureCharacteristic =
        pService->createCharacteristic("1235",
                                       NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC | NIMBLE_PROPERTY::READ_AUTHEN);

    pNonSecureCharacteristic->setValue("Hello Non Secure BLE");
    pSecureCharacteristic->setValue("Hello Secure BLE");

    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID("ABCD");
    pAdvertising->start();

    Serial.println("Advertising started");
}

void loop() {
    nimble_port_teensy_pump();
}
