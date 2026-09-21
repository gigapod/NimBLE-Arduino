/**
 *  NimBLE_Teensy_Async_Client Demo:
 *
 *  Demonstrates asynchronous client operations on Teensy 4.1 + Murata Type
 *  1YN. nimble_port_teensy_pump() must be called every loop() iteration for
 *  the BLE host to run (no RTOS task does it for you on Teensy).
 */

#include <Arduino.h>
#include <NimBLEDevice.h>

static constexpr uint32_t scanTimeMs = 5 * 1000;

class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pClient) override {
        Serial.printf("Connected to: %s\n", pClient->getPeerAddress().toString().c_str());
    }

    void onDisconnect(NimBLEClient* pClient, int reason) override {
        Serial.printf("%s Disconnected, reason = %d - Starting scan\n", pClient->getPeerAddress().toString().c_str(), reason);
        NimBLEDevice::getScan()->start(scanTimeMs);
    }
} clientCallbacks;

class ScanCallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
        Serial.printf("Advertised Device found: %s\n", advertisedDevice->toString().c_str());
        if (advertisedDevice->haveName() && advertisedDevice->getName() == "NimBLE-Server") {
            Serial.printf("Found Our Device\n");

            /** Async connections can be made directly in the scan callbacks */
            auto pClient = NimBLEDevice::getDisconnectedClient();
            if (!pClient) {
                pClient = NimBLEDevice::createClient(advertisedDevice->getAddress());
                if (!pClient) {
                    Serial.printf("Failed to create client\n");
                    return;
                }
            }

            pClient->setClientCallbacks(&clientCallbacks, false);
            if (!pClient->connect(true, true, false)) { // delete attributes, async connect, no MTU exchange
                NimBLEDevice::deleteClient(pClient);
                Serial.printf("Failed to connect\n");
                return;
            }
        }
    }

    void onScanEnd(const NimBLEScanResults& results, int reason) override {
        Serial.printf("Scan ended reason = %d; restarting scan\n", reason);
        NimBLEDevice::getScan()->start(scanTimeMs);
    }
} scanCallbacks;

void setup() {
    Serial.begin(115200);
    Serial.printf("Starting NimBLE Teensy Async Client\n");
    NimBLEDevice::init("Async-Client");
    NimBLEDevice::setPower(3); /** +3db */

    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setScanCallbacks(&scanCallbacks);
    pScan->setInterval(45);
    pScan->setWindow(45);
    pScan->setActiveScan(true);
    pScan->start(scanTimeMs);
}

void loop() {
    nimble_port_teensy_pump();

    static uint32_t lastCheck = 0;
    if (millis() - lastCheck < 1000) {
        return;
    }
    lastCheck = millis();

    auto pClients = NimBLEDevice::getConnectedClients();
    if (!pClients.size()) {
        return;
    }

    for (auto& pClient : pClients) {
        Serial.printf("%s\n", pClient->toString().c_str());
        NimBLEDevice::deleteClient(pClient);
    }

    NimBLEDevice::getScan()->start(scanTimeMs);
}
