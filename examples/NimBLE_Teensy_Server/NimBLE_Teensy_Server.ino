/**
 *  NimBLE_Teensy_Server Demo:
 *
 *  A minimal BLE server for Teensy 4.1 + Murata Type 1YN (Infineon CYW43439),
 *  wired as the SparkFun Teensy Wireless Shield does by default -- see
 *  src/nimble/teensy_port/README.md if your wiring differs.
 *
 *  Unlike ESP32/nRF52, Teensy has no RTOS to run the NimBLE host's own task in
 *  the background, so this port drives it cooperatively: nimble_port_teensy_pump()
 *  must be called often (every loop() iteration here) for anything to happen
 *  -- new connections, incoming writes, advertising -- see
 *  nimble/teensy_port/npl/include/nimble/nimble_port_teensy.h for why.
 */

#include <Arduino.h>
#include <NimBLEDevice.h>

extern "C" void nimble_port_teensy_pump(void);

static NimBLEServer* pServer;
static NimBLECharacteristic* pFoodCharacteristic;

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        Serial.printf("Client connected: %s\n", connInfo.getAddress().toString().c_str());
    }

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        Serial.printf("Client disconnected (reason %d) - restarting advertising\n", reason);
        NimBLEDevice::startAdvertising();
    }
} serverCallbacks;

class CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
        Serial.printf("onWrite(), value: %s\n", pCharacteristic->getValue().c_str());
    }
} chrCallbacks;

void setup() {
    Serial.begin(115200);
    Serial.println("Starting NimBLE Teensy Server");

    NimBLEDevice::init("NimBLE-Teensy");

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(&serverCallbacks);

    NimBLEService* pService = pServer->createService("BAAD");
    pFoodCharacteristic =
        pService->createCharacteristic("F00D", NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
    pFoodCharacteristic->setValue("Fries");
    pFoodCharacteristic->setCallbacks(&chrCallbacks);

    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->setName("NimBLE-Teensy");
    pAdvertising->addServiceUUID(pService->getUUID());
    pAdvertising->enableScanResponse(true);
    pAdvertising->start();

    Serial.println("Advertising started");
}

void loop() {
    // Keep the BLE host alive -- see the file header comment above.
    nimble_port_teensy_pump();

    static uint32_t lastNotify = 0;
    if (pServer->getConnectedCount() && millis() - lastNotify > 2000) {
        lastNotify = millis();
        pFoodCharacteristic->notify();
    }
}
