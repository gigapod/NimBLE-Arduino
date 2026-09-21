/**
 * NimBLE_Teensy_Secure_Client Demo:
 *
 * This example demonstrates the secure passkey protected connection and
 * communication between a server and a Teensy 4.1 + Murata Type 1YN client.
 *
 * nimble_port_teensy_pump() must be called every loop() iteration for the
 * BLE host to run (no RTOS task does it for you on Teensy) -- including
 * while blocked inside pScan->getResults() below, which the cooperative NPL
 * pumps internally while it waits.
 */

#include <Arduino.h>
#include <NimBLEDevice.h>

extern "C" void nimble_port_teensy_pump(void);

class ClientCallbacks : public NimBLEClientCallbacks {
    void onPassKeyEntry(NimBLEConnInfo& connInfo) override {
        Serial.printf("Server Passkey Entry\n");
        /**
         * This should prompt the user to enter the passkey displayed
         * on the peer device.
         */
        NimBLEDevice::injectPassKey(connInfo, 123456);
    }
} clientCallbacks;

void setup() {
    Serial.begin(115200);
    Serial.println("Starting NimBLE Teensy Secure Client");

    NimBLEDevice::init("");
    NimBLEDevice::setPower(3);                               /** +3db */
    NimBLEDevice::setSecurityAuth(true, true, false);        /** bonding, MITM, don't need BLE secure connections as we are using passkey pairing */
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_KEYBOARD_ONLY); /** passkey */
    NimBLEScan*       pScan   = NimBLEDevice::getScan();
    NimBLEScanResults results = pScan->getResults(5 * 1000);

    NimBLEUUID serviceUuid("ABCD");

    for (int i = 0; i < results.getCount(); i++) {
        const NimBLEAdvertisedDevice* device = results.getDevice(i);
        Serial.println(device->getName().c_str());

        if (device->isAdvertisingService(serviceUuid)) {
            NimBLEClient* pClient = NimBLEDevice::createClient();
            pClient->setClientCallbacks(&clientCallbacks, false);

            if (pClient->connect(&device)) {
                pClient->secureConnection();
                NimBLERemoteService* pService = pClient->getService(serviceUuid);
                if (pService != nullptr) {
                    NimBLERemoteCharacteristic* pNonSecureCharacteristic = pService->getCharacteristic("1234");

                    if (pNonSecureCharacteristic != nullptr) {
                        // Testing to read a non secured characteristic, you should be able to read this even if you have mismatching passkeys.
                        std::string value = pNonSecureCharacteristic->readValue();
                        // print or do whatever you need with the value
                        Serial.println(value.c_str());
                    }

                    NimBLERemoteCharacteristic* pSecureCharacteristic = pService->getCharacteristic("1235");

                    if (pSecureCharacteristic != nullptr) {
                        // Testing to read a secured characteristic, you should be able to read this only if you have
                        // matching passkeys, otherwise you should get an error like this. E NimBLERemoteCharacteristic:
                        // "<< readValue rc=261" This means you are trying to do something without the proper
                        // permissions.
                        std::string value = pSecureCharacteristic->readValue();
                        // print or do whatever you need with the value
                        Serial.println(value.c_str());
                    }
                }
            } else {
                // failed to connect
                Serial.println("failed to connect");
            }

            NimBLEDevice::deleteClient(pClient);
        }
    }
}

void loop() {
    nimble_port_teensy_pump();
}
