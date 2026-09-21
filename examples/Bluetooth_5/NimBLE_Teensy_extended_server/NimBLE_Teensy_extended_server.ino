/**
 *  NimBLE Teensy Extended Server Demo:
 *
 *  Demonstrates the Bluetooth 5.x extended advertising capabilities on
 *  Teensy 4.1 + Murata Type 1YN.
 *
 *  This demo will advertise a long data string on the CODED and 1M Phy's and
 *  starts a server allowing connection over either PHY's. It will advertise for
 *  5 seconds then "sleep" for 20 seconds, if a client connects it will sleep once
 *  it has disconnected then repeats.
 *
 *  Teensy has no low-power deep sleep API like ESP32's esp_deep_sleep(), and
 *  no systemRestart() like the n-able nRF52 core, so "sleep" here means: wait
 *  out the interval with delay(), then do a full MCU reset via
 *  a direct AIRCR register write (portable across Cortex-M cores) to start clean, same as the
 *  ESP32/nRF52 versions of this example do via their own reset/sleep APIs.
 *
 *  nimble_port_teensy_pump() must be called every loop() iteration for the
 *  BLE host to run (no RTOS task does it for you on Teensy) -- including
 *  during setup() below, before the reset/sleep branches are reached.
 */

#include <Arduino.h>
#include <NimBLEDevice.h>
#if !MYNEWT_VAL(BLE_EXT_ADV)
# error Must enable extended advertising, see nimconfig.h file.
#endif

extern "C" void nimble_port_teensy_pump(void);

#define SERVICE_UUID        "ABCD"
#define CHARACTERISTIC_UUID "1234"

/** Time in milliseconds to advertise */
static uint32_t advTime = 5000;

/** Time to sleep between advertisements */
static uint32_t sleepSeconds = 20;

/** Primary PHY used for advertising, can be one of BLE_HCI_LE_PHY_1M or BLE_HCI_LE_PHY_CODED */
static uint8_t primaryPhy = BLE_HCI_LE_PHY_CODED;

/**
 *  Secondary PHY used for advertising and connecting,
 *  can be one of BLE_HCI_LE_PHY_1M, BLE_HCI_LE_PHY_2M or BLE_HCI_LE_PHY_CODED
 */
static uint8_t secondaryPhy = BLE_HCI_LE_PHY_1M;

/** Wait out sleepSeconds while still servicing the BLE host, then reset. */
static void sleepThenReset() {
    uint32_t start = millis();
    while (millis() - start < sleepSeconds * 1000) {
        nimble_port_teensy_pump();
    }
    // Cortex-M system reset: VECTKEY 0x05FA + SYSRESETREQ in AIRCR.
    *(volatile uint32_t*)0xE000ED0Cu = 0x05FA0004u;
    while (true) {}
}

/** Handler class for server events */
class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        Serial.printf("Client connected:\n%s", connInfo.toString().c_str());
    }

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        Serial.printf("Client disconnected - sleeping for %" PRIu32 " seconds\n", sleepSeconds);
        sleepThenReset();
    }
} serverCallbacks;

/** Callback class to handle advertising events */
class AdvertisingCallbacks : public NimBLEExtAdvertisingCallbacks {
    void onStopped(NimBLEExtAdvertising* pAdv, int reason, uint8_t instId) override {
        /* Check the reason advertising stopped, don't sleep if client is connecting */
        Serial.printf("Advertising instance %u stopped\n", instId);
        switch (reason) {
            case 0:
                Serial.printf("Client connecting\n");
                return;
            case BLE_HS_ETIMEOUT:
                Serial.printf("Time expired - sleeping for %" PRIu32 " seconds\n", sleepSeconds);
                break;
            default:
                break;
        }

        sleepThenReset();
    }
} advertisingCallbacks;

void setup() {
    Serial.begin(115200);

    /** Initialize NimBLE and set the device name */
    NimBLEDevice::init("Extended advertiser");

    /** Create the server and add the services/characteristics/descriptors */
    NimBLEServer* pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(&serverCallbacks);

    NimBLEService*        pService = pServer->createService(SERVICE_UUID);
    NimBLECharacteristic* pCharacteristic =
        pService->createCharacteristic(CHARACTERISTIC_UUID,
                                       NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);

    pCharacteristic->setValue("Hello World");

    /**
     *  Create an extended advertisement with the instance ID 0 and set the PHY's.
     *  Multiple instances can be added as long as the instance ID is incremented.
     */
    NimBLEExtAdvertisement extAdv(primaryPhy, secondaryPhy);

    /** Set the advertisement as connectable */
    extAdv.setConnectable(true);

    /** As per Bluetooth specification, extended advertising cannot be both scannable and connectable */
    extAdv.setScannable(false); // The default is false, set here for demonstration.

    /** Extended advertising allows for 251 bytes (minus header bytes ~20) in a single advertisement or up to 1650 if chained */
    extAdv.setServiceData(NimBLEUUID(SERVICE_UUID),
                          std::string("Extended Advertising Demo.\r\n"
                                      "Extended advertising allows for "
                                      "251 bytes of data in a single advertisement,\r\n"
                                      "or up to 1650 bytes with chaining.\r\n"
                                      "This example message is 226 bytes long "
                                      "and is using CODED_PHY for long range."));

    extAdv.setName("Extended advertiser");

    /** When extended advertising is enabled `NimBLEDevice::getAdvertising` returns a pointer to `NimBLEExtAdvertising */
    NimBLEExtAdvertising* pAdvertising = NimBLEDevice::getAdvertising();

    /** Set the callbacks for advertising events */
    pAdvertising->setCallbacks(&advertisingCallbacks);

    /**
     *  NimBLEExtAdvertising::setInstanceData takes the instance ID and
     *  a reference to a `NimBLEExtAdvertisement` object. This sets the data
     *  that will be advertised for this instance ID, returns true if successful.
     *
     *  Note: It is safe to create the advertisement as a local variable if setInstanceData
     *  is called before exiting the code block as the data will be copied.
     */
    if (pAdvertising->setInstanceData(0, extAdv)) {
        /**
         *  NimBLEExtAdvertising::start takes the advertisement instance ID to start
         *  and a duration in milliseconds or a max number of advertisements to send (or both).
         */
        if (pAdvertising->start(0, advTime)) {
            Serial.printf("Started advertising\n");
        } else {
            Serial.printf("Failed to start advertising\n");
        }
    } else {
        Serial.printf("Failed to register advertisement data\n");
    }
}

void loop() {
    nimble_port_teensy_pump();
}
